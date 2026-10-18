# Script commands, group "strings": the GOG exe's handlers

Read 2026-10-18 by lib-agent-2's subagent in `game\DAOrigins.exe` (GOG 1.05, base 0x400000) with `tools\daoexe.py --disasm`, for DAOParagonEngine #27. No Ghidra, no play. "(checked)" means read in the disassembly; "(inferred)" means not.

Shared facts (checked):
- Every handler is `__thiscall (commandId, paramCount)`, `ret 8`, and none is shared. Each pops every argument unconditionally, with `StackPopString` 0x00A2F4C0 and `StackPopInteger` 0x00A2F3C0. It returns -639 on a failed pop and -638 on a failed push (`StackPushString` 0x00A2D730, `StackPushInteger` 0x00A2D650), and 0 otherwise.
- A script string is an MSVC 8 `std::wstring` (`dao/core/Msvc8.h`: text inline at +4 when capacity +0x18 < 8, else a pointer there; size +0x14). **Lengths and positions count UTF-16 code units**, not bytes or characters.
- **Positions and counts are compared unsigned** against the length (`jb`/`jae`/`ja`). So a negative position or count behaves as a huge one. **No command here fails the script for a bad position or count**: the result is clamped or empty, as described per command. script.ldf's "returns an empty string on error" is true only in the cases given.

### StringUpperCase (97)
- **Handler:** 0x0064FE30. It calls 0x004C38E0 (copy, then map each unit).
- **Does:** pushes a copy with each unit from 'a' to 'z' (0x61–0x7A) minus 0x20. **ASCII only**: é, ß, ä and the like are unchanged. No locale is involved. (checked)
- **Edge cases:** an empty string gives an empty string. (checked)

### StringLowerCase (98)
- **Handler:** 0x0064FF30. It calls 0x004C3A50.
- **Does:** the same with 'A' to 'Z' (0x41–0x5A) plus 0x20. ASCII only. (checked)

### StringRight (99)
- **Handler:** 0x00650030. It calls 0x004C31A0 (the right part) and `std::wstring::substr` 0x004C2670.
- **Does:** pops sString, then nCount, and pushes the **last** nCount units: `substr(len - n)` with `n = min((unsigned)nCount, len)`. (checked)
- **Edge cases:** nCount ≥ length gives the whole string. **A negative nCount gives the whole string**, not an empty one (unsigned). 0 gives "". script.ldf's prose has Right and Left the wrong way round ("the first nCount characters ... from the right"); the engine does the usual. (checked)

### StringLeft (100)
- **Handler:** 0x00650170. It calls 0x004C30F0 (the left part).
- **Does:** pops sString, then nCount, and pushes the **first** `min((unsigned)nCount, len)` units, `substr(0, n)`. (checked)
- **Edge cases:** as StringRight: a negative nCount or one past the end gives the whole string, and 0 gives "". (checked)

### InsertString (101)
- **Handler:** 0x006502B0. It calls 0x004C25D0 (insert, checked against the length) → 0x004C1360.
- **Does:** pops sDestination, sString, then nPosition. It inserts sString into sDestination before unit nPosition and pushes the result. (checked)
- **Edge cases:** nPosition == length appends. **nPosition > length, or negative (unsigned), leaves sDestination unchanged** and pushes it, not an empty string as script.ldf says. There is no exception and no failure. An empty sString gives sDestination. (checked)

### SubString (102)
- **Handler:** 0x006503F0. It calls 0x004C3250 (`substr` with its own checks).
- **Does:** pops sString, nStart, then nCount, and pushes `substr(nStart, min((unsigned)nCount, len - nStart))`. (checked)
- **Edge cases:** nStart > length or negative gives "". nStart == length gives "". nCount past the end, **or negative, gives the rest of the string** from nStart. nCount 0 gives "". (checked)

### FindSubString (103)
- **Handler:** 0x00650550. It calls `std::wstring::find(const wstring&, pos)` 0x004C0F70.
- **Does:** pops sString, sSubString, then nStart (always popped; the default comes from the compiler). It pushes the index of the first match at or after nStart, or -1. (checked)
- **Edge cases:** nStart > length, or negative, gives -1 (checked). No match gives -1 (npos as int). The match is exact and case-sensitive, unit by unit. An empty sSubString gives nStart when nStart ≤ length (MSVC `find`, inferred).

### IsStringEmpty (104)
- **Handler:** 0x00657D80.
- **Does:** pops sString and pushes 1 if its size (+0x14) is 0, else 0. A string of spaces is not empty. (checked)

### GetStringByStringId (179)
- **Handler:** 0x00650680. It calls the talk string helpers: make 0x004D1C70 (whose reset 0x004D1B80 sets gender 0), text 0x004D1EB0, and resolve 0x004D1D50 → `CTlkTable::GetString` 0x004E4990 on the table at `[0x00C1D08C]` (`Addresses.h` kTlkTable).
- **Does:** pops nId and pushes that talk-table string, always the **male** form (the table's gender +0x1C is set from the talk string's +0x24, which is 0). Every talk table the engine has loaded is searched, the core's, the campaign's and the add-ins'. (checked; the order of the tables is not read here, see `dao/Tlk.h`)
- **Edge cases:** an id that is not in any table, a negative one, or no table loaded all give "" and do not fail the script. Designer tags (`<FirstName/>` …) come back raw, since this path does not call ReplaceDesignerTags (checked). **A debug option**: if `ClientOptions` ([0x00C32938]) +0x674's flag is set, the id is not 0 or -1, and the text is not empty, the text becomes `"{<id> - <text>}"` (format 0x00AF0DB0, `{%u - %ls}`). (checked; the option's name is not read, and it is off in a normal game, inferred)

## Not read
- The name of the `ClientOptions` +0x674 option that decorates talk strings.
- The order in which `CTlkTable::GetString` searches several tables (see `src/dao/Tlk.h`, master `dev/` notes).

## Described elsewhere first (batch 4, read 2026-10-18 by lib-agent-2)

Read 2026-10-18 in the GOG exe. Not seen in play. Strings are `std::wstring` (UTF-16); see the shared facts of `strings.md`.

### GetStringLength (96)
- **Handler:** 0x00657CC0 (not shared). `StackPopString` 0x00A2F4C0, `StackPushInteger` 0x00A2D650.
- **Described in:** `dev/LIBRARY.md` (528 calls of it made from C++ inside script commands in play, all agreeing, 2026-09-30) and `dev/PROBE.md`; not as a handler.
- **Does:** pops sString and pushes the string object's size field (+0x14 of the `std::wstring`, read at 0x00657D2B) as an int, in **UTF-16 code units**. (checked)
- **State:** none.
- **Edge cases:** "" gives 0. There is no cap, no character count and no check of surrogate pairs: a character outside the Basic Multilingual Plane counts 2 (inferred from the size being the unit count). A failed pop returns -639, a failed push -638.

## Not read
- Nothing else; the handler is 14 instructions around the two stack calls.
