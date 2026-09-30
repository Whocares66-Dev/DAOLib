# A PyGhidra post-script: answers a few questions about the analysed exe
# from the command line, for when the MCP server is not up (it holds the
# project, so this and it cannot run at once).
#
#   .\tools\ghidra-mcp.ps1 -Query "vtable VGameModeController" "string COMMAND_TOGGLEGAMEPAUSE" ...
#
#   vtable <class>       the class's vftable(s): address and each slot's function
#   string <text>        strings containing text, with the code that references each
#   refs <0xaddr>        the code referencing an address
#   decompile <0xaddr>   the decompiled function containing an address
#   symbols <text>       symbols whose name contains text
#   analyzers [text]     the analysis options and whether each is on
#   info                 the program: compiler, counts of functions, symbols, vftables
#   export-functions <path>   every function's entry, end and name, tab-separated,
#                        for tools/probe_report.py (game/ghidra/functions.tsv)
#
# The first argument is the file the answers go to; each answer sits between
# "== <question>" and a blank line. Ghidra's own chatter stays in its log.

import sys

from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor

program = currentProgram  # noqa: F821  (PyGhidra provides it)
listing = program.getListing()
symbols = program.getSymbolTable()
references = program.getReferenceManager()
memory = program.getMemory()
address_factory = program.getAddressFactory()


def addr(text):
    return address_factory.getAddress(text)


def function_at(a):
    return listing.getFunctionContaining(a)


def describe(a):
    f = function_at(a)
    if f is None:
        return "%s (no function)" % a
    return "%s in %s @ %s" % (a, f.getName(True), f.getEntryPoint())


def referrers(a, limit=40):
    out = []
    for ref in references.getReferencesTo(a):
        out.append(
            "    <- %s [%s]" % (describe(ref.getFromAddress()), ref.getReferenceType())
        )
        if len(out) >= limit:
            out.append("    ... (more)")
            break
    return out


def q_vtable(name):
    found = 0
    for s in symbols.getSymbols("vftable"):
        ns = s.getParentNamespace().getName(True)
        if name.lower() not in ns.lower():
            continue
        found += 1
        a = s.getAddress()
        print("  %s::vftable @ %s" % (ns, a))
        slot = 0
        p = a
        while True:
            target = memory.getInt(p) & 0xFFFFFFFF
            t = addr("0x%x" % target)
            f = listing.getFunctionAt(t)
            if f is None:
                break
            print("    [%2d] %s  %s" % (slot, t, f.getName(True)))
            slot += 1
            p = p.add(4)
            # a vtable ends where the next one's locator or other data starts
            if symbols.getPrimarySymbol(p) is not None:
                break
    if not found:
        print("  no vftable in a namespace containing %r" % name)


def q_string(text):
    found = 0
    for d in listing.getDefinedData(True):
        if not d.hasStringValue():
            continue
        value = str(d.getValue())
        if text not in value:
            continue
        found += 1
        print("  %s  %r" % (d.getAddress(), value[:120]))
        for line in referrers(d.getAddress()):
            print(line)
        if found >= 30:
            print("  ... (more)")
            break
    if not found:
        print("  no defined string containing %r" % text)


def q_refs(text):
    a = addr(text)
    print("  %s" % describe(a))
    for line in referrers(a, 200):
        print(line)


def q_decompile(text):
    a = addr(text)
    f = function_at(a)
    if f is None:
        print("  no function at %s" % a)
        return
    ifc = DecompInterface()
    ifc.openProgram(program)
    res = ifc.decompileFunction(f, 60, ConsoleTaskMonitor())
    print("  // %s @ %s" % (f.getName(True), f.getEntryPoint()))
    if res.decompileCompleted():
        print(res.getDecompiledFunction().getC())
    else:
        print("  decompile failed: %s" % res.getErrorMessage())
    ifc.dispose()


def q_info(text):
    print("  program      %s" % program.getName())
    print("  language     %s" % program.getLanguageID())
    print(
        "  compiler     %s (spec %s)"
        % (program.getCompiler(), program.getCompilerSpec().getCompilerSpecID())
    )
    print("  format       %s" % program.getExecutableFormat())
    print("  image base   %s" % program.getImageBase())
    print("  functions    %d" % program.getFunctionManager().getFunctionCount())
    print("  symbols      %d" % symbols.getNumSymbols())
    vftables = 0
    for _ in symbols.getSymbols("vftable"):
        vftables += 1
    print("  vftables     %d" % vftables)


def q_analyzers(text):
    options = program.getOptions("Analyzers")
    for name in sorted(options.getOptionNames()):
        if "." in name:
            continue
        value = options.getValueAsString(name)
        if text.lower() in name.lower():
            print("  %-50s %s" % (name, value))


def q_export_functions(path):
    n = 0
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write("# entry\tend\tname, from Ghidra (tools/probe_report.py reads this)\n")
        for fn in program.getFunctionManager().getFunctions(True):
            end = fn.getBody().getMaxAddress().getOffset()
            f.write(
                "%08x\t%08x\t%s\n"
                % (fn.getEntryPoint().getOffset(), end, fn.getName(True))
            )
            n += 1
    print("  wrote %d functions to %s" % (n, path))


def q_symbols(text):
    n = 0
    for s in symbols.getAllSymbols(True):
        if text.lower() in s.getName(True).lower():
            print("  %s  %s" % (s.getAddress(), s.getName(True)))
            n += 1
            if n >= 200:
                print("  ... (more)")
                break
    if not n:
        print("  no symbol containing %r" % text)


HANDLERS = {
    "vtable": q_vtable,
    "string": q_string,
    "refs": q_refs,
    "decompile": q_decompile,
    "symbols": q_symbols,
    "analyzers": q_analyzers,
    "info": q_info,
    "export-functions": q_export_functions,
}

args = list(getScriptArgs())  # noqa: F821
out = open(args[0], "w", encoding="utf-8")
sys.stdout = out
for question in args[1:]:
    print("== %s" % question)
    kind, _, rest = question.partition(" ")
    handler = HANDLERS.get(kind)
    if handler is None:
        print("  unknown question kind %r; one of %s" % (kind, ", ".join(HANDLERS)))
    else:
        try:
            handler(rest.strip())
        except Exception as e:  # noqa: BLE001
            print("  error: %s" % e)
    print("")
out.close()
