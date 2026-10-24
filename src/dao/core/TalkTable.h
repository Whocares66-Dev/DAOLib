// A talk table as its file holds it: the game's text by string id, and a
// plugin's own (dao/Strings.h). Over the bytes, read once into a map, so
// the bytes need not outlive it.
//
// Read 2026-09-30, as tools/gff4.py reads the game's 1,501: GFF4 of type
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
    // The table in `bytes`; nothing if they are not a version 0.2 talk table. An id the table
    // holds with no text is kept, with an empty text, as the game finds it.
    static std::optional<TalkTableFile> Parse(std::span<const std::byte> bytes);

    // Returns the text of an id; nothing if the table does not hold it.
    std::optional<std::wstring> String(std::uint32_t id) const;
    std::size_t Size() const
    {
        return strings_.size();
    }
    // Returns every string with its id, in the ids' order: for a text's search.
    const std::vector<std::pair<std::uint32_t, std::wstring>> &Strings() const
    {
        return strings_;
    }

    // Returns a talk table of the strings laid out byte for byte as the game's own are (GFF4 TLK
    // V0.2), for a mod's own names: an add-in's core\data\talktables\<uid>_c_<language>.tlk, as
    // the game's own add-ins have theirs. An empty text is written as none.
    // @return empty for an id given twice, or an id of kEmptySlot
    static std::vector<std::byte> Write(std::span<const std::pair<std::uint32_t, std::wstring>> strings);

    // Returns the game's hash of an id: its remainder by a table's slots is the slot the game's
    // search for the id starts at.
    static std::uint32_t Hash(std::uint32_t id);

    static constexpr std::uint32_t kStringList = 19001;
    static constexpr std::uint32_t kStringId = 19002;
    static constexpr std::uint32_t kString = 19003;
    // The id of a slot of the list that holds no string.
    static constexpr std::uint32_t kEmptySlot = 0xFFFFFFFF;

  private:
    // Sorted by id; a vector, whose move cannot throw.
    std::vector<std::pair<std::uint32_t, std::wstring>> strings_;
};

} // namespace dao::core
