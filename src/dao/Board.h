// The Chanter's board as its screen shows it, with or without the screen: the quests a board offers
// with whether each is taken, one taken as the board's Accept takes it, and the board put away.
//
//   for (const auto &quest : dao::board::Quests(5)) // Denerim's
//       if (!quest.accepted) ...
//   dao::board::Accept(0);
//   dao::board::Close();
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace dao::board
{

// A quest a board offers.
struct Quest
{
    std::wstring name; // the plot's name, as the board shows it
    std::wstring plot; // the plot's resource, as GetPlotGUID takes it
    std::wstring text; // its journal text, as the board shows it
    bool accepted = false;
};

// Builds a board as the script command ShowChantersGUI does, with no screen opened, and returns its
// quests in its order: those of its plots not yet done whose offering flag is set.
// @param board a row of the jobboards table: 3 Lothering's, 5 Denerim's, 13 Redcliffe's
// @return none with no game loaded, for a board no row names, or with nothing offered
//
// Call it, and the rest, on the game thread. The board stays built until Close.
std::vector<Quest> Quests(std::int32_t board);

// Returns the quests of the board built now, by Quests or by the board's screen.
std::vector<Quest> Shown();

// Takes a quest of the board built now, as its Accept does: the plot's accepted flag set, the
// plot's script run. Refused, false, for an index out of the board, and for a quest taken already,
// whose Accept the board greys out.
bool Accept(std::size_t index);

// Puts the board away, as its screen's closing does, and closes the screen if it is up.
void Close();

} // namespace dao::board
