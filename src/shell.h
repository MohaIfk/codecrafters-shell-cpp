#pragma once
#include <string>
#include <vector>
#include <iostream>

class shell {
public:
  shell();
  ~shell();

  void run();

  static std::vector<std::string> command_split(const std::string & string, char c);

  void dispatch(const std::string &command);
};