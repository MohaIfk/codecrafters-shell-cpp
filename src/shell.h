#pragma once
#include <string>
#include <vector>
#include <iostream>
#include <filesystem>
#include <optional>
#include <sstream>

#ifdef _WIN32
constexpr char PATH_LIST_SEPARATOR = ';';
#else
constexpr char PATH_LIST_SEPARATOR = ':';
#endif

class shell {
  std::string path;
  std::vector<std::string_view> path_dirs;
  std::filesystem::path working_directory_;
public:
  shell();
  ~shell();

  [[noreturn]] void run();
  std::optional<std::filesystem::path> get_path(const std::string& name);
  const std::filesystem::path& working_directory();
  bool set_working_directory(const std::filesystem::path& dir);

  static std::vector<std::string> split(const std::string & string, char c);
  static std::vector<std::string_view> split_view(const std::string & string, char c);

  void dispatch(const std::string &command);
};