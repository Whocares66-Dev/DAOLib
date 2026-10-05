// The party as the player commands it: who is selected in the party bar,
// and who is controlled.
//
//   for (const dao::ObjectId member : dao::party::Selected()) ...
//   dao::party::OnControlChanged([](dao::ObjectId before, dao::ObjectId now) { ... });
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

#include <functional>
#include <vector>

namespace dao::party
{

// The members selected in the party bar, first selected first; empty
// before a game is loaded.
std::vector<ObjectId> Selected();

// The creature the player controls, as the scripts' GetMainControlled
// answers it: the id at the application's +0xD0's +0x5C (0x005D5CA0),
// beside the party at +0x58. kInvalidObject before a game is loaded. On
// the game's thread. Read 2026-10-04 (DAOLib #22). Seen in play the same
// day: changes by an Eluvian order, a move order handing control over and
// the party bar's key, each both ways, and at a load from nobody to the
// hero.
ObjectId Controlled();

// Called at the first frame after the controlled creature changes, by any
// cause: the party bar, a key, the scripts' SetPrimaryControlled, a
// selection by CenterOnObjectMessage, a plugin's order. Many paths set it,
// so it is compared each frame rather than caught at one setter. A game's
// load reads as a change from kInvalidObject. On the game's thread.
void OnControlChanged(std::function<void(ObjectId before, ObjectId now)> observer);

} // namespace dao::party
