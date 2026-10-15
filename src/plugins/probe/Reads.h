// The probe's read watch (DAOLib #135): what the engine does with a resource read from an
// enciphered add-in archive, around a plugin's patch of it. For the resources named in
// DAOLIB_PROBE_READS (a comma-separated list of name prefixes; none, and no detour, without it), the
// detour on Resource::ReadOnFirstUse logs, before and after the read, the resource's source and
// its class, the length in the source (+0x2C), the length read (+0x30), the bytes' address
// (+0x20), the state (+0x34), the borrowed and read bytes (+0x3C, +0x3D), and after the read the
// first and last 16 bytes. Detoured at load, before any plugin's, so a patch made in another
// plugin's detour shows in the "after" line. It changes nothing.
#pragma once

namespace probe
{

// At plugin load.
void AttachReadWatch();

} // namespace probe
