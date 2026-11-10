// Restore the hero's surname after loading the saved character stats (DAOLib #328).
#pragma once

namespace fix
{

// Once from the loader, before plugins load.
void InstallLastNameFix();

} // namespace fix
