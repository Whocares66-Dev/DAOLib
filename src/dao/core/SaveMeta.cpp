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

namespace
{

// Sets a string field to `text` unless the original reads the same; false if the file cannot take it.
bool PutString(gff4::Editor &ed, const gff4::Struct &root, std::uint32_t label, const std::wstring &text)
{
    if (root.String(label).value_or(L"") == text)
    {
        return true;
    }
    return ed.SetString(root, label, text);
}

bool PutNumber(gff4::Editor &ed, const gff4::Struct &root, std::uint32_t label, std::int32_t value)
{
    if (root.Get<std::int32_t>(label).value_or(0) == value)
    {
        return true;
    }
    return ed.SetNumber(root, label, value);
}

} // namespace

// Read 2026-10-21 for #226: the engine writes the metadata with the save (SaveGame::Write 0x00646280 appends
// ".met" to the .das name, 0x00648110 fills it) and the save list reads it for every save, listing only one
// whose file has 16809 ("single player;"; 16810 is "SP;"), so neither is touched here. A string that changes
// is appended and its slot pointed at it; a number is written over its slot. dev/SAVES.md has the rest.
std::optional<SaveMeta::Written> SaveMeta::Write(std::span<const std::byte> original, const SaveMeta &meta)
{
    const auto file = gff4::File::Parse(original);
    if (!file || file->Type() != "SVMT")
    {
        return std::nullopt;
    }
    const auto root = file->Root();
    if (!root)
    {
        return std::nullopt;
    }
    gff4::Editor ed(*file);
    Written out;
    const auto text = [&](std::uint32_t label, const std::wstring &value) {
        if (!PutString(ed, *root, label, value))
        {
            out.unwritten.push_back(label);
        }
    };
    const auto number = [&](std::uint32_t label, std::int32_t value) {
        if (!PutNumber(ed, *root, label, value))
        {
            out.unwritten.push_back(label);
        }
    };
    text(kStory, meta.story);
    text(kArea, meta.area);
    number(kTimePlayed, meta.secondsPlayed);
    number(kLevel, meta.level);
    number(kClass, meta.characterClass);
    number(kGender, meta.gender);
    number(kRace, meta.race);
    number(kBackground, meta.background);
    text(kCharacter, meta.character);
    text(kName, meta.name);
    out.bytes = ed.Bytes();
    return out;
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
            number = (number * 10) + (c - L'0');
        }
        return SlotFolderName{static_cast<std::int32_t>(kind), number};
    }
    return std::nullopt;
}

} // namespace dao::core
