#include "dao/core/FunctionTable.h"

#include <algorithm>

namespace dao::core
{

namespace
{

std::optional<std::uint32_t> Hex(std::string_view text)
{
    if (text.empty() || text.size() > 8)
    {
        return std::nullopt;
    }
    std::uint32_t value = 0;
    for (const char c : text)
    {
        std::uint32_t digit = 0;
        if (c >= '0' && c <= '9')
        {
            digit = static_cast<std::uint32_t>(c - '0');
        }
        else if (c >= 'a' && c <= 'f')
        {
            digit = static_cast<std::uint32_t>(c - 'a' + 10);
        }
        else if (c >= 'A' && c <= 'F')
        {
            digit = static_cast<std::uint32_t>(c - 'A' + 10);
        }
        else
        {
            return std::nullopt;
        }
        value = (value * 16) + digit;
    }
    return value;
}

} // namespace

FunctionTable FunctionTable::Parse(std::string_view text)
{
    FunctionTable table;
    while (!text.empty())
    {
        const auto newline = text.find('\n');
        std::string_view line = text.substr(0, newline);
        text = newline == std::string_view::npos ? std::string_view{} : text.substr(newline + 1);
        if (!line.empty() && line.back() == '\r')
        {
            line.remove_suffix(1);
        }
        if (line.empty() || line.front() == '#')
        {
            continue;
        }
        const auto tab1 = line.find('\t');
        const auto tab2 = tab1 == std::string_view::npos ? tab1 : line.find('\t', tab1 + 1);
        if (tab2 == std::string_view::npos)
        {
            continue;
        }
        const auto entry = Hex(line.substr(0, tab1));
        const auto end = Hex(line.substr(tab1 + 1, tab2 - tab1 - 1));
        if (!entry || !end || *end < *entry)
        {
            continue;
        }
        table.entries_.push_back(Entry{*entry, *end, std::string(line.substr(tab2 + 1))});
    }
    std::ranges::sort(table.entries_, {}, &Entry::entry);
    return table;
}

std::optional<FunctionTable::Hit> FunctionTable::Find(std::uint32_t address) const
{
    // The last function starting at or before the address.
    auto it = std::ranges::upper_bound(entries_, address, {}, &Entry::entry);
    if (it == entries_.begin())
    {
        return std::nullopt;
    }
    --it;
    if (address > it->end)
    {
        return std::nullopt;
    }
    return Hit{.entry = it->entry, .offset = address - it->entry, .name = it->name};
}

} // namespace dao::core
