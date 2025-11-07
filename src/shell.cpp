//
// Created by theGhost on 05/11/2025.
//

#include "shell.h"
#include "utils.h"
#include "inputHelper.h"
#include <optional>
#include  <fstream>

#ifdef _MSC_VER
  #include <cstring> // for errno_t
  #include <malloc.h> // for _dupenv_s (allocates with malloc)
#endif

shell::shell() {
  path = getenv_safe("PATH").value_or("");
  path_dirs = split_view(path, PATH_LIST_SEPARATOR);
  std::error_code ec;
  working_directory_ = fs::current_path(ec);
  if (ec) {
    // fallback to root
    working_directory_ = fs::path("/");
  }

  populate_executable_cache();
  history_file_path = getenv_safe("HISTFILE").value_or("");
  read_history();
}
shell::~shell() {
  write_history();
};

void shell::exit(int n) {
  write_history();
  std::exit(n);
}

std::vector<std::string> shell::split(const std::string &string, char c) {
  std::vector<std::string> parts;
  std::string::size_type pos = 0;
  std::string::size_type prev = 0;
  while ((pos = string.find(c, prev)) != std::string::npos) {
    parts.push_back(string.substr(prev, pos - prev));
    prev = pos + 1;
  }
  parts.push_back(string.substr(prev));
  return parts;
}

std::vector<std::string_view> shell::split_view(const std::string &string, char c) {
  std::vector<std::string_view> parts;
  std::string::size_type pos = 0;
  std::string::size_type prev = 0;
  while ((pos = string.find(c, prev)) != std::string::npos) {
    parts.emplace_back(string.data()+prev, pos-prev);
    prev = pos + 1;
  }
  // last token (may be empty)
  parts.emplace_back(string.data() + prev, string.size() - prev);
  return parts;
}

// Helper function to check if a file is executable
bool shell::is_executable(const std::filesystem::path &p) {
  std::error_code ec;
  if (!fs::is_regular_file(p, ec)) {
    return false;
  }

#ifdef _WIN32
  std::string ext = p.extension().string();
  // Convert extension to lower for case-insensitive comparison
  std::ranges::transform(ext, ext.begin(),
      [](unsigned char c){ return std::tolower(c); });

  return (ext == ".exe" || ext == ".bat" || ext == ".cmd");
#else
  auto perms = fs::status(p).permissions();
  return ((perms & fs::perms::owner_exec) != fs::perms::none) ||
         ((perms & fs::perms::group_exec) != fs::perms::none) ||
         ((perms & fs::perms::others_exec) != fs::perms::none);
#endif
}

void shell::populate_executable_cache() {
  executable_cache.clear();

  for (const auto& dir : path_dirs) {
    if (!fs::exists(dir) || !fs::is_directory(dir)) {
      continue;
    }

    // I use error_code to ignore permission-denied errors
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(dir, ec)) {
      if (is_executable(entry.path())) {
        executable_cache.insert(entry.path().filename().string());
      }
    }
  }
}

void shell::read_history() {
  if (history_file_path.empty()) return;
  std::ifstream ifs(history_file_path);
  if (!ifs.is_open()) {
    std::cout << "history: " << history_file_path << ": No such file or directory" << std::endl;
    return;
  }

  std::string line;
  while (std::getline(ifs, line)) {
    if (!line.empty()) {
      history_list.push_back(line);
    }
  }
  global_commited_history_index = history_list.size();
}

void shell::write_history() {
  if (history_file_path.empty() || history_list.empty() || global_commited_history_index == history_list.size()) return;
  std::ofstream ofs;
  if (std::error_code ec; fs::exists(history_file_path, ec)) {
    // append
    ofs = std::ofstream(history_file_path, std::ios::app);
  } else {
    // create and write
    ofs = std::ofstream(history_file_path);
  }
  if (!ofs.is_open()) {
    std::cout << "history: " << history_file_path << ": No such file or directory" << std::endl;
    return;
  }

  for (size_t i = global_commited_history_index; i < history_list.size(); i++) {
    ofs << history_list[i] << "\n";
  }

  ofs.flush();
  global_commited_history_index = history_list.size();
}

/**
 * Finds the Longest Common Prefix (LCP) among a set of strings.
 */
std::string shell::find_lcp(const std::set<std::string>& matches) {
  if (matches.empty()) {
    return "";
  }

  std::string lcp = *matches.begin();

  for (const auto& s : matches) {
    size_t i = 0;
    // Find the first character that differs
    while (i < lcp.length() && i < s.length() && lcp[i] == s[i]) {
      i++;
    }

    lcp = lcp.substr(0, i);

    if (lcp.empty()) {
      break;
    }
  }
  return lcp;
}

void shell::handle_completion(std::string& line, bool second_tab) const {
  // Guard: Only complete the command itself, not its arguments
  // (We can extend this later, but for now, it's safer)
  if (line.find(' ') != std::string::npos) {
    std::cout << "\x07"; // Ring bell I like it hhhh
    std::cout.flush();
    return;
  }

  std::set<std::string> matches; // Use std::set for auto-deduplication

  for (const auto& cmd : builtins) {
    if (cmd.starts_with(line)) {
      matches.insert(cmd);
    }
  }

  for (const auto& cmd : executable_cache) {
    if (cmd.rfind(line, 0) == 0) {
      matches.insert(cmd);
    }
  }

  if (matches.empty()) {
    std::cout << "\x07"; // Ring bell
    std::cout.flush();
  } else if (matches.size() == 1) {
    std::string completion = *matches.begin() + " "; // Get the single item
    std::string remainder = completion.substr(line.length()); // Find the part we're missing

    line = completion;
    std::cout << remainder;
    std::cout.flush();
  } else {
    std::string common_prefix = find_lcp(matches);
    if (common_prefix.length() > line.length()) {
      std::string remainder = common_prefix.substr(line.length());
      line = common_prefix;
      std::cout << remainder;
      std::cout.flush();
    } else {
      if (second_tab) {
        std::cout << "\n";
        int i = 0;
        for (const auto& match : matches) {
          if (i > 0) std::cout << "  ";
          std::cout << match;
          i++;
        }
        std::cout << "\n$ " << line;
        std::cout.flush();
      } else {
        std::cout << "\x07";
        std::cout.flush();
      }
    }
  }
}

[[noreturn]] void shell::run() {
  enableRawMode();
  std::string line;

  while (true) {
    std::cout << "$ ";
    std::cout.flush();
    line.clear();
    bool second_tab = false;

    size_t history_index = history_list.size();
    std::string current_line_backup;

    while (true) {
      int c = get_char_raw();

#ifdef _WIN32
      // 224 (0xE0) is the prefix for special keys
      if (c == 224) {
        int next_char = get_char_raw();
        if (next_char == 72) {
          // UP arrow
          if (!history_list.empty() && history_index > 0) {
            if (history_index == history_list.size()) {
              // first time pressing up saving the current line
              current_line_backup = line;
            }
            history_index--;

            // Redraw line
            std::cout << "\r$ " << std::string(line.size(), ' ') << "\r$ ";
            line = history_list[history_index];
            std::cout << line;
            std::cout.flush();
          }
        } else if (next_char == 80) {
          // DOWN arrow
          if (history_index < history_list.size()) {
            history_index++;

            std::cout << "\r$ " << std::string(line.size(), ' ') << "\r$ ";

            if (history_index == history_list.size()) {
              // Reached the bottom, restore the original line
              line = current_line_backup;
            } else {
              line = history_list[history_index];
            }
            std::cout << line;
            std::cout.flush();
          }
        }
#else
      if (c == 27) { // ESCAPE key
        int next1 = get_char_raw();
        if (next1 == '[') {
          int next2 = get_char_raw();
          if (next2 == 'A') {
            // UP arrow
            if (!history_list.empty() && history_index > 0) {
              if (history_index == history_list.size()) {
                // first time pressing up saving the current line
                current_line_backup = line;
              }
              history_index--;

              // Clear the current line
              std::cout << "\r$ " << std::string(line.size(), ' ') << "\r$ ";
              line = history_list[history_index];
              std::cout << line;
              std::cout.flush();
            }
          } else if (next2 == 'B') {
            // DOWN arrow
            if (history_index < history_list.size()) {
              history_index++;

              std::cout << "\r$ " << std::string(line.size(), ' ') << "\r$ ";

              if (history_index == history_list.size()) {
                // Reached the bottom, restore the original line
                line = current_line_backup;
              } else {
                line = history_list[history_index];
              }
              std::cout << line;
              std::cout.flush();
            }
          }
        }
#endif
        second_tab = false;
        continue;
      } else if (c == '\t') {
        handle_completion(line, second_tab);
        second_tab = (!second_tab);
        continue;
      } else if (c == '\n' || c == '\r') {
        std::cout << std::endl;
        if (line.empty()) break;

        // Prevent consecutive duplicates
        // if (history_list.empty() || history_list.back() != line) {
        history_list.push_back(line);
        // }

        try {
          std::vector<Command> pipeline = parse_line_to_pipeline(line);
          dispatch_pipeline(pipeline);
        } catch (const std::runtime_error& e) {
          std::cerr << "Error: " << e.what() << std::endl;
        }
        // The 'exit' check is now inside dispatch_pipeline
        break;
      } else if (c == 127 || c == '\b') {
        if (!line.empty()) {
          line.pop_back();
          std::cout << "\b \b";
          std::cout.flush();
        }
      } else {
        line += static_cast<char>(c);
        std::cout << static_cast<char>(c);
        std::cout.flush();
      }
      second_tab = false;
    }
  }

  disableRawMode();
}

/**
 * Parses a full command line, splitting it by '|' into a pipeline of commands.
 * Redirections (>, >>) are attached to the command they immediately follow.
 */
std::vector<Command> shell::parse_line_to_pipeline(const std::string &line) {
  auto args = parse_args(line);
  std::vector<Command> pipeline;
  Command current_command;

  size_t i = 0;
  std::smatch m;
  std::regex re(R"((\d*)?(>>|>))"); // matches optional digit + > or >>
  while (i < args.size()) {
    if (args[i] == "|") {
      if (current_command.args.empty()) {
        // Error: '||' or '| cmd'
        throw std::runtime_error("Invalid pipeline: empty command before '|'");
      }
      pipeline.push_back(current_command);
      current_command = Command{}; // Reset for the next command
      i++;
    } else if (std::regex_match(args[i], m, re)) {
      // Found a redirection operator
      int fd = 1; // default
      if (!m[1].str().empty()) fd = std::stoi(m[1].str());
      bool append = (m[2].str() == ">>");
      if (i + 1 >= args.size()) throw std::runtime_error("No filename provided for redirection");
      // Add redirection to the current command
      current_command.redirections.push_back({fd, args[i+1], append});
      i += 2; // skip > and filename
    } else {
      // Just a regular argument
      current_command.args.push_back(args[i]);
      i++;
    }
  }

  // Add the last command to the pipeline
  if (current_command.args.empty()) {
    if (!pipeline.empty()) {
      // Error: 'cmd |'
      throw std::runtime_error("Invalid pipeline: empty command at the end");
    }
    // else: just an empty line, which is fine
  } else {
    pipeline.push_back(current_command);
  }
  return pipeline;
}

std::optional<fs::path> shell::get_path(const std::string& name) const {
  for (auto& path_dir: path_dirs) {
    auto full_path = fs::path(path_dir) / name;
#ifdef _WIN32
    // Check for .exe, .bat, .cmd extensions on Windows
    std::vector<std::string> exts = {".exe", ".bat", ".cmd"};

    std::string name_lower = name;
    std::ranges::transform(name_lower, name_lower.begin(), tolower);
    bool name_has_ext = false;
    for (const auto& e : exts) {
      if (name_lower.size() >= e.size() && name_lower.compare(name_lower.size() - e.size(), e.size(), e) == 0) {
        name_has_ext = true;
        break;
      }
    }
    if (name_has_ext) {
      if (fs::exists(full_path) && fs::is_regular_file(full_path)) {
        return full_path;
      }
    } else {
      for (const auto& ext : exts) {
        fs::path candidate = full_path;
        candidate += ext; // append extension
        if (fs::exists(candidate) && fs::is_regular_file(candidate)) {
          return candidate;
        }
      }
    }
#else
    if (fs::exists(full_path)) {
      auto p = fs::status(full_path).permissions();
      bool can_exec =
        ((p & fs::perms::owner_exec) != fs::perms::none) ||
        ((p & fs::perms::group_exec) != fs::perms::none) ||
        ((p & fs::perms::others_exec) != fs::perms::none)
        ;
      if (can_exec) {
        return full_path;
      }
    }
#endif
  }
  return std::nullopt;
}

const fs::path &shell::working_directory() {
  return working_directory_;
}

bool shell::set_working_directory(const fs::path &dir) {
  fs::path new_path;
  if (dir.string() == "~") {
    if (auto home_dir = get_home_directory()) {
      new_path = home_dir.value();
    }
  } else {
    if (dir.is_absolute()) {
      new_path = dir;
    } else {
      new_path = working_directory_ / dir;
    }
  }

  if (!fs::exists(new_path) || !fs::is_directory(new_path)) {
    std::cout << "cd: " << dir.string() << ": No such file or directory" << std::endl;
    return false;
  }

  try {
    working_directory_ = fs::canonical(new_path); // resolve symlinks
    return true;
  } catch (const fs::filesystem_error& e) {
    std::cout << "cd: failed: " << e.what() << "\n";
    return false;
  }
}

void shell::execute_builtin(const Command &command) {
  ScopedRedir redir(command.redirections);

  const auto& args = command.args;

  if (args[0] == "echo") {
    for (size_t i = 1; i < args.size(); i++) {
      std::cout << args[i];
      if (i + 1 < args.size()) std::cout << ' ';
    }
    std::cout << std::endl;
    return;
  }
  if (args[0] == "pwd") {
    std::cout << working_directory_.string() << std::endl;
    return;
  }
  if (args[0] == "cd") {
    if (args.size() != 2) {
      if (args.size() == 1) {
        set_working_directory(fs::path("~")); // Default
        return;
      }
      std::cout << "cd: invalid number of args" << std::endl;
      return;
    }
    set_working_directory(fs::path(args[1]));
    return;
  }
  if (args[0] == "type") {
    if (args.size() != 2) {
      std::cout << "Invalid arguments" << args[0] << std::endl;
    }
    if (builtins.contains(args[1])) {
      std::cout << args[1] << " is a shell builtin" << std::endl;
      return;
    }
    if (auto p = get_path(args[1])) {
      std::cout << args[1] << " is " << p.value().string() << std::endl;
      return;
    }
    std::cout << args[1] << ": not found" << std::endl;
    return;
  }
  if (args[0] == "history") {
    if (args.size() == 1) {
      // history
      for (size_t i = 0; i < history_list.size(); ++i) {
        std::cout << "  " << (i + 1) << "\t" << history_list[i] << std::endl;
      }
    } else if (args.size() == 2) {
      if (args[1] == "-c") {
        // history -c
        history_list.clear();
      } else {
        // history n
        int n = 0;
        try {
          n = std::stoi(args[1]);
        } catch (const std::invalid_argument&) {
          std::cout << "history: numeric argument required" << std::endl;
          return;
        } catch (const std::out_of_range&) {
          std::cout << "history: argument out of range" << std::endl;
          return;
        }
        if (n < 1) {
          std::cout << "Invalid history argument: out of range" << std::endl;
          return;
        }

        auto num_to_show = static_cast<size_t>(n);
        size_t start_index = (num_to_show >= history_list.size()) ? 0 : (history_list.size() - num_to_show);

        // only the last n entries.
        for (auto i = start_index; i < history_list.size(); ++i) {
          std::cout << "  " << (i + 1) << "\t" << history_list[i] << std::endl;
        }
      }
    } else if (args.size() == 3) {
      if (args[1] == "-d") {
        // history -d offset
        // Delete the history entry at position offset. If offset is positive, it should be specified as it appears when the history
        // is displayed. If offset is negative, it is interpreted as relative to one greater than the last history position,
        // so negative indices count back from the end of the history, and an index of ‘-1’ refers to the current history -d command.
        int index;
        try {
          index = std::stoi(args[2]);
        } catch (const std::invalid_argument&) {
          std::cout << "history: numeric argument required" << std::endl;
          return;
        } catch (const std::out_of_range&) {
          std::cout << "history: argument out of range" << std::endl;
          return;
        }
        if (index < 1 || index > history_list.size()) {
          std::cout << "Invalid history index" << std::endl;
        }
        history_list.erase(history_list.begin() + index - 1);
      } else if (args[1] == "-r") {
        // history -r [filename]
        // Read the history file and append its contents to the history list.
        std::ifstream f(args[2]);
        if (!f.is_open()) {
          std::cout << "history: " << args[2] << ": No such file or directory" << std::endl;
          return;
        }

        std::string line;
        while (std::getline(f, line)) {
          if (!line.empty()) {
            history_list.push_back(line);
          }
        }
        commited_history_index = history_list.size();
      } else if (args[1] == "-w") {
        // history -w [filename]
        // Write the current history list to the history file, overwriting the history file.
        std::ofstream f(args[2]);
        if (!f.is_open()) {
          std::cout << "history: " << args[2] << ": No such file or directory" << std::endl;
          return;
        }

        for (auto &h: history_list) {
          f << h << "\n";
        }
        f.flush();
        commited_history_index = history_list.size();
      } else if (args[1] == "-a") {
        // history -a [filename]
        // Append the "new" history lines to the history file. These are history lines entered since the beginning
        // of the current Bash session, but not already appended to the history file.
        if (commited_history_index == history_list.size()) return; // can't be true (because this request is also added to the list so history_list.size() will always be at least commited_history_index + 1) but it won't hurt
        std::ofstream f(args[2], std::ios::app);
        if (!f.is_open()) {
          std::cout << "history: " << args[2] << ": No such file or directory" << std::endl;
          return;
        }

        for (size_t i = commited_history_index; i < history_list.size(); ++i) {
          f << history_list[i] << "\n";
        }
        f.flush();
        commited_history_index = history_list.size();
      }
    }
    return;
  }
}

void shell::execute_simple_command(const Command &command) {
  const auto& args = command.args;

  // Check for built-in
  if (builtins.contains(args[0])) {
    execute_builtin(command);
    return;
  }

  // Not a built-in, must be an external command
  if (auto p = get_path(args[0])) {
    std::vector<std::string> passArgs;
    for (int i = 1; i < args.size(); i++) passArgs.emplace_back(args[i]);

    // run_program_and_wait handles file redirections for this single process
    auto exitCodeOpt = run_program_and_wait(p.value(), passArgs, command.redirections);
    if (!exitCodeOpt) {
      std::cerr << "Failed to spawn program\n";
    }
    return;
  }

  std::cout << args[0] << ": command not found" << std::endl;
}

void shell::dispatch_pipeline(const std::vector<Command> &pipeline) {
  if (pipeline.empty()) {
    return;
  }

  if (pipeline.size() == 1) {
    if (!pipeline[0].args.empty() && pipeline[0].args[0] == "exit") {
      disableRawMode();
      if (pipeline[0].args.size() == 2) {
        try {
          exit(std::stoi(pipeline[0].args[1]));
        } catch (...) {
          std::cout << "Invalid exit code." << std::endl;
        }
      }
      exit(0); // Default exit
    }
    // Just a single command, run it normally
    execute_simple_command(pipeline[0]);
    return;
  }

  // This is a multi-command pipeline.
  // Execute the full pipeline
  execute_pipeline(pipeline);
}

// The move of this function to utils.h is canceled du to depending on builtins, execute_builtin
void shell::execute_pipeline(const std::vector<Command> &pipeline) {
#ifdef _WIN32
  std::vector<PROCESS_INFORMATION> process_infos;
  std::vector<HANDLE> handles_to_close; // All handles the parent must close

  HANDLE in_handle = GetStdHandle(STD_INPUT_HANDLE);

  for (size_t i = 0; i < pipeline.size(); ++i) {
    const auto& cmd = pipeline[i];
    bool is_last = (i == pipeline.size() - 1);

    HANDLE hReadPipe = INVALID_HANDLE_VALUE;
    HANDLE hWritePipe = INVALID_HANDLE_VALUE;

    // 1. Create a pipe for this command's output, unless it's the last one
    if (!is_last) {
      SECURITY_ATTRIBUTES sa;
      sa.nLength = sizeof(SECURITY_ATTRIBUTES);
      sa.bInheritHandle = TRUE; // Child process must inherit write handle
      sa.lpSecurityDescriptor = nullptr;

      if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) {
        std::cerr << "Failed to create pipe for command '" << cmd.args[0] << "'." << std::endl;
        return;
      }
      // Make the READ handle *non-inheritable* so the *next* child
      // doesn't accidentally inherit it.
      if (!SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0)) {
        std::cerr << "Failed to set handle information for command '" << cmd.args[0] << "'." << std::endl;
        return;
      }
      handles_to_close.push_back(hReadPipe);
      handles_to_close.push_back(hWritePipe);
    }

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;

    // 2. Set up STD handles
    si.hStdInput = (i > 0) ? in_handle : GetStdHandle(STD_INPUT_HANDLE);
    si.hStdOutput = (!is_last) ? hWritePipe : GetStdHandle(STD_OUTPUT_HANDLE);
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);

    std::vector<HANDLE> redir_file_handles;
    // Apply file redirections (these will override the pipe handles)
    for (auto &r : cmd.redirections) {
      DWORD access = (r.fd == 0) ? GENERIC_READ : GENERIC_WRITE;
      DWORD creation = r.append ? OPEN_ALWAYS : CREATE_ALWAYS;
      HANDLE hFile = CreateFileW(
        windows_widen(r.filename).c_str(), access,
        FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, creation,
        FILE_ATTRIBUTE_NORMAL, nullptr
      );
      if (hFile == INVALID_HANDLE_VALUE) { /* error */ continue; }
      if (r.append) SetFilePointer(hFile, 0, nullptr, FILE_END);

      redir_file_handles.push_back(hFile);
      handles_to_close.push_back(hFile); // Parent must close this

      if (r.fd == 0) si.hStdInput = hFile;
      else if (r.fd == 1) si.hStdOutput = hFile;
      else if (r.fd == 2) si.hStdError = hFile;
    }

    // Find executable
    auto exe_path_opt = get_path(cmd.args[0]);
    if (!exe_path_opt) {
      std::cerr << cmd.args[0] << ": command not found" << std::endl;
      break;
    }

    // Build command line
    std::wstring cmdline = windows_quote_arg(exe_path_opt.value().filename().string());
    for (size_t j = 1; j < cmd.args.size(); ++j) {
      cmdline += L' ';
      cmdline += windows_quote_arg(cmd.args[j]);
    }
    std::vector<wchar_t> cmdbuf(cmdline.begin(), cmdline.end());
    cmdbuf.push_back(L'\0');

    // Create the process
    PROCESS_INFORMATION pi{};
    BOOL ok = CreateProcessW(
        windows_widen(exe_path_opt.value().string()).c_str(),
        cmdbuf.data(),
        nullptr, nullptr,
        TRUE, // bInheritHandles = TRUE
        0, nullptr, nullptr,
        &si, &pi
    );

    // Parent-side cleanup for this loop
    if (i > 0) CloseHandle(in_handle); // Close previous read pipe
    if (!is_last) CloseHandle(hWritePipe); // Close current write pipe
    for (HANDLE h : redir_file_handles) CloseHandle(h); // Close file handles

    if (!ok) {
      std::cerr << "CreateProcessW failed for " << cmd.args[0] << "\n";
      if (!is_last) CloseHandle(hReadPipe); // Must close read pipe
      break; // Stop pipeline
    }

    process_infos.push_back(pi);
    if (!is_last) {
      in_handle = hReadPipe; // Pass read pipe to next child
    }
  }

  // Wait for all child processes to finish
  std::vector<HANDLE> p_handles;
  for (auto& pi : process_infos) p_handles.push_back(pi.hProcess);

  if (!p_handles.empty()) {
    WaitForMultipleObjects(static_cast<DWORD>(p_handles.size()), p_handles.data(), TRUE, INFINITE);
  }

  // Final cleanup
  for (auto& pi : process_infos) {
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
  }
  // Close any pipe/file handles that were opened
  for (HANDLE h : handles_to_close) {
    CloseHandle(h);
  }
#else
  // POSIX: fork + execv + pipe
  std::vector<pid_t> pids;
  int in_fd = STDIN_FILENO; // Input for the *first* command
  int pipe_fds[2];

  for (size_t i = 0; i < pipeline.size(); ++i) {
    const auto& cmd = pipeline[i];
    bool is_last = (i == pipeline.size() - 1);

    // Create a pipe, unless it's the last command
    if (!is_last) {
      if (pipe(pipe_fds) < 0) {
        perror("pipe");
        return;
      }
    }

    // Fork
    pid_t pid = fork();
    if (pid < 0) {
      perror("fork");
      return;
    }

    if (pid == 0) { // --- Child Process ---
      // Set up input
      if (i > 0) {
        if (dup2(in_fd, STDIN_FILENO) < 0) { perror("dup2"); _exit(127); }
        close(in_fd); // Close original
      }

      // Set up output
      if (!is_last) {
        if (dup2(pipe_fds[1], STDOUT_FILENO) < 0) { perror("dup2"); _exit(127); }
        // Child doesn't need pipe ends after dup2
        close(pipe_fds[0]);
        close(pipe_fds[1]);
      }

      // Apply file redirections (overrides pipes)
      if (builtins.contains(cmd.args[0])) {
        execute_builtin(cmd);
        _exit(0);
      } else {
        for (auto &r : cmd.redirections) {
          int flags = (r.fd == 0) ? O_RDONLY : (O_CREAT | O_WRONLY | (r.append ? O_APPEND : O_TRUNC));
          int fd = open(r.filename.c_str(), flags, 0644);
          if (fd < 0) { perror("open"); _exit(127); }
          if (dup2(fd, r.fd) < 0) { perror("dup2"); _exit(127); }
          close(fd);
        }

       // Find and exec command
        auto exe_path_opt = get_path(cmd.args[0]);
        if (!exe_path_opt) {
          std::cerr << cmd.args[0] << ": command not found" << std::endl;
          _exit(127);
        }

        std::vector<char*> argv;
        argv.reserve(cmd.args.size() + 1);
        // argv[0] is the program name
        argv.push_back(const_cast<char*>(cmd.args[0].c_str()));
        for (size_t j = 1; j < cmd.args.size(); ++j) {
          argv.push_back(const_cast<char*>(cmd.args[j].c_str()));
        }
        argv.push_back(nullptr);

        execv(exe_path_opt.value().string().c_str(), argv.data());
        perror("execv");
        _exit(127);
      }
    }
    // --- Parent Process ---
    pids.push_back(pid);

    // Close unneeded pipe ends
    if (i > 0) {
      close(in_fd); // Close previous pipe's read end
    }
    if (!is_last) {
      close(pipe_fds[1]); // Close current pipe's write end
      in_fd = pipe_fds[0]; // Save read end for the next child
    }
  }

  // Parent: wait for all children to finish
  for (pid_t pid : pids) {
    int status = 0;
    waitpid(pid, &status, 0);
    // We could store and return the exit status of the *last* command,
    // but for now, we just wait for all.
  }
#endif
}
