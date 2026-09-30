"""Every script command as a C++ function: src/dao/generated/ScriptApi.h.

dao::script::Invoke (dao/Script.h) calls a script command from C++ the way
a script does. This wraps each command the exe implements in a function
with its script.ldf signature, so a plugin writes

    const std::int32_t team = dao::script::api::GetTeamId(creature);
    dao::script::api::SetCombatState(creature, TRUE);

with the script's names, argument order and defaults. Types map to the
library's: int std::int32_t, float float, string std::wstring (taken as
std::wstring_view), object dao::ObjectId, vector dao::Vector, location
dao::CScriptLocation, effect and event the engine's own (returned owned),
command dao::Command, any array dao::script::Array. A default names a
script constant (dao/generated/ScriptConstants.h), or one of the scripts'
own includes, whose value it takes; OBJECT_SELF is dao::script::Self() at
the call. A command whose signature has a type the library does not move
yet (resource, player, any) is left out, and listed at the header's end.

A wrapper returns the command's answer, or the type's empty value if the
call failed; dao::script::Invoke says whether it did.

    python tools/gen_script_api.py            rewrite the header
    python tools/gen_script_api.py --check    exit 1 if it is stale
"""

import csv
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, "..")
COMMANDS = os.path.join(ROOT, "dev", "script_commands.csv")
CONSTANTS = os.path.join(ROOT, "src", "dao", "generated", "ScriptConstants.h")
INCLUDES = os.path.join(ROOT, "toolset", "extract", "scripts", "_Core Includes")
OUT = os.path.join(ROOT, "src", "dao", "generated", "ScriptApi.h")

# (argument type, return type, empty value) per script type.
TYPES = {
    "int": ("std::int32_t", "std::int32_t", "0"),
    "float": ("float", "float", "0.0f"),
    "string": ("std::wstring_view", "std::wstring", "{}"),
    "object": ("::dao::ObjectId", "::dao::ObjectId", "::dao::kInvalidObject"),
    "vector": ("const ::dao::Vector &", "::dao::Vector", "{}"),
    "location": ("const ::dao::CScriptLocation &", "::dao::CScriptLocation", "{}"),
    "effect": ("const ::dao::CGameEffect &", "::dao::script::OwnedEffect", "{}"),
    "event": ("const ::dao::CScriptEvent &", "::dao::script::OwnedEvent", "{}"),
    "command": ("const ::dao::Command &", "::dao::script::OwnedCommand", "{}"),
    "resource": ("const ::dao::Resource &", "::dao::Resource", "{}"),
}
KEYWORDS = {
    "default",
    "new",
    "delete",
    "class",
    "template",
    "this",
    "operator",
    "register",
    "int",
    "float",
}

SIGNATURE = re.compile(r"(\w+(?:\s*\[\])?)\s+(\w+)\((.*)\)$")
ARGUMENT = re.compile(r"(\w+(?:\s*\[\])?)\s+(\w+)(?:\s*=\s*(.+))?$")


def script_type(t):
    t = t.replace(" ", "")
    if t.endswith("[]"):
        return "array"
    return t


def arg_type(t):
    return "const ::dao::script::Array &" if t == "array" else TYPES[t][0]


def ret_type(t):
    if t == "void":
        return "void"
    return "::dao::script::Array" if t == "array" else TYPES[t][1]


def empty(t):
    return "{}" if t == "array" else TYPES[t][2]


def known_constants():
    """{name: value} from the generated constants and the scripts' includes."""
    out = {}
    if os.path.isdir(INCLUDES):
        for name in sorted(os.listdir(INCLUDES)):
            with open(os.path.join(INCLUDES, name), encoding="latin-1") as f:
                for n, v in re.findall(
                    r"const\s+int\s+([A-Z][A-Z0-9_]*)\s*=\s*(-?\w+)\s*;", f.read()
                ):
                    out.setdefault(n, v)
    with open(CONSTANTS, encoding="utf-8") as f:
        generated = dict(
            re.findall(r"constexpr std::u?int32_t (\w+) = (-?\d+)u?;", f.read())
        )
    out.update(generated)
    return out, set(generated)


def int32(n):
    """A script int as C++ writes it: 32 bits, 4294967295 being -1."""
    return n - (1 << 32) if n > 0x7FFFFFFF else n


def default(value, t, constants, generated):
    """The C++ default for a script default, or None."""
    v = value.strip()
    if v == "OBJECT_SELF" and t == "object":
        return "::dao::script::Self()"
    if v == "OBJECT_INVALID" and t == "object":
        return "::dao::kInvalidObject"
    if v in ("TRUE", "FALSE") and t == "int":
        return "1" if v == "TRUE" else "0"
    if t == "resource" and re.fullmatch(r'R"[^"\\]*"', v):
        return f"::dao::Resource{{L{v[1:]}}}" if v != 'R""' else "::dao::Resource{}"
    if t == "string" and re.fullmatch(r'"[^"\\]*"', v):
        return "L" + v
    if t == "float" and re.fullmatch(r"-?\d+(\.\d*)?f?", v):
        return v.rstrip("fF") + ("f" if "." in v else ".0f")
    if t == "int" and re.fullmatch(r"-?\d+", v):
        return str(int32(int(v)))
    if t == "vector" and re.fullmatch(
        r"\[\s*0(\.0*)?\s*,\s*0(\.0*)?\s*,\s*0(\.0*)?\s*\]", v
    ):
        return "::dao::Vector{}"
    if t == "int" and v in constants:
        if v in generated:
            if int(constants[v]) > 0x7FFFFFFF:
                return f"static_cast<std::int32_t>(::dao::script::constants::{v})"
            return f"::dao::script::constants::{v}"
        resolved = constants[v]
        seen = set()
        while resolved in constants and resolved not in seen:
            seen.add(resolved)
            resolved = constants[resolved]
        if re.fullmatch(r"-?\d+", resolved):
            return f"{int32(int(resolved))} /* {v} */"
    return None


def wrappers(rows, constants, generated):
    out, skipped = [], []
    for r in rows:
        m = SIGNATURE.match(r["signature"])
        if not m:
            skipped.append((r, "signature"))
            continue
        ret, name, args = m.groups()
        ret = script_type(ret)
        params = []
        ok = ret == "void" or ret == "array" or ret in TYPES
        if args.strip():
            for a in re.split(r",(?![^\[]*\])", args):
                am = ARGUMENT.match(a.strip())
                if not am:
                    ok = False
                    break
                t, pname, dflt = am.groups()
                t = script_type(t)
                if t != "array" and t not in TYPES:
                    ok = False
                    break
                pname = pname + "_" if pname in KEYWORDS else pname
                params.append(
                    [t, pname, default(dflt, t, constants, generated) if dflt else None]
                )
        if not ok:
            skipped.append((r, "type"))
            continue
        # C++ defaults must trail: drop any before a parameter without one.
        seen_missing = False
        for p in reversed(params):
            if p[2] is None:
                seen_missing = True
            elif seen_missing:
                p[2] = None
        out.append((int(r["id"]), name, ret, params, r["signature"]))
    return out, skipped


def header(functions, skipped):
    lines = [
        "// Generated by tools/gen_script_api.py from dev/script_commands.csv (script.ldf's",
        "// signatures). Do not edit: run the generator again.",
        "//",
        "// Every script command the exe implements, callable from C++ with its",
        "// script name, argument order and defaults (dao::script::Invoke, dao/Script.h:",
        "// on the scripts' thread only). A wrapper returns the command's answer, or",
        "// the type's empty value if the call failed.",
        "//",
        "// Generated code is left as the generator writes it, for --check.",
        "// clang-format off",
        "#pragma once",
        "",
        '#include "dao/ObjectId.h"',
        '#include "dao/Objects.h"',
        '#include "dao/Script.h"',
        '#include "dao/Structures.h"',
        '#include "dao/generated/ScriptConstants.h"',
        "",
        "#include <cstdint>",
        "#include <string>",
        "#include <string_view>",
        "",
        "namespace dao::script::api",
        "{",
        "",
    ]
    for cid, name, ret, params, signature in functions:
        decl = ", ".join(
            f"{arg_type(t)} {p}" + (f" = {d}" if d else "") for t, p, d in params
        )
        call_args = "".join(f", {p}" for _, p, _ in params)
        lines.append(f"// {signature} = {cid};")
        lines.append(f"inline {ret_type(ret)} {name}({decl})")
        lines.append("{")
        if ret == "void":
            lines.append(f"    Invoke({cid}{call_args});")
        else:
            # value_or on the returned optional: moves an owned answer out,
            # and a trivial one needs no std::move.
            fallback = f"{ret_type(ret)}{{}}" if empty(ret) == "{}" else empty(ret)
            lines.append(
                f"    return Invoke<{ret_type(ret)}>({cid}{call_args}).value_or({fallback});"
            )
        lines.append("}")
        lines.append("")
    lines.append(
        f"// Not wrapped: {len(skipped)} commands with a type the library does not move yet."
    )
    for r, _ in skipped:
        lines.append(f"//   {r['signature'] or r['name']} = {r['id']};")
    lines += ["", "} // namespace dao::script::api", "", "// clang-format on", ""]
    return "\n".join(lines)


def main(argv):
    with open(COMMANDS, encoding="utf-8") as f:
        rows = [
            r
            for r in csv.DictReader(f)
            if r["signature"]
            and r["function_va"] not in ("", "0x00000000")
            and r["script_name"]
        ]
    constants, generated = known_constants()
    functions, skipped = wrappers(rows, constants, generated)
    text = header(functions, skipped)
    if "--check" in argv:
        try:
            with open(OUT, encoding="utf-8") as f:
                have = f.read()
        except FileNotFoundError:
            have = ""
        # The formatter reflows the header; compare without any whitespace.
        if "".join(have.split()) != "".join(text.split()):
            print(
                f"gen_script_api: {os.path.normpath(OUT)} is stale; run tools/gen_script_api.py"
            )
            return 1
        return 0
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        f.write(text)
    print(f"{len(functions)} commands wrapped, {len(skipped)} left out")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
