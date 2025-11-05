#pragma once
#include <filesystem>
#include <string>
#include <vector>
#include <optional>
#include <iostream>
#include <system_error>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h> // CommandLineToArgvW if needed
#include <stdexcept> // For runtime_error
#else
#include <unistd.h>
#include <sys/wait.h>
#endif

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
static std::wstring windows_quote_arg(const std::string& s) {
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
inline std::optional<int> run_program_and_wait(const fs::path& exe, const std::vector<std::string>& args) {
#ifdef _WIN32
  // Build wide command line: exe path followed by quoted args
  std::wstring cmdline; // CreateProcessW expects mutable buffer, so we'll build it here
  // Quote program path (may contain spaces)
  std::wstring exe_quoted = windows_quote_arg(exe.filename().string());
  cmdline += exe_quoted;

  for (const auto& a : args) {
    cmdline += L' ';
    cmdline += windows_quote_arg(a);
  }

  // Must pass writable buffer to CreateProcessW
  std::vector<wchar_t> cmdbuf(cmdline.begin(), cmdline.end());
  cmdbuf.push_back(L'\0');

  STARTUPINFOW si{};
  si.cb = sizeof(si);
  PROCESS_INFORMATION pi{};
  BOOL ok = CreateProcessW(
      windows_widen(exe.string()).c_str(),        // lpApplicationName (nullptr -> use command line)
      cmdbuf.data(),           // lpCommandLine (writable)
      nullptr,                 // lpProcessAttributes
      nullptr,                 // lpThreadAttributes
      FALSE,                   // bInheritHandles
      0,                       // dwCreationFlags
      nullptr,                 // lpEnvironment
      nullptr,                 // lpCurrentDirectory
      &si, &pi
  );
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