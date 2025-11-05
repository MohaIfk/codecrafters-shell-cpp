//
// Created by theGhost on 05/11/2025.
//

#include "shell.h"
#include "utils.h"
#include <optional>

#ifdef _MSC_VER
  #include <cstring> // for errno_t
  #include <malloc.h> // for _dupenv_s (allocates with malloc)
#endif

shell::shell() {
#ifdef _MSC_VER
  // _dupenv_s allocates a buffer with malloc which we must free.
  char* buffer = nullptr;
  size_t requiredSize = 0;
  errno_t err = _dupenv_s(&buffer, &requiredSize, std::string("PATH").c_str());
  if (err != 0 || buffer == nullptr) {
    if (buffer) std::free(buffer);
    path = "";
  }
  // take ownership into std::string then free the C buffer
  path = std::string(buffer);
  std::free(buffer);
#else
  path = std::getenv("PATH") ? std::getenv("PATH") : "";
#endif
  path_dirs = split_view(path, PATH_LIST_SEPARATOR);
  try {
    working_directory_ = fs::current_path();
  } catch (const fs::filesystem_error& e) {
    // fallback to root
    working_directory_ = fs::path("/");
  }
}
shell::~shell() = default;

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


[[noreturn]] void shell::run() {
  while (true) {
    std::cout << "$ ";
    std::string command;
    std::getline(std::cin, command);
    dispatch(command);
  }
}

std::optional<fs::path> shell::get_path(const std::string& name) {
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

void shell::dispatch(const std::string &command) {
  std::vector<std::string> args = split(command, ' ');
  if (args[0] == "exit") {
    if (args.size() == 2) {
      try {
        std::exit(std::stoi(args[1]));
      } catch (std::invalid_argument&) {
        std::cout << "Invalid argument (not a number)" << std::endl;
      } catch (std::out_of_range&) {
        std::cout << "number too large/small for int." << std::endl;
      }
    }
    std::exit(0); // default
  }
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
    if (args[1] == "echo") {
      std::cout << "echo is a shell builtin" << std::endl;
      return;
    }
    if (args[1] == "exit") {
      std::cout << "exit is a shell builtin" << std::endl;
      return;
    }
    if (args[1] == "pwd") {
      std::cout << "pwd is a shell builtin" << std::endl;
      return;
    }
    if (args[1] == "cd") {
      std::cout << "cd is a shell builtin" << std::endl;
      return;
    }
    if (args[1] == "type") {
      std::cout << "type is a shell builtin" << std::endl;
      return;
    }
    if (auto p = get_path(args[1])) {
      std::cout << args[1] << " is " << p.value().string() << std::endl;
      return;
    }
    std::cout << args[1] << ": not found" << std::endl;
    return;
  }
  if (auto p = get_path(args[0])) {
    std::vector<std::string> passArgs;
    for (int i = 1; i < args.size(); i++) passArgs.emplace_back(args[i]);
    auto exitCodeOpt = run_program_and_wait(p.value(), passArgs);
    if (!exitCodeOpt) {
      std::cerr << "Failed to spawn program\n";
    }
    return;
  }
  std::cout << args[0] << ": command not found" << std::endl;
}
