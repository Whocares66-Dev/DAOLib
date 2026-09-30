// The game's Flash UI (Scaleform GFx 3.x), as a plugin talks to it: the
// calls a movie's ActionScript makes into the game, FSCommands and
// ExternalInterface calls, seen, answered, or taken over by name.
//
//   dao::gui::OnExternalInterface("MyMod_GetTurn", [](const dao::gui::Call &call) {
//       call.Return(dao::gui::Value::Number(g_turn));
//   });
//   dao::gui::OnFSCommand("MyMod_EndTurn", [](void *owner, std::string_view args) { EndTurn(); });
//
// And a plugin calls into a movie the game shows:
//
//   dao::gui::Movie movie{dao::gui::MovieOf(owner)};
//   movie.SetVariable("_root.TurnBar._visible", dao::gui::Value::Bool(true));
//   const dao::gui::Value cards[] = {dao::gui::Value::Number(3)};
//   movie.Invoke("_root.TurnBar.SetCardCount", cards);
//
// In ActionScript, `ExternalInterface.call("MyMod_GetTurn")` and
// `fscommand("MyMod_EndTurn", "")` reach those, from any of the game's
// movies or a mod's own.
//
// Read 2026-09-30. The game gives GFx one FSCommand handler and one
// ExternalInterface handler (EclipseGFxFSCommandHandler and
// EclipseGFxExternalInterface, whose callbacks are their vtables' slot 1).
// Both pass the call to the object in 0x00C31264, most likely
// GUI::ScaleformManager (the example plugin logs its class), which asks
// the movie for its user data (GFxMovie's slot 43): the game's object
// that owns the movie, with the movie at its +0x1C. Then it looks the
// call's name up in a registry of handlers by name, at the manager's
// +0xA8, one map for FSCommands and one for ExternalInterface calls,
// keyed by the name's CRC-32. The game registers 67 FSCommands and 17
// ExternalInterface calls there (GetObjectNameByID, HasAbility,
// ListTacticTargets, SendMessage, GetValue, ...). This header detours the
// two lookups, so it sees every call with its name, and a plugin's
// handler for a name runs in place of the game's.
//
// Not yet seen in play: the example plugin logs the first calls, their
// owners and the thread they come on.
#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace dao::gui
{

// GFx's GFxValue, as ExternalInterface calls pass their arguments and
// take their answer: 16 bytes, the type at +0 and the value at +8 (the
// game's handlers read argument i at 16 * i + 8, and answer a number as
// type 3, the value a double at +8). Only the types a call passes by value
// are read here; objects, arrays and display objects are GFx's own.
struct Value
{
    enum class Type : std::uint32_t
    {
        Undefined = 0,
        Null = 1,
        Boolean = 2,
        Number = 3,
        String = 4,
        StringW = 5,
        Object = 6,
        Array = 7,
        DisplayObject = 8,
    };

    std::uint32_t type = 0;
    const void *objectInterface = nullptr; // not read yet; null for plain values
    union {
        double number;
        bool boolean;
        const char *string;
        const wchar_t *wstring;
    } value{};

    // GFx marks a value it manages with 0x40 beside the type.
    Type Kind() const
    {
        return static_cast<Type>(type & 0x0F);
    }
    std::optional<double> AsNumber() const
    {
        return Kind() == Type::Number ? std::optional{value.number} : std::nullopt;
    }
    std::optional<bool> AsBool() const
    {
        return Kind() == Type::Boolean ? std::optional{value.boolean} : std::nullopt;
    }
    std::optional<std::string_view> AsString() const
    {
        return Kind() == Type::String && value.string != nullptr ? std::optional<std::string_view>{value.string}
                                                                 : std::nullopt;
    }
    std::optional<std::wstring_view> AsWString() const
    {
        return Kind() == Type::StringW && value.wstring != nullptr ? std::optional<std::wstring_view>{value.wstring}
                                                                   : std::nullopt;
    }

    static Value Number(double n)
    {
        Value v;
        v.type = static_cast<std::uint32_t>(Type::Number);
        v.value.number = n;
        return v;
    }
    static Value Bool(bool b)
    {
        Value v;
        v.type = static_cast<std::uint32_t>(Type::Boolean);
        v.value.boolean = b;
        return v;
    }
    // The text must outlive the call it is passed to; GFx copies it there.
    static Value String(const char *text)
    {
        Value v;
        v.type = static_cast<std::uint32_t>(Type::String);
        v.value.string = text;
        return v;
    }
    static Value WString(const wchar_t *text)
    {
        Value v;
        v.type = static_cast<std::uint32_t>(Type::StringW);
        v.value.wstring = text;
        return v;
    }
};
static_assert(sizeof(Value) == 16);

// A Flash movie: GFx's GFxMovieView (its class GFxMovieRoot), as the game's
// UI objects hold it (MovieOf). Its vtable, read 2026-09-30 from the
// game's own calls (the paths it passes, _root.OriginalStageWidth,
// _root.fsc_retval, the floaty layer's batch updates), is GFx 3.0's
// GFxMovie in order, and each slot's `ret` agrees: 9 GetVisible, 10
// IsAvailable(path), 11 SetVariable(path, value, setType), 12
// GetVariable(out, path), 13 SetVariableArray(type, path, index, data,
// count, setType), 14 SetVariableArraySize(path, count, setType), 15
// GetVariableArraySize(path), 16 GetVariableArray(type, path, index, data,
// count), 17 the variadic Invoke (__cdecl), 18 Invoke(method, result,
// args, count), 19 InvokeArgs, 42 SetExternalInterfaceRetVal, 43
// GetUserData. No slots for GFx 3.1's CreateString and CreateObject sit
// before SetVariable, so this is the 3.0 interface.
//
// Not yet seen in play. Call it on the thread the movie's own calls come
// on (an FSCommand or ExternalInterface handler runs there).
class Movie
{
  public:
    explicit Movie(void *view) : view_(view)
    {
    }
    explicit operator bool() const
    {
        return view_ != nullptr;
    }
    void *View() const
    {
        return view_;
    }

    // How a set variable holds (GFx's SetVarType). The game sets with
    // Sticky: kept for a clip not loaded yet, and set on it when it is.
    enum class SetType : std::int32_t
    {
        Normal = 0,
        Sticky = 1,
        Permanent = 2,
    };

    // Whether a path names something in the movie now.
    bool IsAvailable(const char *path) const;
    bool SetVariable(const char *path, const Value &value, SetType type = SetType::Sticky);
    // An ActionScript array at a path, from index on, as the game fills
    // _root.fsc_retval and the floaty layer's batch updates (GFx's
    // SetArrayType: 0 int, 1 double, 4 wide string, 5 value).
    bool SetArray(const char *path, std::span<const std::int32_t> values, std::uint32_t index = 0,
                  SetType type = SetType::Sticky);
    bool SetArray(const char *path, std::span<const double> values, std::uint32_t index = 0,
                  SetType type = SetType::Sticky);
    bool SetArray(const char *path, std::span<const wchar_t *const> values, std::uint32_t index = 0,
                  SetType type = SetType::Sticky);
    bool SetArray(const char *path, std::span<const Value> values, std::uint32_t index = 0,
                  SetType type = SetType::Sticky);
    bool SetArraySize(const char *path, std::uint32_t count, SetType type = SetType::Sticky);
    std::uint32_t ArraySize(const char *path) const;
    // Calls an ActionScript function by its path; true if it ran. What it
    // returns is not read: GFx hands strings and objects out as references
    // of its own to release.
    bool Invoke(const char *method, std::span<const Value> args = {});

  private:
    void *view_;
};

// An ExternalInterface call as a handler sees it.
struct Call
{
    // The game's object that owns the calling movie (the movie's user
    // data); its class names the screen (dao::TypeName).
    void *owner = nullptr;
    std::string_view name;
    std::span<const Value> args;

    // Answers the call: what ExternalInterface.call returns to the movie
    // (the movie's slot 42, as the game's handlers answer).
    void Return(const Value &answer) const;
};

using FSCommandHandler = std::function<void(void *owner, std::string_view args)>;
using ExternalInterfaceHandler = std::function<void(const Call &call)>;

// A handler for an FSCommand or ExternalInterface call by name, run in
// place of the game's own handler for that name if it has one. Names are
// case-sensitive, as the game's are. At plugin load or later; the first
// call detours the engine's lookup.
void OnFSCommand(std::string name, FSCommandHandler handler);
void OnExternalInterface(std::string name, ExternalInterfaceHandler handler);

// Every FSCommand or ExternalInterface call, before it is handled. These
// observe; the call goes on to its handler.
void OnAnyFSCommand(std::function<void(void *owner, std::string_view name, std::string_view args)> observer);
void OnAnyExternalInterface(ExternalInterfaceHandler observer);

// The movie a UI object owns (+0x1C), GFx's GFxMovieView; null for none.
void *MovieOf(const void *owner);

// An FSCommand's answer, as the game's own handlers give theirs: an array
// of ints in the calling movie's _root.fsc_retval, which the ActionScript
// reads after its fscommand().
bool ReturnFSCommand(void *owner, std::span<const std::int32_t> values);

} // namespace dao::gui
