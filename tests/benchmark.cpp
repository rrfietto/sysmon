#include "../include/process_reader.hpp"
#include "../include/system_reader.hpp"
#include "../include/terminal.hpp"
#include "../include/ui.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <stdexcept>
#include <sstream>
#include <ctime>
#include <streambuf>
#include <sys/resource.h>
#include <unistd.h>

namespace {
class CountingBuffer : public std::streambuf {
public:
  size_t bytes{0};
protected:
  std::streamsize xsputn(const char*, std::streamsize count) override {
    bytes += static_cast<size_t>(count);
    return count;
  }
  int_type overflow(int_type ch) override {
    if (!traits_type::eq_int_type(ch, traits_type::eof())) ++bytes;
    return traits_type::not_eof(ch);
  }
};

class CaptureOutput {
public:
  CountingBuffer buffer;
  CaptureOutput() : original_(std::cout.rdbuf(&buffer)) {}
  ~CaptureOutput() { std::cout.rdbuf(original_); }
private:
  std::streambuf* original_;
};

double cpu_ms(const rusage& usage) {
  return (usage.ru_utime.tv_sec + usage.ru_stime.tv_sec) * 1000.0 +
         (usage.ru_utime.tv_usec + usage.ru_stime.tv_usec) / 1000.0;
}

void sort(std::vector<sysmon::ProcessInfo>& processes, const std::string& stage) {
  std::sort(processes.begin(), processes.end(), [&](const auto& a, const auto& b) {
    if (stage == "sort-memory") return a.rss_kb > b.rss_kb;
    if (stage == "sort-pid") return a.pid < b.pid;
    return a.cpu_percent > b.cpu_percent;
  });
}
} // namespace

int main(int argc, char** argv) {
  try {
    if (argc != 3 || isatty(STDIN_FILENO) || isatty(STDOUT_FILENO)) {
      throw std::runtime_error("Run via tests/benchmark.py (non-TTY input/output required)");
    }
    const std::string stage = argv[1];
    const int iterations = std::stoi(argv[2]);
    if (iterations < 1 || iterations > 100000)
      throw std::runtime_error("iterations must be between 1 and 100000");
    const std::vector<std::string> stages = {
      "host", "memory", "cpu", "processes", "sort-cpu", "sort-memory",
      "sort-pid", "render", "refresh"};
    if (std::find(stages.begin(), stages.end(), stage) == stages.end())
      throw std::runtime_error("unknown benchmark stage");

    sysmon::CpuTracker cpu_tracker;
    sysmon::ProcessTracker process_tracker;
    auto host = sysmon::read_host_info();
    auto memory = sysmon::read_memory_info();
    auto cpu = cpu_tracker.sample();
    auto processes = process_tracker.sample();
    const auto baseline = processes;
    const auto process_count = processes.size();
    sysmon::UIState state;
    size_t output_bytes = 0;
    size_t observed_processes = 0;
    std::vector<double> times;
    times.reserve(iterations);
    rusage before{}, after{};
    std::string report;
    {
      CaptureOutput capture;
      sysmon::Terminal terminal;
      auto run = [&] {
        if (stage == "host") host = sysmon::read_host_info();
        else if (stage == "memory") memory = sysmon::read_memory_info();
        else if (stage == "cpu") cpu = cpu_tracker.sample();
        else if (stage == "processes") processes = process_tracker.sample();
        else if (stage.rfind("sort-", 0) == 0) {
          // Recopy outside the timed region below to avoid measuring sorted input.
          sort(processes, stage);
        } else if (stage == "render") {
          sysmon::UI::render(terminal, host, memory, cpu, processes, state);
        } else {
          host = sysmon::read_host_info();
          memory = sysmon::read_memory_info();
          cpu = cpu_tracker.sample();
          processes = process_tracker.sample();
          sort(processes, "sort-cpu");
          sysmon::UI::render(terminal, host, memory, cpu, processes, state);
        }
      };
      for (int i = 0; i < 5; ++i) {
        if (stage.rfind("sort-", 0) == 0) processes = baseline;
        run();
      }
      capture.buffer.bytes = 0;
      double total_cpu_ms = 0;
      getrusage(RUSAGE_SELF, &before);
      for (int i = 0; i < iterations; ++i) {
        if (stage.rfind("sort-", 0) == 0) processes = baseline;
        timespec cpu_start{}, cpu_end{};
        clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &cpu_start);
        const auto start = std::chrono::steady_clock::now();
        run();
        const auto end = std::chrono::steady_clock::now();
        clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &cpu_end);
        total_cpu_ms += (cpu_end.tv_sec - cpu_start.tv_sec) * 1000.0 +
                        (cpu_end.tv_nsec - cpu_start.tv_nsec) / 1000000.0;
        times.push_back(std::chrono::duration<double, std::micro>(end - start).count());
        observed_processes += processes.size();
      }
      getrusage(RUSAGE_SELF, &after);
      output_bytes = capture.buffer.bytes;
      std::sort(times.begin(), times.end());
      auto percentile = [&](double p) {
        return times[static_cast<size_t>(std::ceil(p * times.size())) - 1];
      };
      // Publish only after the rendering sink has been restored.
      std::ostringstream result;
      result << std::fixed << std::setprecision(3)
             << "{\"stage\":\"" << stage << "\",\"iterations\":" << iterations
             << ",\"mean_us\":" << std::accumulate(times.begin(), times.end(), 0.0) / iterations
             << ",\"p50_us\":" << percentile(0.50)
             << ",\"p95_us\":" << percentile(0.95)
             << ",\"max_us\":" << times.back()
             << ",\"cpu_us\":" << total_cpu_ms * 1000.0 / iterations
             << ",\"peak_rss_kib\":" << after.ru_maxrss
             << ",\"minor_faults\":" << after.ru_minflt - before.ru_minflt
             << ",\"major_faults\":" << after.ru_majflt - before.ru_majflt
             << ",\"initial_processes\":" << process_count
             << ",\"mean_processes\":" << static_cast<double>(observed_processes) / iterations
             << ",\"bytes_per_frame\":" << static_cast<double>(output_bytes) / iterations
             << ",\"loop_cpu_ms\":" << cpu_ms(after) - cpu_ms(before) << "}";
      report = result.str();
    }
    std::cout << report << '\n';
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
