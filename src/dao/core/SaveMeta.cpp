#include "dao/core/SaveMeta.h"

#include "dao/core/Gff4.h"

#include <array>

namespace dao::core
{

std::optional<SaveMeta> SaveMeta::Parse(std::span<const std::byte> bytes)
{
    const auto file = gff4::File::Parse(bytes);
    if (!file || file->Type() != "SVMT")
    {
        return std::nullopt;
    }
    const auto root = file->Root();
    if (!root)
    {
        return std::nullopt;
    }
    SaveMeta meta;
    meta.area = root->String(kArea).value_or(L"");
    meta.character = root->String(kCharacter).value_or(L"");
    meta.name = root->String(kName).value_or(L"");
    meta.story = root->String(kStory).value_or(L"");
    meta.secondsPlayed = root->Get<std::int32_t>(kTimePlayed).value_or(0);
    meta.level = root->Get<std::int32_t>(kLevel).value_or(0);
    meta.characterClass = root->Get<std::int32_t>(kClass).value_or(0);
    meta.gender = root->Get<std::int32_t>(kGender).value_or(0);
    meta.race = root->Get<std::int32_t>(kRace).value_or(0);
    meta.background = root->Get<std::int32_t>(kBackground).value_or(0);
    return meta;
}

std::optional<SlotFolderName> SlotFolderName::Parse(std::wstring_view folder)
{
    // The kinds' prefixes, by the engine's number for the kind.
    constexpr std::array<std::wstring_view, 3> kPrefixes{L"Slot_", L"AutoSave_", L"QuickSave_"};
    for (std::size_t kind = 0; kind < kPrefixes.size(); ++kind)
    {
        if (!folder.starts_with(kPrefixes[kind]))
        {
            continue;
        }
        const std::wstring_view digits = folder.substr(kPrefixes[kind].size());
        if (digits.empty() || digits.size() > 9)
        {
            return std::nullopt;
        }
        std::int32_t number = 0;
        for (const wchar_t c : digits)
        {
            if (c < L'0' || c > L'9')
            {
                return std::nullopt;
            }
            number = number * 10 + (c - L'0');
        }
        return SlotFolderName{static_cast<std::int32_t>(kind), number};
    }
    return std::nullopt;
}

} // namespace dao::core
