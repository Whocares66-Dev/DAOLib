// The first part of a GFF4 file as the engine writes a save's or a save's metadata: the header, the template
// table and the fields, from a table of templates in code. Shared by savegame::Blank and SaveMeta::Blank.
#pragma once

#include "dao/core/generated/SaveTemplates.h"

#include <cstddef>
#include <span>
#include <string_view>
#include <vector>

namespace dao::core::savegame::detail
{

// The file's bytes up to its data, which starts at the offset the header gives.
// @param type the file's four-letter GFF4 type, "SAVE" or "SVMT"
// @param endWord whether a word of 0xFFFFFFFF closes the fields (a save's has one, its metadata's has none)
std::vector<std::byte> Directory(std::string_view type, std::span<const SaveTemplate> templates,
                                 std::span<const SaveField> fields, bool endWord);

} // namespace dao::core::savegame::detail
