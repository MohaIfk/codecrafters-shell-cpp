#pragma once
#include <filesystem>
#include <string>
#include <vector>
#include <optional>
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <stdexcept> // For runtime_error
#else
#include <unistd.h>
#include <sys/wait.h>
#endif
#include <fcntl.h> // For both os

namespace fs = std::filesystem;

inline std::optional<fs::path> get_home_directory() {
#ifdef _WIN32
  // First try USERPROFILE
  if (const char* userprofile = std::getenv("USERPROFILE")) {
    return fs::path(userprofile);
  }
  // Fallback: HOMEDRIVE + HOMEPATH
  const char* homedrive = std::getenv("HOMEDRIVE");
  const char* homepath = std::getenv("HOMEPATH");
  if (homedrive && homepath) {
    return fs::path(std::string(homedrive) + std::string(homepath));
  }
  return std::nullopt;
#else
  // POSIX: $HOME
  if (const char* home = std::getenv("HOME")) {
    return fs::path(home);
  }
  return std::nullopt;
#endif
}

#ifdef _WIN32
// Quote an argument for Windows CreateProcess command line.
// This is a conservative quoting implementation following MS rules:
// - Surround by double quotes if it contains space or tab or is empty.
// - Escape backslashes before a closing quote.
inline std::wstring windows_quote_arg(const std::string& s) {
  // convert UTF-8 std::string to wide string (UTF-16) for CreateProcessW
  int wlen = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
  std::wstring ws;
  ws.resize(wlen);
  MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &ws[0], wlen);

  bool needQuotes = ws.empty();
  for (wchar_t c : ws) {
    if (c == L' ' || c == L'\t') { needQuotes = true; break; }
  }
  if (!needQuotes) return ws;

  std::wstring out;
  out.push_back(L'"');
  int backslashes = 0;
  for (wchar_t c : ws) {
    if (c == L'\\') {
      ++backslashes;
      out.push_back(c);
    } else if (c == L'"') {
      // Escape all backslashes before a quote, then escape the quote
      out.append(backslashes, L'\\');
      out.push_back(L'\\');
      out.push_back(L'"');
      backslashes = 0;
    } else {
      backslashes = 0;
      out.push_back(c);
    }
  }
  // Escape trailing backslashes
  if (backslashes > 0) out.append(backslashes, L'\\');
  out.push_back(L'"');
  return out;
}

inline std::wstring windows_widen(const std::string& str) {
    if (str.empty()) {
        return {};
    }

    // MultiByteToWideChar takes int for length, so we must cap input size.
    if (str.length() > static_cast<size_t>(INT_MAX)) {
        throw std::runtime_error("String is too large to convert.");
    }

    const int input_length = static_cast<int>(str.length());

    // We tell the API our string is UTF-8 (CP_UTF8).
    // MB_ERR_INVALID_CHARS makes the function fail if it encounters invalid UTF-8.
    int required_size = MultiByteToWideChar(
        CP_UTF8,                      // Code page (UTF-8)
        MB_ERR_INVALID_CHARS,         // Error flags
        str.c_str(),                  // Input string
        input_length,                 // Length of input string (bytes)
        nullptr,                      // No output buffer
        0                             // Request required buffer size
    );

    if (required_size == 0) {
        throw std::runtime_error("Failed to get buffer size for string conversion.");
    }

    std::wstring wide_str;
    wide_str.resize(required_size);

    // Note: &wide_str[0] is a non-const pointer to the internal buffer.
    int bytes_written = MultiByteToWideChar(
        CP_UTF8,                      // Code page (UTF-8)
        MB_ERR_INVALID_CHARS,         // Error flags
        str.c_str(),                  // Input string
        input_length,                 // Length of input string (bytes)
        &wide_str[0],                 // Output buffer
        required_size                 // Size of output buffer (wchar_t's)
    );

    if (bytes_written == 0) {
        throw std::runtime_error("Failed to convert string.");
    }

    return wide_str;
}
#endif

static std::string join_arguments_for_execv(const std::vector<std::string>& args) {
  // Not used for execv (we give array), but useful for diagnostics
  std::string s;
  for (size_t i = 0; i < args.size(); ++i) {
    if (i) s += ' ';
    s += args[i];
  }
  return s;
}

// Runs program at `exe` with arguments `args` (args does NOT need to include exe; we will include it).
// Returns process exit code on success, or std::nullopt on spawn error.
inline std::optional<int> run_program_and_wait(const fs::path& exe, const std::vector<std::string>& args, const std::vector<Redirection>& redir) {
#ifdef _WIN32
  // Build wide command line: exe path followed by quoted args
  // CreateProcessW expects mutable buffer, so we'll build it here
  // Quote program path (may contain spaces)
  std::wstring cmdline = windows_quote_arg(exe.filename().string());

  for (const auto& a : args) {
    cmdline += L' ';
    cmdline += windows_quote_arg(a);
  }

  // Must pass writable buffer to CreateProcessW
  std::vector<wchar_t> cmdbuf(cmdline.begin(), cmdline.end());
  cmdbuf.push_back(L'\0');

  STARTUPINFOW si{};
  si.cb = sizeof(si);
  if (!redir.empty()) si.dwFlags = STARTF_USESTDHANDLES;

  // Open files for redirection
  HANDLE hFiles[3] = { INVALID_HANDLE_VALUE, INVALID_HANDLE_VALUE, INVALID_HANDLE_VALUE };
  for (auto &r : redir) {
    DWORD access = (r.fd == 0) ? GENERIC_READ : GENERIC_WRITE;
    DWORD creation = r.append ? OPEN_ALWAYS : CREATE_ALWAYS;
    HANDLE hFile = CreateFileW(
      windows_widen(r.filename).c_str(),
      access,
      FILE_SHARE_READ | FILE_SHARE_WRITE,
      nullptr,
      creation,
      FILE_ATTRIBUTE_NORMAL,
      nullptr
    );
    if (hFile == INVALID_HANDLE_VALUE) {
      for (HANDLE h : hFiles) if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
      return std::nullopt;
    }

    if (r.append) SetFilePointer(hFile, 0, nullptr, FILE_END);

    if (r.fd == 0) si.hStdInput = hFile;
    else if (r.fd == 1) si.hStdOutput = hFile;
    else if (r.fd == 2) si.hStdError = hFile;

    if (r.fd >=0 && r.fd <=2) hFiles[r.fd] = hFile;
  }

  PROCESS_INFORMATION pi{};
  BOOL ok = CreateProcessW(
      windows_widen(exe.string()).c_str(),        // lpApplicationName (nullptr -> use command line)
      cmdbuf.data(),           // lpCommandLine (writable)
      nullptr,                 // lpProcessAttributes
      nullptr,                 // lpThreadAttributes
      (redir.empty())
        ? FALSE : TRUE,        // bInheritHandles
      0,                       // dwCreationFlags
      nullptr,                 // lpEnvironment
      nullptr,                 // lpCurrentDirectory
      &si, &pi
  );

  for (HANDLE h : hFiles) if (h != INVALID_HANDLE_VALUE) CloseHandle(h); // close handles in parent

  if (!ok) {
    // DWORD err = GetLastError();
    // std::cerr << "CreateProcessW failed: " << err << "\n";
    return std::nullopt;
  }

  // Wait for process to finish
  WaitForSingleObject(pi.hProcess, INFINITE);

  DWORD exitCode = 0;
  if (!GetExitCodeProcess(pi.hProcess, &exitCode)) {
    // Close handles
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return std::nullopt;
  }

  CloseHandle(pi.hProcess);
  CloseHandle(pi.hThread);
  return static_cast<int>(exitCode);
#else
  // POSIX: fork + execv
  pid_t pid = fork();
  if (pid < 0) {
    perror("fork");
    return std::nullopt;
  }
  if (pid == 0) {
    // Apply redirections
    for (auto &r : redir) {
      int flags = (r.fd == 0) ? O_RDONLY : (O_CREAT | O_WRONLY | (r.append ? O_APPEND : O_TRUNC));
      int fd = open(r.filename.c_str(), flags, 0644);
      if (fd < 0) { perror("open"); _exit(127); }
      if (dup2(fd, r.fd) < 0) { perror("dup2"); _exit(127); }
      close(fd);
    }

    // Child: build argv array: argv[0] = exe, argv[1..] = args..., argv[n] = nullptr
    std::vector<char*> argv;
    argv.reserve(args.size() + 2);
    argv.push_back(const_cast<char*>(exe.filename().string().c_str()));
    for (const auto& a : args) argv.push_back(const_cast<char*>(a.c_str()));
    argv.push_back(nullptr);

    // execv replaces the child process
    execv(exe.string().c_str(), argv.data());
    // if execv returns, there was an error:
    _exit(127);
  } else {
    // Parent: wait for child
    int status = 0;
    if (waitpid(pid, &status, 0) == -1) {
      perror("waitpid");
      return std::nullopt;
    }
    if (WIFEXITED(status)) {
      return WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
      int sig = WTERMSIG(status);
      // std::cerr << "Child terminated by signal " << sig << "\n";
      // Return some sentinel (negative) or map to code
      return 128 + sig;
    } else {
      // Other cases
      return std::nullopt;
    }
  }
#endif
}

inline std::vector<std::string> parse_args(const std::string &input) {
    std::vector<std::string> args;
    std::string current;
    bool in_single = false;
    bool in_double = false;
    size_t i = 0;

    auto push_current = [&]() {
        if (!current.empty()) {
            args.push_back(current);
            current.clear();
        }
    };

    while (i < input.size()) {
        char c = input[i];

        if (in_single) {
            if (c == '\'') {
                in_single = false; // end single quote
            } else {
                current += c;
            }
            i++;
        } else if (in_double) {
            if (c == '"') {
                in_double = false; // end double quote
                i++;
            } else if (c == '\\') {
                i++;
                if (i < input.size()) {
                    // Only escape certain chars in double quotes, similar to Bash
                    char next = input[i];
                    if (next == '"' || next == '\\' || next == '$' || next == '`') {
                        current += next;
                    } else {
                        current += '\\';
                        current += next;
                    }
                    i++;
                } else {
                    // trailing backslash at end
                    current += '\\';
                }
            } else {
                current += c;
                i++;
            }
        } else { // not in any quote
            if (c == '\'') {
                in_single = true;
                i++;
            } else if (c == '"') {
                in_double = true;
                i++;
            } else if (c == '\\') {
                i++;
                if (i < input.size()) {
                    current += input[i++];
                } else {
                    current += '\\';
                }
            } else if (c == ' ' || c == '\t') {
                push_current();
                i++;
                // skip consecutive spaces
                while (i < input.size() && (input[i] == ' ' || input[i] == '\t')) i++;
            } else {
                current += c;
                i++;
            }
        }
    }

    if (in_single || in_double) {
        throw std::runtime_error("Unclosed quote in command line");
    }

    push_current();
    return args;
}

// Helper for redirection for builtin
class ScopedRedir {
public:
  explicit ScopedRedir(const std::vector<Redirection>& redirs) {
    for (auto &r : redirs) {
      int target_fd = r.fd;
#ifdef _WIN32
      int flags = _O_TEXT | (r.fd == 0 ? _O_RDONLY : _O_WRONLY | _O_CREAT);
      if (r.append) flags |= _O_APPEND; else flags |= _O_TRUNC;
      int fd = _wopen(windows_widen(r.filename).c_str(), flags, _S_IREAD | _S_IWRITE);
      if (fd < 0) continue;

      old_fds.push_back(target_fd);
      old_copies.push_back(_dup(target_fd));
      _dup2(fd, target_fd);
      _close(fd);
#else
      int flags = (r.fd == 0 ? O_RDONLY : O_CREAT | O_WRONLY);
      flags |= r.append ? O_APPEND : O_TRUNC;
      int fd = open(r.filename.c_str(), flags, 0644);
      if (fd < 0) continue;

      old_fds.push_back(target_fd);
      old_copies.push_back(dup(target_fd));
      dup2(fd, target_fd);
      close(fd);
#endif
    }
  }

  ~ScopedRedir() {
    for (size_t i = 0; i < old_fds.size(); i++) {
#ifdef _WIN32
      _dup2(old_copies[i], old_fds[i]);
      _close(old_copies[i]);
#else
      dup2(old_copies[i], old_fds[i]);
      close(old_copies[i]);
#endif
    }
  }

private:
  std::vector<int> old_fds;
  std::vector<int> old_copies;
};