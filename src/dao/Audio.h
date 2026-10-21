// Tells a plugin when the game starts an FMOD event: every sound effect, voice line, ambience
// and music cue the game plays through its event system.
//
//   dao::audio::OnEventStart([](const dao::audio::EventStart &sound) {
//       spdlog::info("{} at {} {} {}", sound.path, sound.position[0], sound.position[1], sound.position[2]);
//   });
//
// The creature behind a sound is not given: the event has no owner, only a place.
#pragma once

#include <chrono>
#include <functional>
#include <string_view>

namespace dao::audio
{

// One event started, as reported to a callback.
struct EventStart
{
    // The event's full path, "project/group/.../event" as the scripts' PlaySound names it. Valid
    // only during the callback. Empty if FMOD gave no name.
    std::string_view path;
    // Just the event's own name, the last part of `path`. Valid only during the callback.
    std::string_view name;
    // FMOD's FMOD::Event*, to pass to the fmod_event.dll calls. Valid only during the callback:
    // the game may release the instance once it ends.
    void *event = nullptr;
    // Where the event was last placed, in the engine's world coordinates. Read only for a 3-D
    // event (FMOD_3D in its mode); otherwise all zero with `positional` false (music, interface
    // sounds). A 3-D event nobody has placed yet reports its default, usually the origin.
    float position[3] = {0.0F, 0.0F, 0.0F};
    bool positional = false;
    // When the start was seen, on the steady clock.
    std::chrono::steady_clock::time_point when;
};

// Registers a callback for every event the game starts, called once FMOD has started it.
// @param observer receives the event's path, place and time
//
// Hooks fmod_event.dll's `FMOD::Event::start`, so a start by the game through that export is
// seen, and one that skips it is not. Works in any build, since it needs no address. The
// observer runs on the thread that started the event, which is the game's, so keep it short.
// Events the game starts for its sound objects while an area loads are reported too. A start
// FMOD refused is not reported.
void OnEventStart(std::function<void(const EventStart &sound)> observer);

} // namespace dao::audio
