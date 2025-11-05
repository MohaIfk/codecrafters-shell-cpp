#pragma once

#include <string>
#include <vector>
#include <iostream>

#ifdef _WIN32
#include <conio.h> // For _getch()
#else
#include <unistd.h>  // For STDIN_FILENO
#include <termios.h> // For terminal settings
#endif

#ifndef _WIN32
// POSIX (Linux/macOS) implementation
struct termios orig_termios;

inline void disableRawMode() {
  // Restore original terminal attributes
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

inline void enableRawMode() {
  // Get current terminal attributes
  tcgetattr(STDIN_FILENO, &orig_termios);
  // Register disableRawMode to run on exit
  atexit(disableRawMode);

  struct termios raw = orig_termios;
  // Disable canonical mode (line buffering) and echo
  raw.c_lflag &= ~(ECHO | ICANON);
  // Apply new attributes immediately
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

inline int get_char_raw() {
  return getchar(); // Now reads one char at a time
}

#else
// Windows implementation
inline void enableRawMode() {
  // Not needed, _getch() is already raw
}

inline void disableRawMode() {
  // Not needed
}

inline int get_char_raw() {
  return _getch();
}
#endif