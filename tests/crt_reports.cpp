// A debug check that fails in a test (the STL's bounds checks, an assert)
// ends the test run with a message on stderr, not a dialog that waits for a
// click: under tools\mutants.py a mutant that read past a vector hung its
// worker for the whole timeout, ten minutes each (2026-10-03), and ctest
// would wait the same way.
#include <Windows.h>

#include <crtdbg.h>
#include <cstdlib>

namespace
{

bool Quiet()
{
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE | _CRTDBG_MODE_DEBUG);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE | _CRTDBG_MODE_DEBUG);
    _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    // And no Windows Error Reporting box when the run ends on a fault.
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    return true;
}

[[maybe_unused]] const bool g_quiet = Quiet();

} // namespace
