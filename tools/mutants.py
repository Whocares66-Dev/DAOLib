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
built again. A worker builds the debug preset's dao_tests and nothing else,
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
TARGET = "dao_tests"
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
CONVERSATION = "src/dao/core/Conversation.cpp"
TALKTABLE = "src/dao/core/TalkTable.cpp"
SAVEMETA = "src/dao/core/SaveMeta.cpp"
GFX = "src/dao/core/GfxMovie.cpp"
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

# Left out, after the first run (2026-10-03), as changing nothing observable
# or guarding nothing real: a branch inside a function counted again in the
# run around it (each fix-up is worked out from the original offset and
# written to the same place, so twice is once); a `with` block around the
# place, and a forward branch landing exactly on it (no movie patched has
# either, and the second is a choice, not a promise); a null pointer on a
# field path (the next read lands in the first 64 KiB, never mapped); and an
# engine string's capacity bound (at worst an extra line in a crash log).
MUTANTS: list[Mutant] = [
    # The GFF4 reader and the files read with it: saves, conversations, talk tables,
    # and the crash log's parse of whatever buffer a stack word points at. Damaged
    # bytes must never be read past or misread.
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
        "if (field == nullptr || field->type != static_cast<std::uint16_t>(type) || (field->flags & (kList | kStruct)) != 0)",
        "if (field == nullptr || field->type < static_cast<std::uint16_t>(type) || (field->flags & (kList | kStruct)) != 0)",
    ),
    Mutant(
        "gff4-list-not-scalar: a list field is not read as a number",
        GFF4,
        "if (field == nullptr || field->type != static_cast<std::uint16_t>(type) || (field->flags & (kList | kStruct)) != 0)",
        "if (field == nullptr || field->type != static_cast<std::uint16_t>(type) || (field->flags & kStruct) != 0)",
    ),
    Mutant(
        "gff4-reference-followed: a reference field's value is read where its offset points",
        GFF4,
        "        at = file_->DataOffset() + *offset;\n    }\n    return at;",
        "        at = *offset;\n    }\n    return at;",
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
        "if (index >= lines.size())",
        "if (index > lines.size())",
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
        "out.insert(out.begin() + static_cast<std::ptrdiff_t>(inner), tags.begin(), tags.end());",
        "out.insert(out.begin() + static_cast<std::ptrdiff_t>(inner + 2), tags.begin(), tags.end());",
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
        "Write(out, tag.longLength, static_cast<std::uint32_t>(tag.length + tags.size()), 4);",
        "Write(out, tag.longLength, static_cast<std::uint32_t>(tag.length), 4);",
    ),
    Mutant(
        "gfx-unpack-short-file: a file shorter than its header says is no movie",
        GFX,
        "if (file.size() < length)",
        "if (file.size() + 1 < length)",
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
        "mod == 1 ? 1 :",
        "mod == 1 ? 4 :",
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
        "if (capacity > WStringRep::kInlineCapacity)",
        "if (capacity >= WStringRep::kInlineCapacity)",
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
        "dx * dx + dy * dy + dz * dz",
        "dx * dx + dy * dy + 0 * dz",
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
        " ||\n        text[kActionWord.size()] != L' ')",
        ")",
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
