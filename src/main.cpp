#include <iostream>
#include <string>

int main() {
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  for (;;) {
    std::cout << "$ ";
    std::string cmd;
    std::getline(std::cin, cmd);
    std::cout << cmd << ": command not found" << std::endl;
  }
  return 0;
}