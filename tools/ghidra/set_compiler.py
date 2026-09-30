# A PyGhidra pre-script: make the program ready for Ghidra's RTTI pass.
#
# Two things stand in its way on DAOrigins.exe (MSVC 7.1, 2011), both found
# 2026-09-29:
#
# 1. The PE loader names the compiler from the Rich header and linker version,
#    and for this exe left "unknown". Ghidra's RTTI pass
#    (RunRttiAnalyzerScript.java) and class recovery
#    (RecoverClassesFromRTTIScript.java) refuse a program whose compiler is not
#    Visual Studio or clang: "TypeDescriptor data type model is only valid for
#    Visual Studio windows PE", "This script will not work on this program
#    type". The value must be PeLoader's own label, "visualstudio:unknown",
#    which PEUtil.isVisualStudioOrClangPe compares against; the bare
#    "visualstudio" reads right in the program info and fails both.
#
# 2. The RTTI pass records that it has run in the program option "RTTI Found"
#    and returns at once on every later run, whatever the first run achieved.
#    A run made while the compiler was wrong leaves that option set and the
#    program with no RTTI at all. So the option is cleared here while the
#    program holds no RTTI type descriptor, and left alone once it does.
#
# The exe is MSVC's: its RTTI, its SEH prologues and its msvcr71 imports say so.

from ghidra.app.util.opinion import PeLoader
from ghidra.program.model.listing import Program

program = currentProgram  # noqa: F821  (PyGhidra provides it)

want = str(PeLoader.CompilerOpinion.CompilerEnum.VisualStudio)
before = program.getCompiler()
if before != want:
    program.setCompiler(want)
    print("set_compiler: compiler %r -> %r" % (before, program.getCompiler()))
else:
    print("set_compiler: compiler already %r" % before)

RTTI_FOUND = "RTTI Found"  # RttiAnalyzer.RTTI_FOUND_OPTION
options = program.getOptions(Program.PROGRAM_INFO)
has_rtti = any(
    True for _ in program.getSymbolTable().getSymbols("RTTI_Type_Descriptor")
)
if options.contains(RTTI_FOUND) and not has_rtti:
    options.removeOption(RTTI_FOUND)
    print(
        "set_compiler: cleared %r: the program has no RTTI type descriptor yet"
        % RTTI_FOUND
    )
