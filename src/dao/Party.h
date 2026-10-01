// The party as the player commands it: who is selected in the party bar.
//
//   for (const dao::ObjectId member : dao::party::Selected()) ...
//
// Read 2026-09-30. The party object is the application's +0xD0's +0x58
// (0x005D57C0); Party::Select (0x00639E50) adds a member to its selection,
// a std::vector of ids at +0x184 (0x006368F0 adds one, and leaves a lone
// selection of that same member alone), in the order selected. The
// primary click's attack (0x00712830) orders the party's members from its
// own list (0x006360C0, members filtered by a flag), which is not
// necessarily this one. On the game's thread. Not yet seen in play.
#pragma once

#include "dao/ObjectId.h"

#include <vector>

namespace dao::party
{

// The members selected in the party bar, first selected first; empty
// before a game is loaded.
std::vector<ObjectId> Selected();

} // namespace dao::party
