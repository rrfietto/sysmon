#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <unordered_map>

namespace sysmon {

struct ProcessInfo {
    int pid{0};
    std::string name;
    char state{'?'};
    uint64_t utime{0};       // User mode clock ticks
    uint64_t stime{0};       // Kernel mode clock ticks
    uint64_t rss_kb{0};      // Resident Set Size (physical memory) in KiB
    double cpu_percent{0.0}; // CPU percentage (Irix mode: 100% = 1 core)
    double mem_percent{0.0}; // Memory percentage of total system RAM
};

class ProcessTracker {
public:
    ProcessTracker();

    // Scans /proc, calculates CPU & memory deltas, and returns all running processes
    std::vector<ProcessInfo> sample();

private:
    struct PrevProcState {
        uint64_t total_time{0}; // utime + stime
    };

    std::unordered_map<int, PrevProcState> prev_proc_ticks_;
    uint64_t prev_system_total_ticks_{0};
    uint64_t page_size_kb_{4};
    uint64_t total_system_ram_kb_{0};
    size_t num_cores_{1};
    bool initialized_{false};

    uint64_t read_system_total_ticks() const;
};

} // namespace sysmon
