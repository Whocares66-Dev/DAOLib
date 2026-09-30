// A table of the exe's functions, entry, end and name, as Ghidra exports them
// (game\ghidra\functions.tsv; dev/PROBE.md says how): the crash logger names
// every address in the game's code with it, so a crash log says
// `GameModeGUI::vfunction7+0x1C` where it would say `0x007187EC`. Pure: text
// in, lookups out.
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace dao::core
{

class FunctionTable
{
  public:
    struct Hit
    {
        std::uint32_t entry = 0;
        std::uint32_t offset = 0; // from the entry
        std::string_view name;
    };

    // Lines of `entry<TAB>end<TAB>name`, addresses in hex without 0x; lines
    // starting with '#' and lines that do not parse are skipped.
    static FunctionTable Parse(std::string_view text);

    // The function whose entry..end (inclusive) holds the address.
    std::optional<Hit> Find(std::uint32_t address) const;

    std::size_t Size() const
    {
        return entries_.size();
    }

  private:
    struct Entry
    {
        std::uint32_t entry;
        std::uint32_t end;
        std::string name;
    };
    std::vector<Entry> entries_; // sorted by entry
};

} // namespace dao::core
