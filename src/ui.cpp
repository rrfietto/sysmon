#include "../include/ui.hpp"

#include <cmath>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace sysmon {

namespace {
std::string safe_text(std::string text) {
  for (char& c : text) {
    if (static_cast<unsigned char>(c) < 0x20 || c == 0x7f) c = '?';
  }
  return text;
}

// Address each row explicitly: newline wrapping must never scroll the canvas.
void flush_frame(const std::string& frame, TerminalSize size, Theme theme) {
  std::istringstream lines(frame);
  std::string line;
  std::ostringstream out;
  out << "\033[H";
  int row = 1;
  while (row <= size.rows && std::getline(lines, line)) {
    out << "\033[" << row++ << ";1H";
    int columns = 0;
    for (size_t i = 0; i < line.size();) {
      if (line[i] == '\033') {
        size_t end = line.find_first_of("mHKJ", i + 1);
        if (end == std::string::npos) break;
        std::string escape = line.substr(i, end - i + 1);
        if (line[end] == 'm') {
          if (theme == Theme::Ocean) {
            if (escape == "\033[32m") escape = "\033[36m";
            else if (escape == "\033[1;36m") escape = "\033[1;34m";
          }
          if (theme != Theme::Mono || escape == "\033[0m" || escape == "\033[7m")
            out << escape;
        }
        i = end + 1;
        continue;
      }
      if (columns >= size.cols - 1) break;
      unsigned char c = static_cast<unsigned char>(line[i]);
      size_t bytes = c < 0x80 ? 1 : c < 0xe0 ? 2 : c < 0xf0 ? 3 : 4;
      // External names may contain control bytes; never interpret them as commands.
      if (c < 0x20 || c == 0x7f || c >= 0x80) {
        std::string glyph = line.substr(i, bytes);
        if (glyph == "█" || glyph == "░" || glyph == "—" || glyph == "▼" ||
            glyph == "▲" || glyph == "↑" || glyph == "↓") out << glyph;
        else out << '?';
      } else out << line[i];
      i += bytes;
      ++columns;
    }
    out << "\033[0m\033[K";
  }
  out << "\033[J";
  std::cout << out.str() << std::flush;
}

int core_rows(TerminalSize size, const CpuInfo& cpu) {
  return std::min(static_cast<int>((cpu.cores.size() + 3) / 4),
                  std::max(0, size.rows - 13));
}
} // namespace

int UI::visible_process_rows(TerminalSize size, const CpuInfo& cpu) {
  return std::max(1, size.rows - 12 - core_rows(size, cpu));
}

std::string UI::color_for_percent(double percent) {
  if (percent < 60.0) {
    return "\033[32m"; // Green
  } else if (percent < 85.0) {
    return "\033[33m"; // Yellow
  } else {
    return "\033[31m"; // Red
  }
}

std::string UI::render_bar(double percent, int width) {
  if (percent < 0.0)
    percent = 0.0;
  if (percent > 100.0)
    percent = 100.0;

  int filled = static_cast<int>(std::round((percent / 100.0) * width));
  if (filled > width)
    filled = width;

  std::ostringstream oss;
  oss << color_for_percent(percent);
  for (int i = 0; i < filled; ++i) {
    oss << "█";
  }
  oss << "\033[90m"; // Dark gray for empty slots
  for (int i = filled; i < width; ++i) {
    oss << "░";
  }
  oss << "\033[0m";
  return oss.str();
}

void UI::render(const Terminal &terminal, const HostInfo &host,
                const MemoryInfo &mem, const CpuInfo &cpu,
                const std::vector<ProcessInfo> &processes,
                const UIState &state) {
  TerminalSize size = terminal.get_size();

  // Edge case: Terminal too small
  if (size.rows < 14 || size.cols < 50) {
    std::ostringstream small_out;
    small_out << "\033[H\033[2J";
    small_out << "\033[1;33mWindow too small for sysmon\033[0m\033[K\n";
    small_out << "Current: " << size.cols << "x" << size.rows
              << " | Need at least 50x14\033[K\n";
    small_out << "Resize window or press [q] to exit.\033[K";
    flush_frame(small_out.str(), size, state.theme);
    return;
  }

  int banner_width = std::max(50, std::min(size.cols - 2, 100));
  std::string separator(banner_width, '=');
  std::string divider(banner_width, '-');

  std::ostringstream out;

  // Move cursor to top-left (1,1) without clearing to eliminate screen flicker
  out << "\033[H";

  // 1. Header Banner (3 lines)
  out << "\033[1;36m" << separator << "\033[0m\033[K\n";
  out << "\033[1;36m  SYSMON v0.3 \033[0m\033[90m— LINUX SYSTEM MONITOR\033[0m\033[K\n";
  out << "\033[1;36m" << separator << "\033[0m\033[K\n";

  // 2. Compact Host Information Line (1 line)
  out << "\033[1;37m[HOST]\033[0m  " << safe_text(host.hostname) << " \033[90m|\033[0m "
      << safe_text(host.os_name) << " (" << safe_text(host.kernel_version) << ") \033[90m|\033[0m Up: "
      << host.format_uptime() << "\033[K\n";

  // 3. CPU Utilization Section (1 line overall + 1 or 2 lines cores)
  out << "\033[1;37m[CPU]\033[0m   Overall [" << render_bar(cpu.aggregate, 20)
      << "] " << color_for_percent(cpu.aggregate) << std::fixed
      << std::setprecision(1) << std::setw(5) << cpu.aggregate
      << "%\033[0m\033[K\n";

  size_t shown_cores = std::min(cpu.cores.size(), static_cast<size_t>(core_rows(size, cpu) * 4));
  if (shown_cores > 0) {
    out << "        \033[90mCores:\033[0m ";
    if (shown_cores < cpu.cores.size()) {
      out << "(" << shown_cores << "/" << cpu.cores.size() << ") ";
    }
    for (size_t i = 0; i < shown_cores; ++i) {
      out << "C" << i << "[" << color_for_percent(cpu.cores[i])
          << std::fixed << std::setprecision(0) << std::setw(2) << cpu.cores[i] << "%\033[0m] ";
      if ((i + 1) % 4 == 0 && (i + 1) < shown_cores) {
        out << "\033[K\n               ";
      }
    }
    out << "\033[K\n";
  }

  // 4. Memory & Swap Section (2 lines)
  double mem_pct = mem.used_percent();
  out << "\033[1;37m[MEM]\033[0m   RAM     [" << render_bar(mem_pct, 20) << "] "
      << color_for_percent(mem_pct) << std::fixed << std::setprecision(1)
      << std::setw(5) << mem_pct << "%\033[0m  "
      << sysmon::format_kb(mem.used_kb) << " / "
      << sysmon::format_kb(mem.total_kb) << "\033[K\n";

  double swap_pct = mem.swap_used_percent();
  out << "        Swap    [" << render_bar(swap_pct, 20) << "] "
      << color_for_percent(swap_pct) << std::fixed << std::setprecision(1)
      << std::setw(5) << swap_pct << "%\033[0m  "
      << sysmon::format_kb(mem.swap_used_kb) << " / "
      << sysmon::format_kb(mem.swap_total_kb) << "\033[K\n";

  // 5. Interactive Process Table Header (3 lines)
  std::string cpu_hdr = (state.sort_by == SortBy::CPU) ? "\033[1;33m CPU% ▼\033[0m" : "\033[1;37m   CPU%\033[0m";
  std::string mem_hdr = (state.sort_by == SortBy::Memory) ? "\033[1;33m MEM% ▼\033[0m" : "\033[1;37m   MEM%\033[0m";
  std::string rss_hdr = (state.sort_by == SortBy::Memory) ? "\033[1;33m   RSS (MB)\033[0m" : "\033[1;37m        RSS\033[0m";
  std::string pid_hdr = (state.sort_by == SortBy::PID) ? "\033[1;33m   PID ▲\033[0m" : "\033[1;37m     PID\033[0m";
  std::string state_hdr = "\033[1;37mSTATE\033[0m";

  out << "\033[1;37m[PROCESSES]\033[0m \033[90m(Total: " << processes.size()
      << " | Sort: [c] CPU  [m] Mem  [p] PID)\033[0m\033[K\n";

  out << "  \033[1;37mCOMMAND                 \033[0m "
      << cpu_hdr << " "
      << mem_hdr << " "
      << rss_hdr << " "
      << pid_hdr << " "
      << state_hdr << "\033[K\n";
  out << "\033[90m  " << divider << "\033[0m\033[K\n";

  // Twelve fixed rows plus the actual number of core rows.
  int visible_rows = visible_process_rows(size, cpu);

  int start_idx = state.scroll_offset;
  int end_idx = std::min(static_cast<int>(processes.size()), start_idx + visible_rows);

  for (int i = start_idx; i < end_idx; ++i) {
    const auto &p = processes[i];
    bool is_selected = (i == state.selected_index);

    if (is_selected) {
      out << "\033[7m> "; // Reverse video highlight for selection
    } else {
      out << "  ";
    }

    std::string name_disp = safe_text(p.name);
    if (name_disp.length() > 24) {
      name_disp = name_disp.substr(0, 22) + "..";
    }

    std::ostringstream row_oss;
    row_oss << std::left << std::setw(24) << name_disp << " "
            << std::right << std::fixed << std::setprecision(1)
            << std::setw(6) << p.cpu_percent << "% "
            << std::setw(6) << p.mem_percent << "% "
            << std::setw(11) << sysmon::format_kb(p.rss_kb) << " "
            << std::setw(8) << p.pid << " "
            << std::setw(5) << p.state;

    out << row_oss.str();

    if (is_selected) {
      out << "\033[0m"; // Reset highlight
    }
    out << "\033[K\n";
  }

  // Clear any extra allocated process lines if fewer processes than visible_rows
  for (int i = end_idx - start_idx; i < visible_rows; ++i) {
    out << "\033[K\n";
  }

  // 6. Interactive Controls Footer (2 lines)
  out << "\033[90m" << divider << "\033[0m\033[K\n";
  // The VERY LAST line must NOT end with '\n' to prevent terminal scrolling!
  out << "[t] Theme | \033[1;33m[q]\033[0m \033[90mQuit\033[0m | "
      << "\033[1;33m[↑/↓]\033[0m \033[90mScroll\033[0m | "
      << "\033[1;33m[c]\033[0m \033[90mCPU\033[0m | "
      << "\033[1;33m[m]\033[0m \033[90mMem\033[0m | "
      << "\033[1;33m[p]\033[0m \033[90mPID\033[0m | "
      << "\033[90mRow " << (processes.empty() ? 0 : state.selected_index + 1) << "/" << processes.size() << "\033[0m\033[K";

  flush_frame(out.str(), size, state.theme);
}

} // namespace sysmon
