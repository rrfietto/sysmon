#include "../include/process_reader.hpp"
#include "../include/system_reader.hpp"

#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unistd.h>

namespace sysmon {

ProcessTracker::ProcessTracker() {
  // 1. System page size in KiB
  long page_size = sysconf(_SC_PAGESIZE);
  if (page_size > 0) {
    page_size_kb_ = static_cast<uint64_t>(page_size) / 1024;
  } else {
    page_size_kb_ = 4;
  }

  // 2. Number of online CPU cores
  long ncores = sysconf(_SC_NPROCESSORS_ONLN);
  if (ncores > 0) {
    num_cores_ = static_cast<size_t>(ncores);
  } else {
    num_cores_ = 1;
  }

  // 3. Total physical system RAM in KiB
  MemoryInfo mem = read_memory_info();
  total_system_ram_kb_ = mem.total_kb;

  // 4. Initial system ticks and process cache
  prev_system_total_ticks_ = read_system_total_ticks();
  sample(); // Populate baseline ticks
  initialized_ = true;
}

uint64_t ProcessTracker::read_system_total_ticks() const {
  std::ifstream file("/proc/stat");
  if (!file.is_open())
    return 0;

  std::string cpu_label;
  uint64_t user = 0, nice = 0, system = 0, idle = 0, iowait = 0, irq = 0,
           softirq = 0, steal = 0;
  if (file >> cpu_label >> user >> nice >> system >> idle >> iowait >> irq >>
      softirq >> steal) {
    return user + nice + system + idle + iowait + irq + softirq + steal;
  }
  return 0;
}

std::vector<ProcessInfo> ProcessTracker::sample() {
  std::vector<ProcessInfo> processes;
  std::unordered_map<int, PrevProcState> next_proc_ticks;

  uint64_t curr_system_ticks = read_system_total_ticks();
  uint64_t delta_system_ticks = 0;
  if (curr_system_ticks > prev_system_total_ticks_) {
    delta_system_ticks = curr_system_ticks - prev_system_total_ticks_;
  }

  std::error_code ec;
  auto dir_it = std::filesystem::directory_iterator(
      "/proc", std::filesystem::directory_options::skip_permission_denied, ec);
  auto end_it = std::filesystem::directory_iterator();

  for (; !ec && dir_it != end_it; dir_it.increment(ec)) {
    if (ec) {
      ec.clear();
      continue;
    }

    const auto &entry = *dir_it;
    std::string filename = entry.path().filename().string();
    if (filename.empty() || !std::isdigit(filename[0])) {
      continue;
    }

    // Verify PID folder name is entirely numeric
    bool is_pid = true;
    for (char c : filename) {
      if (!std::isdigit(c)) {
        is_pid = false;
        break;
      }
    }
    if (!is_pid)
      continue;

    int pid = 0;
    try {
      pid = std::stoi(filename);
    } catch (...) {
      continue;
    }

    // Open /proc/[pid]/stat
    std::ifstream stat_file(entry.path() / "stat");
    if (!stat_file.is_open()) {
      continue; // Process may have terminated
    }

    std::string line;
    if (!std::getline(stat_file, line)) {
      continue;
    }

    // Parse: comm is enclosed between first '(' and last ')'
    size_t open_paren = line.find('(');
    size_t close_paren = line.rfind(')');
    if (open_paren == std::string::npos || close_paren == std::string::npos ||
        close_paren <= open_paren) {
      continue;
    }

    ProcessInfo proc;
    proc.pid = pid;
    proc.name = line.substr(open_paren + 1, close_paren - open_paren - 1);

    // Parse remaining fields after ')'
    std::istringstream iss(line.substr(close_paren + 1));

    // Field 3: state
    if (!(iss >> proc.state))
      continue;

    // Skip fields 4 through 13
    std::string dummy;
    bool skip_ok = true;
    for (int i = 4; i <= 13; ++i) {
      if (!(iss >> dummy)) {
        skip_ok = false;
        break;
      }
    }
    if (!skip_ok)
      continue;

    // Field 14: utime, Field 15: stime
    if (!(iss >> proc.utime >> proc.stime))
      continue;

    // Skip fields 16 through 23
    for (int i = 16; i <= 23; ++i) {
      if (!(iss >> dummy)) {
        skip_ok = false;
        break;
      }
    }
    if (!skip_ok)
      continue;

    // Field 24: rss in pages
    int64_t rss_pages = 0;
    if (iss >> rss_pages && rss_pages > 0) {
      proc.rss_kb = static_cast<uint64_t>(rss_pages) * page_size_kb_;
    }

    // Calculate Memory %
    if (total_system_ram_kb_ > 0) {
      proc.mem_percent = (static_cast<double>(proc.rss_kb) /
                          static_cast<double>(total_system_ram_kb_)) *
                         100.0;
    }

    // Calculate CPU %
    uint64_t curr_proc_ticks = proc.utime + proc.stime;
    next_proc_ticks[pid] = {curr_proc_ticks};

    if (initialized_ && delta_system_ticks > 0) {
      auto prev_it = prev_proc_ticks_.find(pid);
      if (prev_it != prev_proc_ticks_.end() &&
          curr_proc_ticks >= prev_it->second.total_time) {
        uint64_t delta_proc_ticks =
            curr_proc_ticks - prev_it->second.total_time;
        proc.cpu_percent = (static_cast<double>(delta_proc_ticks) /
                            static_cast<double>(delta_system_ticks)) *
                           100.0 * static_cast<double>(num_cores_);
      }
    }

    processes.push_back(proc);
  }

  prev_proc_ticks_ = std::move(next_proc_ticks);
  prev_system_total_ticks_ = curr_system_ticks;

  return processes;
}

} // namespace sysmon
