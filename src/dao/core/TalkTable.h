// A talk table as its file holds it: the game's text by string id, and a
// plugin's own (dao/Strings.h). Over the bytes, read once into a map, so
// the bytes need not outlive it.
//
// Read 2026-09-30, as tools/tlk.py reads the game's 1,501: GFF4 of type
// "TLK " version "V0.2", the top struct's list of strings (19001), each a
// STRN of its id (19002, a u32) and its text (19003), as xoreos reads them
// (src/aurora/talktable_gff.cpp). Version 0.5, whose text is Huffman-coded,
// is not read; none of the game's files are that.
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace dao::core
{

class TalkTableFile
{
  public:
    // The table in `bytes`; nothing if they are not a version 0.2 talk table.
    static std::optional<TalkTableFile> Parse(std::span<const std::byte> bytes);

    std::optional<std::wstring> String(std::uint32_t id) const;
    std::size_t Size() const
    {
        return strings_.size();
    }

    static constexpr std::uint32_t kStringList = 19001;
    static constexpr std::uint32_t kStringId = 19002;
    static constexpr std::uint32_t kString = 19003;

  private:
    // Sorted by id; a vector, whose move cannot throw.
    std::vector<std::pair<std::uint32_t, std::wstring>> strings_;
};

} // namespace dao::core
