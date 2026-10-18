// A plot (.plo) read, over bytes: the plot's GUID, as a conversation's
// PlotCheck names it, its script, and its flags, each with its journal text,
// its reward and whether it ends the plot, and whether it is stored or
// defined: computed by the plot's script when asked.
//
//   const auto plot = dao::core::plot::PlotOf(file);
//   const auto *flag = plot->Find(1); // AOA_DARKSPAWN_GENS_COMPLETE
//
// The file is GFF4 "PLTS V1.0", one plot a file (dev/gff4_fields.csv, 13000
// to 13025). Read 2026-10-17 over the 1,191 the game takes but the
// enciphered add-ins' (DAOLib #169): 9,558 flags numbered 0 to 127, the
// plot's four 32-bit words of stored state (PlotFlags1 to 4, all 0 but in
// 41 files), and 1,695 numbered 256 to 318, the defined ones; 532 plots run
// plot_core, the rest a script of their own. The engine takes a flag of 128
// or more for a defined one by its number alone (0x0075EB5A); dev/PLOTS.md
// has how a flag is read and set, and what the plot's script is given.
#pragma once

#include "dao/core/Gff4.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace dao::core::plot
{

// The first defined flag's number: a flag below it is stored in the plot's state, one at or above
// it is the plot script's to answer.
inline constexpr std::int32_t kFirstDefined = 128;

// An object that marks the way to a flag's place on the map.
struct Assist
{
    std::wstring tag;
    std::uint8_t advancesPlot = 0; // 255 where reaching it advances the plot, as the file has it
};

struct Flag
{
    std::int32_t id = 0;
    std::wstring name; // "AOA_DARKSPAWN_GENS_STARTED", the scripts' constant
    std::int32_t reward = 0;
    gff4::TlkString journal;   // its journal entry's text
    std::uint8_t endsPlot = 0; // 255 for a flag that ends the plot
    std::uint8_t multiReward = 0;
    std::wstring areaLocationTag;
    std::wstring offerId;
    std::vector<Assist> assists;

    // Whether the plot's script computes the flag rather than the plot storing it.
    bool Defined() const
    {
        return id >= kFirstDefined;
    }
};

struct Plot
{
    std::wstring resref; // "aoa100pt_darkspawn_gens"; the file is plt_<resref>.plo
    std::wstring guid;   // 32 hex digits, "7F0D8B7B630644D3BB8DC50023550AB7"
    gff4::TlkString name;
    std::wstring script; // "plot_core"
    std::int32_t priority = 0;
    std::array<std::uint32_t, 4> stored{}; // PlotFlags1 to 4: flags 0 to 127 as the file sets them
    std::wstring journalImage;
    std::wstring parent; // the parent plot's resref; empty for none
    std::wstring parentGuid;
    std::int32_t entryType = 0;
    std::uint8_t allowPausing = 0;
    std::vector<Flag> flags;

    // Returns the flag of a number; null for none.
    const Flag *Find(std::int32_t id) const;
    // Returns whether a stored flag is set in the file's state; false for a defined one.
    bool Stored(std::int32_t id) const;
};

// Returns a plot file's plot, or nothing for a file that is not one.
std::optional<Plot> PlotOf(const gff4::File &file);

} // namespace dao::core::plot
