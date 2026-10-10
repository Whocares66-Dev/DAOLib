// Sending the engine's messages, and the detour on its dispatch that lets plugins watch and take
// them.
//
// A message class derives from Messages::GameMessage and Messages::Message (RTTI), polymorphic,
// with 11 virtual functions; slot 0 answers its type id, slot 1 clones it, and the clone copies
// exactly its fields. The layouts in the header are read from the clone and the handler that uses
// them; every class's size and field offsets are in dev/messages.csv (tools/messages.py), their
// meanings not yet. Send, inline in the header, calls the engine's own Messages::Send
// (dao/Addresses.h), as the engine sends its own.
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
    AddressId vftable{}; // the message class's, or none (0) for every message
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
    return std::ranges::find(t_delivering, message) != t_delivering.end();
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
        if (s.vftable != AddressId{} && Module::Address(s.vftable) != vftable)
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

// Read 2026-09-30: every delivery, sent or posted, goes through the registry's Dispatch
// (dao/Addresses.h), which only Send calls; posted messages are queued and sent later. This
// detours Dispatch. It runs for every message (the cursor's moves too), so observers must be
// quick; a class without a struct in the header is subscribed to by its vftable
// (dev/messages.csv), its name by dao::TypeName. Delivery is on the engine's thread that runs the
// game (the scripts'); not yet seen in play.
void OnAnyMessage(MessageObserver observer)
{
    Add({AddressId{}, std::move(observer), {}});
}

void OnMessage(AddressId vftable, MessageObserver observer)
{
    Add({vftable, std::move(observer), {}});
}

void InterceptMessage(AddressId vftable, MessageInterceptor interceptor)
{
    Add({vftable, {}, std::move(interceptor)});
}

// The engine makes the message and has it read its own fields from the text (dao/Addresses.h,
// "Messages from text"), as a movie's button does with its callback and the engine does with what
// a script or a data table gives it. Seen in play, 2026-10-01, through the Eluvian server:
// "GUIPopupLayer.ScriptDialogResult 1 1 -1", a box's own Yes, answered true and the box went;
// "Nobody.NoSuchThing 1" answered false.
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

// The engine makes the message from the binding's text, and it goes to the game mode controller,
// where a pressed key's goes. Seen in play, 2026-10-01, through the Eluvian server, in explore
// mode: "UserTogglePauseMessage" answered true and the game was paused, and again and it ran;
// "NoSuchMessage 1" answered false and nothing happened. With two in the party and the first
// selected and controlled, "SelectPartyMemberIndexMessage 1 1" selected both, the first still
// controlled, and "SelectPartyMemberIndexMessage 0 1" after it left the second selected alone and
// controlled: with its second field 1 the message adds to the selection or takes from it, as the
// key does with the add-to-selection key held. A second field of 0 was not tried.
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
