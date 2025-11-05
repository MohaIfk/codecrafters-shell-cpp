//
// Created by info on 05/11/2025.
//

#include "shell.h"

shell::shell() = default;
shell::~shell() = default;

std::vector<std::string> shell::command_split(const std::string &string, char c) {
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


[[noreturn]] void shell::run() {
  while (true) {
    std::cout << "$ ";
    std::string command;
    std::getline(std::cin, command);
    dispatch(command);
  }
}

void shell::dispatch(const std::string &command) {
  std::vector<std::string> args = command_split(command, ' ');
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
  std::cout << args[0] << ": command not found" << std::endl;
}
