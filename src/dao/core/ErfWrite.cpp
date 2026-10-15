// An archive of version 2.0 written as the game's own are laid out (DAOLib
// #122), for a mod made with the writers to ship as one file: an .erf in an
// override folder is a source of the game's (dev/FILES.md).
//
// Read 2026-10-14 over every 2.0 archive of packages\core\data and \patch:
// the header is "ERF V2.0" in UTF-16, the count, the year less 1900 and the
// days since 1 January of the day it was built (2da.erf: 109 and 258, in
// September 2009), and 0xFFFFFFFF; the
// entries are in their names' order, every name lower case and at most 31
// units; the files' bytes follow the directory with no gap before the first,
// in the entries' order, each after the one before at the next multiple of
// four.
#include "dao/core/Erf.h"

#include <algorithm>
#include <cstring>

namespace dao::core::erf
{

namespace
{

template <class T> void Put(std::vector<std::byte> &out, std::size_t at, T value)
{
    std::memcpy(out.data() + at, &value, sizeof(T));
}

std::wstring Lower(std::wstring name)
{
    std::ranges::transform(name, name.begin(), [](wchar_t c) {
        return c >= L'A' && c <= L'Z' ? static_cast<wchar_t>(c + (L'a' - L'A')) : c;
    });
    return name;
}

} // namespace

std::vector<std::byte> Write(std::span<const Item> files, std::chrono::sys_days written)
{
    std::vector<std::pair<std::wstring, const Item *>> sorted;
    for (const Item &file : files)
    {
        if (file.name.empty() || file.name.size() > kLongestName ||
            std::ranges::any_of(file.name, [](wchar_t c) { return c < L' ' || c > L'~'; }))
        {
            return {};
        }
        sorted.emplace_back(Lower(file.name), &file);
    }
    std::ranges::sort(sorted, {}, &std::pair<std::wstring, const Item *>::first);
    if (sorted.empty() ||
        std::ranges::adjacent_find(sorted, {}, &std::pair<std::wstring, const Item *>::first) != sorted.end())
    {
        return {};
    }
    const std::size_t directory = kHeaderSize + (sorted.size() * kEntrySize);
    std::size_t end = directory;
    for (const auto &[name, file] : sorted)
    {
        end = ((end + 3) & ~std::size_t{3}) + file->bytes.size();
    }
    std::vector<std::byte> out(end);
    const std::u16string magic = u"ERF V2.0";
    std::memcpy(out.data(), magic.data(), magic.size() * 2);
    const std::chrono::year_month_day date{written};
    const std::chrono::sys_days newYear{date.year() / std::chrono::January / 1};
    Put(out, 16, static_cast<std::uint32_t>(sorted.size()));
    Put(out, 20, static_cast<std::uint32_t>(static_cast<int>(date.year()) - 1900));
    Put(out, 24, static_cast<std::uint32_t>((written - newYear).count()));
    Put(out, 28, 0xFFFFFFFFU);
    std::size_t at = directory;
    for (std::size_t i = 0; i < sorted.size(); ++i)
    {
        const auto &[name, file] = sorted[i];
        const std::size_t entry = kHeaderSize + (i * kEntrySize);
        for (std::size_t unit = 0; unit < name.size(); ++unit)
        {
            Put(out, entry + (unit * 2), static_cast<char16_t>(name[unit]));
        }
        at = (at + 3) & ~std::size_t{3};
        Put(out, entry + 64, static_cast<std::uint32_t>(at));
        Put(out, entry + 68, static_cast<std::uint32_t>(file->bytes.size()));
        std::ranges::copy(file->bytes, out.begin() + static_cast<std::ptrdiff_t>(at));
        at += file->bytes.size();
    }
    return out;
}

} // namespace dao::core::erf
