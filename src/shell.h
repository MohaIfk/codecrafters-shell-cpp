#pragma once
#include <string>
#include <vector>
#include <iostream>
#include <filesystem>
#include <optional>
#include <regex>
#include <set>

#ifdef _WIN32
constexpr char PATH_LIST_SEPARATOR = ';';
#else
constexpr char PATH_LIST_SEPARATOR = ':';
#endif

struct Redirection {
  int fd; // 0=stdin,1=stdout,2=stderr
  std::string filename;
  bool append = false;
};

struct Command {
  std::vector<std::string> args;
  std::vector<Redirection> redirections;
};

class shell {
  std::string path;
  std::vector<std::string_view> path_dirs;
  std::filesystem::path working_directory_;
  const std::set<std::string> builtins = {"echo", "exit", "pwd", "cd", "type", "history"};
  std::set<std::string> executable_cache;
  std::vector<std::string> history_list;

public:
  shell();
  ~shell();

  static bool is_executable(const std::filesystem::path& p);
  void populate_executable_cache();

  static std::string find_lcp(const std::set<std::string>& matches);
  void handle_completion(std::string& line, bool second_tab = false) const;
  [[noreturn]] void run();
  static std::vector<Command> parse_line_to_pipeline(const std::string &line);
  std::optional<std::filesystem::path> get_path(const std::string& name);
  const std::filesystem::path& working_directory();
  bool set_working_directory(const std::filesystem::path& dir);

  static std::vector<std::string> split(const std::string & string, char c);
  static std::vector<std::string_view> split_view(const std::string & string, char c);

  void execute_builtin(const Command& command);
  void execute_simple_command(const Command &command);
  void dispatch_pipeline(const std::vector<Command>& pipeline);
  void execute_pipeline(const std::vector<Command>& pipeline);
};