// The party picker's choices (dao/PartyPicker.h).
//
// Notes on the party picker: read 2026-10-24 (DAOLib #259) in the GOG exe and partypicker.gfx,
// with dev/PARTY.md's reading (lib-agent-2, 2026-10-21) and the Eluvian server's party tool,
// which chose by sweeping the cursor over the stage until the name under it matched and pressing
// the binding (its src/core/Party.cpp). The picker is GameModePartyPicker, the mode object of
// GameMode 12; +0xA0 is set while it is showing and +0xA1 while it is leaving, +0xA2 (16 bits)
// counts the chosen with the hero and the locked, and the entries are a vector at +0x8C/+0x90 of
// 0x34-byte records: +0 the creature, +8 its state (FOLLOWER_STATE_*), +0x10 the hero's byte,
// +0x24 its slot among SelectedPartyMember's four or -1. A click is
// HandleMessage_SelectPartyMemberUnderCursorMessage (0x00768AE0), which refuses while showing or
// leaving, unless the party's picker status (party +0x74, GetPartyPickerGUIStatus) is USE (2),
// while the cursor is over a movie (the GUI system's slot 71, ScaleformManager::MouseOverInterface,
// 6593: every mode's click tests it so a click on the interface does not land in the world too; it
// routes the click and is no rule of the picker's, so a choice with no cursor leaves it out), and
// for the hero; then
// state 1 lets go (0x007661B0) and state 2 chooses (SelectEntry, 0x00766040), any other neither.
// SelectEntry itself checks only that fewer than four are chosen: it would force any entry to 1,
// so the click's checks are made here first. It sets the state, plays the figure's AddAnimation
// and its sound, puts the id in the first free slot and counts it, and redraws the entry. A
// portrait's click is "GameModeController.UnselectPartyMember <slot>" (0x00767100), which makes
// the click's checks itself for a slot (the status, not the hero, state 1) and plays the error
// sound otherwise; Remove All is ClearPartyPicker (0x00767230), that for each slot, disabled by
// the movie unless the status is USE; Accept is SubmitPartyChanges (0x00767060), refused only
// while showing or leaving, which raises the follower events 67 and 66 and PARTYPICKER_CLOSED
// (71) through the AI's queue and leaves. The party itself changes in the module's scripts after
// (module_core.nss), so it lags the picker's close; nothing changes the party before Accept.
//
// Seen in play 2026-10-24 (examples/partypicker), Lyna at Redcliffe with the picker opened by
// SetPartyPickerGUIStatus(USE) and ShowPartyPickerGUI: ten entries, the hero and three chosen in
// slots 0 to 3, six available; with four chosen, Choose was refused PartyFull; Alistair let go
// (state 2, slot -1) and again AlreadySo; Dog chosen into slot 1, and again AlreadySo; the hero
// refused Locked both ways; Accept closed it, and the party was then Lyna, Dog, Leliana and Wynne.
#include "dao/PartyPicker.h"

#include "dao/Addresses.h"
#include "dao/ClientExoApp.h"
#include "dao/GameModes.h"
#include "dao/Messages.h"
#include "dao/Module.h"
#include "dao/SafeRead.h"
#include "dao/generated/ScriptApi.h"

#include <format>

namespace dao::partypicker
{

namespace
{

namespace api = script::api;

constexpr std::size_t kShowing = 0xA0;
constexpr std::size_t kLeaving = 0xA1;
constexpr std::size_t kChosenCount = 0xA2;
constexpr std::size_t kEntriesBegin = 0x8C;
constexpr std::size_t kEntriesEnd = 0x90;
constexpr std::size_t kEntrySize = 0x34;
constexpr std::size_t kEntryState = 0x8;
constexpr std::size_t kEntryHero = 0x10;
constexpr std::size_t kEntrySlot = 0x24;
constexpr std::size_t kMostEntries = 64;
constexpr std::int16_t kMostChosen = 4;

// The picker, if it is the current mode and neither showing nor leaving.
std::uintptr_t Picker()
{
    const CClientExoApp *app = CClientExoApp::Get();
    const GameModeController *modes = app != nullptr ? app->GameModes() : nullptr;
    if (modes == nullptr || modes->Mode() != GameMode::PartyPicker)
    {
        return 0;
    }
    const auto picker = reinterpret_cast<std::uintptr_t>(modes->ModeObject(GameMode::PartyPicker));
    if (picker == 0 || TryRead<std::uint8_t>(picker + kShowing).value_or(1) != 0 ||
        TryRead<std::uint8_t>(picker + kLeaving).value_or(1) != 0)
    {
        return 0;
    }
    return picker;
}

// The entry of a creature in the picker's list, or 0.
std::uintptr_t EntryOf(std::uintptr_t picker, ObjectId creature)
{
    const auto begin = TryRead<std::uintptr_t>(picker + kEntriesBegin).value_or(0);
    const auto end = TryRead<std::uintptr_t>(picker + kEntriesEnd).value_or(0);
    for (std::uintptr_t at = begin; begin != 0 && at < end && at < begin + (kMostEntries * kEntrySize);
         at += kEntrySize)
    {
        if (TryRead<ObjectId>(at).value_or(kInvalidObject) == creature)
        {
            return at;
        }
    }
    return 0;
}

Entry Read(std::uintptr_t entry)
{
    return {TryRead<ObjectId>(entry).value_or(kInvalidObject),
            static_cast<State>(TryRead<std::int32_t>(entry + kEntryState).value_or(0)),
            TryRead<std::int32_t>(entry + kEntrySlot).value_or(-1),
            TryRead<std::uint8_t>(entry + kEntryHero).value_or(0) != 0};
}

bool ForUse()
{
    return api::GetPartyPickerGUIStatus() == script::constants::PP_GUI_STATUS_USE;
}

} // namespace

std::vector<Entry> Entries()
{
    std::vector<Entry> entries;
    const std::uintptr_t picker = Picker();
    if (picker == 0)
    {
        return entries;
    }
    const auto begin = TryRead<std::uintptr_t>(picker + kEntriesBegin).value_or(0);
    const auto end = TryRead<std::uintptr_t>(picker + kEntriesEnd).value_or(0);
    for (std::uintptr_t at = begin; begin != 0 && at < end && entries.size() < kMostEntries; at += kEntrySize)
    {
        entries.push_back(Read(at));
    }
    return entries;
}

Refusal Choose(ObjectId creature)
{
    const std::uintptr_t picker = Picker();
    if (picker == 0)
    {
        return Refusal::NotShown;
    }
    if (!ForUse())
    {
        return Refusal::ReadOnly;
    }
    const std::uintptr_t entry = EntryOf(picker, creature);
    if (entry == 0)
    {
        return Refusal::NotOffered;
    }
    const Entry read = Read(entry);
    if (read.hero)
    {
        return Refusal::Locked;
    }
    if (read.state == State::Chosen)
    {
        return Refusal::AlreadySo;
    }
    if (read.state != State::Available)
    {
        return Refusal::Locked;
    }
    if (TryRead<std::int16_t>(picker + kChosenCount).value_or(kMostChosen) >= kMostChosen)
    {
        return Refusal::PartyFull;
    }
    using SelectFn = void(__fastcall *)(std::uintptr_t picker, int edx, std::uintptr_t entry);
    reinterpret_cast<SelectFn>(Module::Address(addr::kGameModePartyPicker_SelectEntry))(picker, 0, entry);
    return Refusal::None;
}

Refusal Release(ObjectId creature)
{
    const std::uintptr_t picker = Picker();
    if (picker == 0)
    {
        return Refusal::NotShown;
    }
    if (!ForUse())
    {
        return Refusal::ReadOnly;
    }
    const std::uintptr_t entry = EntryOf(picker, creature);
    if (entry == 0)
    {
        return Refusal::NotOffered;
    }
    const Entry read = Read(entry);
    if (read.hero || read.state == State::LockedActive)
    {
        return Refusal::Locked;
    }
    if (read.state != State::Chosen || read.slot < 0)
    {
        return Refusal::AlreadySo;
    }
    messages::SendText(std::format(L"GameModeController.UnselectPartyMember {}", read.slot));
    return Refusal::None;
}

Refusal ReleaseAll()
{
    if (Picker() == 0)
    {
        return Refusal::NotShown;
    }
    if (!ForUse())
    {
        return Refusal::ReadOnly;
    }
    messages::SendText(L"GameModeController.ClearPartyPicker");
    return Refusal::None;
}

Refusal Accept()
{
    if (Picker() == 0)
    {
        return Refusal::NotShown;
    }
    messages::SendText(L"GameModeController.SubmitPartyChanges");
    return Refusal::None;
}

} // namespace dao::partypicker
