#include "dao/Messages.h"

#include "dao/Hook.h"

#include <atomic>
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

bool __fastcall Dispatch(void *registry, int edx, void *receiver, const Message *message)
{
    const auto subscriptions = g_subscriptions.load(std::memory_order_acquire);
    if (message != nullptr && !subscriptions->empty())
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
