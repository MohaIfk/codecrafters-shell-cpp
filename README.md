# codecrafters-shell-cpp — C++ interactive shell

[![progress-banner](https://backend.codecrafters.io/progress/shell/b4bb982c-8154-4a14-a2e1-7cfef9a34391)](https://app.codecrafters.io/users/MohaIfk?r=2qF)

This repository contains an enhanced C++ implementation of a small interactive
shell (REPL). It started from the CodeCrafters "Build Your Own Shell"
challenge and was extended with command history, tab completion, pipelines,
redirections, cross-platform process spawning and several developer-friendly
helpers.


## Key features (current)

- Cross-platform process spawning: POSIX `fork`+`execv` and Windows
  `CreateProcessW` implementations.
- Pipeline support (`|`), including proper pipe wiring on POSIX and Windows.
- Redirections: `>`, `>>` and numeric fd redirections (e.g. `2>err.txt`).
- Builtin commands: `echo`, `cd` (supports `~`), `pwd`, `type`, `exit`,
  `history` (with `-c`, `-d`, `-r`, `-w`, `-a` subcommands).
- Command history: in-memory list plus optional file persistence controlled by
  the `HISTFILE` environment variable.
- Tab completion for commands (uses a cached set of executables + builtins).
- Robust argument parsing including single/double quotes and backslash
  escapes; throws on unclosed quotes.
- Terminal raw-mode input and arrow-key navigation for history (POSIX via
  `termios`, Windows via `_getch`).
- Scoped redirections for builtins so builtins honor redirection during their
  execution.

## Project layout (important files)

- `src/main.cpp` — program entrypoint.
- `src/shell.h`, `src/shell.cpp` — main shell class, parsing, dispatching,
  builtins, pipeline execution, completion and history.
- `src/utils.h` — helpers: `getenv_safe`, `get_home_directory`, `run_program_and_wait`,
  `parse_args`, and `ScopedRedir`.
- `src/inputHelper.h` — small cross-platform helper for raw-mode input and
  `get_char_raw()` abstraction used by the REPL.
- `CMakeLists.txt` — build configuration.

## Quick developer contract

- Input: user-entered command lines (strings) via a REPL prompt.
- Output: stdout/stderr of builtins and spawned processes; exit status comes
  from executed programs (pipelines are executed and waited upon).
- Error modes: parsing errors (e.g. unclosed quotes) raise runtime errors
  printed to stderr; spawn errors return messages to stderr.

Edge cases considered: empty lines, unclosed quotes, missing redirection
targets, trying to run non-executable files, and PATH entries with permission
errors (these are skipped during executable cache population).

## Build (Windows and POSIX)

Requirements: C++20 compiler, CMake (3.10+ recommended).

Windows (Developer Command Prompt / PowerShell):

```powershell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
# Executable typically: build\Release\shell.exe
```

WSL / Linux / macOS:

```bash
cmake -S . -B cmake-build-debug-wsl -DCMAKE_BUILD_TYPE=Release
cmake --build cmake-build-debug-wsl -- -j$(nproc)
# Executable typically: cmake-build-debug-wsl/shell
```

There is a helper script `your_program.sh` that can be adapted to run the
built binary in your environment.

## Runtime usage (examples)

Start the shell and try commands:

```bash
./cmake-build-debug-wsl/shell
# or on Windows
build\Release\shell.exe
```

Examples inside the prompt:

- Simple builtins:
  - echo: `echo hello world`
  - pwd: `pwd`
  - cd: `cd /tmp` or `cd ~`
  - type: `type ls` or `type echo`
  - exit: `exit` or `exit 2`

- History:
  - up/down arrows to navigate previous commands (raw-mode input)
  - `HISTFILE` env var controls persisted history file path; history is read
    at startup and appended on exit (if configured).
  - `history` builtin examples: `history`, `history 10`, `history -c`,
    `history -d 5`, `history -w historyfile`, `history -r historyfile`,
    `history -a historyfile`.

- External programs and completion:
  - tab completion for commands (builtins + cached executables from `PATH`).
  - `type <name>` reports whether a name is a builtin or prints the resolved
    path to an executable.

- Pipelines and redirections:
  - `ls -la | grep txt | wc -l`
  - `echo hi > out.txt`
  - `echo again >> out.txt`
  - `ls 2> err.txt`
  - `myprog 1>out.txt 2>>err.txt`

Notes:

- Numeric fd redirections are supported (e.g. `2>file` targets stderr).
- Builtins use `ScopedRedir` so redirection affects builtin output as expected.

## Developer notes — where to look

- Argument parsing: `parse_args` in `src/utils.h`.
- Line parsing into pipeline + redirections: `parse_line_to_pipeline` in
  `src/shell.cpp`.
- Pipeline execution: `execute_pipeline` in `src/shell.cpp` (separate
  implementations for POSIX and Windows).
- Single-command execution: `execute_simple_command` and `run_program_and_wait`
  (`src/utils.h`).
- History and completion: `populate_executable_cache`, `read_history`,
  `write_history`, `handle_completion` in `src/shell.cpp`.
- Terminal input: `enableRawMode`, `disableRawMode`, and `get_char_raw` in
  `src/inputHelper.h`.

## Limitations and next improvements

- No job control (background `&`, `fg`, `bg`) yet.
- Environment variable and tilde expansion is limited (tilde `~` support in
  `cd` only). No `$VAR` expansion or globbing implemented.
- Quoting and escaping aims to be shell-like but is not a full POSIX shell
  parser; edge cases may differ from bash/zsh.

Planned/possible next steps (I can implement any of these on request):

- Add `$VAR` expansion and filename globbing.
- Implement job control and background/foreground management.
- Add unit tests for `parse_args`, `parse_line_to_pipeline` and
  `execute_pipeline` behavior.

## License & attribution

This project was started from the CodeCrafters Shell challenge. Respect the
original challenge terms when sharing or publishing derived work.
