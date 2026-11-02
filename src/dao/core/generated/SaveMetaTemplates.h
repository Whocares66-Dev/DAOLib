// Written by tools/gen_save_templates.py from the template of a .das.met; do not edit.
//
// The struct template of a save's metadata (GFF4 type SVMT): the engine's own definition, the same in every
// .das.met, with each field's label (dev/gff4_fields.csv), type, flags and offset.
// SaveMeta::Blank writes it as the exe does.
#pragma once

#include "dao/core/generated/SaveTemplates.h"

#include <array>

namespace dao::core::detail
{

inline constexpr std::array<savegame::detail::SaveField, 12> kMetaFields{{
    {16511, 14, 0x0000, 0},
    {16800, 14, 0x0000, 4},
    {16801, 5, 0x0000, 8},
    {16802, 5, 0x0000, 12},
    {16803, 5, 0x0000, 16},
    {16804, 5, 0x0000, 20},
    {16805, 5, 0x0000, 24},
    {16806, 5, 0x0000, 28},
    {16807, 14, 0x0000, 32},
    {16808, 14, 0x0000, 36},
    {16809, 14, 0x0000, 40},
    {16810, 14, 0x0000, 44},
}};

inline constexpr std::array<savegame::detail::SaveTemplate, 1> kMetaTemplates{{
    {{{'\0', '\0', '\0', '\0'}}, 48, 0, 12},
}};

} // namespace dao::core::detail
