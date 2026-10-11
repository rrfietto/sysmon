#include "../include/system_reader.hpp"
#include <iomanip>
#include <iostream>
#include <thread>
#include <chrono>

int main() {
  std::cout << "========================================\n";
  std::cout << "   SYSMON - MILESTONE 2 (PHASE A TEST)  \n";
  std::cout << "========================================\n\n";

  // 1. Read Host Information
  sysmon::HostInfo host = sysmon::read_host_info();
  std::cout << "[HOST INFORMATION]\n";
  std::cout << "  Hostname:       " << host.hostname << "\n";
  std::cout << "  OS:             " << host.os_name << "\n";
  std::cout << "  Kernel:         " << host.kernel_version << "\n";
  std::cout << "  Uptime:         " << host.format_uptime() << " ("
            << host.uptime_seconds << "s)\n\n";

  // 2. Read Memory Information
  sysmon::MemoryInfo mem = sysmon::read_memory_info();
  std::cout << "[MEMORY USAGE]\n";
  std::cout << "  Total RAM:      " << sysmon::format_kb(mem.total_kb) << "\n";
  std::cout << "  Used RAM:       " << sysmon::format_kb(mem.used_kb) << " ("
            << std::fixed << std::setprecision(1) << mem.used_percent()
            << "%)\n";
  std::cout << "  Available RAM:  " << sysmon::format_kb(mem.available_kb)
            << "\n";
  std::cout << "  Free RAM:       " << sysmon::format_kb(mem.free_kb) << "\n\n";

  std::cout << "[SWAP USAGE]\n";
  std::cout << "  Total Swap:     " << sysmon::format_kb(mem.swap_total_kb)
            << "\n";
  std::cout << "  Used Swap:      " << sysmon::format_kb(mem.swap_used_kb)
            << " (" << std::fixed << std::setprecision(1)
            << mem.swap_used_percent() << "%)\n";
  std::cout << "  Free Swap:      " << sysmon::format_kb(mem.swap_free_kb)
            << "\n\n";

  // 3. Test CPU Delta Tracker
  std::cout << "[CPU USAGE (Sampling 3 times with 1s interval)]\n";
  sysmon::CpuTracker cpu_tracker;

  for (int i = 1; i <= 3; ++i) {
    std::cout << "  Waiting 1 second for sample #" << i << "...\n" << std::flush;
    std::this_thread::sleep_for(std::chrono::seconds(1));

    sysmon::CpuInfo cpu_info = cpu_tracker.sample();

    std::cout << "  Sample #" << i << " Overall CPU: "
              << std::fixed << std::setprecision(1) << cpu_info.aggregate << "%\n";

    std::cout << "    Cores: ";
    for (size_t c = 0; c < cpu_info.cores.size(); ++c) {
      std::cout << "C" << c << ":" << std::fixed << std::setprecision(1) << cpu_info.cores[c] << "% ";
    }
    std::cout << "\n\n";
  }

  std::cout << "========================================\n";
  return 0;
}
