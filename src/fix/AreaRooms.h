// An area's rooms stay loaded when the party leaves it, and the rooms of
// the indoor areas beside it are loaded ahead (DAOLib issue #72).
//
// An area is laid out in rooms, each loaded from a file of its own
// (dao/Addresses.h, "Rooms and their streaming"). A transition's step 10
// unloads every room of the area the party leaves, and the arrival then
// loads the rooms of the area arrived in and waits for them: on the way
// back, the same rooms again; on the way into a place for the first time,
// rooms that could have been loading while the party walked to its door.
// In play, by a door between Redcliffe village and its tavern, that was 0.1
// to 0.5 s of a loading screen of 0.4 to 1.2 s.
//
// So, while the address space has room (fix/AddressSpace.h):
//
// - The engine's unloading of the rooms of the area left is not done. They
//   go with their area, when its list is deleted: the engine's own doing,
//   or the area lists fix's when a list kept is the oldest of too many.
//   (At a swap to another list the engine does not come to that unloading
//   at all once the list left is kept: in play no call of it came there.)
// - In play, from the update of the layout the party is in, the rooms of
//   the list's other areas that are indoors are asked for, one at a time:
//   a load in the background, the engine's own (CRoom::SetState with 4),
//   the next when none is loading, none within two seconds of a loading
//   screen. A room's objects are built on the streaming thread and taken in
//   by the room's slot 24, which the engine calls only for the rooms of the
//   layout the party is in, so that call is made here for the others. An
//   outdoor area's rooms are its terrain's cells, loaded another way, and
//   are left to the arrival.
//
// A room is the area's art in a scene of the area's own, which is drawn
// only when the party is there; an area's creatures, placeables and
// triggers are made with its list, by the engine, whether or not its rooms
// are loaded, and its arrival's events are the loading mode's to send.
//
// In play, 2026-10-11, GOG, windowed, this code as a plugin beside the
// present's fix: the tavern by its door 271 to 353 ms on a first visit and
// after, where it had been 361 to 481 on a first and 378 to 401 after; 15
// rooms of the village's ten indoor areas asked for in the three seconds
// after a load, each asking and each taking in 18 ms of its frame at the
// most, with one second of 32 to 44 frames in it; nothing of another area
// drawn or walked into over some eighty loading screens; about 30 MiB more
// held for the village's interiors. Not known: why the arrival back in the
// village by the map was 0.5 and 1.1 s in the wait for rooms in two
// launches of seven, the update thread loading terrain cells inside the
// layout's update, and 0.06 to 0.28 s in the others.
#pragma once

namespace fix
{

// Once, from the loader's way into the game, before the plugins load: every
// game, whatever plugins it has. DAOLIB_AREA_ROOMS=0 in the environment
// turns it off: an area's rooms are unloaded when the party leaves, and
// none is loaded ahead, the engine as it was.
void InstallAreaRoomsFix();

} // namespace fix
