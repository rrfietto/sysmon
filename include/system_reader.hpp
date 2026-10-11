#pragma once

#include <string>
#include <cstdint>
#include <vector>

namespace sysmon {

struct HostInfo {
    std::string hostname;
    std::string os_name;
    std::string kernel_version;
    uint64_t uptime_seconds{0};

    // Helper to format uptime into "X days, Y hours, Z mins, W secs"
    std::string format_uptime() const;
};

struct MemoryInfo {
    uint64_t total_kb{0};
    uint64_t free_kb{0};
    uint64_t available_kb{0};
    uint64_t used_kb{0};

    uint64_t swap_total_kb{0};
    uint64_t swap_free_kb{0};
    uint64_t swap_used_kb{0};

    // Percentage of RAM currently used (0.0% to 100.0%)
    double used_percent() const;

    // Percentage of Swap currently used (0.0% to 100.0%)
    double swap_used_percent() const;
};

// Represents a single point-in-time snapshot of CPU ticks from /proc/stat
struct CpuSnapshot {
    std::string label; // "cpu" (aggregate) or "cpu0", "cpu1", ...
    uint64_t user{0};
    uint64_t nice{0};
    uint64_t system{0};
    uint64_t idle{0};
    uint64_t iowait{0};
    uint64_t irq{0};
    uint64_t softirq{0};
    uint64_t steal{0};

    uint64_t total_time() const;
    uint64_t idle_time() const;
};

// Calculates CPU utilization percentage between two snapshots (0.0% - 100.0%)
double calculate_cpu_percentage(const CpuSnapshot& prev, const CpuSnapshot& curr);

// Reads all CPU counters from /proc/stat. Index 0 is aggregate ("cpu"), followed by cores.
std::vector<CpuSnapshot> read_cpu_snapshots();

// Structure holding computed CPU percentages
struct CpuInfo {
    double aggregate{0.0};
    std::vector<double> cores;
};

// Stateful tracker that calculates CPU deltas over time
class CpuTracker {
public:
    CpuTracker();

    // Samples /proc/stat and returns both aggregate and per-core CPU usage percentages
    CpuInfo sample();

private:
    std::vector<CpuSnapshot> prev_snapshots_;
    bool initialized_{false};
};

// Functions to read system info from Linux /proc and /etc files
HostInfo read_host_info();
MemoryInfo read_memory_info();

// Helper to format kilobytes into human-readable strings (e.g. "11.56 GiB")
std::string format_kb(uint64_t kb);

} // namespace sysmon
