#include "../include/system_reader.hpp"
#include "../include/terminal.hpp"
#include "../include/ui.hpp"

#include <chrono>
#include <thread>

int main() {
  // Terminal RAII sets up raw mode and alternate screen, restoring on exit
  sysmon::Terminal terminal;
  sysmon::CpuTracker cpu_tracker;

  bool running = true;
  while (running) {
    // 1. Collect system metrics
    sysmon::HostInfo host = sysmon::read_host_info();
    sysmon::MemoryInfo mem = sysmon::read_memory_info();
    sysmon::CpuInfo cpu = cpu_tracker.sample();

    // 2. Render UI to screen
    sysmon::UI::render(terminal, host, mem, cpu);

    // 3. Sleep 1000ms while polling for input every 50ms (20 ticks * 50ms =
    // 1000ms). This makes the UI immediately responsive to the 'q' keypress.
    for (int i = 0; i < 20; ++i) {
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
      auto key = terminal.poll_key();
      if (key.has_value()) {
        if (*key == 'q' || *key == 'Q' || *key == 27 /* ESC */) {
          running = false;
          break;
        }
      }
    }
  }

  return 0;
}
