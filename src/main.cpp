#include <iostream>
#include <string>
#include "shell.h"

int main() {
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;
  shell shell_;
  shell_.run();
  return 0;
}