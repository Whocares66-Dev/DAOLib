#include "dao/Messages.h"

#include "dao/Addresses.h"
#include "dao/ClientExoApp.h"
#include "dao/GameModes.h"
#include "dao/Guard.h"
#include "dao/Hook.h"
#include "dao/Module.h"
#include "dao/String.h"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>

namespace dao::messages
{

namespace
{

// One subscription: a message class by its vftable (0 for every class),
// and an observer or an interceptor.
struct Subscription
{
    std::uintptr_t vftable = 0;
    MessageObserver observe;
    MessageInterceptor intercept;
};
using Subscriptions = std::vector<Subscription>;

// Dispatch runs for every message the engine delivers, a cursor's moves
// among them, so it reads a snapshot without a lock; adding copies it.
std::atomic<std::shared_ptr<const Subscriptions>> g_subscriptions{std::make_shared<const Subscriptions>()};
std::mutex g_adding;

// The registry's Dispatch (dao/Addresses.h): __thiscall (receiver,
// message), `ret 8`, true if a handler took the message.
using DispatchFn = bool(__fastcall *)(void *registry, int edx, void *receiver, const Message *message);
Detour<DispatchFn> g_dispatch;

// A message comes through Dispatch more than once. Its sender may offer it
// to receivers in turn until one has a handler, and a handler may pass it
// on: the game mode controller's handler for every game or GUI message
// (0x00640080, 0x006400E0) sends it to the GUI system, and if that does
// not take it, to the current mode, inside its own delivery. In play,
// 2026-09-30, a click came through four times, then three with only
// receivers that had a handler. So observers see a message once: at the
// first receiver with a handler, and not again in deliveries nested inside
// that one (the messages being delivered on this thread, outermost
// first).
thread_local std::vector<const Message *> t_delivering;

// Whether Dispatch will find this receiver a handler for this message, as
// it decides (0x004D8330, read 2026-09-30): the receiver's byte at +4 set,
// and Find (__thiscall (receiver's class, message's class), `ret 8`,
// walking both classes' parents) giving one. Each class is what the
// object's vtable slot 0 returns.
bool Handles(void *registry, const void *receiver, const Message *message)
{
    if (receiver == nullptr || *(static_cast<const std::uint8_t *>(receiver) + 4) == 0)
    {
        return false;
    }
    using ClassFn = const void *(__fastcall *)(const void *, int);
    const auto classOf = [](const void *object) { return (*static_cast<ClassFn *const *>(object))[0](object, 0); };
    using FindFn = const void *(__fastcall *)(void *, int, const void *, const void *);
    return reinterpret_cast<FindFn>(Module::Address(addr::kMessageRegistry_Find))(registry, 0, classOf(receiver),
                                                                                  classOf(message)) != nullptr;
}

bool Nested(const Message *message)
{
    return std::find(t_delivering.begin(), t_delivering.end(), message) != t_delivering.end();
}

bool __fastcall Dispatch(void *registry, int edx, void *receiver, const Message *message)
{
    const auto subscriptions = g_subscriptions.load(std::memory_order_acquire);
    if (message == nullptr || subscriptions->empty() || Nested(message) || !Handles(registry, receiver, message))
    {
        return g_dispatch.Original()(registry, edx, receiver, message);
    }
    const auto vftable = reinterpret_cast<std::uintptr_t>(message->vtable);
    bool taken = false;
    for (const auto &s : *subscriptions)
    {
        if (s.vftable != 0 && Module::Address(s.vftable) != vftable)
        {
            continue;
        }
        if (s.observe)
        {
            detail::Guard("a message observer", [&] { s.observe(receiver, *message); });
        }
        else if (!taken && s.intercept)
        {
            // A throw counts as not taking it.
            detail::Guard("a message interceptor", [&] { taken = s.intercept(receiver, *message); });
        }
    }
    if (taken)
    {
        return true;
    }
    t_delivering.push_back(message);
    const bool handled = g_dispatch.Original()(registry, edx, receiver, message);
    t_delivering.pop_back();
    return handled;
}

void Add(Subscription subscription)
{
    const std::lock_guard lock(g_adding);
    auto next = std::make_shared<Subscriptions>(*g_subscriptions.load());
    next->push_back(std::move(subscription));
    g_subscriptions.store(std::move(next), std::memory_order_release);
    if (g_dispatch.Original() == nullptr)
    {
        g_dispatch.Attach(addr::kMessageRegistry_Dispatch, &Dispatch);
    }
}

} // namespace

void OnAnyMessage(MessageObserver observer)
{
    Add({0, std::move(observer), {}});
}

void OnMessage(std::uintptr_t vftable, MessageObserver observer)
{
    Add({vftable, std::move(observer), {}});
}

void InterceptMessage(std::uintptr_t vftable, MessageInterceptor interceptor)
{
    Add({vftable, {}, std::move(interceptor)});
}

bool SendText(std::wstring_view text)
{
    // Messages::FromCallbackText's answer: who receives, and the message,
    // made on the engine's heap.
    struct Made
    {
        void *receiver = nullptr;
        Message *message = nullptr;
    };
    WString callback(text);
    Made made;
    using FromFn = Made *(__cdecl *)(Made *, const core::WStringRep *);
    reinterpret_cast<FromFn>(Module::Address(addr::kMessages_FromCallbackText))(&made, callback.Engine());
    if (made.message == nullptr)
    {
        return false;
    }
    if (made.receiver != nullptr)
    {
        Send(made.receiver, *made.message);
    }
    // As the movies' SendMessage lets its message go: the deleting
    // destructor, slot 4.
    using DeleteFn = void *(__fastcall *)(Message *, int, std::uint32_t);
    const auto *slots = *reinterpret_cast<DeleteFn **>(made.message);
    slots[4](made.message, 0, 1);
    return made.receiver != nullptr;
}

bool SendBinding(std::wstring_view text)
{
    CClientExoApp *app = CClientExoApp::Get();
    void *controller = app != nullptr ? app->GameModes() : nullptr;
    if (controller == nullptr)
    {
        return false;
    }
    WString binding(text);
    using FromFn = Message *(__cdecl *)(const core::WStringRep *);
    Message *message = reinterpret_cast<FromFn>(Module::Address(addr::kMessages_FromBindingText))(binding.Engine());
    if (message == nullptr)
    {
        return false;
    }
    Send(controller, *message);
    using DeleteFn = void *(__fastcall *)(Message *, int, std::uint32_t);
    const auto *slots = *reinterpret_cast<DeleteFn **>(message);
    slots[4](message, 0, 1);
    return true;
}

} // namespace dao::messages
