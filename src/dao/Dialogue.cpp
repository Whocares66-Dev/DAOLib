#include "dao/Dialogue.h"

#include "dao/Addresses.h"
#include "dao/ClientExoApp.h"
#include "dao/GameModes.h"
#include "dao/Guard.h"
#include "dao/Hook.h"
#include "dao/Messages.h"
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

Conversation *Current()
{
    const void *mode = DialogMode();
    return mode != nullptr ? At<Conversation *>(mode, kConversation) : nullptr;
}

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
