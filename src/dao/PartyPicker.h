// The party picker: who it offers and in what state, a follower chosen or let go as a click on
// them does, each refused where the picker refuses the click, and its Remove All and Accept.
//
//   for (const auto &entry : dao::partypicker::Entries())
//       if (entry.state == dao::partypicker::State::Available)
//           dao::partypicker::Choose(entry.creature);
//   dao::partypicker::Accept();
#pragma once

#include "dao/ObjectId.h"

#include <cstdint>
#include <vector>

namespace dao::partypicker
{

// An entry's state, the follower's as the picker took it (FOLLOWER_STATE_*), and changed by
// choosing.
enum class State : std::int32_t
{
    Chosen = 1,       // in the party when the picker is accepted
    Available = 2,    // may be chosen
    Unavailable = 3,  // shown, not to be chosen
    Suspended = 5,    // not to be chosen
    LockedActive = 7, // kept in the party by the story; not to be let go
};

// A creature the picker shows.
struct Entry
{
    ObjectId creature = kInvalidObject;
    State state = State::Unavailable;
    std::int32_t slot = -1; // its place among the four chosen, or -1
    bool hero = false;      // the hero, always chosen
};

// Returns the creatures the picker shows, in its order.
// @return none unless the party picker is the current mode and done showing
//
// Call it, and the rest, on the game thread.
std::vector<Entry> Entries();

// Why a step was refused, or None if it was taken.
enum class Refusal : std::uint8_t
{
    None,
    NotShown,   // the party picker is not the current mode, or is showing or leaving
    ReadOnly,   // the picker is shown only to look at (GetPartyPickerGUIStatus is not USE)
    NotOffered, // the picker shows no such creature
    Locked,     // the hero, or a follower the story keeps or keeps out
    AlreadySo,  // chosen already, or not chosen to let go
    PartyFull,  // four are chosen
};

// Chooses a follower as a click on them does: their figure steps forward and they take a place.
Refusal Choose(ObjectId creature);

// Lets a chosen follower go as a click on them, or on their portrait, does.
Refusal Release(ObjectId creature);

// Lets every chosen follower go but the hero and those the story keeps, as Remove All does.
Refusal ReleaseAll();

// Accepts the party as the picker's button does and closes it. The party changes after, when the
// module's scripts have had the picker's events: wait for a frame or more before reading it.
Refusal Accept();

} // namespace dao::partypicker
