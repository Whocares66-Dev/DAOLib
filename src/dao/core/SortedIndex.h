// A lookup kept as a sorted vector of (key, place): what GdaFile and m2da::Merged find a row by its
// ID and a column by its hash with (DAOLib #274). A vector, which the tables holding it move without
// allocating, as a hash map in MSVC's library does not.
#pragma once

#include <algorithm>
#include <cstddef>
#include <optional>
#include <utility>
#include <vector>

namespace dao::core
{

template <class Key> using SortedIndex = std::vector<std::pair<Key, std::size_t>>;

// Sorts an index's pairs by key and keeps the first place of each key: the lowest, as a scan from
// the start would find it.
template <class Key> void KeepFirstOfEach(SortedIndex<Key> &index)
{
    std::ranges::sort(index);
    const auto [from, to] = std::ranges::unique(index, {}, &std::pair<Key, std::size_t>::first);
    index.erase(from, to);
}

// Returns the place a sorted index gives a key, if any.
template <class Key> std::optional<std::size_t> PlaceOf(const SortedIndex<Key> &index, Key key)
{
    const auto found = std::ranges::lower_bound(index, key, {}, &std::pair<Key, std::size_t>::first);
    return found != index.end() && found->first == key ? std::optional(found->second) : std::nullopt;
}

} // namespace dao::core
