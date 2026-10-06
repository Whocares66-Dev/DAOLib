"""The scripts' constants, as C++: src/dao/generated/ScriptConstants.h.

A script's numbers mean what its constants say: an event of type 1003 is
EVENT_TYPE_COMMAND_COMPLETE, an effect of type 32 is EFFECT_TYPE_DAMAGE.
The engine's own are declared in script.ldf (dev/SCRIPT_COMMANDS.md), 480
of them; the effect types are the scripts' own, in effect_constants_h.nss
among the toolset's script sources (tools/toolset_scripts.py), and so are
the event types the scripts add to the engine's (events_h.nss: a damage
over time's tick, EVENT_TYPE_DOT_TICK, and the plots' EVENT_TYPE_CUSTOM_
EVENT_*, which a script sends itself by DelayEvent). This turns them all
into

- one typed enum per group of three or more sharing a prefix: EVENT_TYPE_*
  becomes `enum class EventType`, EVENT_TYPE_COMMAND_COMPLETE its member
  CommandComplete; the enum is 32-bit, so a value no constant names still
  fits;
- every constant again under its script name, `dao::script::constants::
  EVENT_TYPE_COMMAND_COMPLETE`, so code carried over from a script or the
  wiki reads the same;
- a Name() per enum, the script name of a value at run time:
  Name(EventType::CommandComplete) is "EVENT_TYPE_COMMAND_COMPLETE", for
  logs and tools that show a number to a person. Where two constants share
  a value, the first one's name.

A creature property's id (PROPERTY_ATTRIBUTE_*, _SIMPLE_*, _DEPLETABLE_*)
is a row of the game's properties table (properties.gda), which the
engine builds each creature's properties from. Some of script.ldf's ids
are stale: the table has another property at that row, and the game none
of the name (PROPERTY_ATTRIBUTE_MOVEMENT_SPEED_MODIFIER, 20, is
Flanking_Angle; found in play by the turn-based mod, 2026-09-30). Each
such constant says so in a comment, from the table.

    python tools/gen_script_constants.py            rewrite the header
    python tools/gen_script_constants.py --check    exit 1 if it is stale;
                                                    0 with a note when a source is missing
"""

import math
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gda  # noqa: E402
import script_commands  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, "..")
OUT = os.path.join(ROOT, "src", "dao", "generated", "ScriptConstants.h")
EFFECTS = os.path.join(
    ROOT, "toolset", "extract", "scripts", "_Game Effects", "effect_constants_h.nss"
)
# The game's data tables by number (TABLE_ABILITIES_TALENTS = 1, ...), what
# dao/Gda.h and the scripts' GetM2DA* commands take; and the damage types
# (DAMAGE_TYPE_*), which EVENT_TYPE_DAMAGED's integer 0 carries. Only those
# two groups are taken from it.
FROM_TABLES = ("TABLE_", "DAMAGE_TYPE_")
TABLES = os.path.join(
    ROOT, "toolset", "extract", "scripts", "_Core Includes", "2da_constants_h.nss"
)
# The scripts' own event types, beside the engine's: only that group is
# taken from it. Where one shares the engine's value (1070), the engine's
# name comes first and Name() gives it.
FROM_EVENTS = ("EVENT_TYPE_",)
EVENTS = os.path.join(
    ROOT, "toolset", "extract", "scripts", "_Core Includes", "events_h.nss"
)

CONSTANT = re.compile(
    r"^\s*(?:const\s+)?(int|float)\s+([A-Z][A-Z0-9_]*)\s*=\s*([^;]+?)\s*;", re.M
)
# Groups whose automatic name would mislead in C++.
RENAMES = {
    "OBJECT_TYPE_": "ObjectTypeFlags",  # the scripts' bitmask, not dao::core::ObjectType
}
MIN_GROUP = 3
# Prefixes that make one enum whatever words follow, where grouping by the
# first two words would split them: TABLE_AI_*, TABLE_ITEMS, ... are all
# tables.
FORCED = {"TABLE_"}
RENAMES["TABLE_"] = "Table"
# Names <Windows.h> defines as macros, which a C++ constant cannot share;
# C++ has true and false.
SKIPPED = {"TRUE", "FALSE"}


# The prefixes whose constants are rows of properties.gda.
PROPERTY_IDS = ("PROPERTY_ATTRIBUTE_", "PROPERTY_SIMPLE_", "PROPERTY_DEPLETABLE_")


def properties():
    """{id: (Stat, ToolsetName)} from the game's properties table, or None."""
    if not os.path.exists(gda.ARCHIVE):
        return None
    columns, rows = gda.read_gda(
        script_commands.erf_entries(gda.ARCHIVE)["properties.gda"]
    )
    at = {h: i for i, (h, _) in enumerate(columns)}
    ident, stat, tool = (at[gda.column_hash(n)] for n in ("ID", "Stat", "ToolsetName"))
    return {r[ident]: (str(r[stat] or ""), str(r[tool] or "")) for r in rows}


def squash(name):
    return name.replace("_", "").lower()


def property_notes(constants, table):
    """{constant: comment} for each property id the table names otherwise."""
    notes = {}
    for name, (kind, value, _) in constants.items():
        prefix = next((p for p in PROPERTY_IDS if name.startswith(p)), None)
        if kind != "int" or prefix is None:
            continue
        ours = squash(name[len(prefix) :])
        stat, tool = table.get(value, ("", ""))
        if not stat:
            notes[name] = f"Stale: properties.gda has no row {value}."
            continue
        # The table's name, or the toolset's for it (Cunning is the toolset's
        # Intelligence), or one the start of the other (AP, AP_BONUS).
        if any(
            t and (t.startswith(ours) or ours.startswith(t))
            for t in (squash(stat), squash(tool))
        ):
            continue
        notes[name] = (
            f"Stale: properties.gda row {value} is {stat}; the game has no such property."
        )
    return notes


def script_ldf():
    erf = os.path.join(script_commands.GAME, "packages", "core", "data", "scripts.erf")
    if os.path.exists(erf):
        return script_commands.erf_entries(erf)["script.ldf"].decode(
            "latin-1"
        ), "script.ldf"
    return None, None


def parse(text, source, out):
    """Adds {name: (type, value text, source)} for each constant, in order."""
    for kind, name, value in CONSTANT.findall(text):
        out.setdefault(name, (kind, value.strip(), source))


def resolve(constants):
    """{name: (type, number)}: literal values, and names of other constants."""
    out = {}
    for name, (kind, value, source) in constants.items():
        v = value.rstrip("fF") if kind == "float" else value
        seen = set()
        while v in constants and v not in seen:
            seen.add(v)
            v = constants[v][1]
        if v == "TRUE":
            v = "1"
        elif v == "FALSE":
            v = "0"
        try:
            number = float(v.rstrip("fF")) if kind == "float" else int(v, 0)
        except ValueError:
            continue
        out[name] = (kind, number, source)
    return out


def common_prefix(names):
    first = names[0].split("_")
    n = len(first)
    for name in names[1:]:
        parts = name.split("_")
        k = 0
        while k < min(n, len(parts) - 1) and parts[k] == first[k]:
            k += 1
        n = k
    return "_".join(first[:n]) + "_" if n else ""


def pascal(words):
    return "".join(w[:1].upper() + w[1:].lower() for w in words.split("_") if w)


def groups(constants):
    """[(enum name, prefix, [(member, value, name)])] for int groups."""
    by_two = {}
    for name, (kind, _, _) in constants.items():
        if kind != "int":
            continue
        forced = next((p for p in FORCED if name.startswith(p)), None)
        if forced:
            by_two.setdefault(forced, []).append(name)
            continue
        parts = name.split("_")
        if len(parts) < 3:
            continue
        by_two.setdefault("_".join(parts[:2]), []).append(name)
    out = []
    for key in sorted(by_two):
        names = by_two[key]
        if len(names) < MIN_GROUP:
            continue
        prefix = key if key in FORCED else common_prefix(names)
        enum = RENAMES.get(prefix) or pascal(prefix)
        members, used = [], set()
        for name in names:
            member = pascal(name[len(prefix) :]) or "Value"
            if member[0].isdigit():
                member = "_" + member
            base, i = member, 2
            while member in used:
                member = f"{base}{i}"
                i += 1
            used.add(member)
            members.append((member, constants[name][1], name))
        out.append((enum, prefix, members))
    return out


def header(constants, sources, notes):
    lines = [
        "// Generated by tools/gen_script_constants.py from",
        f"// {', '.join(sources[:-1])} and {sources[-1]}.",
        "// Do not edit: run the generator again.",
        "//",
        "// The scripts' constants: a typed enum per group, and every constant under",
        "// its script name in dao::script::constants. An enum's values are the",
        "// scripts'; a value no constant names still fits the 32-bit enum.",
        "#pragma once",
        "",
        "#include <cstdint>",
        "#include <string_view>",
        "",
        "namespace dao::script",
        "{",
        "",
    ]
    for enum, prefix, members in groups(constants):
        unsigned = any(v > 0x7FFFFFFF for _, v, _ in members)
        base = "std::uint32_t" if unsigned else "std::int32_t"
        source = constants[members[0][2]][2]
        lines.append(f"// {prefix}* ({source}), {len(members)} values.")
        lines.append(f"enum class {enum} : {base}")
        lines.append("{")
        for member, value, name in members:
            # A note on a line of its own: the formatter would wrap a long
            # trailing one, and the check compares tokens.
            if name in notes:
                lines.append(f"    // {notes[name]}")
            lines.append(f"    {member} = {value if value >= 0 else f'({value})'},")
        lines.append("};")
        lines.append("")
        # Short and fixed, so the formatter never rewraps it.
        lines.append("// The value's script name; empty for a value no constant names.")
        lines.append(f"constexpr std::string_view Name({enum} value)")
        lines.append("{")
        lines.append("    switch (value)")
        lines.append("    {")
        seen = set()
        for member, value, name in members:
            if value in seen:
                continue
            seen.add(value)
            lines.append(f"    case {enum}::{member}:")
            lines.append(f'        return "{name}";')
        lines.append("    default:")
        lines.append("        return {};")
        lines.append("    }")
        lines.append("}")
        lines.append("")
    lines.append("namespace constants")
    lines.append("{")
    lines.append("")
    for name, (kind, value, _) in constants.items():
        if name in SKIPPED:
            continue
        if kind == "float":
            line = f"inline constexpr float {name} = {value!r}f;"
            # The scripts' own value, which clang-tidy would replace with
            # std::numbers' (PI is 3.141592, not pi; DAOLib #33).
            if any(abs(value - c) < 1e-3 for c in (math.pi, math.e)):
                line += " // NOLINT(modernize-use-std-numbers): the scripts' own value"
            lines.append(line)
        elif value > 0x7FFFFFFF:
            lines.append(f"inline constexpr std::uint32_t {name} = {value}u;")
        else:
            if name in notes:
                lines.append(f"// {notes[name]}")
            lines.append(f"inline constexpr std::int32_t {name} = {value};")
    lines += ["", "} // namespace constants", "", "} // namespace dao::script", ""]
    return "\n".join(lines)


def main(argv):
    ldf, name = script_ldf()
    table = properties()
    have_effects = all(os.path.exists(p) for p in (EFFECTS, TABLES, EVENTS))
    if ldf is None or table is None or not have_effects:
        missing = (
            "the game's scripts.erf and 2da.erf"
            if ldf is None or table is None
            else "the extracted toolset scripts"
        )
        if "--check" in argv:
            print(f"gen_script_constants: no {missing}, nothing to check")
            return 0
        print(f"gen_script_constants: no {missing}", file=sys.stderr)
        return 2
    raw = {}
    parse(ldf, name, raw)
    with open(EFFECTS, encoding="latin-1") as f:
        parse(f.read(), "effect_constants_h.nss", raw)
    with open(TABLES, encoding="latin-1") as f:
        tables = {}
        parse(f.read(), "2da_constants_h.nss", tables)
        for k, v in tables.items():
            if k.startswith(FROM_TABLES):
                raw.setdefault(k, v)
    with open(EVENTS, encoding="latin-1") as f:
        events = {}
        parse(f.read(), "events_h.nss", events)
        for k, v in events.items():
            if k.startswith(FROM_EVENTS):
                raw.setdefault(k, v)
    constants = resolve(raw)
    notes = property_notes(constants, table)
    text = header(
        constants,
        [
            name,
            "effect_constants_h.nss",
            "2da_constants_h.nss",
            "events_h.nss",
            "properties.gda",
        ],
        notes,
    )
    if "--check" in argv:
        try:
            with open(OUT, encoding="utf-8") as f:
                have = f.read()
        except FileNotFoundError:
            have = ""
        if have.split() != text.split():
            print(
                f"gen_script_constants: {os.path.normpath(OUT)} is stale; run tools/gen_script_constants.py"
            )
            return 1
        return 0
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        f.write(text)
    enums = groups(constants)
    print(
        f"{len(constants)} constants, {len(enums)} enums: {', '.join(e for e, _, _ in enums)}"
    )
    for k, v in notes.items():
        print(f"  {k}: {v}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
