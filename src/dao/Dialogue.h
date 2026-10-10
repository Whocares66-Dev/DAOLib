// Watch and steer conversations: the lines spoken, the replies offered, and which lines are
// available. You can also pick a reply or skip a line the way the player would, and tell whether
// a cutscene or a movie is playing and skip it.
//
//   dao::dialogue::OnNpcLine([](auto &, const auto &line) {
//       spdlog::info("{}", dao::tlk::String(line.text));
//   });
//   dao::dialogue::OnLineAvailable([](auto &, const auto &line, bool available) {
//       return available && line.icon != kIntimidate; // hide every intimidate option
//   });
//
// Callbacks run on the game thread, in the middle of the engine's own work. Keep them short, and
// do not start or end a conversation from one.
#pragma once

#include "dao/ObjectId.h"
#include "dao/core/Conversation.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <vector>

namespace dao::dialogue
{

// A line of a conversation, as stored in the conversation's file (dao/core/Conversation.h).
using Line = core::ConversationLine;

// A conversation that is playing. You never create one: Current() and the callbacks give you one.
class Conversation
{
  public:
    Conversation() = delete;
    Conversation(const Conversation &) = delete;
    Conversation &operator=(const Conversation &) = delete;
    ~Conversation() = delete;

    // Returns the raw bytes of the conversation's file (GFF4, type "CONV"), as the engine loaded
    // it. The span is valid only while the conversation runs.
    std::span<const std::byte> File() const;
    // Returns a line of the conversation by its index in the file.
    // @return the line, or nullopt if the index is past the last line
    std::optional<Line> LineAt(std::uint32_t index) const;
    // Returns the index of the most recently spoken line, whether the NPC's or the player's.
    // @return the index, or nullopt before the first line
    std::optional<std::uint32_t> CurrentLine() const;

    // Returns the conversation's owner: the creature or placeable it was started with
    // (BeginConversation's oTarget). Lines whose speaker is OWNER are spoken by it.
    ObjectId Owner() const;
    // Returns who speaks a line, worked out the same way the engine does for the camera and the
    // lip sync:
    //
    // - OWNER is the conversation's owner and PLAYER is the player.
    // - HENCH:<tag> or HENCH<n> is a party member.
    // - Any other tag is the creature with that tag near the owner.
    //
    // If none of these finds anyone, the speaker is the owner. Call it on the game thread.
    // @param line the line's index in the file
    // @return the speaker's id, or nullopt if the index is past the last line
    std::optional<ObjectId> Speaker(std::uint32_t line) const;
    // Returns who a line is spoken to, worked out the same way as Speaker. If the line names no
    // listener, it is BeginConversation's oInitiator, or the controlled character when a
    // creature was told to talk to itself. Call it on the game thread.
    // @param line the line's index in the file
    // @return the listener's id, or nullopt if the index is past the last line
    std::optional<ObjectId> Listener(std::uint32_t line) const;
};

// Returns the conversation on the screen.
// @return the conversation, or null unless the game is in dialogue mode
Conversation *Current();

// Returns the replies on the screen, in order: the lines the player can choose from right now.
// The list is empty outside a conversation and while the NPC is speaking.
std::vector<Line> Replies();

// Adds a callback that decides whether a line is available. It sees every NPC line the engine
// considers as the next line spoken, and every reply it considers offering.
// @param decide receives the conversation, the line and the decision so far, and returns the
//  final decision
//
// A callback can hide a line whose condition passes, or offer one whose condition fails, but
// only among the lines the file lists under that NPC line. It cannot add a line the file does not
// have.
void OnLineAvailable(std::function<bool(Conversation &, const Line &line, bool available)> decide);

// Adds a callback that runs when an NPC line is about to be spoken.
void OnNpcLine(std::function<void(Conversation &, const Line &line)> observer);

// Adds a callback that runs once the replies to an NPC line are on the screen. It receives them
// in the order shown.
void OnReplies(std::function<void(Conversation &, std::span<const Line> replies)> observer);

// Adds a callback that runs when the conversation moves on from the reply the player chose.
// Moving on without a reply does not call it.
void OnReplyChosen(std::function<void(Conversation &, const Line &reply)> observer);

// Notes on answering as the player does: Choose, Continue, SkipCutscene and SkipMovie send the
// messages the screen and the keys send (dao/Messages.h), read 2026-10-01 from the dialogue
// mode's handlers. dao/Dialogue.cpp has what was seen in play.

// Chooses a reply on the screen, the same as clicking it or pressing its number key.
// @param index the reply's position in Replies()
// @return true if the choice was sent (OnReplyChosen then reports it); false, with nothing sent,
//  outside a conversation or if the index is past the last reply
//
// Dialogue mode accepts it only while replies are on the screen. It ignores a reply with no
// text, the hidden "continue" reply the screen does not list; use Continue for that one. Call it
// on the game thread, and not from one of the callbacks above. To click through a conversation:
//
//   if (!dao::dialogue::Replies().empty())
//       dao::dialogue::Choose(0);   // the first reply
//   else
//       dao::dialogue::Continue();  // skip the line being spoken
bool Choose(std::size_t index);

// Skips ahead, the same as the skip key in a conversation. The NPC line being spoken is cut
// short and its replies appear at once. If the first reply has no text, the conversation
// continues through it. Call it on the game thread, and not from one of the callbacks above.
// @return false outside a conversation
bool Continue();

// Notes on cutscenes, read 2026-10-01: dao/Dialogue.cpp has what puts the game in cutscene mode
// and what was seen in play.

// Returns whether a cutscene is playing, that is, whether the game is in cutscene mode. Many of
// the game's staged scenes are conversations instead, and those run in dialogue mode.
bool InCutscene();
// Skips the cutscene that is playing, the same as pressing Escape.
// @return false if no cutscene is playing
bool SkipCutscene();

// Returns whether a pre-rendered video is playing, that is, whether the game is in movie mode.
// The scripts' PlayMovie starts one.
bool InMovie();
// Skips the video that is playing, the same as pressing the skip key.
// @return false if no video is playing
bool SkipMovie();

} // namespace dao::dialogue
