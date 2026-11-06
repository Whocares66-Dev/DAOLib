// A saved hero (.chr), read and written as the game reads and writes it (DAOLib #326): the hero
// character creation's Save button writes, its head as a preset (dao/core/HeadPreset.h), its
// race, class, background, name and choices. The game writes one to
// Characters\<character>\<name>.chr and lists the ones in Pre-release Characters\ on the
// creation screen.
//
//   const auto hero = dao::core::hero::HeroOf(*gff4::File::Parse(bytes));
//   // hero->name == L"Jaden", hero->preset == L"HM_CPS_P01.mop"
//   const std::vector<std::byte> again = dao::core::hero::Write(*hero); // the same bytes
#pragma once

#include "dao/core/Gff4.h"
#include "dao/core/HeadPreset.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace dao::core::hero
{

// Points put into an attribute on the creation screen.
struct Attribute
{
    std::uint32_t id = 0;
    std::int32_t points = 0;
};

struct Hero
{
    // The head, as a preset holds one. Its name is not written: the game writes "Head_Morph".
    preset::Preset head;
    std::wstring preset;     // the preset the head started from, "HM_CPS_P01.mop"
    std::uint8_t gender = 0; // the appearance's: 1 male, 2 female
    std::int32_t race = 0;
    std::int32_t characterClass = 0;
    std::int32_t background = 0;
    std::wstring name;
    std::vector<Attribute> attributes; // written in order of id, as the game keeps them
    std::vector<std::int32_t> abilities;
    std::array<float, 7> portrait{}; // the portrait's settings, as the creation screen keeps them
};

// Returns a saved hero, or nothing for a file that is no hero. A field the file lacks is 0 or empty.
std::optional<Hero> HeroOf(const gff4::File &file);

// Returns the file of a hero as the game writes it, byte for byte.
std::vector<std::byte> Write(const Hero &hero);

// Returns the heroes (.chr files) in a folder and the folders in it, in the order Windows lists
// them, as the creation screen lists Pre-release Characters\. An empty list if there is no such
// folder.
std::vector<std::filesystem::path> HeroesIn(const std::filesystem::path &folder);

} // namespace dao::core::hero
