// Conversations as the engine plays them (dao/Dialogue.h): the hooks on the engine's conversation
// functions, and answering by the messages the screen and the keys send.
//
// Read 2026-09-30. A conversation the engine plays is an object of its own (no vtable; the
// dialogue mode keeps the one on the screen at +0x1C8) holding its file, the conversation's GFF4
// "CONV" bytes, at +0x20 with their size at +0x24 (0x007F2210 checks the header there). Its lines
// are numbered by their place in the file's list, and dao/core/Conversation.h reads them. What it
// does with them:
// - Whether a line may be offered (0x007F0D30, bool __thiscall (line), `ret 4`): its plot
//   condition or script, and the engine's own checks (a recursion guard of 1000 among them), all
//   of which must hold. The NPC's answer is the first of a line's children to pass (0x007F1100),
//   and the replies offered are those that pass (0x007F1A40).
// - Speaking an NPC line (0x007F1EF0, __thiscall (line), `ret 4`): it becomes the current line
//   (+0x38) and dialogue mode's (+0x2C0), it is spoken, and its replies are offered.
// - Offering the replies (dialogue mode's 0x00690B70, __thiscall (list, flag), `ret 8`): the
//   screen's list is cleared and filled, a fast-path reply first, and dialogue mode keeps the
//   lines in that order at +0x1A8 (the index SelectResponseMessage names is a place in it).
// - Going on from the reply chosen (Conversation::Advance, 0x007F1910, __thiscall (line),
//   `ret 4`; -1 for going on with no reply, which OnReplyChosen does not report).
//
// The data tree has what the conversation screen shows: Conversation.NPCLine, .Responses (each a
// Text and an Icon), .ResponsesReady, .HasVO (dao/Data.h). Ambient conversations, the lines
// creatures speak in passing, are conversations too, and the hooks see them as well.
//
// Seen in play 2026-09-30 (examples/dialogue, Eleanor at Highever): NPC lines and each set of
// replies as on the screen. What came with them: a line with no text (an NPC line used to route,
// "[OWNER]" and empty); one NPC line heard twice a millisecond apart; a "continue" reply with no
// text; the talk table's markup (<emp>) left in the text; the speaker as the file writes it
// (OWNER, not the creature). BeginConversation called in a fight answered 1 and started nothing
// (the Eluvian server, 2026-09-30).
//
// Answering as the player does (Choose, Continue, SkipCutscene, SkipMovie) sends the messages the
// screen and the keys send (dao/Messages.h), read 2026-10-01 from the dialogue mode's handlers,
// for a tool that drives the game (a test that must talk its way from one place to the next).
#include "dao/Dialogue.h"

#include "dao/Addresses.h"
#include "dao/ClientExoApp.h"
#include "dao/GameModes.h"
#include "dao/Guard.h"
#include "dao/Hook.h"
#include "dao/Messages.h"
#include "dao/Module.h"
#include "dao/core/Msvc8.h"

#include <mutex>
#include <utility>

namespace dao::dialogue
{

namespace
{

// In the conversation (the engine's, no vtable).
constexpr std::size_t kFileData = 0x20;
constexpr std::size_t kFileSize = 0x24;
constexpr std::size_t kOwner = 0x28;
constexpr std::size_t kCurrentLine = 0x38;
// In dialogue mode (GameModeDialog).
constexpr std::size_t kReplies = 0x1A4; // a std::vector of line indices, its first at +0x1A8
constexpr std::size_t kConversation = 0x1C8;
constexpr std::uint32_t kNone = 0xFFFFFFFF;

template <class T> const T &At(const void *object, std::size_t offset)
{
    return *reinterpret_cast<const T *>(static_cast<const std::byte *>(object) + offset);
}

const void *DialogMode()
{
    const CClientExoApp *app = CClientExoApp::Get();
    const GameModeController *modes = app != nullptr ? app->GameModes() : nullptr;
    return modes != nullptr && modes->Mode() == GameMode::Dialog ? modes->ModeObject(GameMode::Dialog) : nullptr;
}

std::mutex g_lock;
std::vector<std::function<bool(Conversation &, const Line &, bool)>> g_decide;
std::vector<std::function<void(Conversation &, const Line &)>> g_npcLines;
std::vector<std::function<void(Conversation &, std::span<const Line>)>> g_replies;
std::vector<std::function<void(Conversation &, const Line &)>> g_chosen;

template <class T> std::vector<T> Copy(const std::vector<T> &callbacks)
{
    const std::lock_guard lock(g_lock);
    return callbacks;
}

// Conversation::IsLineAvailable: bool __thiscall (line), `ret 4`.
using AvailableFn = bool(__fastcall *)(Conversation *, int, std::uint32_t);
Detour<AvailableFn> g_available;

bool __fastcall Available(Conversation *conversation, int edx, std::uint32_t index)
{
    bool available = g_available.Original()(conversation, edx, index);
    const auto decide = Copy(g_decide);
    if (conversation == nullptr || decide.empty())
    {
        return available;
    }
    const auto line = conversation->LineAt(index);
    if (!line)
    {
        return available;
    }
    for (const auto &callback : decide)
    {
        detail::Guard("a dialogue line's availability callback",
                      [&] { available = callback(*conversation, *line, available); });
    }
    return available;
}

// Conversation::PlayLine: bool __thiscall (line), `ret 4`. The observers
// hear of it before it is spoken.
using PlayLineFn = bool(__fastcall *)(Conversation *, int, std::uint32_t);
Detour<PlayLineFn> g_playLine;

bool __fastcall PlayLine(Conversation *conversation, int edx, std::uint32_t index)
{
    const auto observers = Copy(g_npcLines);
    if (conversation != nullptr && !observers.empty())
    {
        if (const auto line = conversation->LineAt(index); line && line->kind == Line::Kind::Npc)
        {
            for (const auto &observer : observers)
            {
                detail::Guard("a dialogue NPC line callback", [&] { observer(*conversation, *line); });
            }
        }
    }
    return g_playLine.Original()(conversation, edx, index);
}

// GameModeDialog::ShowResponses: __thiscall (const list *, bool), `ret 8`.
// The observers see the list once it is on the screen.
using ShowRepliesFn = void(__fastcall *)(void *, int, const void *, bool);
Detour<ShowRepliesFn> g_showReplies;

void __fastcall ShowReplies(void *mode, int edx, const void *list, bool flag)
{
    g_showReplies.Original()(mode, edx, list, flag);
    const auto observers = Copy(g_replies);
    if (mode == nullptr || observers.empty())
    {
        return;
    }
    auto *conversation = At<Conversation *>(mode, kConversation);
    if (conversation == nullptr)
    {
        return;
    }
    std::vector<Line> lines;
    for (const std::uint32_t index : At<core::VectorRep<std::uint32_t>>(mode, kReplies).View())
    {
        if (auto line = conversation->LineAt(index))
        {
            lines.push_back(std::move(*line));
        }
    }
    for (const auto &observer : observers)
    {
        detail::Guard("a dialogue replies callback", [&] { observer(*conversation, lines); });
    }
}

// Conversation::Advance: bool __thiscall (reply line, -1 for none), `ret 4`.
using AdvanceFn = bool(__fastcall *)(Conversation *, int, std::uint32_t);
Detour<AdvanceFn> g_advance;

bool __fastcall Advance(Conversation *conversation, int edx, std::uint32_t index)
{
    const auto observers = Copy(g_chosen);
    if (conversation != nullptr && index != kNone && !observers.empty())
    {
        if (const auto line = conversation->LineAt(index); line && line->kind == Line::Kind::Player)
        {
            for (const auto &observer : observers)
            {
                detail::Guard("a dialogue reply chosen callback", [&] { observer(*conversation, *line); });
            }
        }
    }
    return g_advance.Original()(conversation, edx, index);
}

void Attach()
{
    static std::once_flag once;
    std::call_once(once, [] {
        g_available.Attach(addr::kConversation_IsLineAvailable, &Available);
        g_playLine.Attach(addr::kConversation_PlayLine, &PlayLine);
        g_showReplies.Attach(addr::kGameModeDialog_ShowResponses, &ShowReplies);
        g_advance.Attach(addr::kConversation_Advance, &Advance);
    });
}

} // namespace

std::span<const std::byte> Conversation::File() const
{
    const auto *data = At<const std::byte *>(this, kFileData);
    const auto size = At<std::uint32_t>(this, kFileSize);
    return data != nullptr ? std::span<const std::byte>(data, size) : std::span<const std::byte>();
}

std::optional<Line> Conversation::LineAt(std::uint32_t index) const
{
    const auto file = core::ConversationFile::Parse(File());
    return file ? file->Line(index) : std::nullopt;
}

std::optional<std::uint32_t> Conversation::CurrentLine() const
{
    const auto line = At<std::uint32_t>(this, kCurrentLine);
    return line != kNone ? std::optional<std::uint32_t>(line) : std::nullopt;
}

ObjectId Conversation::Owner() const
{
    return At<ObjectId>(this, kOwner);
}

namespace
{

// LineSpeaker or LineListener: __thiscall (line), an object's id.
std::optional<ObjectId> Resolve(const Conversation &conversation, AddressId function, std::uint32_t line)
{
    if (!conversation.LineAt(line))
    {
        return std::nullopt;
    }
    using Fn = ObjectId(__fastcall *)(const Conversation *, int, std::uint32_t);
    return reinterpret_cast<Fn>(Module::Address(function))(&conversation, 0, line);
}

} // namespace

// Speaker and Listener, read 2026-10-03 (dao/Addresses.h, LineSpeaker), for a tool reading a
// conversation whose every line says OWNER (the scripts' GetLastSpeaker has no implementation in
// this build). The engine resolves a line's speaker and listener for the camera and the lip sync:
// OWNER the owner, PLAYER the player, HENCH:<tag> or HENCH<n> a party member, any other tag the
// creature by that tag about the owner. Where it resolves to nothing, the speaker is the owner,
// and the listener the other party: BeginConversation's oInitiator (the character the player
// controls when a creature was told to talk to itself), the conversation's +0x2C; the owner,
// BeginConversation's oTarget, is at +0x28. Seen in play 2026-10-03 through the Eluvian server, in
// Redcliffe: a militia man's one-line conversation, its lines OWNER, gave him for each; the
// player's reply, PLAYER, gave the hero. Owner itself, the listener and a speaker named by tag not
// yet seen.
std::optional<ObjectId> Conversation::Speaker(std::uint32_t line) const
{
    return Resolve(*this, addr::kConversation_LineSpeaker, line);
}

std::optional<ObjectId> Conversation::Listener(std::uint32_t line) const
{
    return Resolve(*this, addr::kConversation_LineListener, line);
}

Conversation *Current()
{
    const void *mode = DialogMode();
    return mode != nullptr ? At<Conversation *>(mode, kConversation) : nullptr;
}

// Choose and Continue, seen in play 2026-10-01 through the Eluvian server: Choose took a reply at
// Tamlen's conversation, OnReplyChosen following, and Continue went on through the reply with no
// text and the conversation ended. (That note also called InCutscene and SkipCutscene not yet
// seen; they were seen the same day, above InCutscene below.)
bool Choose(std::size_t index)
{
    auto *app = CClientExoApp::Get();
    auto *modes = app != nullptr ? app->GameModes() : nullptr;
    const void *mode = DialogMode();
    if (modes == nullptr || mode == nullptr ||
        index >= At<core::VectorRep<std::uint32_t>>(mode, kReplies).View().size())
    {
        return false;
    }
    auto message = messages::Make<messages::SelectResponseMessage>();
    message.index = static_cast<std::int32_t>(index);
    messages::Send(modes->ModeObject(GameMode::Dialog), message);
    return true;
}

bool Continue()
{
    auto *app = CClientExoApp::Get();
    auto *modes = app != nullptr ? app->GameModes() : nullptr;
    if (modes == nullptr || DialogMode() == nullptr)
    {
        return false;
    }
    messages::Send(modes->ModeObject(GameMode::Dialog), messages::Make<messages::SkipDialogMessage>());
    return true;
}

// What puts the game in cutscene mode, read 2026-10-01: the scripts' LoadCutscene (0x0066D230)
// finds the cutscene's resource and, unless a flag of the application's forbids, sets the game's
// mode to Cutscene at once with the cutscene loaded (0x00692150), then puts each replacement
// object under its tag. Its bPlayImmediately is popped and not used, and PlayCutscene (0x00651240)
// pops its object and does nothing: loading is playing. Much of the game's staging is
// conversations instead, which run in dialogue mode: the Dalish origin's mirror scene and the
// camp's awakening were dialogue throughout in play, though the module has cutscene files beside
// them.
//
// Seen in play, 2026-10-01, through the Eluvian server: LoadCutscene with
// L"bed110cs_tainted_mirror.cut" and two empty arrays put the game in cutscene mode within half a
// second, InCutscene true, the cutscene on the screen; with the name alone, no extension, nothing
// happened; and a cutscene of an area not loaded did nothing either way. SkipCutscene 3.5 s in
// answered true and the game was exploring again within half a second, the hero where she had
// stood. With no cutscene it answered false.
bool InCutscene()
{
    return CClientExoApp::CurrentMode() == GameMode::Cutscene;
}

bool SkipCutscene()
{
    auto *app = CClientExoApp::Get();
    auto *modes = app != nullptr ? app->GameModes() : nullptr;
    void *cutscene = modes != nullptr && InCutscene() ? modes->ModeObject(GameMode::Cutscene) : nullptr;
    if (cutscene == nullptr)
    {
        return false;
    }
    messages::Send(cutscene, messages::Make<messages::SkipCutsceneMessage>());
    return true;
}

// The scripts' PlayMovie (0x0065ECB0, a resource: a .bik of the game's movies folders) starts a
// pre-rendered video. Seen in play, 2026-10-01, through the Eluvian server: PlayMovie with
// L"bed100cs_duncan_finds_pc.bik" put the game in movie mode within half a second, InMovie true
// and InCutscene false; SkipMovie two seconds in answered true and the game was exploring again
// within half a second. With no movie it answered false.
bool InMovie()
{
    return CClientExoApp::CurrentMode() == GameMode::Movie;
}

bool SkipMovie()
{
    auto *app = CClientExoApp::Get();
    auto *modes = app != nullptr ? app->GameModes() : nullptr;
    void *movie = modes != nullptr && InMovie() ? modes->ModeObject(GameMode::Movie) : nullptr;
    if (movie == nullptr)
    {
        return false;
    }
    messages::Send(movie, messages::Make<messages::SkipMovieMessage>());
    return true;
}

std::vector<Line> Replies()
{
    std::vector<Line> lines;
    const void *mode = DialogMode();
    const Conversation *conversation = mode != nullptr ? At<Conversation *>(mode, kConversation) : nullptr;
    if (conversation == nullptr)
    {
        return lines;
    }
    for (const std::uint32_t index : At<core::VectorRep<std::uint32_t>>(mode, kReplies).View())
    {
        if (auto line = conversation->LineAt(index))
        {
            lines.push_back(std::move(*line));
        }
    }
    return lines;
}

void OnLineAvailable(std::function<bool(Conversation &, const Line &line, bool available)> decide)
{
    Attach();
    const std::lock_guard lock(g_lock);
    g_decide.push_back(std::move(decide));
}

void OnNpcLine(std::function<void(Conversation &, const Line &line)> observer)
{
    Attach();
    const std::lock_guard lock(g_lock);
    g_npcLines.push_back(std::move(observer));
}

void OnReplies(std::function<void(Conversation &, std::span<const Line> replies)> observer)
{
    Attach();
    const std::lock_guard lock(g_lock);
    g_replies.push_back(std::move(observer));
}

void OnReplyChosen(std::function<void(Conversation &, const Line &reply)> observer)
{
    Attach();
    const std::lock_guard lock(g_lock);
    g_chosen.push_back(std::move(observer));
}

} // namespace dao::dialogue
