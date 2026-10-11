#pragma once

#include "system_reader.hpp"
#include "terminal.hpp"
#include <string>

namespace sysmon {

class UI {
public:
    // Renders the entire dashboard frame
    static void render(const Terminal& terminal,
                       const HostInfo& host,
                       const MemoryInfo& mem,
                       const CpuInfo& cpu);

private:
    // Creates a colored progress gauge, e.g. [████████░░░░░░░░] 50.0%
    static std::string render_bar(double percent, int width = 24);

    // Returns ANSI color code depending on resource threshold
    static std::string color_for_percent(double percent);
};

} // namespace sysmon
