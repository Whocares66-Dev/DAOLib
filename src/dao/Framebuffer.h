// The framebuffer's effects: what the game does to the whole picture after the world is drawn
// and before the interface is, such as its colour grade (`da_HSVMatrix`) and its bloom
// (`da_bloom`). Each effect has a name, is on or off, and has named values. The setting is the
// script commands FB_SetEffectEnabled and FB_SetEffectResource; the reading is what they lack,
// so a plugin can put back what it changed.
//
//   namespace fb = dao::framebuffer;
//   const auto was = fb::Value(L"da_HSVMatrix", L"Brightness");
//   fb::SetValue(L"da_HSVMatrix", L"Brightness", 0.5F);
//   ...
//   if (was) fb::SetValue(L"da_HSVMatrix", L"Brightness", *was);
#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace dao::framebuffer
{

// Returns the names of the effects the game has.
// @return an empty vector before the renderer has made them
//
// Call it on the game thread, as with all of these.
std::vector<std::wstring> Effects();

// Returns whether an effect is on.
// @return nullopt if the game has no effect of that name
std::optional<bool> Enabled(std::wstring_view effect);

// Returns one of an effect's values.
// @param value its name, as FB_SetEffectResource takes it: `Brightness`, `Hue`
// @return nullopt if there is no such effect or value, or the value is not a float
std::optional<float> Value(std::wstring_view effect, std::wstring_view value);

// Turns an effect on or off. It does nothing if the game has no effect of that name.
void SetEnabled(std::wstring_view effect, bool on);

// Sets one of an effect's values. It does nothing if there is no such effect or value, or the
// value is not a float.
void SetValue(std::wstring_view effect, std::wstring_view value, float to);

} // namespace dao::framebuffer
