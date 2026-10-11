#include "../include/system_reader.hpp"

#include <fstream>
#include <iomanip>
#include <sstream>
#include <unistd.h>
#include <vector>

namespace sysmon {

std::string HostInfo::format_uptime() const {
  uint64_t days = uptime_seconds / 86400;
  uint64_t hours = (uptime_seconds % 86400) / 3600;
  uint64_t minutes = (uptime_seconds % 3600) / 60;
  uint64_t seconds = uptime_seconds % 60;

  std::ostringstream oss;
  if (days > 0) {
    oss << days << (days == 1 ? " day, " : " days, ");
  }
  if (hours > 0 || days > 0) {
    oss << hours << "h ";
  }
  oss << minutes << "m " << seconds << "s";
  return oss.str();
}

double MemoryInfo::used_percent() const {
  if (total_kb == 0)
    return 0.0;
  return (static_cast<double>(used_kb) / static_cast<double>(total_kb)) * 100.0;
}

double MemoryInfo::swap_used_percent() const {
  if (swap_total_kb == 0)
    return 0.0;
  return (static_cast<double>(swap_used_kb) /
          static_cast<double>(swap_total_kb)) *
         100.0;
}

static std::string read_single_line_file(const std::string &path) {
  std::ifstream file(path);
  if (!file.is_open())
    return "";
  std::string line;
  std::getline(file, line);
  // Strip trailing newline/whitespace
  while (!line.empty() &&
         (line.back() == '\n' || line.back() == '\r' || line.back() == ' ')) {
    line.pop_back();
  }
  return line;
}

static std::string read_os_name() {
  std::ifstream file("/etc/os-release");
  if (!file.is_open()) {
    return "Linux (Unknown OS)";
  }

  std::string line;
  std::string pretty_name;
  std::string name;

  while (std::getline(file, line)) {
    if (line.rfind("PRETTY_NAME=", 0) == 0) {
      pretty_name = line.substr(12);
    } else if (line.rfind("NAME=", 0) == 0 && name.empty()) {
      name = line.substr(5);
    }
  }

  std::string result =
      !pretty_name.empty() ? pretty_name : (!name.empty() ? name : "Linux");
  // Strip surrounding quotes if present (e.g. "Ubuntu 24.04 LTS" ->
  // Ubuntu 24.04 LTS)
  if (result.size() >= 2 && result.front() == '"' && result.back() == '"') {
    result = result.substr(1, result.size() - 2);
  }
  return result;
}

static uint64_t read_uptime_seconds() {
  std::ifstream file("/proc/uptime");
  if (!file.is_open())
    return 0;

  double uptime_val = 0.0;
  if (file >> uptime_val) {
    return static_cast<uint64_t>(uptime_val);
  }
  return 0;
}

HostInfo read_host_info() {
  HostInfo info;

  // 1. Hostname: prefer /proc/sys/kernel/hostname, fallback to gethostname()
  info.hostname = read_single_line_file("/proc/sys/kernel/hostname");
  if (info.hostname.empty()) {
    char buf[256] = {0};
    if (gethostname(buf, sizeof(buf)) == 0) {
      info.hostname = buf;
    } else {
      info.hostname = "localhost";
    }
  }

  // 2. Kernel release: /proc/sys/kernel/osrelease
  info.kernel_version = read_single_line_file("/proc/sys/kernel/osrelease");
  if (info.kernel_version.empty()) {
    info.kernel_version = "Unknown Kernel";
  }

  // 3. Operating System Name: /etc/os-release
  info.os_name = read_os_name();

  // 4. Uptime: /proc/uptime
  info.uptime_seconds = read_uptime_seconds();

  return info;
}

MemoryInfo read_memory_info() {
  MemoryInfo mem;
  std::ifstream file("/proc/meminfo");
  if (!file.is_open()) {
    return mem;
  }

  std::string line;
  bool found_available = false;

  while (std::getline(file, line)) {
    std::istringstream iss(line);
    std::string key;
    uint64_t val = 0;
    if (!(iss >> key >> val)) {
      continue;
    }

    if (key == "MemTotal:") {
      mem.total_kb = val;
    } else if (key == "MemFree:") {
      mem.free_kb = val;
    } else if (key == "MemAvailable:") {
      mem.available_kb = val;
      found_available = true;
    } else if (key == "SwapTotal:") {
      mem.swap_total_kb = val;
    } else if (key == "SwapFree:") {
      mem.swap_free_kb = val;
    }
  }

  // Calculate used memory:
  // Linux kernel provides MemAvailable (accurate estimate of memory available
  // without swapping). Therefore, Used = Total - Available.
  if (found_available && mem.total_kb >= mem.available_kb) {
    mem.used_kb = mem.total_kb - mem.available_kb;
  } else if (mem.total_kb >= mem.free_kb) {
    mem.used_kb = mem.total_kb - mem.free_kb;
  } else {
    mem.used_kb = 0;
  }

  // Calculate used swap:
  if (mem.swap_total_kb >= mem.swap_free_kb) {
    mem.swap_used_kb = mem.swap_total_kb - mem.swap_free_kb;
  } else {
    mem.swap_used_kb = 0;
  }

  return mem;
}

std::string format_kb(uint64_t kb) {
  std::ostringstream oss;
  oss << std::fixed << std::setprecision(2);

  constexpr double KIB = 1.0;
  constexpr double MIB = 1024.0;
  constexpr double GIB = 1024.0 * 1024.0;
  constexpr double TIB = 1024.0 * 1024.0 * 1024.0;

  double val = static_cast<double>(kb);
  if (val >= TIB) {
    oss << (val / TIB) << " TiB";
  } else if (val >= GIB) {
    oss << (val / GIB) << " GiB";
  } else if (val >= MIB) {
    oss << (val / MIB) << " MiB";
  } else {
    oss << (val / KIB) << " KiB";
  }

  return oss.str();
}

uint64_t CpuSnapshot::total_time() const {
  return user + nice + system + idle + iowait + irq + softirq + steal;
}

uint64_t CpuSnapshot::idle_time() const {
  return idle + iowait;
}

double calculate_cpu_percentage(const CpuSnapshot& prev, const CpuSnapshot& curr) {
  uint64_t prev_total = prev.total_time();
  uint64_t curr_total = curr.total_time();
  uint64_t prev_idle = prev.idle_time();
  uint64_t curr_idle = curr.idle_time();

  if (curr_total <= prev_total) {
    return 0.0;
  }

  uint64_t delta_total = curr_total - prev_total;
  uint64_t delta_idle = curr_idle - prev_idle;

  if (delta_total == 0) {
    return 0.0;
  }
  if (delta_idle > delta_total) {
    delta_idle = delta_total;
  }

  return (static_cast<double>(delta_total - delta_idle) / static_cast<double>(delta_total)) * 100.0;
}

std::vector<CpuSnapshot> read_cpu_snapshots() {
  std::vector<CpuSnapshot> snapshots;
  std::ifstream file("/proc/stat");
  if (!file.is_open()) {
    return snapshots;
  }

  std::string line;
  while (std::getline(file, line)) {
    // Process only CPU lines (e.g. "cpu", "cpu0", "cpu1")
    if (line.rfind("cpu", 0) != 0) {
      break;
    }

    std::istringstream iss(line);
    CpuSnapshot snap;
    if (iss >> snap.label
            >> snap.user
            >> snap.nice
            >> snap.system
            >> snap.idle
            >> snap.iowait
            >> snap.irq
            >> snap.softirq
            >> snap.steal) {
      snapshots.push_back(snap);
    }
  }

  return snapshots;
}

CpuTracker::CpuTracker() {
  prev_snapshots_ = read_cpu_snapshots();
  initialized_ = !prev_snapshots_.empty();
}

CpuInfo CpuTracker::sample() {
  CpuInfo info;
  auto current = read_cpu_snapshots();
  if (current.empty()) {
    return info;
  }

  if (!initialized_ || prev_snapshots_.empty()) {
    prev_snapshots_ = current;
    initialized_ = true;
    if (current.size() > 1) {
      info.cores.assign(current.size() - 1, 0.0);
    }
    return info;
  }

  // Aggregate percentage
  info.aggregate = calculate_cpu_percentage(prev_snapshots_[0], current[0]);

  // Per-core percentages (indices 1..N)
  size_t count = std::min(prev_snapshots_.size(), current.size());
  for (size_t i = 1; i < count; ++i) {
    info.cores.push_back(calculate_cpu_percentage(prev_snapshots_[i], current[i]));
  }

  prev_snapshots_ = std::move(current);
  return info;
}

} // namespace sysmon
