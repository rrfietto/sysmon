#include "../include/terminal.hpp"

#include <csignal>
#include <iostream>
#include <sys/ioctl.h>
#include <unistd.h>

namespace sysmon {

static Terminal *g_active_terminal = nullptr;

static void clean_exit_handler(int signum) {
  if (g_active_terminal) {
    g_active_terminal->disable_raw_mode();
    g_active_terminal = nullptr;
  }
  _exit(signum == SIGINT ? 130 : 0);
}

Terminal::Terminal() {
  enable_raw_mode();
  g_active_terminal = this;

  // Safety fallback: catch SIGINT and SIGTERM so the terminal is restored
  std::signal(SIGINT, clean_exit_handler);
  std::signal(SIGTERM, clean_exit_handler);
}

Terminal::~Terminal() {
  disable_raw_mode();
  if (g_active_terminal == this) {
    g_active_terminal = nullptr;
  }
}

void Terminal::enable_raw_mode() {
  if (tcgetattr(STDIN_FILENO, &orig_termios_) == -1) {
    return;
  }

  termios raw = orig_termios_;
  // Disable canonical mode (line buffering) and local echo
  raw.c_lflag &= ~(ICANON | ECHO);
  // Non-blocking read: return immediately with 0 bytes if no input is pending
  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 0;

  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) != -1) {
    raw_mode_enabled_ = true;
  }

  // Switch to alternate screen buffer and hide the cursor
  std::cout << "\033[?1049h\033[?25l" << std::flush;
}

void Terminal::disable_raw_mode() {
  if (raw_mode_enabled_) {
    // Restore cursor visibility and return to primary screen buffer
    std::cout << "\033[?25h\033[?1049l" << std::flush;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios_);
    raw_mode_enabled_ = false;
  }
}

std::optional<char> Terminal::poll_key() {
  char c = 0;
  ssize_t bytes_read = read(STDIN_FILENO, &c, 1);
  if (bytes_read == 1) {
    return c;
  }
  return std::nullopt;
}

TerminalSize Terminal::get_size() const {
  TerminalSize size{24, 80};
  winsize ws{};
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) != -1 && ws.ws_col > 0 &&
      ws.ws_row > 0) {
    size.rows = ws.ws_row;
    size.cols = ws.ws_col;
  }
  return size;
}

void Terminal::clear_screen() const {
  // Reset cursor to (1,1) and clear display
  std::cout << "\033[H\033[2J" << std::flush;
}

} // namespace sysmon
