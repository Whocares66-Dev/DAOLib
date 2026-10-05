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

std::vector<ObjectId> Selected()
{
    const CClientExoApp *app = CClientExoApp::Get();
    if (app == nullptr)
    {
        return {};
    }
    const auto holder = TryRead<std::uintptr_t>(reinterpret_cast<std::uintptr_t>(app) + kPartyHolder);
    const auto party = holder && *holder != 0 ? TryRead<std::uintptr_t>(*holder + kParty) : std::nullopt;
    if (!party || *party == 0)
    {
        return {};
    }
    const auto selection = TryRead<core::VectorRep<ObjectId>>(*party + kSelection);
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

ObjectId Controlled()
{
    const CClientExoApp *app = CClientExoApp::Get();
    const auto holder =
        app != nullptr ? TryRead<std::uintptr_t>(reinterpret_cast<std::uintptr_t>(app) + kPartyHolder) : std::nullopt;
    const auto party = holder && *holder != 0 ? TryRead<std::uintptr_t>(*holder + kParty) : std::nullopt;
    // GetMainControlled asks only once the party is there.
    if (!party || *party == 0)
    {
        return kInvalidObject;
    }
    return script::api::GetMainControlled();
}

void OnControlChanged(std::function<void(ObjectId before, ObjectId now)> observer)
{
    static std::once_flag once;
    std::call_once(once, [] { OnFrame(&CompareControlled); });
    const std::lock_guard lock(g_lock);
    g_controlObservers.push_back(std::move(observer));
}

} // namespace dao::party
