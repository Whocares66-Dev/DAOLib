// The probe's script checks: each field dao/generated/ScriptFields.h
// harvested from a getter command, compared with that command's own answer
// every time a script calls it in play.
//
// The getters' slots in the command table (dao/Script.h) are given to one
// shadow handler. It peeks at the object the script passed (pops it and
// pushes it back), calls the engine's handler, peeks at the answer the same
// way, then looks the object up as the getter does and reads our field
// from it. The script gets exactly what the engine gave it. The counts --
// calls, matches, mismatches, calls on no object of the class -- go to
// probe.log at every recorder health line, and the first few mismatches of
// each getter are logged as they happen, with both values.
//
// A match is the library's claim checked: that the offset, the width, the
// lookup and the VM's stack functions are what the tables say. A getter
// scripts never call stays unchecked; the report says which.
#pragma once

namespace probe
{

// Detours CVirtualMachineCommands::InitializeCommands, so the shadow goes
// into the table as soon as the engine has filled it, before any script
// runs. At plugin load. DAOLIB_PROBE_SCRIPT_CHECKS=0 in the environment
// turns the checks off.
void AttachScriptChecks();

// Logs the counts since the start, one line per getter called.
void ReportScriptChecks();

} // namespace probe
