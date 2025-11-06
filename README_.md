[![progress-banner](https://backend.codecrafters.io/progress/shell/b4bb982c-8154-4a14-a2e1-7cfef9a34391)](https://app.codecrafters.io/users/MohaIfk?r=2qF)

# codecrafters-shell-cpp

An enhanced C++ implementation of the "Build Your Own Shell" challenge from
CodeCrafters. This repository contains a small interactive shell (REPL) that
supports builtin commands, executing external programs, basic redirections, and
cross-platform process launching on both POSIX and Windows.

This README documents how to build and run the project, shows usage examples
and explains a number of extensions implemented beyond the minimal starter
requirements.

## Highlights / Features

- Cross-platform: builds and runs on POSIX (Linux/WSL/macOS) and Windows.
- Builtin commands: `echo`, `cd` (supports `~`), `pwd`, `type`, `exit`.
- External program execution (resolves PATH entries).
- Redirections: supports `>`, `>>` and numeric fds like `2>err.txt`.
- Robust argument parsing: single/double quotes, backslash escapes inside and
   outside quotes, and error on unclosed quotes.
- Windows-specific support: `.exe/.bat/.cmd` resolution, CreateProcessW based
   spawning, and careful argument quoting and UTF-8 -> UTF-16 conversion.
- Scoped redirections for builtins (so builtins honor redirected stdout/stderr).
- `cd` resolves and canonicalizes the target directory and supports `~`.

The implementation files of note are in `src/`:

- `src/main.cpp` — program entrypoint and minimal runner.
- `src/shell.h`, `src/shell.cpp` — shell class, parsing, dispatching, PATH
   resolution and builtin implementations.
- `src/utils.h` — process launching (POSIX fork/exec and Windows CreateProcess),
   argument parser, home directory lookup, redirection helpers, etc.

## What I implemented beyond the starter (quick summary)

If you or someone on your team extended the starter, the most notable additions
in this codebase are:

- Full cross-platform process launching: CreateProcessW for Windows with
   careful quoting and UTF-8 to UTF-16 conversion. POSIX uses `fork` + `execv`.
- Argument parser that closely mirrors shell-like quoting behavior (single
   quotes, double quotes with limited escaping, and backslash escapes).
- Redirection parsing and handling that supports numeric fds and append
   (`>>`) and a `ScopedRedir` helper that temporarily redirects file
   descriptors for builtin commands.
- PATH-resolution that tries platform-appropriate executable extensions on
   Windows (.exe, .bat, .cmd) and checks executable bit on POSIX.
- Improved `cd` handling: supports `~` and canonicalizes the working
   directory (resolving symlinks where possible).

These features make the shell more robust and suitable for manual testing on
both Windows and POSIX environments.

## Requirements

- C++17 compatible compiler (MSVC, g++, clang++)
- CMake (3.10+ recommended)
- On Windows: Visual Studio or Build Tools (so CMake can generate a Visual
   Studio solution or Ninja files). On Linux/WSL: make/ninja + standard build
   toolchain.

## Build and run

The repository contains a CMake-based build. Example commands below assume you
run them from the repository root.

Windows (recommended via Developer Command Prompt or PowerShell):

```powershell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
# Executable will usually be at build\Release\shell.exe (or the configured target dir)
.
```

WSL / Linux / macOS:

```bash
cmake -S . -B cmake-build-debug-wsl -DCMAKE_BUILD_TYPE=Release
cmake --build cmake-build-debug-wsl -- -j$(nproc)
# Executable will be at cmake-build-debug-wsl/shell
```

You can also use the provided `your_program.sh` wrapper if present and
appropriate for your environment (it simply runs the built executable).

## Usage examples

Run the shell (example path depends on your build):

```bash
./cmake-build-debug-wsl/shell
# or on Windows
build\Release\shell.exe
```

Once inside the prompt, try examples:

- Builtins:

- echo: `echo hello world`
- pwd: `pwd`
- cd: `cd /tmp` or `cd ~`
- type: `type ls` or `type echo`
- exit: `exit` or `exit 2`

- External commands:

- `ls -la` (POSIX)
- `dir` (Windows cmd builtin via external program resolution if available)

- Redirection examples:

- `echo hi > out.txt` (overwrite)
- `echo again >> out.txt` (append)
- `ls 2> err.txt` (redirect stderr to file)
- `myprog 1>out.txt 2>>err.txt` (mix stdout overwrite + stderr append)

Notes:

- Numeric fd redirections are supported (e.g. `2>file` targets stderr).
- Builtins use `ScopedRedir` so redirection affects builtin output as expected.

## Limitations and TODO / next steps

- Pipes (`|`), command chaining (`&&`, `||`), and background jobs (`&`) are
   not implemented in this codebase.
- Shell expansions (globbing, environment variable expansion like `$HOME`),
   input here-documents (`<<`), and job control are not implemented.
- Quoting and escapes are generous, but there may be corner cases that differ
   from a full POSIX shell.

If you want, I can help implement pipes and job control next — they're the
natural next features.

## Manual tests to verify functionality

1. Build the project (see Build instructions).
2. Start the shell executable.
3. Verify builtins: run `echo`, `pwd`, `cd`, `type`, `exit`.
4. Verify external programs: run `which`/`type` or `ls`/`dir` depending on
    platform (or run a small program you compiled and put on PATH).
5. Verify redirections: `echo hello > /tmp/test.txt`, then `cat /tmp/test.txt`.
6. On Windows, test an `.exe` in PATH and a `.bat` script; `type` should show
    resolved paths.

## Where to look in the code

- `parse_args` in `src/utils.h` — argument parsing.
- `parse_command_with_redirect` in `src/shell.cpp` — builds `Command` with
   argument vector and redirections parsed.
- `run_program_and_wait` in `src/utils.h` — platform-specific spawning.
- `ScopedRedir` in `src/utils.h` — temporarily redirects fds for builtins.

## License & attribution

This solution is based on the CodeCrafters Shell challenge. Follow the
original challenge terms where applicable when publishing or sharing your
solution.