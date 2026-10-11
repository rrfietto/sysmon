#pragma once

#include <termios.h>
#include <optional>

namespace sysmon {

struct TerminalSize {
    int rows{24};
    int cols{80};
};

class Terminal {
public:
    Terminal();
    ~Terminal();

    // Prevent copying to ensure RAII terminal state restoration
    Terminal(const Terminal&) = delete;
    Terminal& operator=(const Terminal&) = delete;

    // Non-blocking single-character read (returns std::nullopt if no key was pressed)
    std::optional<char> poll_key();

    // Queries current terminal dimensions (columns and rows)
    TerminalSize get_size() const;

    // Clears the screen and positions cursor at (1, 1)
    void clear_screen() const;

    // Manually restores original terminal state
    void disable_raw_mode();

private:
    termios orig_termios_{};
    bool raw_mode_enabled_{false};

    void enable_raw_mode();
};

} // namespace sysmon
