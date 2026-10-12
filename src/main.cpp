#include "../include/process_reader.hpp"
#include "../include/system_reader.hpp"

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <thread>
#include <vector>

static void print_process_table(const std::vector<sysmon::ProcessInfo> &procs,
                                size_t limit) {
  std::cout << std::left << std::setw(8) << "PID" << std::setw(6) << "STATE"
            << std::right << std::setw(8) << "CPU%" << std::setw(8) << "MEM%"
            << std::setw(12) << "RSS"
            << "  " << std::left << std::setw(25) << "NAME"
            << "\n";
  std::cout << std::string(67, '-') << "\n";

  size_t count = std::min(procs.size(), limit);
  for (size_t i = 0; i < count; ++i) {
    const auto &p = procs[i];
    std::cout << std::left << std::setw(8) << p.pid << std::setw(6) << p.state
              << std::right << std::fixed << std::setprecision(1)
              << std::setw(7) << p.cpu_percent << "%" << std::setw(7)
              << p.mem_percent << "%" << std::setw(12)
              << sysmon::format_kb(p.rss_kb) << "  " << std::left << p.name
              << "\n";
  }
  std::cout << "\n";
}

int main() {
  std::cout << "==============================================================="
               "====\n";
  std::cout << "               SYSMON - MILESTONE 3 (PROCESS COLLECTOR)        "
               "    \n";
  std::cout << "==============================================================="
               "====\n\n";

  sysmon::HostInfo host = sysmon::read_host_info();
  sysmon::MemoryInfo mem = sysmon::read_memory_info();

  std::cout << "[HOST]   " << host.hostname << " | " << host.os_name << " ("
            << host.kernel_version << ")\n";
  std::cout << "[MEMORY] Total: " << sysmon::format_kb(mem.total_kb)
            << " | Used: " << sysmon::format_kb(mem.used_kb) << " ("
            << std::fixed << std::setprecision(1) << mem.used_percent()
            << "%)\n\n";

  std::cout << "Initializing ProcessTracker and taking baseline snapshot...\n";
  sysmon::ProcessTracker tracker;

  std::cout << "Waiting 1 second to accumulate CPU delta window...\n"
            << std::flush;
  std::this_thread::sleep_for(std::chrono::seconds(1));

  auto processes = tracker.sample();
  std::cout << "Discovered " << processes.size() << " running processes.\n\n";

  // 1. Sort by CPU utilization descending
  std::sort(processes.begin(), processes.end(),
            [](const auto &a, const auto &b) {
              return a.cpu_percent > b.cpu_percent;
            });
  std::cout << "[TOP 10 PROCESSES BY CPU UTILIZATION]\n";
  print_process_table(processes, 10);

  // 2. Sort by Memory (RSS) descending
  std::sort(processes.begin(), processes.end(),
            [](const auto &a, const auto &b) { return a.rss_kb > b.rss_kb; });
  std::cout << "[TOP 10 PROCESSES BY MEMORY USAGE (RSS)]\n";
  print_process_table(processes, 10);

  std::cout << "==============================================================="
               "====\n";
  return 0;
}
