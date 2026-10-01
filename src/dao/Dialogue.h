// Conversations as the engine plays them: the lines spoken and the replies
// offered, each as its file has it, and a say in which lines are offered.
//
//   dao::dialogue::OnNpcLine([](auto &, const auto &line) {
//       spdlog::info("{}", dao::tlk::String(line.text));
//   });
//   dao::dialogue::OnLineAvailable([](auto &, const auto &line, bool available) {
//       return available && line.icon != kIntimidate; // no threats today
//   });
//
// Read 2026-09-30. A conversation the engine plays is an object of its own
// (no vtable; the dialogue mode keeps the one on the screen at +0x1C8)
// holding its file, the conversation's GFF4 "CONV" bytes, at +0x20 with
// their size at +0x24 (0x007F2210 checks the header there). Its lines are
// numbered by their place in the file's list, and dao/core/Conversation.h
// reads them. What it does with them:
// - Whether a line may be offered (0x007F0D30, bool __thiscall (line),
//   `ret 4`): its plot condition or script, and the engine's own checks
//   (a recursion guard of 1000 among them), all of which must hold. The
//   NPC's answer is the first of a line's children to pass (0x007F1100),
//   and the replies offered are those that pass (0x007F1A40).
// - Speaking an NPC line (0x007F1EF0, __thiscall (line), `ret 4`): it
//   becomes the current line (+0x38) and dialogue mode's (+0x2C0), it is
//   spoken, and its replies are offered.
// - Offering the replies (dialogue mode's 0x00690B70, __thiscall (list,
//   flag), `ret 8`): the screen's list is cleared and filled, a fast-path
//   reply first, and dialogue mode keeps the lines in that order at +0x1A8
//   (the index SelectResponseMessage names is a place in it).
// The data tree has what the conversation screen shows: Conversation.
// NPCLine, .Responses (each a Text and an Icon), .ResponsesReady, .HasVO
// (dao/Data.h). Ambient conversations, the lines creatures speak in
// passing, are conversations too, and the hooks see them as well. Seen in
// play 2026-09-30 (examples/dialogue, Eleanor at Highever): NPC lines and
// each set of replies as on the screen. What came with them: a line with
// no text (an NPC line used to route, "[OWNER]" and empty); one NPC line
// heard twice a millisecond apart; a "continue" reply with no text; the
// talk table's markup (<emp>) left in the text; the speaker as the file
// writes it (OWNER, not the creature). BeginConversation called in a fight
// answered 1 and started nothing (the Eluvian server, 2026-09-30).
//
// And answering, for a tool that drives the game (a test that must talk
// its way from one place to the next):
//
//   if (!dao::dialogue::Replies().empty())
//       dao::dialogue::Choose(0);   // the first reply
//   else
//       dao::dialogue::Continue();  // skip the line being spoken
//
// Callbacks run on the game's thread, inside the engine's own work: keep
// them short, and make no conversation start or end from one.
#pragma once

#include "dao/core/Conversation.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <vector>

namespace dao::dialogue
{

using Line = core::ConversationLine;

class Conversation
{
  public:
    Conversation() = delete;
    Conversation(const Conversation &) = delete;
    Conversation &operator=(const Conversation &) = delete;
    ~Conversation() = delete;

    // The conversation's file as the engine loaded it: valid while the
    // conversation runs.
    std::span<const std::byte> File() const;
    // A line of it, by its index; nothing for an index past its lines.
    std::optional<Line> LineAt(std::uint32_t index) const;
    // The line spoken last, NPC's or player's; nothing before the first.
    std::optional<std::uint32_t> CurrentLine() const;
};

// The conversation on the screen: dialogue mode's, while the game is in
// it; null otherwise.
Conversation *Current();

// The replies on the screen, in their order: the lines the player may
// choose from now. Empty outside a conversation or while the NPC speaks.
std::vector<Line> Replies();

// Whether each line may be offered: every NPC line the engine considers
// as an answer and every reply it considers offering passes through, and
// each callback gets the verdict so far and returns its own, so a plugin
// can hide a line whose condition holds or offer one whose condition does
// not. Offering a reply the conversation has as a child of the NPC line is
// all this can do; a line not in the file cannot be added.
void OnLineAvailable(std::function<bool(Conversation &, const Line &line, bool available)> decide);

// An NPC line about to be spoken.
void OnNpcLine(std::function<void(Conversation &, const Line &line)> observer);

// The replies offered after an NPC line, in their order on the screen.
void OnReplies(std::function<void(Conversation &, std::span<const Line> replies)> observer);

// The reply the player chose, as the conversation goes on from it
// (Conversation::Advance, 0x007F1910, __thiscall (line), `ret 4`; -1 for
// going on with no reply, which is not reported).
void OnReplyChosen(std::function<void(Conversation &, const Line &reply)> observer);

// Answering as the player does, by the messages the screen and the keys
// send (dao/Messages.h), read 2026-10-01 from the dialogue mode's handlers.
// On the game's thread, and not from one of the callbacks above. Not yet
// seen in play.

// Chooses the reply at a place in Replies(): what a click on it, or its
// number key, sends. The dialogue mode takes it only while replies are on
// the screen, and does nothing for a reply with no text, the "continue"
// the screen does not list; Continue takes that one. False, and nothing
// sent, outside a conversation or for a place past the replies; true says
// it was sent, and OnReplyChosen says it was taken.
bool Choose(std::size_t index);

// Goes on, as the skip key does in a conversation: the NPC's line being
// spoken is cut short and its replies offered at once, and where the first
// reply is one with no text, the conversation goes on through it. False
// outside a conversation.
bool Continue();

// Whether a cutscene is playing (the game is in its cutscene mode), and
// skipping it as Escape does. SkipCutscene is false when none is playing.
bool InCutscene();
bool SkipCutscene();

} // namespace dao::dialogue
