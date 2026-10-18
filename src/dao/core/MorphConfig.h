// What the character creation's appearance scene offers, as
// chargenmorphcfg.xml (misc.erf) lists it, read as text (DAOLib #171): the
// head presets (dao/core/HeadPreset.h), hairs, beards, tints, tattoos and
// skins, some lists by race and gender and some for all.
//
//   const auto config = dao::core::preset::ReadMorphConfig(xml);
//   const auto *heads = config.Find("heads", dao::core::preset::RaceGender::HumanMale);
//   // heads->resources[0].name == "HM_CPS_P01.mop", slider 0x28's first choice
//
// Read 2026-10-18 on the game's one copy: <morph_config> holds fourteen
// lists, each of <resource name="..."/> in order. heads (nine .mop a race
// and gender, eight *_CPS_P0n and *_PCC_B01), hairs (16 for a male, 17 for a
// female, the model's name with its "_0", each with a cut: "0234" or "02" on
// the bald one, "0" on hd4a for a male, "1" on the rest) and beards (seven
// for human_male and dwarf_male, the first empty for none) are by race and
// gender. The rest are for all: hair_colors 9, skin_colors 7, eyes_colors
// 11, eyes_makeup_colors 31, blush_makeup_colors 25 and lip_makeup_colors 25
// (each first empty for none), brow_stubble_colors and crew_cut_colors 9
// (the same files), tattoo_colors 15, all tint files' names; tattoos 9 and
// skins 6, textures' names. A stray "1" between two elements of the file is
// text, which the reader leaves.
//
// lib-agent-2's reading of the exe, 2026-10-18: the engine loads the file
// once (0x008042D0, its element handler 0x00800B20) and numbers the race and
// gender lists as RaceGender does; the lists' counts are the appearance
// sliders' ranges, slider 0x28 loading the race and gender's heads entry, 7
// the eyes_colors entry, 0x29 a hair choice (HairChoices), 0x2D skin_colors,
// 0x2C skins. A hair is offered once per digit of its cut, each time with
// that digit for its cut, and the cut c, if not 0, sets HairM1's
// g_vFacialHairWeights component c + 3 (inferred: the scalp's crew-cut
// masks). A list the file leaves empty is given one empty entry.
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace dao::core::preset
{

// The race and gender lists, in the engine's order.
enum class RaceGender : std::uint8_t
{
    HumanMale = 0,
    HumanFemale = 1,
    DwarfMale = 2,
    DwarfFemale = 3,
    ElfMale = 4,
    ElfFemale = 5,
};

// Returns a race and gender's element in the file: "human_male".
std::string_view GroupOf(RaceGender raceGender);

// One entry of a list: a resource's name as written, and a hair's cut.
struct Resource
{
    std::string name; // "HM_CPS_P01.mop", "hm_har_blda_0", "t3_har_wht"; "" for none
    std::string cut;  // a hair's "0234", one choice per digit; "" where the entry has none
};

// One of the file's lists.
struct List
{
    std::string kind;  // its element: "heads", "hairs", "hair_colors"
    std::string group; // its race and gender's element, "human_male"; "" for a list for all
    std::vector<Resource> resources;
};

// One choice of the Hair slider: a hair and the cut it is offered with.
struct HairChoice
{
    std::string name; // "hm_har_blda_0"
    int cut = 0;      // 0 to 4
};

struct Config
{
    std::vector<List> lists; // in file order

    // Returns a list by its kind and its group ("" for a list for all); null for none.
    const List *Find(std::string_view kind, std::string_view group = {}) const;
    const List *Find(std::string_view kind, RaceGender raceGender) const;
};

// Returns the lists of a chargenmorphcfg.xml; none for text that has no <morph_config>.
Config ReadMorphConfig(std::string_view xml);

// Returns a hairs list's choices in the slider's order: a hair once per digit 0 to 4 of its cut,
// once with cut 0 where it has none.
std::vector<HairChoice> HairChoices(const List &hairs);

} // namespace dao::core::preset
