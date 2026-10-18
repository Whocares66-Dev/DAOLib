// The party's selection and controlled creature (dao/Party.h).
//
// Read 2026-09-30. The party object is the application's +0xD0's +0x58
// (0x005D57C0); Party::Select (0x00639E50) adds a member to its selection,
// a std::vector of ids at +0x184 (0x006368F0 adds one, and leaves a lone
// selection of that same member alone), in the order selected. The
// primary click's attack (0x00712830) orders the members 0x006360C0
// lists: this selection itself (its pointer mirror at +0x194), less the
// members whose effect flags meet a mask (corrected 2026-10-18 by
// lib-agent-2; not a list of the party's own, dev/PARTY.md).
#include "dao/Party.h"

#include "dao/ClientExoApp.h"
#include "dao/Frame.h"
#include "dao/Guard.h"
#include "dao/SafeRead.h"
#include "dao/core/Msvc8.h"
#include "dao/generated/ScriptApi.h"

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <utility>

namespace dao::party
{

namespace
{
constexpr std::size_t kPartyHolder = 0xD0; // in the application
constexpr std::size_t kParty = 0x58;       // in what it holds
constexpr std::size_t kSelection = 0x184;  // in the party: a std::vector of ids

std::mutex g_lock;
std::vector<std::function<void(ObjectId, ObjectId)>> g_controlObservers;
ObjectId g_controlled = kInvalidObject; // as the last frame found it

void CompareControlled()
{
    const ObjectId now = Controlled();
    if (now == g_controlled)
    {
        return;
    }
    const ObjectId before = std::exchange(g_controlled, now);
    std::vector<std::function<void(ObjectId, ObjectId)>> observers;
    {
        const std::lock_guard lock(g_lock);
        observers = g_controlObservers;
    }
    for (const auto &observer : observers)
    {
        detail::Guard("a control change callback", [&] { observer(before, now); });
    }
}
} // namespace

std::uintptr_t EngineObject()
{
    const CClientExoApp *app = CClientExoApp::Get();
    const auto holder =
        app != nullptr ? TryRead<std::uintptr_t>(reinterpret_cast<std::uintptr_t>(app) + kPartyHolder) : std::nullopt;
    const auto party = holder && *holder != 0 ? TryRead<std::uintptr_t>(*holder + kParty) : std::nullopt;
    return party.value_or(0);
}

// The selection vector at the party's +0x184 (2026-09-30). Not yet seen in play.
std::vector<ObjectId> Selected()
{
    const std::uintptr_t party = EngineObject();
    if (party == 0)
    {
        return {};
    }
    const auto selection = TryRead<core::VectorRep<ObjectId>>(party + kSelection);
    if (!selection)
    {
        return {};
    }
    const auto ids = selection->View();
    if (ids.size() > 64)
    {
        return {}; // not a party's selection
    }
    return {ids.begin(), ids.end()};
}

// The controlled creature is the id at the application's +0xD0's +0x5C (0x005D5CA0), beside the
// party at +0x58; the scripts' GetMainControlled reads it. Read 2026-10-04 (DAOLib #22). Seen in
// play the same day: changes by an Eluvian order, a move order handing control over and the party
// bar's key, each both ways, and at a load from nobody to the hero.
ObjectId Controlled()
{
    // GetMainControlled asks only once the party is there.
    if (EngineObject() == 0)
    {
        return kInvalidObject;
    }
    return script::api::GetMainControlled();
}

// Many paths set the controlled creature: the party bar, a key, the scripts'
// SetPrimaryControlled, a selection by CenterOnObjectMessage, a plugin's order. So it is
// compared each frame rather than caught at one setter.
void OnControlChanged(std::function<void(ObjectId before, ObjectId now)> observer)
{
    static std::once_flag once;
    std::call_once(once, [] { OnFrame(&CompareControlled); });
    const std::lock_guard lock(g_lock);
    g_controlObservers.push_back(std::move(observer));
}

} // namespace dao::party
