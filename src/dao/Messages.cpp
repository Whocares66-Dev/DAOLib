#include "dao/Messages.h"

#include "dao/Addresses.h"
#include "dao/Hook.h"
#include "dao/Module.h"

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

// Whether Dispatch will find this receiver a handler for this message, as
// it decides (0x004D8330, read 2026-09-30): the receiver's byte at +4 set,
// and Find (__thiscall (receiver's class, message's class), `ret 8`,
// walking both classes' parents) giving one. Each class is what the
// object's vtable slot 0 returns. The engine delivers one message to
// several receivers, and only one has a handler: in play, 2026-09-30, a
// click's message came through Dispatch four times and its handler ran
// once.
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

bool __fastcall Dispatch(void *registry, int edx, void *receiver, const Message *message)
{
    const auto subscriptions = g_subscriptions.load(std::memory_order_acquire);
    if (message != nullptr && !subscriptions->empty() && Handles(registry, receiver, message))
    {
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
                s.observe(receiver, *message);
            }
            else if (!taken && s.intercept && s.intercept(receiver, *message))
            {
                taken = true;
            }
        }
        if (taken)
        {
            return true;
        }
    }
    return g_dispatch.Original()(registry, edx, receiver, message);
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

} // namespace dao::messages
