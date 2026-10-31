// What the loader's performance parts share: their switches' file and their
// section on the options screen.
#pragma once

#include "dao/Options.h"

#include <filesystem>
#include <vector>

namespace perf
{

// daolib.ini beside the logs, so an instance of the game has its own.
std::filesystem::path SettingsFile();

// The "Performance" section on the options screen's Game tab, the switches in
// the order given; nothing if there are none.
void AddOptions(std::vector<dao::options::Toggle> toggles);

} // namespace perf
