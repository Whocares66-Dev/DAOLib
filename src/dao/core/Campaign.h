// A campaign's info file (.cif) read, over bytes: the module script the
// module's own events go to, the area and place a new game starts in, the
// campaign's variables, and its tree of areas, each with the archives it is
// loaded from. The single player campaign's is modules/single
// player/singleplayer.cif; each add-in's is its module folder's <uid>.cif.
//
//   const auto campaign = dao::core::campaign::CampaignOf(file);
//   campaign->script; // "sp_module": EVENT_TYPE_MODULE_LOAD and MODULE_START's
//
// The file is GFF4 "CIF V0.1" (dev/gff4_fields.csv, 3290 to 3294 and 17000
// to 17124). Read 2026-10-17 over the install's 41 (DAOLib #168): the
// single player campaign's script is sp_module, its entry area
// default_start_area, 130 variables and 192 groups of areas; an add-in that
// extends another campaign has no entry area or script. A variable's value is
// a generic field: an int for its type 1, a string for 12, none written for
// the 3s and 4s of most files.
#pragma once

#include "dao/core/Gff4.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace dao::core::campaign
{

struct Variable
{
    std::wstring name;
    std::uint8_t type = 0; // the scripts' 1 int, 2 float, 3 string, 12 resource, as written
    std::variant<std::monostate, std::int32_t, float, std::wstring> value; // none where none is written
};

// A node of the campaign's area tree: a group (its tag, no resref) or an area (its .are's
// resref) and the archives (.rim) it is loaded from.
struct Node
{
    std::wstring tag;
    std::wstring area;                  // the area's resref; empty for a group
    std::vector<std::wstring> archives; // "lak100d", "al_arl01al_redcliffe_villag"
    std::vector<Node> children;
};

struct Campaign
{
    std::wstring script;       // the module's script; empty for an add-in to another campaign
    std::wstring clientScript; // empty in every file read
    std::wstring entryArea;    // where a new game starts
    std::wstring entryAreaList;
    std::array<float, 3> entryPosition{};
    std::array<float, 3> entryOrientation{}; // a direction
    std::wstring displayName;                // the English one
    std::vector<Variable> variables;
    Node areas;                         // the root, the campaign's own tag
    std::vector<std::wstring> packages; // as written; most files list none
};

// Returns a campaign's info, or nothing for a file that is not one.
std::optional<Campaign> CampaignOf(const gff4::File &file);

} // namespace dao::core::campaign
