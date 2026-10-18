# Script commands, group "conversions": the GOG exe's handlers

Read 2026-10-18 by lib-agent-2's subagent in `game\DAOrigins.exe` (GOG 1.05, base 0x400000) with `tools\daoexe.py --disasm`, for DAOParagonEngine #27. No Ghidra, no play. "(checked)" means read in the disassembly; "(inferred)" means not; CRT behaviour is MSVC 8's, inferred from which CRT entry is called.

Shared facts (checked):
- Every handler is `__thiscall (commandId, paramCount)`, `ret 8`. It returns -639 on a failed pop, -638 on a failed push, and 0 otherwise. Strings are `std::wstring` (UTF-16). Pops: `StackPopInteger` 0x00A2F3C0, `StackPopFloat` 0x00A2F410, `StackPopVector` 0x00A2F460, `StackPopString` 0x00A2F4C0, `StackPopObject` 0x00A2F6D0, and the untyped 0x00A2F720 (an `any`, as {VM type tag, value}, freed by 0x00A2F7C0). Pushes: `StackPushInteger` 0x00A2D650, `StackPushFloat` 0x00A2D690, `StackPushVector` 0x00A2D6D0, `StackPushString` 0x00A2D730.
- Text is made by two wide `printf` helpers:
  - 0x004C3030 `(wstring *dst, fmt, ...)` sizes the output first (`_vscwprintf`), so it is never cut.
  - 0x004C2B10 (int, `"%ld"`) and 0x004C2B80 (float, `"%f"`) format into a 32-unit stack buffer with a limit of 31 (0x004C0330 → 0x008E78E7). An overflow is cut to 30 units and terminated, with no failure. The cut was read in the CRT's tail at 0x008E78D3. (checked)
- Parsing is the CRT's: 0x004C0950 `ForScript_StringToInt` is `wcstol(s, &end, 0)` (0x008E835F → 0x008E8168 with flags 0, signed; 0x008E8388 is the unsigned twin). 0x004C09D0 `ForScript_StringToFloat` calls 0x008E84B5 `(s, &end)` (wcstod, inferred) and rounds the double to a float. Neither looks at `end`, so trailing junk is ignored and **no parse error is ever reported**.

### IntToFloat (48)
- **Handler:** 0x0064F3A0.
- **Does:** `fild` the int, stored as a 32-bit float, and pushes it. (checked)
- **Edge cases:** above 2^24 the result is rounded to the nearest float (x87 round-to-nearest, inferred); for example 16777217 gives 16777216.0.

### FloatToInt (49)
- **Handler:** 0x0064F3F0. It calls `_ftol2` 0x008E34F0.
- **Does:** pushes the float **truncated toward zero**: 2.9 → 2, -2.9 → -2. (checked)
- **Edge cases:** NaN and values outside ±2^31 give 0x80000000 (-2147483648), the integer indefinite (inferred from `_ftol2`).

### IntToString (50)
- **Handler:** 0x0064F440. It calls 0x004C2B10.
- **Does:** pushes `"%ld"` of the int: signed decimal, a '-' when negative, no '+' or padding. (checked)

### ToString (149)
- **Handler:** 0x00668F30. It calls 0x00A2F720, 0x004C2B10, 0x004C2B80 and `GetGameObjectById` 0x005D5840.
- **Does:** pops one `any` and switches on its VM type (`dev/NCS.md`'s type bytes). (checked)
  - 3 (int) gives `"%ld"`.
  - 4 (float) gives `"%f"`, six decimals, no padding, unlike FloatToString.
  - 6 (object) gives the object's **tag**, by the same path as GetTag (0x006596A0): vtable +0x18, then the `std::wstring` at +0xBC.
- **Edge cases:** an object id that matches no object gives "". An object whose +0x18 returns null gives "OBJECT_INVALID". **Any other type, a string or a vector or a structure, gives `"(can not cast variable to string)"`**, so ToString of a string does not return the string. A float whose `%f` text is over 30 characters (about 1e24 and up) is cut at 30, giving a wrong number with no failure. (checked)

### StringToInt (51)
- **Handler:** 0x0064F510 (shared with HexStringToInt 58; there is no switch on the number). It calls `ForScript_StringToInt` 0x004C0950.
- **Does:** pushes `wcstol(sNumber, &end, 0)`. (checked)
- **Edge cases (CRT, inferred):**
  - Leading white space and a sign are accepted, and parsing stops at the first unit that is not a digit ("12abc" → 12).
  - No digits gives 0.
  - **Base 0 means C prefixes**: "0x1A" → 26, and **a leading 0 is octal**, so "010" → 8 and "09" → 0.
  - Overflow clamps to 2147483647 or -2147483648.

### FloatToString (52)
- **Handler:** 0x00669100. It calls 0x004C3030 twice.
- **Does:** pops fFloat, nWidth and nDecimals. It clamps the width to 0..18 and the decimals to 0..9 (checked). It builds the format `"%<w>.<0d>f"` (`%%%2d.%02df`, or `%%%1d...` for widths under 10; e.g. "%18.09f"), then formats the float as a double with it. (checked)
- **Edge cases:** **nWidth is the whole field's minimum width, padded with leading spaces**, not "digits before the decimal" as script.ldf says. With the defaults, 1.5 → `"       1.500000000"` (18 characters). Width 0 means no padding. Decimals 0 gives no decimal point. The output is never cut. A float's binary error shows at 9 decimals (0.1 → "0.100000001"; inferred). Rounding is the CRT's `%f` (inferred).

### StringToFloat (53)
- **Handler:** 0x0064F7C0. It calls `ForScript_StringToFloat` 0x004C09D0.
- **Does:** pushes the wcstod of the string, rounded to a float. (checked)
- **Edge cases (CRT, inferred):** leading white space, a sign, decimals and an exponent ("1.5e2") are accepted, and the C locale's '.' is the decimal point. No digits gives 0.0, and trailing junk is ignored. Out of a float's range gives ±inf (after the narrowing).

### ObjectToString (54)
- **Handler:** 0x0064F890.
- **Does:** pushes `"%d"` of the raw object id (0x00AEA900). The object is not looked up. (checked)
- **Edge cases:** OBJECT_INVALID gives "-1", and ids with the top bit set are negative. A destroyed object's id still prints. (checked)

### VectorToString (55)
- **Handler:** 0x0064F9D0.
- **Does:** pushes `"%f %f %f"` (0x00AF7090) of x, y and z, each as a double: six decimals, single spaces, no padding, never cut. (checked)

### StringToVector (56)
- **Handler:** 0x0064FAD0.
- **Does:** **always pushes (0, 0, 0).** The handler sets a zero vector and pops the string. It then calls the *printf* helper 0x004C3030 (not a scanf) with the popped string as the output, `"%f %f %f"` as the format, and the three components' addresses as the arguments. So it overwrites its own copy of the string with three garbage numbers and pushes the untouched zero vector. (checked) This is a vanilla bug that a port matching vanilla must keep: scripts get [0,0,0] for any input.

### IntToHexString (57)
- **Handler:** 0x0064FBC0.
- **Does:** pushes `"0x%08x"` (0x00B07D88): lower case, always 8 digits, two's complement for negatives (-1 → "0xffffffff"). (checked)

### HexStringToInt (58)
- **Handler:** 0x0064F510, **the same body as StringToInt** with no switch, `wcstol(s, &end, 0)`. (checked)
- **Edge cases (CRT, inferred):**
  - Hex is parsed only with a "0x"/"0X" prefix: "0xff" → 255, but **"ff" → 0** and "10" → 10 (decimal).
  - Because the parse is signed, **"0xffffffff" → 2147483647** (clamped). So `HexStringToInt(IntToHexString(n))` returns n only for n ≥ 0.

### CharToInt (59)
- **Handler:** 0x006692A0.
- **Does:** pushes the first UTF-16 unit of the string, zero-extended (0..65535). (checked)
- **Edge cases:** an empty string gives 0 (its terminator). A non-ASCII character gives its code unit (é → 233), not a byte. (checked)

### IntToChar (60)
- **Handler:** 0x0064FCA0. It calls 0x004C2B10.
- **Does:** **pushes the decimal text of the int's low byte, sign-extended**, not a character. It takes `movsx byte` of the int and formats it with `"%ld"`: IntToChar(65) → "65", not "A"; IntToChar(200) → "-56"; IntToChar(256) → "0". (checked) This contradicts script.ldf, and it is vanilla behaviour to keep.

### FeetToMeters (61)
- **Handler:** 0x0064FD70.
- **Does:** pushes `fFeet * 0.30480000376701355` (a double at 0x00B07D98, 0.3048 rounded to a float), stored as a float. (checked)

### YardsToMeters (62)
- **Handler:** 0x0064FDD0.
- **Does:** pushes `fYards * 0.9114000201225281` (a double at 0x00B07DA0), stored as a float. (checked)
- **Edge cases:** **the factor is 0.9114, not the true 0.9144**. script.ldf says 0.9114 too; vanilla has this error. (checked)

## Not read
- The CRT's exact rounding for `%f` (0.5 cases) and its Unicode digit handling in wcstol/wcstod; the behaviour above is MSVC 8's documented behaviour, not traced.
- Which script types the compiler lets reach ToString's `any` (only the handler's switch was read).
