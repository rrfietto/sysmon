#pragma once

#include "system_reader.hpp"
#include "process_reader.hpp"
#include "terminal.hpp"
#include <string>
#include <vector>

namespace sysmon {

enum class SortBy {
    CPU,
    Memory,
    PID
};

enum class Theme { Default, Ocean, Mono };

struct UIState {
    Theme theme{Theme::Default};
    SortBy sort_by{SortBy::CPU};
    int selected_index{0};
    int scroll_offset{0};
};

class UI {
public:
    static int visible_process_rows(TerminalSize size, const CpuInfo& cpu);

    // Renders the entire dashboard frame including gauges and process table
    static void render(const Terminal& terminal,
                       const HostInfo& host,
                       const MemoryInfo& mem,
                       const CpuInfo& cpu,
                       const std::vector<ProcessInfo>& processes,
                       const UIState& state);

private:
    // Creates a colored progress gauge, e.g. [████████░░░░░░░░] 50.0%
    static std::string render_bar(double percent, int width = 24);

    // Returns ANSI color code depending on resource threshold
    static std::string color_for_percent(double percent);
};

} // namespace sysmon
