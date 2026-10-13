// The mouse's movement takes its time from the last record read, not from
// the one after it (DAOLib issue #105).
//
// ECInput::RawInputWin32::ReadMouse reads the mouse's buffered records from
// DirectInput (GetDeviceData, 0x14 bytes a record): it asks how many there
// are, allocates that many, reads them and walks them. After the walk, if
// the mouse moved, it makes an event for each axis from a record's time
// stamp and sequence (its +8 and +0xC), and the record it takes them from is
// the one at the count, one past the last: `buffer + count * 0x14`, where
// the walk's index stopped. When every record asked for was read, that is 8
// to 16 bytes past the block's end. In the engine's own pools that was other
// memory of the pool, and the event got whatever lay there. With the
// engine's memory on the system's heap (perf/EngineHeap.h) the block can be
// the last thing before a page that is not there: in play, 2026-10-12, the
// game crashed 30 s after its start with the mouse in use, five records
// ending 12 bytes before such a page (dev/CRASHES.md).
//
// The four reads are corrected where they are, a byte each: the
// displacement of each made 0x14 less, which is the last record's field.
// The code is checked against what was read first, in all three builds the
// same bytes at the same offsets (the address library's session read them,
// 2026-10-12), and left alone if it is anything else.
#pragma once

namespace fix
{

// Once, from the loader's way into the game, before the plugins load: every
// game, whatever plugins it has.
void InstallMouseRecordsFix();

} // namespace fix
