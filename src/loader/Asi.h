#pragma once

#include <filesystem>

namespace loader::asi
{

// Load every *.asi beside the loader, then in its scripts\ and plugins\
// folders, each folder in name order, and call each one's InitializeASI
// export where it has one: what Ultimate ASI Loader does with its default
// settings. Called on the game's first thread before the game's code runs.
void LoadAll(const std::filesystem::path &folder);

} // namespace loader::asi
