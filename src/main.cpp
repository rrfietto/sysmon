#include "../include/system_reader.hpp"
#include "../include/process_reader.hpp"
#include "../include/terminal.hpp"
#include "../include/ui.hpp"

#include <chrono>
#include <thread>
#include <algorithm>

namespace {

void sort_processes(std::vector<sysmon::ProcessInfo>& procs, sysmon::SortBy sort_by) {
  switch (sort_by) {
    case sysmon::SortBy::CPU:
      std::sort(procs.begin(), procs.end(), [](const auto& a, const auto& b) {
        return a.cpu_percent > b.cpu_percent;
      });
      break;
    case sysmon::SortBy::Memory:
      std::sort(procs.begin(), procs.end(), [](const auto& a, const auto& b) {
        return a.rss_kb > b.rss_kb;
      });
      break;
    case sysmon::SortBy::PID:
      std::sort(procs.begin(), procs.end(), [](const auto& a, const auto& b) {
        return a.pid < b.pid;
      });
      break;
  }
}

} // namespace

int main() {
  sysmon::Terminal terminal;
  sysmon::CpuTracker cpu_tracker;
  sysmon::ProcessTracker proc_tracker;
  sysmon::UIState ui_state;

  // Let initial CPU delta window accumulate slightly for frame #1
  std::this_thread::sleep_for(std::chrono::milliseconds(200));

  // Collect initial baseline data
  sysmon::HostInfo host = sysmon::read_host_info();
  sysmon::MemoryInfo mem = sysmon::read_memory_info();
  sysmon::CpuInfo cpu = cpu_tracker.sample();
  auto processes = proc_tracker.sample();
  sort_processes(processes, ui_state.sort_by);

  // Initial render
  sysmon::UI::render(terminal, host, mem, cpu, processes, ui_state);

  auto last_sample_time = std::chrono::steady_clock::now();
  bool running = true;

  while (running) {
    bool needs_redraw = terminal.check_and_clear_resized();
    if (terminal.exit_requested()) break;
    auto now = std::chrono::steady_clock::now();

    // 1. Metric refresh tick (every 1000ms)
    if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_sample_time).count() >= 1000) {
      host = sysmon::read_host_info();
      mem = sysmon::read_memory_info();
      cpu = cpu_tracker.sample();
      processes = proc_tracker.sample();
      sort_processes(processes, ui_state.sort_by);

      last_sample_time = now;
      needs_redraw = true;
    }

    // 2. Poll non-blocking keyboard input
    sysmon::KeyEvent ev = terminal.poll_event();
    if (ev.key != sysmon::Key::None) {
      switch (ev.key) {
        case sysmon::Key::Quit:
          running = false;
          break;

        case sysmon::Key::Up:
          if (ui_state.selected_index > 0) {
            --ui_state.selected_index;
            needs_redraw = true;
          }
          break;

        case sysmon::Key::Down:
          if (!processes.empty() && ui_state.selected_index + 1 < static_cast<int>(processes.size())) {
            ++ui_state.selected_index;
            needs_redraw = true;
          }
          break;

        case sysmon::Key::PageUp:
          ui_state.selected_index = std::max(0, ui_state.selected_index - 10);
          needs_redraw = true;
          break;

        case sysmon::Key::PageDown:
          if (!processes.empty()) {
            ui_state.selected_index = std::min(static_cast<int>(processes.size()) - 1, ui_state.selected_index + 10);
            needs_redraw = true;
          }
          break;

        case sysmon::Key::Home:
          ui_state.selected_index = 0;
          needs_redraw = true;
          break;

        case sysmon::Key::End:
          if (!processes.empty()) {
            ui_state.selected_index = static_cast<int>(processes.size()) - 1;
            needs_redraw = true;
          }
          break;

        case sysmon::Key::Char:
          if (ev.ch == 'c' || ev.ch == 'C') {
            ui_state.sort_by = sysmon::SortBy::CPU;
            sort_processes(processes, ui_state.sort_by);
            needs_redraw = true;
          } else if (ev.ch == 'm' || ev.ch == 'M') {
            ui_state.sort_by = sysmon::SortBy::Memory;
            sort_processes(processes, ui_state.sort_by);
            needs_redraw = true;
          } else if (ev.ch == 't' || ev.ch == 'T') {
            ui_state.theme = static_cast<sysmon::Theme>((static_cast<int>(ui_state.theme) + 1) % 3);
            needs_redraw = true;
          } else if (ev.ch == 'p' || ev.ch == 'P') {
            ui_state.sort_by = sysmon::SortBy::PID;
            sort_processes(processes, ui_state.sort_by);
            needs_redraw = true;
          }
          break;

        default:
          break;
      }
    }

    // 3. Keep scroll offset clamped to current selection
    ui_state.selected_index = std::clamp(ui_state.selected_index, 0,
                                        std::max(0, static_cast<int>(processes.size()) - 1));
    int visible_rows = sysmon::UI::visible_process_rows(terminal.get_size(), cpu);
    ui_state.scroll_offset = std::clamp(ui_state.scroll_offset, 0,
                                      std::max(0, static_cast<int>(processes.size()) - visible_rows));
    if (ui_state.selected_index < ui_state.scroll_offset) {
      ui_state.scroll_offset = ui_state.selected_index;
    } else if (ui_state.selected_index >= ui_state.scroll_offset + visible_rows) {
      ui_state.scroll_offset = ui_state.selected_index - visible_rows + 1;
    }
    if (ui_state.scroll_offset < 0) {
      ui_state.scroll_offset = 0;
    }

    // 4. Render frame if metrics changed or user navigated
    if (needs_redraw && running) {
      sysmon::UI::render(terminal, host, mem, cpu, processes, ui_state);
    }

    // 5. Short sleep slice (25ms = 40Hz input response with near-zero CPU footprint)
    std::this_thread::sleep_for(std::chrono::milliseconds(25));
  }

  return terminal.exit_requested() ? 128 + terminal.exit_requested() : 0;
}
