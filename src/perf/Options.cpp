#include "perf/Options.h"

#include "dao/Log.h"

#include <utility>

namespace perf
{

std::filesystem::path SettingsFile()
{
    return dao::LogDirectory() / "daolib.ini";
}

void AddOptions(std::vector<dao::options::Toggle> toggles)
{
    if (toggles.empty())
    {
        return;
    }
    const std::size_t count = toggles.size();
    const bool shown =
        dao::options::AddSection(L"Performance", std::move(toggles), {},
                                 dao::options::Column::Right); // the left is where a plugin's section goes first
    spdlog::info("performance: {} switch(es) {}", count,
                 shown ? "in the options screen's Game tab" : "not on the options screen");
}

} // namespace perf
