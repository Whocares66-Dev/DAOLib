// A resource read at its own offset in its archive's file, not at the
// file's shared position (DAOLib issue #3).
//
// A resource's bytes are read by a ReadRequestTask, which seeks the
// archive's one shared file and then reads at its position, under no lock
// another load holds: a load asked for now runs on the asking thread, a
// queued one on a worker. One load seeking between the other's seek and its
// read hands the other the wrong bytes: another model's GFF, whose data
// offset is past the size the parse was given, crashed the game in a
// model's parse (0x004D53A0 reading 0x24), and bytes that are not a GFF lose
// a model quietly. The read is now at the offset its own seek asked for, a
// positioned ReadFile that shares nothing. Each time two loads overlap on a
// file is counted, and said in loader.log. Read 2026-10-03.
#pragma once

namespace fix
{

// Once, from the loader's way into the game, before the plugins load: every
// game, whatever plugins it has.
void InstallArchiveReadFix();

} // namespace fix
