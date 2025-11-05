//
// Created by info on 05/11/2025.
//

#include "shell.h"
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
  for (const auto & path : path_dirs) {
    std::cout << path << '\n';
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
    if (args[1] == "type") {
      std::cout << "type is a shell builtin" << std::endl;
      return;
    }
    // Check if a file with the command name exists.
    // If the file exists and has execute permissions, print <command> is <full_path> and stop searching.
    // If the file exists but doesn't have execute permissions, skip it and continue to the next directory.
    for (auto& path_dir: path_dirs) {
      auto full_path = std::filesystem::path(path_dir) / args[1];
#ifdef _WIN32
      // Check for .exe, .bat, .cmd extensions on Windows
      std::vector<std::string> exts = {".exe", ".bat", ".cmd"};
      for (auto& ext : exts) {
        auto condidat = full_path;
        condidat += ext;
        if (std::filesystem::exists(condidat)) {
          std::cout << args[1] << " is " << condidat.string() << std::endl;
          return;
        }
      }
#else
      if (std::filesystem::exists(full_path)) {
        auto p = std::filesystem::status(full_path).permissions();
        bool can_exec =
          ((p & std::filesystem::perms::owner_exec) != std::filesystem::perms::none) ||
          ((p & std::filesystem::perms::group_exec) != std::filesystem::perms::none) ||
          ((p & std::filesystem::perms::others_exec) != std::filesystem::perms::none)
          ;
        if (can_exec) {
          std::cout << args[1] << " is " << full_path.string() << std::endl;
          return;
        }
      }
#endif
    }
    std::cout << args[1] << ": not found" << std::endl;
    return;
  }
  std::cout << args[0] << ": command not found" << std::endl;
}
