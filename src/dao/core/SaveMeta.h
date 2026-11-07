// A save's metadata and its slot's folder name, as the files have them: the
// part of dao/Saves.h that needs no game.
//
// The metadata is the save's <name>.das.met, what the load screen shows
// without opening the save (dev/SAVES.md). Read 2026-09-30 from the saves
// on this machine with tools/gff4.py: GFF4 of type "SVMT" version "V1.1",
// one struct of the journal's story text, the area's name, the time
// played, the level, class, gender, race and background, the character's
// and the save's names (the field names xoreos gives, dev/gff4_fields.csv),
// and two more xoreos does not name, holding "single player;" and "SP;".
// The time played is in seconds: two autosaves made 264 s apart differ by
// 263. The save's name is empty for an autosave or a quicksave.
//
// A slot's folder is named for its kind and number (SaveGame::SlotFolder,
// dao/Addresses.h): Slot_<n>, AutoSave_<n>, QuickSave_<n>.
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace dao::core
{

struct SaveMeta
{
    std::wstring area;      // where it was saved, "Castle Cousland"
    std::wstring character; // the character's name, "Aedan"
    std::wstring name;      // the save's own, empty for an autosave or a quicksave
    std::wstring story;     // the journal's story so far
    std::int32_t secondsPlayed = 0;
    std::int32_t level = 0;
    // As the scripts number them (their CLASS_*, GENDER_*, RACE_* and
    // BACKGROUND_* constants): Aedan's at Castle Cousland, a human noble
    // warrior, has class 1, gender 1, race 3, background 5.
    std::int32_t characterClass = 0;
    std::int32_t gender = 0;
    std::int32_t race = 0;
    std::int32_t background = 0;
    // The modules the save was made in, each followed by ';' ("single player;"), and their overrides
    // ("SP;" for each). The save list lists a save only with the first, and the hero import leaves out a
    // hero whose newest save holds the module being played. A new SaveMeta's are a single-player save's, as
    // the engine writes them.
    std::wstring modules = L"single player;";
    std::wstring moduleOverrides = L"SP;";

    // The metadata in `bytes`; nothing if they are not a save's metadata.
    // A field the file lacks is left empty or 0.
    static std::optional<SaveMeta> Parse(std::span<const std::byte> bytes);

    // What Write made: the new file's bytes, and the labels of the changed fields the file has no field for.
    struct Written
    {
        std::vector<std::byte> bytes;
        std::vector<std::uint32_t> unwritten;
    };

    // A save's metadata file with `meta` written over `original`, the file `meta` was parsed from.
    // Only the fields that differ from the original are set; the rest go through untouched, and an unchanged write is
    // byte-identical. Nothing for bytes that are not a save's metadata. The screenshot is not in this file but beside
    // the save.
    static std::optional<Written> Write(std::span<const std::byte> original, const SaveMeta &meta);

    // The bytes of a metadata file with the two fields the save list lists a save by (16809 and 16810) set and
    // the rest empty or 0, to give Parse and then Write as `original`.
    // A caller sets every field the list shows: Write it with the area, the character, the time played, the level
    // and the rest. Whether the game's list shows such a file has not been seen in play.
    static std::vector<std::byte> Blank();

    static constexpr std::uint32_t kStory = 16511;
    static constexpr std::uint32_t kArea = 16800;
    static constexpr std::uint32_t kTimePlayed = 16801;
    static constexpr std::uint32_t kLevel = 16802;
    static constexpr std::uint32_t kClass = 16803;
    static constexpr std::uint32_t kGender = 16804;
    static constexpr std::uint32_t kRace = 16805;
    static constexpr std::uint32_t kBackground = 16806;
    static constexpr std::uint32_t kCharacter = 16807;
    static constexpr std::uint32_t kName = 16808;
    static constexpr std::uint32_t kModules = 16809;
    static constexpr std::uint32_t kModuleOverrides = 16810;
};

// A slot's kind as the engine numbers it (0 a manual save, 1 an autosave,
// 2 the quicksave; dao/Saves.h, Kind) and its number, from its folder's
// name; nothing for a folder of another name.
struct SlotFolderName
{
    std::int32_t kind = 0;
    std::int32_t number = 0;

    static std::optional<SlotFolderName> Parse(std::wstring_view folder);
};

} // namespace dao::core
