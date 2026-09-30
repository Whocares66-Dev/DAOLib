// The probe's event watch: every event the engine's queue delivers, counted
// by its queue id and, for script events, by EVENT_TYPE_*.
//
// The queue (dao/Addresses.h, "The engine's event queue") is how
// SignalEvent reaches a script. What else goes through it is the question:
// if EVENT_TYPE_COMMAND_COMPLETE does, a turn-based mode can hold a
// creature's next decision there (dev/TURN_BASED.md). The detour on
// AIMaster::ProcessEvent reads the event node before the engine takes it,
// counts it, and lets the engine go on unchanged; the counts go to
// probe.log with the recorder's health line, and the first few of each
// kind are logged as they come, with the target and caller.
#pragma once

namespace probe
{

// At plugin load.
void AttachEventWatch();

void ReportEventWatch();

} // namespace probe
