"""Break src/dao/core on purpose, one way at a time, and see the tests fail.

A green suite can pass while testing nothing: breaking the code on purpose
shows what is never checked (the rule DAOTurnBased took from
SkyrimFollowerTactics, and this tool's shape is that repo's tools/mutants.py).
Each mutant below is one promise of the core broken by a small edit: a bound
off by one, a field misread, a size left unfixed around inserted code. The
tool applies it, builds the tests, runs them, and puts the file back. A
mutant is caught if a test fails (or the run hangs past the timeout), and
survives if every test passes. A survivor is a promise no test holds.

Only the core: nothing else can be unit-tested, there being no headless
game (CLAUDE.md). The user, 2026-10-03: "make sure tests and mutations earn
their keep", so the mutants are the promises whose failure would cost in
play (a misread save, a movie patched wrong, a crash log cut short), not
every operator of every line; and a survivor gets a test only where the
promise matters, else the mutant goes as changing nothing observable.

The mutants run side by side (the user, 2026-10-03: "running tests/ mutants
in parallel where possible"). Each worker has a copy of what the build needs
under build\\mutants\\wN, with a build of its own, and breaks that copy: the
working tree is read and never written, so it can be edited while they run,
and a run that is stopped leaves nothing broken. The copies are kept between
runs and brought up to the tree file by file, so that only what changed is
built again. A worker builds the debug preset's dao_core_tests and nothing else,
with the tree's own vcpkg packages, and runs it; everything runs below normal
priority, as tools\\build.ps1 builds.

Run from a shell where tools\\build.ps1 has imported the developer
environment, after one `.\\tools\\build.ps1 -Preset debug`:

    python tools\\mutants.py               every mutant
    python tools\\mutants.py gff4 movie    those whose name contains "gff4" or "movie"
    python tools\\mutants.py -j 4          four at a time (by default one a core)

Exits non-zero if any mutant survives or fails to compile.
"""

from __future__ import annotations

import argparse
import os
import queue
import subprocess
import sys
import threading
import time
from dataclasses import dataclass
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
WORK = REPO / "build" / "mutants"
PRESET = "debug"
# What a worker's copy holds: the debug preset configures from these.
FILES = (
    "CMakeLists.txt",
    "CMakePresets.json",
    "vcpkg.json",
    "vcpkg-configuration.json",
    "ruff.toml",
    ".clang-format",
    ".clang-tidy",
)
TREES = (
    "cmake",
    "src",
    "tests",
    "tools",
    "examples",
    "dev",
    "extern/DAOAddressLibrary",
)
# Not copied from the trees: version control, builds, the game's files.
SKIP = {".git", "build", "game", "toolset", "__pycache__", ".ruff_cache"}
# The debug preset's packages (spdlog, Detours, Catch2), installed once by the
# tree's own build.
PACKAGES = REPO / "build" / PRESET / "vcpkg_installed"
TARGET = "dao_core_tests"
# The tests take a second or two; a run past this has hung (a debug check's
# dialog did, before tests/crt_reports.cpp sent them to stderr).
TIMEOUT_S = 120
# Below normal, as tools\\build.ps1 runs the build: the machine stays usable.
LOW = getattr(subprocess, "BELOW_NORMAL_PRIORITY_CLASS", 0)


@dataclass(frozen=True)
class Mutant:
    name: str
    file: str
    old: str
    new: str


GFF4 = "src/dao/core/Gff4.cpp"
GRID = "src/dao/core/Grid.cpp"
LEVEL = "src/dao/core/Level.cpp"
WALKPATCH = "src/dao/core/WalkPatch.cpp"
COLLISION = "src/dao/core/Collision.cpp"
COLLISION_H = "src/dao/core/Collision.h"
RAYCAST = "src/dao/core/RayCast.cpp"
AREA = "src/dao/core/Area.cpp"
PLOT = "src/dao/core/Plot.cpp"
CUTSCENE = "src/dao/core/Cutscene.cpp"
WIND = "src/dao/core/Wind.cpp"
STAGE = "src/dao/core/Stage.cpp"
LDF = "src/dao/core/Ldf.cpp"
NCS = "src/dao/core/Ncs.cpp"
ANB = "src/dao/core/Anb.cpp"
CHARACTER = "src/dao/core/Character.cpp"
HEADPRESET = "src/dao/core/HeadPreset.cpp"
MORPHCONFIG = "src/dao/core/MorphConfig.cpp"
CLIP = "src/dao/core/Clip.cpp"
EFFECT = "src/dao/core/Effect.cpp"
FEV = "src/dao/core/Fev.cpp"
MODEL = "src/dao/core/Model.cpp"
MORPH = "src/dao/core/Morph.cpp"
CONVERSATION = "src/dao/core/Conversation.cpp"
TALKTABLE = "src/dao/core/TalkTable.cpp"
SAVEMETA = "src/dao/core/SaveMeta.cpp"
GFX = "src/dao/core/GfxMovie.cpp"
OPTIONS = "src/dao/core/OptionsMovie.cpp"
LOGNAMES = "src/dao/core/LogNames.cpp"
INFLATE = "src/dao/core/Inflate.cpp"
RANGE = "src/dao/core/Range.h"
RELOCATE = "src/dao/core/Relocate.h"
CALLSITE = "src/dao/core/CallSite.cpp"
RTTI = "src/dao/core/Rtti.cpp"
FUNCTIONS = "src/dao/core/FunctionTable.cpp"
STACKTEXT = "src/dao/core/StackText.cpp"
PAGES = "src/dao/core/Pages.h"
MSVC8 = "src/dao/core/Msvc8.h"
VMSTACK = "src/dao/core/VmStack.h"
SCRIPTVALUE = "src/dao/core/ScriptValue.cpp"
HELD = "src/dao/core/HeldEvents.h"
OWED = "src/dao/core/OwedTime.h"
FIELDPATH = "src/dao/core/FieldPath.h"
PATH = "src/dao/core/Path.h"
ANGLE = "src/dao/core/Angle.h"
INPUT = "src/dao/core/InputCode.h"
MAO = "src/dao/core/MaterialObject.cpp"
XMLTAGS = "src/dao/core/XmlTags.h"
PICTURE = "src/dao/core/Picture.cpp"
ERF = "src/dao/core/Erf.cpp"
DDS = "src/dao/core/Dds.cpp"
GFF3 = "src/dao/core/Gff3.cpp"
GDAFILE = "src/dao/core/GdaFile.cpp"
M2DA = "src/dao/core/M2da.cpp"
ADDINS = "src/dao/core/AddIns.cpp"
INSTALL = "src/dao/core/Install.cpp"
TALKTABLES = "src/dao/core/TalkTables.cpp"
ARCHIVECACHE = "src/dao/core/ArchiveCache.cpp"
ADDINORDER = "src/dao/core/AddInOrder.h"
FSB = "src/dao/core/Fsb.cpp"
SAVEGAME = "src/dao/core/Savegame.cpp"
SAVEGAME_H = "src/dao/core/Savegame.h"
SAVEGAME_WRITE = "src/dao/core/SavegameWrite.cpp"
CTAB = "src/dao/core/Ctab.cpp"
FACEFX = "src/dao/core/FaceFx.cpp"

# Left out, after the first run (2026-10-03), as changing nothing observable
# or guarding nothing real: a branch inside a function counted again in the
# run around it (each fix-up is worked out from the original offset and
# written to the same place, so twice is once); a `with` block around the
# place, and a forward branch landing exactly on it (no movie patched has
# either, and the second is a choice, not a promise); a null pointer on a
# field path (the next read lands in the first 64 KiB, never mapped); and an
# engine string's capacity bound (at worst an extra line in a crash log).
MUTANTS: list[Mutant] = [
    # The talk tables the game searches, in its order (#106).
    Mutant(
        "talktables-gender-front: setting a gender moves its tables ahead, reversed",
        TALKTABLES,
        "        if (tables[i].female == female)",
        "        if (tables[i].female != female)",
    ),
    Mutant(
        "talktables-en-us-fallback: a name with no table in the language takes en-us's",
        TALKTABLES,
        '{FileName(name, language, female), FileName(name, "en-us", female)}',
        "{FileName(name, language, female)}",
    ),
    Mutant(
        "talktables-campaign-own: the loaded campaign's own table comes last",
        TALKTABLES,
        "        Load(index, CampaignFileName(campaign), tables);",
        "        (void)CampaignFileName(campaign);",
    ),
    # The order made public for a merged table's files (#106): copies of any names sorted by
    # their sources, and the engine source a copy is read from.
    Mutant(
        "install-sortcopies: copies of different resources are sorted, not left as given",
        INSTALL,
        "{\n    Sort(*this, copies);\n}\n\nvoid Index::SortFiles",
        "{\n    (void)copies;\n}\n\nvoid Index::SortFiles",
    ),
    Mutant(
        "install-engine-source: a loose file's source is its folder, not the file",
        INSTALL,
        "+ KeyOf(*this, copy).name;",
        "+ Lower(Where(copy));",
    ),
    Mutant(
        "install-engine-source-base: the player's override and the install's are two sources",
        INSTALL,
        'BaseName(sources_[copy.source].base) + ":"',
        'std::string("game:")',
    ),
    # The plugins folder (#143): the one the game was started with, and only plugins the
    # loader loads.
    Mutant(
        "install-plugins-folder: the plugins folder asked for is read in place of the game's",
        INSTALL,
        "options.plugins.empty() ? game",
        "true ? game",
    ),
    Mutant(
        "install-plugin-dll: a plugin's override is a source only beside its DLL",
        INSTALL,
        'fs::is_regular_file(folder / (folder.filename().wstring() + L".dll"), error) &&',
        "true &&",
    ),
    # The archives' directories kept between builds (#144).
    Mutant(
        "install-cache-stale: an archive whose size changed is read again, not taken from the cache",
        INSTALL,
        "found->second.size == read.size && ",
        "",
    ),
    Mutant(
        "install-cache-time: an archive whose last-write time changed is read again",
        INSTALL,
        " && found->second.written == read.written",
        "",
    ),
    Mutant(
        "archive-cache-version: a cache of another version is no cache",
        ARCHIVECACHE,
        "*version != kVersion",
        "false",
    ),
    # The readers moved in from Eluvian's ghilana (#106): GFF3, a GDA table,
    # the M2DA merge, AddIns.xml, an install's sources and copies.
    Mutant(
        "gff3-type-strict: a number field of another type gives nothing",
        GFF3,
        "if (field == nullptr || field->type != type || !IsInline(type))",
        "if (field == nullptr || !IsInline(type))",
    ),
    Mutant(
        "gff3-sections-inside: a file whose list indices run past its end is refused",
        GFF3,
        "!Inside(size, h.indices, h.indicesSize) || !Inside(size, h.lists, h.listsSize))",
        "!Inside(size, h.indices, h.indicesSize))",
    ),
    Mutant(
        "gff3-child-bounded: a struct field naming no struct gives nothing",
        GFF3,
        "field->data >= file_->Sections().structCount)",
        "false)",
    ),
    Mutant(
        "gff3-field-indices: a struct of several fields finds them through the field indices",
        GFF3,
        "        if (count > 1)",
        "        if (count > 99)",
    ),
    Mutant(
        "gda-cell-by-column: each column's cell is its own field",
        GDAFILE,
        "kFirstCell + static_cast<std::uint32_t>(i)",
        "kFirstCell",
    ),
    Mutant(
        "gda-type: only a G2DA is read as a table",
        GDAFILE,
        'if (!file || file->Type() != "G2DA")',
        "if (!file)",
    ),
    Mutant(
        "m2da-merged-first: a file is an M2DA's before an old table's",
        M2DA,
        "if (!found.empty())",
        "if (false)",
    ),
    Mutant(
        "m2da-alike-across-parts: rows alike are folded only across files",
        M2DA,
        "return rows[v].part != rows[r].part && rows[v].cells == rows[r].cells;",
        "return rows[v].cells == rows[r].cells;",
    ),
    Mutant(
        "m2da-conflict-across-parts: a file's own two rows of an ID are no conflict",
        M2DA,
        "if (versions.size() < 2 || partsOf.size() < 2)",
        "if (versions.size() < 2)",
    ),
    # FMOD's audio (#191): its banks and projects are resources, at the audio folders'
    # priorities, another language's voice not the game's.
    Mutant(
        "install-audio: FMOD's banks and projects are resources",
        INSTALL,
        'if (name == "manifest.xml")',
        'if (name == "manifest.xml" || extension == "fsb" || extension == "fev")',
    ),
    Mutant(
        "install-campaign-voice-language: the campaign's voice of another language is not the game's",
        INSTALL,
        "return {50006, path.size() > 3 && path[2] == options.language ? Unused::No : Unused::Language};",
        "return {50006};",
    ),
    Mutant(
        "install-addin-sound: an add-in's sound is at its base less one",
        INSTALL,
        "return {base - 1};",
        "return {base};",
    ),
    Mutant(
        "install-override-apart: the core's override folder is a source of its own",
        INSTALL,
        "if (!skip.empty() && it->is_directory(type) && Lower(Utf8(it->path())) == skipped)",
        "if (false)",
    ),
    # The game's order (#106, step 4; dev/FILES.md).
    Mutant(
        "install-order-lowest-first: of the copies the game has, the lowest priority first",
        INSTALL,
        "return copies[a].priority < copies[b].priority;",
        "return copies[a].priority > copies[b].priority;",
    ),
    Mutant(
        "install-order-loose-first: a loose file before an archive of the same priority",
        INSTALL,
        "return keys[a].kind < keys[b].kind;",
        "return keys[a].kind > keys[b].kind;",
    ),
    Mutant(
        "install-order-later-name: of two archives in one folder, the later name first",
        INSTALL,
        "return keys[a].name > keys[b].name;",
        "return keys[a].name < keys[b].name;",
    ),
    Mutant(
        "install-order-unused-after: the copies the game has come before those it has not",
        INSTALL,
        "return hasA;",
        "return hasB;",
    ),
    Mutant(
        "install-addin-game-folder: an add-in in the game's folder is 10000 behind one in Documents",
        INSTALL,
        "(inGame ? kInGameFolder : 0)",
        "0",
    ),
    Mutant(
        "install-addin-override: an add-in's override comes six before its data",
        INSTALL,
        "return {base - 6};",
        "return {base};",
    ),
    Mutant(
        "install-campaign-loaded: a campaign's files are the game's only while it is loaded",
        INSTALL,
        "if (found.unused == Unused::No && UidOf(source) != campaign_)",
        "if (false)",
    ),
    Mutant(
        "install-talk-table-language: a talk table of another language is not the game's",
        INSTALL,
        "return isCode && code != language;",
        "return isCode && language.empty();",
    ),
    Mutant(
        "m2da-first-taken: of an ID's rows the game keeps the first",
        M2DA,
        "Row &first = rows[ofId.front()];",
        "Row &first = rows[ofId.back()];",
    ),
    Mutant(
        "m2da-id-first-column: a part whose first column is not ID gives no row",
        M2DA,
        "const bool gives = id && *id == 0;",
        "const bool gives = id.has_value();",
    ),
    Mutant(
        "m2da-every-worksheet: a file is part of every worksheet it starts with",
        M2DA,
        "found.push_back(i);\n        }\n    }\n    if (!found.empty())",
        "found.push_back(i);\n            break;\n        }\n    }\n    if (!found.empty())",
    ),
    Mutant(
        "m2da-single: a single table is its own file alone",
        M2DA,
        "return worksheet.Single() ? lower == worksheet.prefix : lower.starts_with(worksheet.prefix);",
        "return lower.starts_with(worksheet.prefix);",
    ),
    Mutant(
        "install-files-by-name: a source lists its own files by name",
        INSTALL,
        "return !names.empty() && names[a] < names[b];",
        "return !names.empty() && names[a] > names[b];",
    ),
    Mutant(
        "addinorder-from-end: the game takes AddIns.xml's items from the last",
        ADDINORDER,
        "for (std::size_t i = items.size(); i-- > 0;)",
        "for (std::size_t i = 0; i < items.size(); ++i)",
    ),
    Mutant(
        "addinorder-highest-first: the highest Priority is taken first",
        ADDINORDER,
        "return items[a].priority.value_or(0) > items[b].priority.value_or(0);",
        "return items[a].priority.value_or(0) < items[b].priority.value_or(0);",
    ),
    Mutant(
        "addinorder-median: the pivot is MSVC 2005's median of three",
        ADDINORDER,
        "if (less_(a_[last], a_[mid]))",
        "if (less_(a_[mid], a_[last]))",
    ),
    # The archives (#106): version 2.2's directory, and a file read by name.
    Mutant(
        "erf-v22-packed-first: version 2.2's entry has the packed size before the size",
        ERF,
        "entry.size = ReadAt<std::uint32_t>(start, numbers + 8).value_or(0);",
        "entry.size = ReadAt<std::uint32_t>(start, numbers + 4).value_or(0);",
    ),
    Mutant(
        "erf-v22-flags: version 2.2's flags are read from the header",
        ERF,
        "header.flags = ReadAt<std::uint32_t>(start, kFlags).value_or(0);",
        "header.flags = 0;",
    ),
    Mutant(
        "erf-enciphered-unread: an enciphered archive's bytes are never given as a file's",
        ERF,
        "if (!Readable() || std::uint64_t",
        "if (std::uint64_t",
    ),
    Mutant(
        "erf-first-of-a-name: of two files of one name, the archive's first is found",
        ERF,
        "std::ranges::stable_sort(archive.byName_",
        "std::ranges::sort(archive.byName_",
    ),
    Mutant(
        "erf-size-bounded: a file past the archive's end is refused before it is allocated",
        ERF,
        "if (!Readable() || std::uint64_t{entry.offset} + entry.size > size_)",
        "if (!Readable())",
    ),
    # The GFF4 reader and the files read with it: saves, conversations, talk tables,
    # and the crash log's parse of whatever buffer a stack word points at. Damaged
    # bytes must never be read past or misread.
    Mutant(
        "gff4-tlk-text-none: a talk-table string's text word of 0 is no text, not an offset to the top struct",
        GFF4,
        "    if (file_->U32(*slot + 4) == std::optional<std::uint32_t>(0))",
        "    if (file_->U32(*slot + 4) == std::optional<std::uint32_t>(0xFFFFFFFE))",
    ),
    Mutant(
        "gff4-string-count-bounded: a damaged string count is refused, not allocated",
        GFF4,
        "if (!count || *count > (bytes_.size() - std::min(bytes_.size(), p + 4)) / 2)",
        "if (!count)",
    ),
    Mutant(
        "gff4-list-count-bounded: a damaged list count makes no runaway list",
        GFF4,
        "if (!count || *count > file_->Bytes().size())",
        "if (!count)",
    ),
    Mutant(
        "gff4-child-template-bounded: a struct field naming no template gives nothing",
        GFF4,
        "field->type >= file_->Templates().size())",
        "field->type > file_->Templates().size())",
    ),
    Mutant(
        "gff4-generic-template-bounded: a generic list's element naming no template is skipped",
        GFF4,
        "if ((flags & kStruct) == 0 || type >= templates.size())",
        "if ((flags & kStruct) == 0 || type > templates.size())",
    ),
    Mutant(
        "gff4-typed-template-bounded: a list of a template the file lacks gives nothing",
        GFF4,
        "if ((field->flags & kStruct) == 0 || field->type >= templates.size())",
        "if ((field->flags & kStruct) == 0 || field->type > templates.size())",
    ),
    Mutant(
        "gff4-flags-high-half: a field's flags are the high half of its word",
        GFF4,
        "static_cast<std::uint16_t>(*typeAndFlags >> 16), *fieldOffset});",
        "static_cast<std::uint16_t>(*typeAndFlags >> 15), *fieldOffset});",
    ),
    Mutant(
        "gff4-scalar-type-checked: a field of another type is not read as this one",
        GFF4,
        "    if (field->type != static_cast<std::uint16_t>(type))\n",
        "    if (field->type < static_cast<std::uint16_t>(type))\n",
    ),
    Mutant(
        "gff4-list-not-scalar: a list field is not read as a number",
        GFF4,
        "    if (field == nullptr || (field->flags & (kList | kStruct)) != 0)\n    {\n        return std::nullopt;\n    }\n    // A generic field",
        "    if (field == nullptr || (field->flags & kStruct) != 0)\n    {\n        return std::nullopt;\n    }\n    // A generic field",
    ),
    Mutant(
        "gff4-reference-followed: a reference field's value is read where its offset points",
        GFF4,
        "        at = file_->DataOffset() + *offset;\n    }\n    return at;",
        "        at = *offset;\n    }\n    return at;",
    ),
    Mutant(
        "gff4-generic-reference-followed: a generic field's value is read where its offset points",
        GFF4,
        "        element.at = file.DataOffset() + *offset;",
        "        element.at = *offset;",
    ),
    Mutant(
        "grid-kind-bits: a cell's kind is its low four bits, not the counters above them",
        GRID,
        "static_cast<std::uint8_t>(cells[at] & kKindBits)",
        "static_cast<std::uint8_t>(cells[at])",
    ),
    Mutant(
        "grid-second-place: a second-level entrance is in sixteenths of its block",
        GRID,
        "return {left + ((cut.blockCells * (place >> 4)) / 16), top + ((cut.blockCells * (place & 0x0F)) / 16)};",
        "return {left + (place >> 4), top + (place & 0x0F)};",
    ),
    Mutant(
        "grid-direction: an edge's direction 1 is the block to the right",
        GRID,
        "    {0, -1},\n    {1, 0},\n    {0, 1},",
        "    {0, -1},\n    {0, 1},\n    {1, 0},",
    ),
    # A room's trees (#182): each names its model by its own field.
    Mutant(
        "level-tree-model: a room's tree's model is its controller id, not another field",
        LEVEL,
        ".tree = node.Get<std::int32_t>(kTreeModel).value_or(0),",
        ".tree = node.Get<std::int32_t>(kTreeScale).value_or(0),",
    ),
    # A room's connections (#186): their own list, not the visibility list beside it.
    Mutant(
        "level-room-connects: a room's connections are EnvRoomConnectivityList, not the rooms it sees",
        LEVEL,
        "room.connects = root->Numbers(kRoomConnects);",
        "room.connects = root->Numbers(kRoomSees);",
    ),
    # A placeable's walk-grid patch stamped as 0x00A46CC0 does (#176): over a cell under 8 only, and turned from +y
    # to the facing, clockwise for +x.
    Mutant(
        "walkpatch-below-8: a stamp leaves a cell of 8 as it is",
        WALKPATCH,
        "if (grid.Kind(cell.row, cell.column) < kBlocked)",
        "if (grid.Kind(cell.row, cell.column) <= kBlocked)",
    ),
    Mutant(
        "walkpatch-turn: facing +x turns the patch a quarter clockwise",
        WALKPATCH,
        ".sin = -facing[0] / length",
        ".sin = facing[0] / length",
    ),
    Mutant(
        "clip-order: the largest part goes where the key's place says, the rest in order",
        CLIP,
        "{{0, 1, 2, 3}, {1, 0, 2, 3}, {1, 2, 0, 3}, {1, 2, 3, 0}}",
        "{{0, 1, 2, 3}, {0, 1, 2, 3}, {1, 2, 0, 3}, {1, 2, 3, 0}}",
    ),
    Mutant(
        "clip-64-split: a 64-bit rotation's middle part is the low word's top ten and the high word's low eleven",
        CLIP,
        "((low >> 22) | ((high & 0x7FFU) << 10))",
        "((low >> 22) | ((high & 0x7FFU) << 11))",
    ),
    Mutant(
        "clip-48-place: a 48-bit rotation's place is the first number's low bit, then the second's",
        CLIP,
        "((first & 1U) << 1U) | (second & 1U)",
        "((second & 1U) << 1U) | (first & 1U)",
    ),
    Mutant(
        "area-unturned: an object with no WOrientation is unturned, w 1",
        AREA,
        'Number(in, "WOrientation", 1.0F)',
        'Number(in, "WOrientation", 0.0F)',
    ),
    Mutant(
        "area-room-none: a placeable with no RoomID1 is in no room, -1, as the engine constructs it",
        AREA,
        'Number<std::int32_t>(in, "RoomID1", -1)',
        'Number<std::int32_t>(in, "RoomID1", 0)',
    ),
    Mutant(
        "area-variable-float: a variable whose Value is a Float is a float, not dropped",
        AREA,
        "            variable.value = *real;\n",
        "",
    ),
    Mutant(
        "character-clothing-alone: Clothing draws the chest alone, no gloves or boots",
        CHARACTER,
        "(chestBase == kClothing && slot != Slot::Chest) ||",
        "(chestBase == kClothing && slot == Slot::Boots) ||",
    ),
    Mutant(
        "character-no-chest-offset: no chest counts as a WeaponOffset of 0.02",
        CHARACTER,
        "                    : kNoChestOffset;",
        "                    : 0.0F;",
    ),
    Mutant(
        "character-sheathed-left: the off hand sheathes to WearL, not WearR",
        CHARACTER,
        'right ? "WearR" : "WearL"',
        '"WearR"',
    ),
    Mutant(
        "dds-cube-six-chains: a cube's chain is the longest whose six copies the file holds",
        DDS,
        "if ((chain + bytes) * kFaces > room)",
        "if ((chain + bytes) * (kFaces - 1) > room)",
    ),
    Mutant(
        "model-export-name: a node's export is its xprt child's name",
        "src/dao/core/Model.cpp",
        '.name = child.String(kExportName).value_or(L""),',
        '.name = child.String(kMmhName).value_or(L""),',
    ),
    Mutant(
        "anb-no-replacement: a placeable's lookup follows no replacement",
        ANB,
        "        if (!replacements)\n",
        "        if (!replacements && animation < 0)\n",
    ),
    Mutant(
        "anb-replacement-followed: an animation no tree has is played as its Replacement",
        ANB,
        "        id = *replacement;\n",
        "",
    ),
    Mutant(
        "character-head-row-by-part: a head part's row is its PartList element's, by the element's struct id",
        CHARACTER,
        "creature.headRows[part] = static_cast<std::uint8_t>",
        "creature.headRows[0] = static_cast<std::uint8_t>",
    ),
    Mutant(
        "character-head-worksheet: a head part's worksheet is apr_base's column of that part",
        CHARACTER,
        'head.hairModel = partOf(2, "Hair_Worksheet");',
        'head.hairModel = partOf(2, "Head_Worksheet");',
    ),
    Mutant(
        "character-h-has-a-head: an H creature (a darkspawn) has a head of parts, as a P one does",
        CHARACTER,
        "if (character.parts || kind == L'h')",
        "if (character.parts)",
    ),
    Mutant(
        "character-h-no-helmet: only a P creature draws a helmet",
        CHARACTER,
        "HeadOf(context, creature, itemOf, character.prefix, character.parts)",
        "HeadOf(context, creature, itemOf, character.prefix, true)",
    ),
    Mutant(
        "character-tint-table: SkinTint is a row of skin_tints, not of another tint table",
        CHARACTER,
        '.skin = file(kSkinTints, creature.skinTint, "TintFile"),',
        '.skin = file(kHairTints, creature.skinTint, "TintFile"),',
    ),
    Mutant(
        "ncs-const-string-length: a CONST string is its two bytes, its count and its bytes",
        NCS,
        "i.length = 4 + *count;",
        "i.length = 2 + *count;",
    ),
    Mutant(
        "ncs-destruct-signed: DESTRUCT's offset of the part to keep is signed",
        NCS,
        "i.second = Signed(*keep, 2);",
        "i.second = static_cast<std::int32_t>(*keep);",
    ),
    Mutant(
        "ldf-vector-default-whole: a vector default's commas split no parameter",
        LDF,
        "else if (text[i] == separator && !quoted && depth == 0)",
        "else if (text[i] == separator && !quoted)",
    ),
    Mutant(
        "gff4-generic-value-typed: a generic field's value reads only as its element's own type",
        GFF4,
        "if (!element || element->type != static_cast<std::uint16_t>(type) || (element->flags & (kList | kStruct)) != 0)",
        "if (!element || (element->flags & (kList | kStruct)) != 0)",
    ),
    Mutant(
        "plot-defined-from-128: a flag of 128 and up is defined, by its number alone",
        PLOT.replace("Plot.cpp", "Plot.h"),
        "inline constexpr std::int32_t kFirstDefined = 128;",
        "inline constexpr std::int32_t kFirstDefined = 256;",
    ),
    Mutant(
        "plot-stored-word: a stored flag's bit is in the word of its number over 32",
        PLOT,
        "stored[static_cast<std::size_t>(id / kBitsAWord)]",
        "stored[0]",
    ),
    Mutant(
        "cutscene-end-script: the end script is CutsceneEndScript's, not the blend tree's",
        CUTSCENE,
        "constexpr std::uint32_t kEndScript = 5001;",
        "constexpr std::uint32_t kEndScript = 5007;",
    ),
    Mutant(
        "cutscene-transition-controls: a transition's second control is its own field",
        CUTSCENE,
        ".control2 = VertexOf(transition.Child(kTransitionControl2))",
        ".control2 = VertexOf(transition.Child(kTransitionControl1))",
    ),
    Mutant(
        "cutscene-step-holds: a step transition holds the first vertex's value, not the next's",
        CUTSCENE,
        "    case TransitionType::Step:\n        return from.value;",
        "    case TransitionType::Step:\n        return to.value;",
    ),
    Mutant(
        "cutscene-gad-extended: a clip that has ended with ExtendGad leaves its last key",
        CUTSCENE,
        "if (ended && action.extendGad)",
        "if (ended && !action.extendGad)",
    ),
    Mutant(
        "cutscene-gad-live-turned: a live clip's travel is turned by the clips ended before it",
        CUTSCENE,
        "local = Add(local, Rotate(extended, travel.position));",
        "local = Add(local, travel.position);",
    ),
    Mutant(
        "cutscene-headtrack-speed: a head-tracking action's speed is its own field, not the clip's speed",
        CUTSCENE,
        "constexpr std::uint32_t kHeadtrackSpeed = 5621;",
        "constexpr std::uint32_t kHeadtrackSpeed = 5402;",
    ),
    Mutant(
        "cutscene-linear-fraction: a linear transition's fraction is of the segment's own length",
        CUTSCENE,
        "((time - from.time) / (to.time - from.time))",
        "((time - from.time) / to.time)",
    ),
    Mutant(
        "gff4-null-string-empty: a string slot of 0xFFFFFFFF is an empty string, not a failure",
        GFF4,
        "        return std::wstring();\n    }\n    const std::size_t p = data_ + *offset;",
        "        return std::nullopt;\n    }\n    const std::size_t p = data_ + *offset;",
    ),
    Mutant(
        "gff4-string-nul-trimmed: a string's closing NULs are dropped",
        GFF4,
        "while (!text.empty() && text.back() == L'\\0')",
        "while (!text.empty() && text.back() == L'\\1')",
    ),
    Mutant(
        "gff4-typed-list-stride: a list's structs in place are a template's size apart",
        GFF4,
        "const std::size_t size = reference ? 4 : templates[field->type].size;",
        "const std::size_t size = reference ? 4 : std::size_t{4};",
    ),
    Mutant(
        "conversation-index-bounded: a line past the list gives nothing",
        CONVERSATION,
        "if (index >= lines_.size())",
        "if (index > lines_.size())",
    ),
    Mutant(
        "conversation-ambient-field: a line is ambient by its own field, not by skip's",
        CONVERSATION,
        "out.ambient = line.Get<std::uint8_t>(kLineAmbient)",
        "out.ambient = line.Get<std::uint8_t>(kLineSkip)",
    ),
    Mutant(
        "conversation-switch-value: a line's switch is read from its value, not its tag",
        CONVERSATION,
        "entry.Get<std::int32_t>(kSwitchValue)",
        "entry.Get<std::int32_t>(kSwitchTag)",
    ),
    Mutant(
        "conversation-stage-at-location: a line's stage is at the speaker by its own field, not by noVoInGame's",
        CONVERSATION,
        "line.Get<std::uint8_t>(kLineStageAtCurrentLocation)",
        "line.Get<std::uint8_t>(kLineNoVoInGame)",
    ),
    Mutant(
        "talktable-version: a Huffman-coded V0.5 table is refused",
        TALKTABLE,
        'file->TypeVersion() != "V0.2"',
        'file->TypeVersion() == "V0.9"',
    ),
    Mutant(
        "savemeta-level-field: the level is read from its own field",
        SAVEMETA,
        "meta.level = root->Get<std::int32_t>(kLevel).value_or(0);",
        "meta.level = root->Get<std::int32_t>(kClass).value_or(0);",
    ),
    Mutant(
        "savemeta-write-level: the level is written to its own field",
        SAVEMETA,
        "    number(kLevel, meta.level);",
        "    number(kLevel, meta.level + 1);",
    ),
    Mutant(
        "savemeta-write-unchanged-string: a string that reads the same is not appended again",
        SAVEMETA,
        '    if (root.String(label).value_or(L"") == text)\n    {\n        return true;\n    }',
        "    if (false)\n    {\n        return true;\n    }",
    ),
    Mutant(
        "savegame-event-payload-removed: a script event made plain is written afresh, not kept with its payload",
        SAVEGAME_WRITE,
        "(!e.script && old->Child(L::kEventScript))",
        "(false)",
    ),
    Mutant(
        "savegame-event-plain-fallback: a plain event of a file with no EBAS takes the ESCR template",
        SAVEGAME_WRITE,
        'return index || label != "EBAS" ? index : IndexOf("ESCR");',
        "return index;",
    ),
    # A movie patched in memory: every size around inserted code grows, every
    # branch across it is fixed, placements land in the right sprite and frame;
    # and inflate, which unpacks the game's movies.
    Mutant(
        "gfx-v1-function-around-anchor: a DefineFunction enclosing the place grows",
        GFX,
        "else if (action.code == kDefineFunction || action.code == kDefineFunction2)",
        "else if (action.code == kDefineFunction2)",
    ),
    Mutant(
        "gfx-function2-parameters: a DefineFunction2's parameters are read with their registers",
        GFX,
        "            at += 1;\n        }\n        bytes.Text(at, action.next);",
        "            at += 0;\n        }\n        bytes.Text(at, action.next);",
    ),
    Mutant(
        "gfx-push-word-constant: a pool string pushed by a 16-bit index is found",
        GFX,
        "value = {Pushed::Kind::Text, pooled(bytes.U16(at)), 0};",
        "value = {Pushed::Kind::Text, pooled(bytes.U8(at)), 0};",
    ),
    Mutant(
        "gfx-place-first-frame: placements go before the sprite's first ShowFrame",
        GFX,
        "out.insert(out.begin() + static_cast<std::ptrdiff_t>(*showFrame), tags.begin(), tags.end());",
        "out.insert(out.begin() + static_cast<std::ptrdiff_t>(*showFrame + 2), tags.begin(), tags.end());",
    ),
    Mutant(
        "gfx-place-tag-long-form: a placement of 63 bytes or more is written in the long form",
        GFX,
        "if (body.size() < 0x3F)",
        "if (body.size() <= 0x3F)",
    ),
    Mutant(
        "gfx-colour-transform: a placement's colour transform is stepped over to its name",
        GFX,
        "(adds ? 4 : 0) + (multiplies ? 4 : 0)",
        "(adds ? 4 : 0) + (multiplies ? 3 : 0)",
    ),
    Mutant(
        "gfx-function-size-fits: a function that would pass 64K is refused",
        GFX,
        "if (bytes.U16(at) + added > 0xFFFF)",
        "if (bytes.U16(at) + added > 0x1FFFF)",
    ),
    Mutant(
        "gfx-doinitaction-sprite-id: a DoInitAction's sprite id is not read as actions",
        GFX,
        "inside = InActions(bytes, tag.body + 2, tagEnd, {}, anchor, place);",
        "inside = InActions(bytes, tag.body, tagEnd, {}, anchor, place);",
    ),
    Mutant(
        "gfx-long-tag-body: a long tag's body starts after its 32-bit length",
        GFX,
        "tag.body = at + 6;",
        "tag.body = at + 2;",
    ),
    Mutant(
        "gfx-fade-alpha-term: the alpha is the fourth of a colour transform's multiply terms",
        GFX,
        "if (multiplies && i == 3)",
        "if (multiplies && i == 2)",
    ),
    Mutant(
        "gfx-fade-past-end: a frame is past the fade's end only when its alpha would be read beyond it",
        GFX,
        "into > last / times ? last",
        "into >= last / times ? last",
    ),
    Mutant(
        "gfx-fade-life-ends: what a depth has after the fade's thing is taken away is not the fade's",
        GFX,
        "break; // its life is over",
        "continue; // its life is over",
    ),
    Mutant(
        "gfx-fade-out-only: an alpha that ever rises is no fade out, and its terms might not fit",
        GFX,
        "std::ranges::is_sorted(shown, std::ranges::greater())",
        "true",
    ),
    Mutant(
        "gfx-remove-sprite-size: a placement taken out of a sprite shrinks the sprite's tag by its size",
        GFX,
        "static_cast<std::uint32_t>(clip->length) - size",
        "static_cast<std::uint32_t>(clip->length)",
    ),
    Mutant(
        "gfx-move-fits: a place that does not fit the bits the old one has is refused",
        GFX,
        "> placed.place->first.count",
        "> placed.place->first.count + 1",
    ),
    Mutant(
        "options-video-once: a Video tab laid out by another copy of the library is left as it is",
        OPTIONS,
        "if (fbfx->sprite == vsync->sprite)",
        "if (fbfx->sprite == 0)",
    ),
    Mutant(
        "options-video-picture: the gamma's picture goes up with its slider",
        OPTIONS,
        "one.x, one.y - gammaUp}",
        "one.x, one.y + gammaUp}",
    ),
    Mutant(
        "gfx-fields-after-move: a field after the place is written where it moved to",
        GFX,
        "return at >= place.at ? at + added : at;",
        "return at >= place.at ? at : at;",
    ),
    Mutant(
        "gfx-forward-branch-grows: a branch over the place jumps further by the code put in",
        GFX,
        "branch.forward ? offset + static_cast<std::int32_t>(added)",
        "branch.forward ? offset - static_cast<std::int32_t>(added)",
    ),
    Mutant(
        "gfx-pool-into-functions: a function's body reads the pool before it",
        GFX,
        "const Search inside = InActions(bytes, action.next, next, *pool, anchor, place);",
        "const Search inside = InActions(bytes, action.next, next, outer, anchor, place);",
    ),
    Mutant(
        "gfx-place-header-length: placing grows the header's length",
        GFX,
        "Write(out, 4, bytes.U32(4) + static_cast<std::uint32_t>(tags.size()), 4);",
        "Write(out, 4, bytes.U32(4), 4);",
    ),
    Mutant(
        "gfx-place-sprite-length: placing grows the sprite's length",
        GFX,
        "Write(out, tag->longLength, static_cast<std::uint32_t>(tag->length + tags.size()), 4);",
        "Write(out, tag->longLength, static_cast<std::uint32_t>(tag->length), 4);",
    ),
    Mutant(
        "gfx-unpack-short-file: a file shorter than its header says is no movie",
        GFX,
        "if (file.size() < length)",
        "if (file.size() + 1 < length)",
    ),
    # The options screen's dropdown (DAOLib #66): an open list is drawn at the
    # dropdown's depth, and a dropdown stands in two rows.
    Mutant(
        "options-dropdown-deepest: a dropdown is deeper than what is under it, so its open list covers that",
        OPTIONS,
        "return placed.character != picker;",
        "return placed.character == picker;",
    ),
    Mutant(
        "options-dropdown-two-rows: a dropdown's second row is taken for the next plugin's section",
        OPTIONS,
        "else if (instance.character == symbols.picker)",
        "else if (instance.character == 0)",
    ),
    Mutant(
        "options-slider-pitch: a section's sliders stand 41 apart, the game's own pitch, and take the rows they reach into",
        OPTIONS,
        "constexpr std::int32_t kSliderPitch = 41;",
        "constexpr std::int32_t kSliderPitch = 35;",
    ),
    Mutant(
        "options-slider-rows: the row a section's last slider reaches into is taken for the next plugin's section",
        OPTIONS,
        "            height = kSliderPitch;",
        "            height = kControlHeight;",
    ),
    Mutant(
        "options-video-last-row: the Video tab's right column ends at the Game tab's last row, above the screen's buttons",
        OPTIONS,
        '{"", 448, 38, 37, 4}',
        '{"", 483, 38, 37, 4}',
    ),
    Mutant(
        "options-online-last-row: the Online tab's columns end at the Game tab's last row, above the screen's buttons",
        OPTIONS,
        '{"OptAutoDLOldEntitlements_mc", 381,',
        '{"OptAutoDLOldEntitlements_mc", 416,',
    ),
    Mutant(
        "options-button-rows: the row under a button's own, which it reaches into, is taken for the next plugin's section",
        OPTIONS,
        "            height = kButtonPitch;",
        "            height = kControlHeight;",
    ),
    Mutant(
        "options-button-symbol: a section with a button is left out of a movie that has not the game's button",
        OPTIONS,
        "(section.buttons.empty() || symbols.button) &&",
        "(section.buttons.empty() || true) &&",
    ),
    Mutant(
        "dds-pictures-by-bytes: a file's pictures are counted by what its bytes hold, not by its header's count",
        DDS,
        "    while (each(at, width, height) && (header.more & kNotFlat) == 0 && std::max(width, height) > 1)",
        "    while (each(at, width, height) && (header.more & kNotFlat) == 0 && std::max(width, height) > 2)",
    ),
    Mutant(
        "dds-first-whole: a first picture the bytes stop short of is not counted",
        DDS,
        "        if (file.size() - kPixels >= first)",
        "        if (file.size() - kPixels + first >= first)",
    ),
    Mutant(
        "install-package-order: a campaign's later package is behind its earlier",
        INSTALL,
        "kPackageBase + (kPackageStep * i)",
        "kPackageBase - (kPackageStep * i)",
    ),
    Mutant(
        "install-package-campaign: a package the loaded campaign does not load is not the game's",
        INSTALL,
        "            if (slot == loaded.end())",
        "            if (false)",
    ),
    Mutant(
        "install-package-patch: a package's patch is ahead of its data",
        INSTALL,
        "        return {base + 2};",
        "        return {base + 7};",
    ),
    Mutant(
        "lognames-other-process: a report does not quote the log of a game running beside its own",
        LOGNAMES,
        "return !ownsLogs && last == std::to_string(pid);",
        "return !ownsLogs || last == std::to_string(pid);",
    ),
    Mutant(
        "options-tabs-apart: a section left out of one tab does not keep another tab's out",
        OPTIONS,
        "        if (section.tab != tab)\n        {\n            continue;",
        "        if (section.tab != tab)\n        {\n            return;",
    ),
    Mutant(
        "inflate-blocks-after-first: a stream of several blocks is read to its last",
        INFLATE,
        "        if (last)\n        {\n            break;",
        "        if (last || type <= 2)\n        {\n            break;",
    ),
    Mutant(
        "inflate-longest-match: a match of 258 bytes is 258",
        INFLATE,
        "131, 163, 195, 227, 258}",
        "131, 163, 195, 227, 257}",
    ),
    Mutant(
        "inflate-far-distance: the farthest distances are where the table says",
        INFLATE,
        "8193, 12289, 16385, 24577}",
        "8193, 12289, 16385, 24576}",
    ),
    Mutant(
        "inflate-distance-codes-bound: more than 30 distance codes is refused",
        INFLATE,
        "literals > 286 || distances > 30",
        "literals > 286 || distances > 32",
    ),
    Mutant(
        "inflate-match-before-start: a match reaching before the output is refused",
        INFLATE,
        "distance > out.size() ||",
        "distance > out.size() + 1 ||",
    ),
    Mutant(
        "inflate-stored-complement: a stored block's length must match its complement",
        INFLATE,
        "(length ^ 0xFFFFU) != inverse",
        "(length ^ 0xFFFFU) < inverse",
    ),
    Mutant(
        "inflate-repeat-previous: code 16 repeats the previous length 3 to 6 times",
        INFLATE,
        "times = 3 + bits.Take(2);",
        "times = 4 + bits.Take(2);",
    ),
    # Addresses and what the crash log reads: an image's range, relocation between
    # bases, a call before a return address, a class name, a function's extent,
    # the stack's engine strings and the pages read; the engine's std::map walk,
    # the script VM's stack and a command's signature; the held events and owed
    # time on a plugin's clocks; and the small arithmetic the plugins lean on.
    Mutant(
        "range-wraps: an address above the image's end is outside it",
        RANGE,
        "address >= begin && address <= end && bytes",
        "address >= begin && bytes",
    ),
    Mutant(
        "range-end: a range may end exactly at the image's end",
        RANGE,
        "bytes <= end - address",
        "bytes < end - address",
    ),
    Mutant(
        "relocate: an address moves with the base the exe got",
        RELOCATE,
        "return preferredVa - kPreferredBase + actualBase;",
        "return preferredVa - actualBase + kPreferredBase;",
    ),
    Mutant(
        "callsite-reg: only FF /2 is a call",
        CALLSITE,
        "if (Reg(modrm) != 2)",
        "if (Reg(modrm) < 2)",
    ),
    Mutant(
        "callsite-disp8: call [reg+disp8] is three bytes",
        CALLSITE,
        "displacement = 1;",
        "displacement = 4;",
    ),
    Mutant(
        "callsite-sib-disp32: call [index*scale+disp32] is seven bytes",
        CALLSITE,
        "(hasSib && (sib & 7) == 5)",
        "(hasSib && (sib & 7) == 6)",
    ),
    Mutant(
        "callsite-rel32: a call rel32 starts five bytes back",
        CALLSITE,
        "if (at(5) == 0xE8)",
        "if (at(4) == 0xE8)",
    ),
    Mutant(
        "callsite-longest: a call may be seven bytes",
        CALLSITE,
        "for (std::size_t n = 2; n <= 7; ++n)",
        "for (std::size_t n = 2; n < 7; ++n)",
    ),
    Mutant(
        "rtti-order: namespaces come outermost first",
        RTTI,
        "for (auto it = parts.rbegin(); it != parts.rend(); ++it)",
        "for (auto it = parts.begin(); it != parts.end(); ++it)",
    ),
    Mutant(
        "rtti-template: a template comes back decorated, not split",
        RTTI,
        'decorated.substr(kPrefix.size() + 1).starts_with("?$")',
        'decorated.substr(kPrefix.size()).starts_with("?$")',
    ),
    Mutant(
        "functions-end: a function's last byte is its own",
        FUNCTIONS,
        "if (address > it->end)",
        "if (address >= it->end)",
    ),
    Mutant(
        "functions-sorted: lines in any order find their function",
        FUNCTIONS,
        "std::ranges::sort(table.entries_, {}, &Entry::entry);",
        "(void)table.entries_.size();",
    ),
    Mutant(
        "stacktext-gff-eight: eight bytes are enough to know a GFF4",
        STACKTEXT,
        "if (head.size() < kV40.size())",
        "if (head.size() <= kV40.size())",
    ),
    Mutant(
        "stacktext-full-inline: an inline string may use all seven characters",
        STACKTEXT,
        "size < 3 || size > capacity",
        "size < 3 || size >= capacity",
    ),
    Mutant(
        "stacktext-length: an inline string's length is where its NUL is",
        STACKTEXT,
        "if (!text || text->size() != size)",
        "if (!text)",
    ),
    Mutant(
        "stacktext-pointer: only a capacity past 7 means a heap pointer",
        STACKTEXT,
        "if (capacity > kWStringInlineCapacity)",
        "if (capacity >= kWStringInlineCapacity)",
    ),
    Mutant(
        "stacktext-least: text of exactly the least length is taken",
        STACKTEXT,
        "return text.size() >= least ?",
        "return text.size() > least ?",
    ),
    Mutant(
        "pages-guard: a guard page is never read",
        PAGES,
        "(protect & kPageGuard) == 0 && ",
        "",
    ),
    Mutant(
        "pages-commit: a reserved page is not read",
        PAGES,
        "return state == kMemCommit &&",
        "return state != 0 &&",
    ),
    Mutant(
        "msvc8-inline: seven characters are borrowed inline",
        MSVC8,
        "if (text.size() <= kInlineCapacity)",
        "if (text.size() < kInlineCapacity)",
    ),
    Mutant(
        "msvc8-list-count: the list walk stops at its count",
        MSVC8,
        "i < *size && node",
        "i <= *size && node",
    ),
    Mutant(
        "msvc8-list-head: the list walk stops back at its head",
        MSVC8,
        " && *node != *head;",
        ";",
    ),
    Mutant(
        "msvc8-count: the map walk stops at its count",
        MSVC8,
        "i < size && node != nullptr",
        "i <= size && node != nullptr",
    ),
    Mutant(
        "msvc8-leftmost: after a right step, the leftmost below comes next",
        MSVC8,
        "            while (node->left->isNil == 0)\n            {\n                node = node->left;",
        "            if (node->left->isNil == 0)\n            {\n                node = node->left;",
    ),
    Mutant(
        "msvc8-climb: climbing stops at the first parent reached from the left",
        MSVC8,
        "node == parent->right",
        "node == parent->left",
    ),
    Mutant(
        "vmstack-top: the n-th value counts down from the top",
        VMSTACK,
        "return depth - 1 - n;",
        "return n;",
    ),
    Mutant(
        "vmstack-bottom: past the bottom is nothing",
        VMSTACK,
        "n < 0 || n >= depth",
        "n < 0 || n > depth",
    ),
    Mutant(
        "vmstack-type: a value of another type is not taken",
        VMSTACK,
        "if (!i || tags[*i] != tag)",
        "if (!i || (tags[*i] != tag && tag < 0))",
    ),
    Mutant(
        "scriptvalue-too-many: one argument too many is refused",
        SCRIPTVALUE,
        "if (given.size() > command.params.size())",
        "if (given.size() > command.params.size() + 1)",
    ),
    Mutant(
        "scriptvalue-int-float: an int is taken for a float, not the reverse",
        SCRIPTVALUE,
        "value.type == ScriptType::Int && param.type == ScriptType::Float",
        "value.type == ScriptType::Float && param.type == ScriptType::Int",
    ),
    Mutant(
        "held-boundary: an event due exactly at the clock comes out",
        HELD,
        "events_.begin()->first > target_",
        "events_.begin()->first >= target_",
    ),
    Mutant(
        "hold-rule: a kind left out stays out of a hold by delay",
        HELD,
        """        if (leftOut.contains(type))
        {
            return false;
        }
        if (held.contains(type))
        {
            return true;
        }""",
        """        if (held.contains(type))
        {
            return true;
        }
        if (leftOut.contains(type) && false)
        {
            return false;
        }""",
    ),
    Mutant(
        "hold-rule: a delay at the bound itself is held",
        HELD,
        "return from > 0 && delay >= from;",
        "return from > 0 && delay > from;",
    ),
    Mutant(
        "hold-rule: no delay rule at 0",
        HELD,
        "return from > 0 && delay >= from;",
        "return from >= 0 && delay >= from;",
    ),
    Mutant(
        "held-release: a released event keeps only the time it has left",
        HELD,
        "left.emplace_back(due - now, std::move(event));",
        "left.emplace_back(due - 0 * now, std::move(event));",
    ),
    Mutant(
        "held-own: the shared advance passes by a target with its own clock",
        HELD,
        "if (!own_.contains(target))",
        "if (own_.contains(target))",
    ),
    Mutant(
        "held-day: a whole day's seconds are a day",
        HELD,
        "while (seconds >= dayLength)",
        "while (seconds > dayLength)",
    ),
    Mutant(
        "held-rearm: an event re-armed while delivering counts from its due time",
        HELD,
        "return delivering_ ? now_ : target_;",
        "return target_;",
    ),
    Mutant(
        "owed-floor: only whole milliseconds are given",
        OWED,
        "std::floor(owed)",
        "std::ceil(owed)",
    ),
    Mutant(
        "owed-own: an owner on its own clock is owed no shared time",
        OWED,
        "(own ? 0 : shared_ - seen->second.shared)",
        "(shared_ - seen->second.shared)",
    ),
    Mutant(
        "path-z: a path's length counts height",
        PATH,
        "(dx * dx) + (dy * dy) + (dz * dz)",
        "(dx * dx) + (dy * dy) + (0 * dz)",
    ),
    Mutant(
        "angle-wrap: a turn past pi goes the other way round",
        ANGLE,
        "d -= kTurn;",
        "d += kTurn;",
    ),
    Mutant(
        "input-modifiers: only shift, control and alt go into a key's word",
        INPUT,
        "| (modifiers & (kShift | kControl | kAlt)) |",
        "| modifiers |",
    ),
    Mutant(
        "input-length: a name of 48 is allowed",
        INPUT,
        "name.size() > 48",
        "name.size() >= 48",
    ),
    Mutant(
        "input-space: the action word is followed by a space",
        INPUT,
        " || text[kActionWord.size()] != L' ')",
        ")",
    ),
    Mutant(
        "mao-end: the last close, where elements go before",
        MAO,
        "text.rfind(kClose)",
        "text.find(kClose)",
    ),
    Mutant(
        "xml-whole: an attribute is found by its whole name, not inside another's",
        XMLTAGS,
        "const bool starts = at > 0 && Space(tag[at - 1]);",
        "const bool starts = true;",
    ),
    Mutant(
        "mao-spaces: the spaces before the comment's end are not the path's",
        MAO,
        "while (!path.empty() && (path.back() == ' ' || path.back() == '\\t'))",
        "while (false)",
    ),
    Mutant(
        "picture-edge: a middle on the span's end is in it, as Pillow has it",
        PICTURE,
        "(at > -0.5 && at <= 0.5)",
        "(at > -0.5 && at < 0.5)",
    ),
    Mutant(
        "preset-target-difference: a target moves a vertex by its difference from the base, weighed",
        HEADPRESET,
        "places[i].at(k) += weight * (to->at(k) - from->at(k));",
        "places[i].at(k) += weight * to->at(k);",
    ),
    Mutant(
        "preset-seam-nearest: of a seam's vertices sharing a coordinate, the nearest is matched",
        HEADPRESET,
        "            if (distance < bestDistance)",
        "            if (distance > bestDistance)",
    ),
    Mutant(
        "preset-value-component: a material value's index is its vector's (index / 4) and element",
        HEADPRESET,
        ".at(at / 4).at(at % 4) = value.value;",
        ".at(0).at(at % 4) = value.value;",
    ),
    Mutant(
        "morphconfig-hair-cut: a hair is offered once per digit of its cut, with that digit",
        MORPHCONFIG,
        "choices.push_back({.name = hair.name, .cut = digit - '0'});",
        "choices.push_back({.name = hair.name, .cut = 0});",
    ),
    Mutant(
        "m2da-taken-row: a table's lookup answers the row the game takes, not a dropped one of its ID",
        M2DA,
        "return row.taken && row.id == id; });",
        "return row.id == id; });",
    ),
    Mutant(
        "effect-volume-stride: a mesh spawn volume's points are 16 bytes apart, the fourth float junk",
        EFFECT,
        "        for (std::size_t at = 0; at + 16 <= bytes.size(); at += 16)\n",
        "        for (std::size_t at = 0; at + 12 <= bytes.size(); at += 12)\n",
    ),
    Mutant(
        "gff4-vector3-list: a list of Vector3 is read, each in 16 bytes",
        GFF4,
        "    case Type::Vector3:\n    case Type::Vector4:\n",
        "    case Type::Vector4:\n",
    ),
    Mutant(
        "effect-drives-parameter: a channel's parameter is its export's tag variable type",
        EFFECT,
        ".parameter = static_cast<Parameter>(exported.variableType)};",
        ".parameter = static_cast<Parameter>(exported.controllerType)};",
    ),
    Mutant(
        "model-crust-hook: a crst node keeps its hook id, the place on the creature it is",
        MODEL,
        "            node.crustHook = static_cast<std::uint8_t>(*hook);\n",
        "",
    ),
    Mutant(
        "effect-model-nearest: the model whose size is nearer the owner's is taken, as the engine does",
        EFFECT,
        "sizes[i].size - ownerSize < ownerSize - sizes[i - 1].size",
        "sizes[i].size - ownerSize > ownerSize - sizes[i - 1].size",
    ),
    Mutant(
        "clip-event-hand: an equip event's hand is events.gda's RightHand, not its EquipEvent",
        CLIP,
        '.rightHand = flag("RightHand"),',
        '.rightHand = flag("EquipEvent"),',
    ),
    Mutant(
        "clip-event-time: an event's time is a double, in seconds",
        CLIP,
        ".time = event.Get<double>(kEventTime).value_or(0.0),",
        ".time = event.Get<float>(kEventTime).value_or(0.0F),",
    ),
    # A level's static physics and a placeable's shapes (#189): the masks a ray tests, built as 0x00589DB0 builds
    # them, each actor's room by its own record, and the .opf's poses by rows.
    Mutant(
        "collision-sight-mask: the eye-to-eye ray meets static geometry as well as placeables",
        COLLISION_H,
        "kSightMask = kPlaceables | kStaticGeometry;",
        "kSightMask = kPlaceables;",
    ),
    Mutant(
        "collision-not-fadeable: 0x2000 goes only on static geometry that is not fadeable",
        COLLISION,
        "if (!flag(kFadeable) && (mask & kStaticGeometry) != 0)",
        "if (!flag(kFadeable))",
    ),
    Mutant(
        "collision-any-bit: any flag sets 0x80000000",
        COLLISION,
        "        mask |= kAny;",
        "        mask |= 0;",
    ),
    Mutant(
        "collision-room-of-record: an actor's room is its own record's, not the first's",
        COLLISION,
        "actor.room = physics.actorRecords[static_cast<std::size_t>(actor.record)].room;",
        "actor.room = physics.actorRecords[0].room;",
    ),
    Mutant(
        "collision-shape-records: a shape's record follows its actor's in userProperties",
        COLLISION,
        "actor.shapes[s].record = indices[s + 1];",
        "actor.shapes[s].record = indices[s];",
    ),
    Mutant(
        "collision-pose-rows: a pose's numbers are its turn's rows",
        COLLISION,
        "(pose.rotation[(row * 3) + 1] * point[1])",
        "(pose.rotation[row + 3] * point[1])",
    ),
    Mutant(
        "collision-records-end: the records end the bytes exactly",
        COLLISION,
        "return at == bytes.size();",
        "return at <= bytes.size();",
    ),
    Mutant(
        "collision-shape-node: a .phy shape is under the nearest named node",
        COLLISION,
        "Walk(child, own ? *own : name, type, out);",
        "Walk(child, name, type, out);",
    ),
    # The overlap query (#233): a capsule through a triangle, a box as a solid and its edges, the tree's bounds by
    # the radius, a point in a pose's own space by the turn's transpose, a level's mesh shapes, the snap's mask, and a
    # creature's capsule about its centre.
    Mutant(
        "overlap-pierce: a capsule passing through a triangle touches it, its ends far",
        RAYCAST,
        "if (const auto met = Meets(triangle, p0, direction, 0, 1))",
        "if (const auto met = Meets(triangle, p0, direction, 2, 3))",
    ),
    Mutant(
        "overlap-box-through: a capsule through a box, its ends outside it, overlaps it",
        RAYCAST,
        "if (const auto entry = SegmentEntersBox(p0, p1, low, high))",
        "if (const auto entry = SegmentEntersBox(p0, p0, high, high))",
    ),
    Mutant(
        "overlap-box-edges: a capsule beside a box's edge, its ends far, overlaps it",
        RAYCAST,
        "if ((i & bit) == 0)",
        "if ((i & bit) == 8)",
    ),
    Mutant(
        "overlap-tree-bounds: the tree's nodes are met by the capsule's bounds grown by its radius",
        RAYCAST,
        "std::min({capsule.p0[axis], capsule.p1[axis], end0, end1}) - capsule.radius;",
        "std::min({capsule.p0[axis], capsule.p1[axis], end0, end1});",
    ),
    Mutant(
        "overlap-unapply: a point in a pose's space is turned by the turn's transpose",
        COLLISION,
        "(pose.rotation[3 + column] * v[1])",
        "(pose.rotation[(column * 3) + 1] * v[1])",
    ),
    Mutant(
        "overlap-level-mesh: a level's mesh shapes are overlapped by their triangles",
        COLLISION,
        "MeshOverlaps(capsule, physics.meshes[static_cast<std::size_t>(shape.mesh)], pose)",
        "false",
    ),
    Mutant(
        "overlap-placement-mask: the snap's mask meets terrain walls",
        COLLISION_H,
        "kPlacementMask = 0x02011134;",
        "kPlacementMask = 0x02001134;",
    ),
    Mutant(
        "overlap-creature-capsule: a creature's capsule runs half its height each way from its centre",
        COLLISION,
        "centre - (height * 0.5F)",
        "centre - height",
    ),
    # The capsule sweep (#238): a start overlap is t = 0 and says so, the root is bisected toward the capsule's
    # side of the touch, a touch at the move's very end is found, the swept bounds hold the move's end, a box's hit
    # comes back out of its own space, a mesh's motion goes into its own, the pick's mode, the camera's centre, the
    # swept shape's height and a level hit's room.
    Mutant(
        "sweep-start-flag: a capsule already touching a shape is reported as starting inside it",
        RAYCAST,
        "return HitAt(0, true, start, shapeRadius, motion);",
        "return HitAt(0, false, start, shapeRadius, motion);",
    ),
    Mutant(
        "sweep-start-t: a capsule already touching a shape is reported at t = 0",
        RAYCAST,
        "return HitAt(0, true, start, shapeRadius, motion);",
        "return HitAt(1, true, start, shapeRadius, motion);",
    ),
    Mutant(
        "sweep-bisect: the touch is bisected toward the side where the capsule is within the radii",
        RAYCAST,
        "if (at(middle).squared <= most)",
        "if (at(middle).squared >= most)",
    ),
    # Not a mutant: the sweep's check of f(1) after the golden section (a touch exactly at the move's end). In float the
    # section's 60 steps end on 1.0 itself for a minimum at the end, so dropping the check changes no answer; it stays
    # as the guard for a section that stops a step short.
    Mutant(
        "sweep-bounds-end: the swept bounds hold the capsule's end of the move",
        RAYCAST,
        "std::max({capsule.p0[axis], capsule.p1[axis], end0, end1}) + capsule.radius;",
        "std::max({capsule.p0[axis], capsule.p1[axis]}) + capsule.radius;",
    ),
    Mutant(
        "sweep-box-point: a box's hit point comes back out of the box's space",
        COLLISION,
        "return SweepHit{local.t, Apply(pose, local.point), Turn(pose, local.normal), local.startsInside};",
        "return SweepHit{local.t, local.point, Turn(pose, local.normal), local.startsInside};",
    ),
    Mutant(
        "sweep-box-normal: a box's hit normal is turned out of the box's space",
        COLLISION,
        "return SweepHit{local.t, Apply(pose, local.point), Turn(pose, local.normal), local.startsInside};",
        "return SweepHit{local.t, Apply(pose, local.point), local.normal, local.startsInside};",
    ),
    Mutant(
        "sweep-mesh-motion: a mesh's motion is turned into the mesh's space",
        COLLISION,
        "const Point localMotion = Unturn(pose, motion);",
        "const Point localMotion = motion;",
    ),
    Mutant(
        "sweep-level-room: a level's hit carries its actor's room",
        COLLISION,
        "static_cast<std::int32_t>(a), actor.room,",
        "static_cast<std::int32_t>(a), -1,",
    ),
    Mutant(
        "sweep-swept-capsule: the engine's swept capsule is 0.1 m tall",
        COLLISION,
        "from[2] + 0.1F}, radius};",
        "from[2] + 0.2F}, radius};",
    ),
    Mutant(
        "sweep-pick-mode: mode 0 keeps the nearest hit and mode 1 the farthest",
        COLLISION,
        "if (pick == Pick::Nearest ? distance < chosenDistance : distance > chosenDistance)",
        "if (pick == Pick::Farthest ? distance < chosenDistance : distance > chosenDistance)",
    ),
    Mutant(
        "sweep-pick-centre: the camera's centre is the hit's point plus its normal times the radius",
        COLLISION,
        "centre[axis] = chosen->hit.point[axis] + (chosen->hit.normal[axis] * radius);",
        "centre[axis] = chosen->hit.point[axis] - (chosen->hit.normal[axis] * radius);",
    ),
    # A weapon trail's node (#236): its duration is its own field, not its segment's length.
    Mutant(
        "model-trail-duration: a trail's duration is MMHWeapontrailDuration, not its segment's length",
        MODEL,
        ".duration = from.Get<float>(kTrailDuration)",
        ".duration = from.Get<float>(kTrailSegmentLength)",
    ),
    # A morph written (#237): a mesh node's vertex count is its places', which the reader takes the data's length by.
    Mutant(
        "morph-write-count: a written node's MeshStreamVertexCount is the count of its places",
        MORPH,
        "editor.SetNumber(at, kVertexCount, static_cast<std::int64_t>(node.places.size()))",
        "editor.SetNumber(at, kVertexCount, 0)",
    ),
    # A mesh node's cut-away flag (#239), which the area's cutoff fades it by.
    Mutant(
        "model-cut-away: a mesh node's cutAway is its own MMHMeshCutAway",
        MODEL,
        ".cutAway = Number(from, kMeshCutAway).value_or(0) != 0",
        ".cutAway = false",
    ),
    # A room model's cut-away override (#241), which decides an effect's part in the area's cutoff.
    Mutant(
        "level-cut-away-override: a room model's override is its own EnvModelCutAwayOverride",
        LEVEL,
        "placed.cutAwayOverride = model.Get<std::uint8_t>(kModelCutAwayOverride).value_or(0);",
        "placed.cutAwayOverride = 0;",
    ),
    # A room's light visibility list (#243): -1, every room, must stay -1.
    Mutant(
        "level-light-vis: a room's EnvRoomLightVisList is read as signed, its -1 kept",
        LEVEL,
        "room.lightVis.push_back(static_cast<std::int32_t>(lit));",
        "room.lightVis.push_back(static_cast<std::int32_t>(lit & 0x7FFFFFFFU));",
    ),
    # The .arl's binary NXU stream (#197): a pose as three rows of four, a string's NUL, the fields skipped before
    # the ones read, the later SDK's block numbers, a mesh's index width, and a stream that cannot be read whole.
    Mutant(
        "collision-binary-pose-rows: a binary pose is each row of the turn then that row's translation",
        COLLISION,
        "pose.translation[row] = Take<float>();",
        "pose.translation[2 - row] = Take<float>();",
    ),
    Mutant(
        "collision-binary-string-nul: a string's characters are followed by a NUL the length leaves out",
        COLLISION,
        "Skip(std::size_t{length} + 1);",
        "Skip(std::size_t{length});",
    ),
    Mutant(
        "collision-binary-actor-skip: an actor's quaternion, density and flags come before its group",
        COLLISION,
        "c.Skip(16 + 4 + 4); // a quaternion",
        "c.Skip(16 + 4); // a quaternion",
    ),
    Mutant(
        "collision-binary-shape-skip: a shape's quaternion and flags come before its group",
        COLLISION,
        "c.Skip(16 + 4); // a quaternion",
        "c.Skip(16); // a quaternion",
    ),
    Mutant(
        "collision-binary-sdk-281: the later SDK numbers its capsule after its sphere",
        COLLISION,
        ".sphere = 35, .capsule = 36, .mesh = 39, .actor = 42}",
        ".sphere = 35, .capsule = 35, .mesh = 39, .actor = 42}",
    ),
    Mutant(
        "collision-binary-index-width: a mesh with NX_MF_16_BIT_INDICES has 16-bit indices",
        COLLISION,
        "(flags & kMeshFlag16Bit) != 0 ? 2 : 4;",
        "(flags & kMeshFlag16Bit) != 0 ? 4 : 4;",
    ),
    Mutant(
        "collision-binary-mesh-by-name: a mesh shape's meshData names the mesh it uses",
        COLLISION,
        "const auto found = meshIds_.find(c.String());",
        "const auto found = meshIds_.find(std::string{});",
    ),
    Mutant(
        "collision-binary-whole: a stream that fails anywhere, or holds no block, gives no shapes",
        COLLISION,
        "return !top.empty() && !failed_;",
        "return true;",
    ),
    # FMOD's banks (#191): each sample's bytes where they are, its header's own size, its
    # sync points bounded, and IMA ADPCM as FMOD lays it out.
    Mutant(
        "fsb-data-after-padding: the data starts after the samples' padded headers",
        FSB,
        "std::uint64_t at = kHeaderSize + std::uint64_t{h.headersSize};",
        "std::uint64_t at = kHeaderSize + (std::uint64_t{h.samples} * kSampleHeaderSize);",
    ),
    Mutant(
        "fsb-header-size: the next sample's header is past this one's sync points",
        FSB,
        "        place += size;",
        "        place += kSampleHeaderSize;",
    ),
    Mutant(
        "fsb-sample-in-file: a sample whose data runs past the file's end is a damaged bank",
        FSB,
        "if (!sample || at > fileSize || fileSize - at < sample->size)",
        "if (!sample || at > fileSize)",
    ),
    Mutant(
        "fsb-sync-bound: a sync point count past the header is a damaged bank",
        FSB,
        "if (count > (extra.size() - kSyncHeader) / 4)",
        "if (false)",
    ),
    Mutant(
        "fsb-name-whole: a name of all 30 bytes is read whole",
        FSB,
        "std::find(name, name + kNameSize, '\\0')",
        "std::find(name, name + kNameSize - 1, '\\0')",
    ),
    Mutant(
        "ima-header-first: a block's header is its first frame, then the nibbles",
        FSB,
        "const std::size_t n = f - 1;",
        "const std::size_t n = f;",
    ),
    Mutant(
        "ima-low-nibble-first: of a byte's two nibbles the low is played first",
        FSB,
        "(inWord % 2) == 0 ? byte & 0xF : byte >> 4",
        "(inWord % 2) == 0 ? byte >> 4 : byte & 0xF",
    ),
    Mutant(
        "ima-stereo-words: a stereo block's channels take turns by words of 4 bytes",
        FSB,
        "(((word * channels) + c) * 4)",
        "(((c * 8) + word) * 4)",
    ),
    Mutant(
        "ima-clip: a sample clips to 16 bits",
        FSB,
        "sample = std::clamp((nibble & 8) != 0 ? sample - delta : sample + delta, -32768, 32767);",
        "sample = (nibble & 8) != 0 ? sample - delta : sample + delta;",
    ),
    # The saved game's reader (#202): where the engine's reader differs from a plain one.
    Mutant(
        "savegame-list-absent: a list the template lacks is none, not an empty list that clears the creature's own",
        SAVEGAME,
        "    if (Has(s, label))\n    {\n        std::vector<T> v;\n        List(s, label, v);",
        "    if (true)\n    {\n        std::vector<T> v;\n        List(s, label, v);",
    ),
    Mutant(
        "savegame-item-ability: an item's property lists are read only when its ability is present",
        SAVEGAME,
        "    if (Has(s, L::kItemAbilityId))",
        "    if (true)",
    ),
    Mutant(
        "savegame-aoe-flags: an AOE with no flags takes its ability",
        SAVEGAME,
        "        out.flags = out.abilityId;\n",
        "",
    ),
    Mutant(
        "savegame-effect-priority: an effect of duration type 6 with no saved priority has 999",
        SAVEGAME_H,
        "            return 999;",
        "            return 0;",
    ),
    Mutant(
        "savegame-event-script: the event ids 10 and 24 are script events",
        SAVEGAME_H,
        "        case 24:",
        "        case 25:",
    ),
    Mutant(
        "savegame-event-plain: the event id 27 is a plain event",
        SAVEGAME_H,
        "        case 27:",
        "        case 28:",
    ),
    Mutant(
        "savegame-effect-counter: a save with no game state starts the effect counter at 1",
        SAVEGAME_H,
        "effectStartingId = 1;",
        "effectStartingId = 0;",
    ),
    # The saved game's writer (#221): what a save written back must keep of the file, and what it may not share.
    Mutant(
        "savegame-write-pair-source: a list's element is paired with the struct it was read from, not the one at its place",
        SAVEGAME_WRITE,
        "const auto it = values[i].source == kNoSource ? bySource.end() : bySource.find(values[i].source);\n"
        "                at = it == bySource.end() ? old.size() : it->second;",
        "at = i < old.size() ? i : old.size();",
    ),
    Mutant(
        "savegame-write-source-once: a struct of the file is carried over for one element only, a copy is built",
        SAVEGAME_WRITE,
        "if (at < old.size() && !used[at] && labelOf(values[i], &old[at]))",
        "if (at < old.size() && labelOf(values[i], &old[at]))",
    ),
    Mutant(
        "savegame-write-opt-none: an Opt that is none leaves the field as the file has it",
        SAVEGAME_WRITE,
        "if (value && !Set(n, label, *value))",
        "if (!Set(n, label, value.value_or(T{})))",
    ),
    Mutant(
        "savegame-write-item-ability: an item's property lists are written only where they are read, behind its ability",
        SAVEGAME_WRITE,
        "if (Writer::Has(n, L::kItemAbilityId))",
        "if (true)",
    ),
    Mutant(
        "savegame-write-aoe-flags: an AOE's flags are not news where the field is absent and the member is its ability",
        SAVEGAME_WRITE,
        "if (Writer::Has(n, L::kAoeFlags) || v.flags != v.abilityId)",
        "if (true)",
    ),
    Mutant(
        "savegame-write-string-same: a string equal to the file's is not appended again",
        SAVEGAME_WRITE,
        "return (now && *now == value) || ed_.SetString(n.edit, label, value);",
        "static_cast<void>(now);\n        return ed_.SetString(n.edit, label, value);",
    ),
    Mutant(
        "savegame-write-numbers-same: a list of numbers equal to the file's is not written again",
        SAVEGAME_WRITE,
        "n.old->Numbers(label) : std::vector<std::uint32_t>()) == bits)",
        "n.old->Numbers(label) : std::vector<std::uint32_t>()) != bits)",
    ),
    Mutant(
        "savegame-write-vector-junk: a vector list whose points are the file's keeps the junk word of each",
        SAVEGAME_WRITE,
        "const bool same = n.old && SameBits(Decoded(*n.old, label, Type::Vector3), Decoded(flat));",
        "const bool same = false;",
    ),
    Mutant(
        "savegame-write-list-same: a list of the file's size and order is edited in place, not written again",
        SAVEGAME_WRITE,
        "bool same = values.size() == plan.old.size();",
        "bool same = false;",
    ),
    Mutant(
        "savegame-write-variable-kind: a variable of an object is made in the template of an object",
        SAVEGAME_WRITE,
        'case 3:\n        return "SVO1";',
        'case 3:\n        return "SVI1";',
    ),
    Mutant(
        "savegame-write-trigger-kind: a trigger of type 1 is made in the template of a transition",
        SAVEGAME_WRITE,
        'case Trigger::Kind::Transition:\n        return "TRAT";',
        'case Trigger::Kind::Transition:\n        return "TRGR";',
    ),
    Mutant(
        "savegame-write-plain-child: a child struct the file lacks is made only if something is written into it",
        SAVEGAME_WRITE,
        "if (ed_.Size() == end && Slice(blank->Offset(), pristine.size()) == pristine)",
        "if (static_cast<void>(end), static_cast<void>(pristine), false)",
    ),
    Mutant(
        "savegame-write-blank-id: a new struct's object id is -1, the engine's default",
        SAVEGAME_WRITE,
        "ed_.SetNumber(*blank, L::kObjectId, -1);",
        "ed_.SetNumber(*blank, L::kObjectId, 0);",
    ),
    Mutant(
        "savegame-write-width: a number is written only into a field of the member's width",
        SAVEGAME_WRITE,
        "return WidthOf(f->type) == sizeof(T) && ed_.SetNumber(",
        "return WidthOf(f->type) != 0 && ed_.SetNumber(",
    ),
    Mutant(
        "gff4-blank-null: a blank struct's lists, strings and references are null, not 0",
        GFF4,
        "std::memcpy(bytes_.data() + slot, &kNull, 4); // a talk-table string's text word stays 0",
        "static_cast<void>(slot); // a talk-table string's text word stays 0",
    ),
    Mutant(
        "gff4-structs-flag: a generic list's element names its template and says it is a struct",
        GFF4,
        "const std::uint32_t word = static_cast<std::uint32_t>(one.Index()) | (std::uint32_t{kStruct} << 16);",
        "const std::uint32_t word = static_cast<std::uint32_t>(one.Index());",
    ),
    Mutant(
        "fev-sample-index: a wave's sample index is the number the file has, the sample in its bank",
        FEV,
        "entry.sampleIndex = in.U32();",
        "entry.sampleIndex = in.U32() + 1;",
    ),
    Mutant(
        "fev-wave-bank: a wave names its file first and its bank second",
        FEV,
        "entry.file = in.String();\n        entry.bank = in.String();",
        "entry.bank = in.String();\n        entry.file = in.String();",
    ),
    Mutant(
        "fev-directive-trimmed: a directive's fields are trimmed of the spaces some names have",
        FEV,
        "directive.fields.emplace_back(Trim(rest.substr(0, comma)));",
        "directive.fields.emplace_back(rest.substr(0, comma));",
    ),
    Mutant(
        "fev-property-float: a float user property is a float, not an integer's bits",
        FEV,
        "property.real = in.F32();",
        "property.real = static_cast<float>(in.I32());",
    ),
    Mutant(
        "fev-layer-order: a layer has its priority before its controlling parameter",
        FEV,
        "layer.priority = in.Read<std::int16_t>();\n    layer.parameter = in.Read<std::int16_t>();",
        "layer.parameter = in.Read<std::int16_t>();\n    layer.priority = in.Read<std::int16_t>();",
    ),
    Mutant(
        "fev-category-unknown: a category has two unknown words before its children",
        FEV,
        "    category.unknown = in.Array<std::uint32_t, 2>();\n",
        "",
    ),
    Mutant(
        "fev-cut-short: a project cut short anywhere is none",
        FEV,
        "    if (!in.Ok())\n    {\n        return std::nullopt;\n    }\n    project.trailer",
        "    project.trailer",
    ),
    Mutant(
        "fev-reverb-low-frequency: a reverb's low frequency room level is the integer before its reference",
        FEV,
        "r.roomLF = in.I32();",
        "r.roomLF = in.I32() + 1;",
    ),
    Mutant(
        "install-listed-campaign: a listing, and a worksheet, take no file only another campaign has",
        INSTALL,
        "name.ends_with(extension) && !copies.empty() && copies.front().unused == Unused::No)",
        "name.ends_with(extension) && !copies.empty() && copies.front().unused != Unused::Level)",
    ),
    Mutant(
        "facefx-magic-face: an actor with FaceFX's own magic is read",
        FACEFX,
        '&& std::memcmp(bytes_.data(), "FACE", 4) != 0))',
        '&& std::memcmp(bytes_.data(), "BIOE", 4) != 0))',
    ),
    Mutant(
        "ctab-register-index: a constant's first register is its +6, not its count at +8",
        CTAB,
        "const auto index = ReadAt<std::uint16_t>(table, at + 6);",
        "const auto index = ReadAt<std::uint16_t>(table, at + 8);",
    ),
    Mutant(
        "ctab-comment-length: a comment's length is in dwords",
        CTAB,
        "const std::size_t length = (*token >> 16) * sizeof(std::uint32_t);",
        "const std::size_t length = (*token >> 16);",
    ),
    Mutant(
        "ctab-at-last-register: a constant's registers end before index + count",
        CTAB,
        "reg - constant.index < constant.count;",
        "reg - constant.index <= constant.count;",
    ),
    Mutant(
        "ctab-register-set: a register set past the sampler is no table's",
        CTAB,
        "*set > static_cast<std::uint16_t>(RegisterSet::Sampler)",
        "*set > static_cast<std::uint16_t>(RegisterSet::Sampler) + 1",
    ),
    Mutant(
        "wind-strength: a wind's strength is WndStrength, not the radius falloff",
        WIND,
        "constexpr std::uint32_t kStrength = 22003;",
        "constexpr std::uint32_t kStrength = 22005;",
    ),
    Mutant(
        "wind-gust-frequency: the gust frequency is the level's 3730, not its strength",
        WIND,
        "constexpr std::uint32_t kGustFrequency = 3730;",
        "constexpr std::uint32_t kGustFrequency = 3713;",
    ),
    Mutant(
        "wind-gust-duration: the gusts' longest duration is its own field",
        WIND,
        ".gustMaxDuration = One(root, kGustMaxDuration),",
        ".gustMaxDuration = One(root, kGustMinDuration),",
    ),
    Mutant(
        "wind-leaf-rocking: leaf rocking is not the rustling",
        WIND,
        ".treeLeafRocking = Several<4>(root, kTreeLeafRocking),",
        ".treeLeafRocking = Several<4>(root, kTreeLeafRustling),",
    ),
    Mutant(
        "stage-camera-fov: a camera's field of view is StageCameraFOV",
        STAGE,
        "constexpr std::uint32_t kCameraFov = 11003;",
        "constexpr std::uint32_t kCameraFov = 11004;",
    ),
    Mutant(
        "stage-looking-at: a camera looks at its primary place before its secondary",
        STAGE,
        "constexpr std::uint32_t kLookingAtPrimary = 11007;",
        "constexpr std::uint32_t kLookingAtPrimary = 11008;",
    ),
    Mutant(
        "stage-looking-from: a camera's looking-from is its own field",
        STAGE,
        "constexpr std::uint32_t kLookingFrom = 11006;",
        "constexpr std::uint32_t kLookingFrom = 11007;",
    ),
    Mutant(
        "stage-places-in-shot: a camera lists the places in its shot",
        STAGE,
        ".placesInShot = in.Strings(kPlacesInShot),",
        ".placesInShot = {},",
    ),
    Mutant(
        "stage-place-found: a place is found by its tag",
        STAGE,
        "if (place.tag == tag)",
        "if (place.tag != tag)",
    ),
    Mutant(
        "stage-camera-found: a camera is found by its tag",
        STAGE,
        "if (camera.tag == tag)",
        "if (camera.tag != tag)",
    ),
]


def run(
    command: list[str], cwd: Path, timeout: int | None = None
) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        command,
        cwd=cwd,
        capture_output=True,
        text=True,
        timeout=timeout,
        check=False,
        creationflags=LOW,
    )


def sources() -> list[Path]:
    found = [REPO / name for name in FILES if (REPO / name).exists()]
    for tree in TREES:
        found.extend(
            path
            for path in (REPO / tree).rglob("*")
            if path.is_file() and not SKIP.intersection(path.relative_to(REPO).parts)
        )
    return found


def sync(worker: Path) -> None:
    """Brings a worker's copy up to the tree: a file is written only where it
    differs, so what did not change is not built again."""
    wanted: set[Path] = set()
    for source in sources():
        target = worker / source.relative_to(REPO)
        wanted.add(target)
        data = source.read_bytes()
        if not target.exists() or target.read_bytes() != data:
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(data)
    for tree in TREES:
        for stale in (worker / tree).rglob("*"):
            if stale.is_file() and stale not in wanted:
                stale.unlink()


def made_with() -> str:
    """The build program the tree's own debug build was configured with."""
    cache = REPO / "build" / PRESET / "CMakeCache.txt"
    if cache.exists():
        for line in cache.read_text(encoding="utf-8", errors="replace").splitlines():
            if line.startswith("CMAKE_MAKE_PROGRAM:"):
                return line.partition("=")[2].strip()
    return ""


def tests_of(worker: Path) -> Path:
    return worker / "build" / PRESET / "tests" / f"{TARGET}.exe"


def build(worker: Path, jobs: int) -> subprocess.CompletedProcess[str]:
    return run(
        [
            "cmake",
            "--build",
            "--preset",
            PRESET,
            "--target",
            TARGET,
            "--parallel",
            str(jobs),
        ],
        worker,
    )


def prepare(worker: Path, jobs: int) -> str:
    """A worker's copy, configured and built as the tree is. Answers what went
    wrong, or nothing."""
    sync(worker)
    if not (worker / "build" / PRESET / "build.ninja").exists():
        command = [
            "cmake",
            "--preset",
            PRESET,
            f"-DVCPKG_INSTALLED_DIR={PACKAGES.as_posix()}",
            "-DVCPKG_MANIFEST_INSTALL=OFF",
        ]
        # Ninja as the tree's own build found it: it need not be on the path
        # of the shell this runs in.
        if ninja := made_with():
            command.append(f"-DCMAKE_MAKE_PROGRAM={ninja}")
        configured = run(command, worker)
        if configured.returncode != 0:
            return (configured.stdout + configured.stderr)[-2000:]
    built = build(worker, jobs)
    if built.returncode != 0:
        return (built.stdout + built.stderr)[-2000:]
    return ""


def test(mutant: Mutant, worker: Path) -> str:
    """One mutant, in a worker's copy, from the copy as it was made: an edit
    to the tree while mutants run is not taken up half way."""
    path = worker / mutant.file
    original = path.read_text(encoding="utf-8")
    if original.count(mutant.old) != 1:
        return f"not applied: the text is in {mutant.file} {original.count(mutant.old)} times"
    try:
        path.write_text(original.replace(mutant.old, mutant.new), encoding="utf-8")
        # One file changed: one compiler, or a few for a header.
        built = build(worker, 2)
        if built.returncode != 0:
            output = built.stdout + built.stderr
            # A test's static_assert or STATIC_REQUIRE failing is a catch.
            if "C2607" in output or "static assertion failed" in output:
                return "caught (at compile time)"
            first = next(
                (line for line in output.splitlines() if " error " in line), ""
            )
            return "did not compile: " + first.strip()[-160:]
        try:
            result = run([str(tests_of(worker))], worker, timeout=TIMEOUT_S)
        except subprocess.TimeoutExpired:
            return "caught (the tests hung)"
        return "caught" if result.returncode != 0 else "SURVIVED"
    finally:
        path.write_text(original, encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument(
        "names", nargs="*", help="only mutants whose name contains one of these"
    )
    parser.add_argument(
        "-j",
        "--jobs",
        type=int,
        default=os.cpu_count() or 2,
        help="how many mutants at a time (by default one a core)",
    )
    args = parser.parse_args()
    chosen = [
        m for m in MUTANTS if not args.names or any(w in m.name for w in args.names)
    ]
    if not chosen:
        print("no mutant by those names")
        return 1
    if not PACKAGES.exists():
        print(
            f"no {PACKAGES}: build the debug preset once (tools\\build.ps1 -Preset debug)"
        )
        return 1
    # Each mutant's text, once in its file in the tree: a stale mutant is said
    # before any worker is made.
    stale = [
        m.name
        for m in chosen
        if (REPO / m.file).read_text(encoding="utf-8").count(m.old) != 1
    ]
    if stale:
        print("mutants whose text is not once in its file:\n  " + "\n  ".join(stale))
        return 1
    count = max(1, min(args.jobs, len(chosen)))
    workers = [WORK / f"w{n}" for n in range(count)]
    started = time.monotonic()

    # The copies, built side by side: each with its share of the cores.
    share = max(1, (os.cpu_count() or 2) // count)
    problems: dict[Path, str] = {}

    def ready(worker: Path) -> None:
        problems[worker] = prepare(worker, share)

    threads = [threading.Thread(target=ready, args=(w,)) for w in workers]
    for thread in threads:
        thread.start()
    for thread in threads:
        thread.join()
    for worker, problem in problems.items():
        if problem:
            print(f"{worker.name} does not configure or build:\n{problem}")
            return 1
    if run([str(tests_of(workers[0]))], workers[0]).returncode != 0:
        print("the unmutated core does not pass its tests; fix that first")
        return 1
    print(
        f"{count} at a time, in {WORK} ({time.monotonic() - started:.0f} s to make ready)",
        flush=True,
    )

    todo: queue.Queue[Mutant] = queue.Queue()
    for mutant in chosen:
        todo.put(mutant)
    verdicts: dict[str, str] = {}
    said = threading.Lock()

    def work(worker: Path) -> None:
        while True:
            try:
                mutant = todo.get_nowait()
            except queue.Empty:
                return
            start = time.monotonic()
            # Whatever goes wrong is said, and counted as not caught.
            try:
                verdict = test(mutant, worker)
            except Exception as error:  # noqa: BLE001
                verdict = f"failed: {error}"
            with said:
                verdicts[mutant.name] = verdict
                print(
                    f"{verdict:<32} {mutant.name} ({time.monotonic() - start:.0f} s)",
                    flush=True,
                )

    threads = [threading.Thread(target=work, args=(w,)) for w in workers]
    for thread in threads:
        thread.start()
    for thread in threads:
        thread.join()
    bad = [
        name for name, verdict in verdicts.items() if not verdict.startswith("caught")
    ]
    hung = [name for name, verdict in verdicts.items() if "hung" in verdict]
    for name in hung:
        print(f"hung, and counted as caught: {name}")
    for name in sorted(bad):
        print(f"not caught: {name}: {verdicts[name]}")
    print(
        f"{len(chosen) - len(bad)} of {len(chosen)} caught in {time.monotonic() - started:.0f} s"
    )
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
