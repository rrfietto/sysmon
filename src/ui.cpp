#include "../include/ui.hpp"

#include <cmath>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace sysmon {

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
                const MemoryInfo &mem, const CpuInfo &cpu) {
  TerminalSize size = terminal.get_size();
  int banner_width = std::max(68, std::min(size.cols - 2, 90));
  std::string separator(banner_width, '=');
  std::string divider(banner_width, '-');

  std::ostringstream out;

  // Move cursor to top-left without clearing to eliminate screen flicker
  out << "\033[H";

  // Header Banner
  out << "\033[1;36m" << separator << "\033[0m\033[K\n";
  out << "\033[1;36m  SYSMON v0.1 \033[0m\033[90m— LINUX SYSTEM MONITOR "
         "(MILESTONE 2 MVP)\033[0m\033[K\n";
  out << "\033[1;36m" << separator << "\033[0m\033[K\n\n";

  // 1. Host Information Block
  out << "\033[1;37m[HOST INFORMATION]\033[0m\033[K\n";
  out << "  \033[90mHostname:\033[0m " << host.hostname << "\033[K\n";
  out << "  \033[90mOS:\033[0m       " << host.os_name << "\033[K\n";
  out << "  \033[90mKernel:\033[0m   " << host.kernel_version << "\033[K\n";
  out << "  \033[90mUptime:\033[0m   " << host.format_uptime() << "\033[K\n\n";

  // 2. CPU Utilization Block
  out << "\033[1;37m[CPU UTILIZATION]\033[0m\033[K\n";
  out << "  \033[1mOverall CPU\033[0m  [" << render_bar(cpu.aggregate, 26)
      << "] " << color_for_percent(cpu.aggregate) << std::fixed
      << std::setprecision(1) << std::setw(5) << cpu.aggregate
      << "%\033[0m\033[K\n";

  // Per-core grid (2 cores per line if available)
  if (!cpu.cores.empty()) {
    out << "  \033[90mIndividual Cores:\033[0m\033[K\n";
    for (size_t i = 0; i < cpu.cores.size(); i += 2) {
      out << "    C" << std::setw(2) << i << " ["
          << render_bar(cpu.cores[i], 12) << "] "
          << color_for_percent(cpu.cores[i]) << std::fixed
          << std::setprecision(1) << std::setw(5) << cpu.cores[i] << "%\033[0m";

      if (i + 1 < cpu.cores.size()) {
        out << "   C" << std::setw(2) << (i + 1) << " ["
            << render_bar(cpu.cores[i + 1], 12) << "] "
            << color_for_percent(cpu.cores[i + 1]) << std::fixed
            << std::setprecision(1) << std::setw(5) << cpu.cores[i + 1]
            << "%\033[0m";
      }
      out << "\033[K\n";
    }
  }
  out << "\033[K\n";

  // 3. Memory & Swap Block
  out << "\033[1;37m[MEMORY & SWAP]\033[0m\033[K\n";
  double mem_pct = mem.used_percent();
  out << "  \033[1mRAM \033[0m         [" << render_bar(mem_pct, 26) << "] "
      << color_for_percent(mem_pct) << std::fixed << std::setprecision(1)
      << std::setw(5) << mem_pct << "%\033[0m  "
      << sysmon::format_kb(mem.used_kb) << " / "
      << sysmon::format_kb(mem.total_kb) << "\033[K\n";

  double swap_pct = mem.swap_used_percent();
  out << "  \033[1mSwap\033[0m         [" << render_bar(swap_pct, 26) << "] "
      << color_for_percent(swap_pct) << std::fixed << std::setprecision(1)
      << std::setw(5) << swap_pct << "%\033[0m  "
      << sysmon::format_kb(mem.swap_used_kb) << " / "
      << sysmon::format_kb(mem.swap_total_kb) << "\033[K\n\n";

  // 4. Footer controls
  out << "\033[90m" << divider << "\033[0m\033[K\n";
  out << "  \033[1;33m[q]\033[0m \033[90mQuit\033[0m  |  \033[90mRefresh Rate: "
         "1000ms\033[0m\033[K\n";

  // Atomic flush to screen
  std::cout << out.str() << std::flush;
}

} // namespace sysmon
