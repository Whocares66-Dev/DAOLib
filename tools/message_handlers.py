"""The engine's message handler table, read out of game/DAOrigins.exe.

Eclipse passes messages (UserTogglePauseMessage, SetGameModeMessage,
RawInputMessage, ...) to handler objects, one class per (owner, message)
pair, instantiated from a template whose RTTI name says both and names the
method:

    Messages::Private::MemberFunctionMessageHandler
        <GameModeController, UserTogglePauseMessage, &GameModeController::HandleMessage>

The template's invoke slot is shared code that calls a member function
pointer kept in the handler at +8, with a `this` adjustment at +0xC. So the
method's address is not in the vtable; it is where the handler is built:

    push  0x10 ; call operator new
    mov   ecx, 0x006403C0          ; the method
    xor   edx, edx                 ; the this-adjustment
    mov   dword ptr [eax], 0x00B06648   ; the handler's vftable
    mov   dword ptr [eax + 8], ecx
    mov   dword ptr [eax + 0xc], edx

GUI::ScaleformManager's cursor handlers are a smaller form, eight bytes:
the method at +4 and no adjustment, which the invoke slot (0x00511350)
calls on the owner (DAOLib #188):

    push  8 ; call operator new
    mov   dword ptr [eax], 0x00AF5480       ; the handler's vftable
    mov   dword ptr [eax + 4], 0x00512980   ; the method

This finds each handler vftable (tools/daoexe.py's RTTI walk), the code
that stores it, and the values stored at +8 and +0xC there (or +4), tracking
constants loaded into registers since the function began. It writes
dev/message_handlers.csv, one row per handler:

    owner,message,method,function_va,this_adjust,vftable_va,built_at_va,trivial

function_va is 0 where the pattern was not recognised. trivial is 1 for a
stub of eight bytes or fewer, shared by the compiler with every function
of the same body (see trivial()); the probe does not count those and
tools/names.py does not name them. tools/names.py names
each function Owner::Method_Message in Ghidra (Method overloads on the
message type, so the type goes in the name).

    python tools/message_handlers.py            rewrite the CSV, report counts
    python tools/message_handlers.py --print    print the table instead
"""

import csv
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import daoexe  # noqa: E402
import msvc_names  # noqa: E402

OUT = os.path.join(HERE, "..", "dev", "message_handlers.csv")
TEMPLATE = "Messages::Private::MemberFunctionMessageHandler<"
REGISTERS = ("eax", "ebx", "ecx", "edx", "esi", "edi", "ebp")


def constant(op, regs):
    """The value of an operand if it is an immediate or a register holding one."""
    op = op.strip()
    if op.startswith("0x") or op.isdigit():
        return int(op, 0)
    return regs.get(op)


def built_with(img, vftable, ref):
    """(function va, this adjust) stored beside the vftable in the code at ref."""
    start = img.function_start(ref)
    if start is None:
        return None
    regs = {}
    base = None
    found = {}
    # Read on past a ret: the int3 padding function_start looks for is not always
    # there, so start may be a function before the one holding ref (0x0063A240 for
    # CCParty's SelectCharacterMessage, built at 0x0063A2C8; DAOLib #188). Nothing
    # tracked before a ret holds after it.
    for ins in img.disasm(start, count=1 << 20, limit=ref - start + 0x40):
        ops = [o.strip() for o in ins.op_str.split(",")]
        if ins.address > ref + 0x30:
            break
        if ins.mnemonic == "ret":
            regs, base, found = {}, None, {}
            continue
        if (
            ins.mnemonic == "mov"
            and len(ops) == 2
            and ops[0].startswith("dword ptr [")
            and constant(ops[1], regs) == vftable
        ):
            base = ops[0][len("dword ptr [") : -1]
            continue
        if base and ins.mnemonic == "mov" and len(ops) == 2:
            for disp, key in (("4", "plain"), ("8", "fn"), ("0xc", "adjust")):
                if ops[0] == f"dword ptr [{base} + {disp}]":
                    found[key] = constant(ops[1], regs)
        # Register tracking: a constant loaded, zeroed, or anything else clobbering it.
        if ins.mnemonic == "mov" and len(ops) == 2 and ops[0] in REGISTERS:
            value = constant(ops[1], regs)
            if value is None:
                regs.pop(ops[0], None)
            else:
                regs[ops[0]] = value
        elif (
            ins.mnemonic == "xor"
            and len(ops) == 2
            and ops[0] == ops[1]
            and ops[0] in REGISTERS
        ):
            regs[ops[0]] = 0
        elif ins.mnemonic == "call":
            for r in ("eax", "ecx", "edx"):
                regs.pop(r, None)
        elif ops and ops[0] in REGISTERS:
            regs.pop(ops[0], None)
        if "fn" in found and "adjust" in found:
            break
    if found.get("fn") is not None:
        return found["fn"], found.get("adjust") or 0
    if found.get("plain") is not None:
        return found["plain"], 0
    return None


def trivial(img, fn):
    """Whether a function is a stub of eight bytes or fewer to its return,
    like `xor al, al; ret 4`. The compiler folds identical functions into
    one copy, so a handler that only returns false shares its address with
    every other function in the exe that only returns false: 0x0091FC50,
    GoToForumsMessage's, ran 20,000 times a second in play (2026-09-29).
    A count or a name at such an address says nothing about the handler."""
    if not fn:
        return False
    instructions = img.disasm(fn, count=4)
    size = 0
    for ins in instructions:
        size += ins.size
        if ins.mnemonic == "ret":
            return size <= 8
    return False


def handlers(img):
    rows = []
    for va, decorated, offset, _ in img.vtables:
        name = msvc_names.undecorate(decorated)
        if offset or not name.startswith(TEMPLATE):
            continue
        args = msvc_names.template_args(name)
        if len(args) != 3 or not args[2].startswith("&"):
            continue
        owner, message = args[0], args[1]
        method = msvc_names.split_top(args[2][1:])[-1]
        fn, adjust, built = 0, 0, 0
        for ref in img.code_refs(va):
            got = built_with(img, va, ref)
            if got:
                (fn, adjust), built = got, ref
                break
        rows.append((owner, message, method, fn, adjust, va, built))
    rows.sort()
    return rows


def main(argv):
    img = daoexe.Image()
    rows = handlers(img)
    found = sum(1 for r in rows if r[3])
    if "--print" in argv:
        for owner, message, method, fn, adjust, _, _ in rows:
            adj = f" this+{adjust:#x}" if adjust else ""
            print(f"{fn:#010x}  {owner}::{method}({message}){adj}")
    else:
        with open(OUT, "w", encoding="utf-8", newline="") as f:
            w = csv.writer(f)
            w.writerow(
                [
                    "owner",
                    "message",
                    "method",
                    "function_va",
                    "this_adjust",
                    "vftable_va",
                    "built_at_va",
                    "trivial",
                ]
            )
            for owner, message, method, fn, adjust, va, built in rows:
                w.writerow(
                    [
                        owner,
                        message,
                        method,
                        f"{fn:#010x}",
                        adjust,
                        f"{va:#010x}",
                        f"{built:#010x}",
                        1 if trivial(img, fn) else 0,
                    ]
                )
        print(f"wrote {os.path.normpath(OUT)}")
    owners = len({r[0] for r in rows})
    print(
        f"{len(rows)} handlers over {owners} owner classes; {found} with the method found"
    )
    missing = [r for r in rows if not r[3]]
    for owner, message, method, _, _, va, _ in missing[:10]:
        print(f"  not found: {owner}::{method}({message}) vftable {va:#x}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
