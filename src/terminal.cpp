#include "../include/terminal.hpp"

#include <csignal>
#include <iostream>
#include <sys/ioctl.h>
#include <unistd.h>

namespace sysmon {

static volatile sig_atomic_t g_exit_signal = 0;
static volatile sig_atomic_t g_window_resized = 0;

static void clean_exit_handler(int signum) {
  // Only signal-safe state changes here; RAII restores the terminal in main.
  g_exit_signal = signum;
}

static void sigwinch_handler(int) {
  g_window_resized = 1;
}

Terminal::Terminal() {
  enable_raw_mode();
  g_exit_signal = 0;

  // Catch SIGINT and SIGTERM for graceful restoration
  std::signal(SIGINT, clean_exit_handler);
  std::signal(SIGTERM, clean_exit_handler);

  // Catch SIGWINCH for live window resizing
  std::signal(SIGWINCH, sigwinch_handler);
}

Terminal::~Terminal() {
  disable_raw_mode();

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

  if (raw_mode_enabled_) {
    // Switch to alternate screen buffer and hide the cursor.
    std::cout << "\033[?1049h\033[?25l" << std::flush;
  }
}

void Terminal::disable_raw_mode() {
  if (raw_mode_enabled_) {
    // Restore cursor visibility and return to primary screen buffer
    std::cout << "\033[?25h\033[?1049l" << std::flush;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios_);
    raw_mode_enabled_ = false;
  }
}

KeyEvent Terminal::poll_event() {
  KeyEvent ev;
  char c = 0;
  ssize_t bytes_read = read(STDIN_FILENO, &c, 1);
  if (bytes_read != 1) {
    return ev;
  }

  if (c == 27) { // ESC or escape sequence
    char seq[3] = {0};
    if (read(STDIN_FILENO, &seq[0], 1) == 1) {
      if (seq[0] == '[') {
        if (read(STDIN_FILENO, &seq[1], 1) == 1) {
          if (seq[1] >= '0' && seq[1] <= '9') {
            if (read(STDIN_FILENO, &seq[2], 1) == 1 && seq[2] == '~') {
              switch (seq[1]) {
                case '5': ev.key = Key::PageUp; return ev;
                case '6': ev.key = Key::PageDown; return ev;
                case '1': case '7': ev.key = Key::Home; return ev;
                case '4': case '8': ev.key = Key::End; return ev;
              }
            }
          } else {
            switch (seq[1]) {
              case 'A': ev.key = Key::Up; return ev;
              case 'B': ev.key = Key::Down; return ev;
              case 'C': ev.key = Key::Right; return ev;
              case 'D': ev.key = Key::Left; return ev;
              case 'H': ev.key = Key::Home; return ev;
              case 'F': ev.key = Key::End; return ev;
            }
          }
        }
      }
    }
    // Standalone ESC -> Quit
    ev.key = Key::Quit;
    return ev;
  }

  if (c == 'q' || c == 'Q') {
    ev.key = Key::Quit;
    ev.ch = c;
    return ev;
  }

  ev.key = Key::Char;
  ev.ch = c;
  return ev;
}

std::optional<char> Terminal::poll_key() {
  KeyEvent ev = poll_event();
  if (ev.key == Key::Char || ev.key == Key::Quit) {
    return ev.ch;
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

int Terminal::exit_requested() const {
  return g_exit_signal;
}

bool Terminal::check_and_clear_resized() {
  if (g_window_resized) {
    g_window_resized = 0;
    return true;
  }
  return false;
}

void Terminal::clear_screen() const {
  // Reset cursor to (1,1) and clear display
  std::cout << "\033[H\033[2J" << std::flush;
}

} // namespace sysmon
