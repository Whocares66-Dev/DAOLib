// An audio event project (.fev, FMOD Designer 4.x) read, over bytes (DAOLib
// #192): the plain description a reimplementation of the game's sound plays
// from (DAOParagonEngine #30). The wave banks it plays from, the category
// tree with its volumes, the event groups and their events with every
// property, layer, parameter and envelope, the sound definitions with each
// entry's bank and sample index, the user properties, and, in
// master_reverb.fev, the reverb presets. dev/AUDIO.md has the reading.
//
//   const auto project = dao::core::fev::ProjectOf(bytes);
//   const auto *line = project->FindEvent("bhm100_jowan/VO/36641_m");
//   line->parameters[0].name;                    // "#music:bhm_mage_end"
//   fev::DirectiveOf(line->parameters[0].name);  // {"music", {"bhm_mage_end"}}
//
// The format is FEV1, version 0x003A0000. No spec exists; xoreos' loader
// (src/sound/fmodeventfile.cpp, GPL) has the header, banks, categories,
// groups and an event's fixed properties right and stops at layers. The
// layout here was worked out from the bytes by lib-agent-2 on 2026-10-18 over
// all 9,020 of the install's projects (core, campaign, add-ins, every
// language), every one read to its end. A string is a u32 length counting
// the NUL, then the bytes; numbers are little-endian. What is not understood
// is kept as the file has it, in members named unknown*.
//
// Where the file puts what:
//   header     "FEV1", u32 version, two unknown u32, the project's name,
//              the banks, the category tree (a root, "master"), the groups
//   groups     name, user properties, u32 subgroup count, u32 event count,
//              the subgroups, the events
//   after      the sound-definition templates, the sound definitions, the
//              reverb presets (a count, 0 in all but master_reverb.fev), and
//              the rest, 8 bytes in 9,019 of the 9,020 ("comp" with a length)
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace dao::core::fev
{

// A wave bank the project's sound definitions name.
struct Bank
{
    std::uint32_t flags = 0;   // 0x80 stream from disk, 0x200 load into memory
    std::uint32_t unknown = 0; // 1, 2 or 4
    std::string name;          // the .fsb without its extension
};

// A node of the category tree: a bus whose volume events in it are mixed under.
struct Category
{
    std::string name;
    float volume = 1.0F; // linear
    float pitch = 0.0F;
    std::array<std::uint32_t, 2> unknown{};
    std::vector<Category> children;

    // The node of that name in this subtree, this one included; null if none.
    const Category *Find(std::string_view node) const;
};

// A user property of a group or an event: a name and a value of one of three types.
struct Property
{
    std::string name;
    std::uint32_t type = 0; // 0 integer, 1 float, 2 string
    std::int32_t integer = 0;
    float real = 0.0F;
    std::string text;
};

// A sound definition played in a layer: from where of the controlling parameter's range to where.
struct Instance
{
    std::uint16_t soundDefinition = 0; // index into Project::soundDefinitions
    float start = 0.0F;                // fractions of the parameter's range: min + start * (max - min)
    float length = 0.0F;
    std::uint32_t loopMode = 0; // 0 or 1
    std::uint32_t unknown1 = 0; // 0, 1 or 2
    std::int32_t loopCount = 0; // -1 for no limit
    std::array<std::uint32_t, 4> unknown2{};
    std::array<float, 3> unknown3{}; // 1, -1, -1
    std::uint32_t fadeInCurve = 0;
    std::uint32_t fadeOutCurve = 0;
};

struct EnvelopePoint
{
    float x = 0.0F;
    float y = 0.0F;
    std::uint32_t curve = 0;
};

// A curve over a parameter that drives a DSP effect, or the event's volume or pitch when it names none.
struct Envelope
{
    std::int32_t parameter = -1; // index into the event's parameters; -1 for none
    std::string dsp;             // "", "FMOD Lowpass Simple", "FMOD Echo"
    std::uint32_t unknown1 = 0;
    std::uint32_t flags = 0; // 0x204, 0x404, 0xC, 0x2004 ...
    std::uint32_t unknown2 = 0;
    std::vector<EnvelopePoint> points;
    std::array<std::uint32_t, 2> unknown3{};
};

// A layer of a complex event: sounds laid along a parameter, and the envelopes over it.
struct Layer
{
    std::uint16_t flags = 0;
    std::int16_t priority = 0;
    std::int16_t parameter = -1; // the controlling parameter, an index into the event's; -1 for none
    std::vector<Instance> instances;
    std::vector<Envelope> envelopes;
};

// An event's parameter. Its name carries the game's own orders when it starts with '#': see DirectiveOf.
struct Parameter
{
    std::string name;
    float velocity = 0.0F;
    float min = 0.0F;
    float max = 0.0F;
    std::uint32_t flags = 0; // 3 on the first, 2 on the rest
    float seekSpeed = 0.0F;
    std::array<std::uint32_t, 2> unknown{};
};

// An event: something the game starts by its path.
struct Event
{
    bool simple = false; // type 16: one instance and no layer header, parameters or user properties; else 8
    std::string name;
    std::array<std::uint8_t, 16> guid{};
    float volume = 1.0F; // linear
    float pitch = 0.0F;
    float pitchRandomization = 0.0F;
    float volumeRandomization = 0.0F;
    std::uint32_t unknown16 = 0;     // 41 in nearly every event, 128, 20 or 0 in some
    std::uint32_t maxPlaybacks = 0;  // 1 mostly
    std::uint32_t unknown24 = 0;     // 10000
    std::uint32_t mode = 0;          // FMOD_MODE bits; 0x00180008 on every music event
    float minDistance = 0.0F;        // 3-D, metres
    float maxDistance = 0.0F;        //
    std::uint32_t flags2 = 0;        // 0x00080000 on every simple event and the ducking events, 0 on looping music
    std::array<float, 8> speakers{}; // left, right, centre, LFE, left rear, right rear, left side, right side
    float coneInside = 0.0F;         // degrees
    float coneOutside = 0.0F;        //
    float coneOutsideVolume = 0.0F;  //
    std::uint32_t maxPlaybacksBehaviour = 0;
    float doppler = 0.0F;
    float reverbDry = 0.0F;
    float reverbWet = 0.0F;
    float spread = 0.0F;
    std::uint32_t fadeInMs = 0;
    std::uint32_t fadeOutMs = 0;
    float spawnIntensity = 0.0F;
    float spawnIntensityRandomization = 0.0F;
    float panLevel = 0.0F; // 3-D
    std::uint32_t positionRandomization = 0;
    std::vector<Layer> layers; // a simple event has one, with one instance, start 0 and length 1
    std::vector<Parameter> parameters;
    std::vector<Property> properties;
    std::uint32_t unknownTail = 0; // 1
    std::string category;          // the category it is mixed under, by name

    // The user property of that name; null if none.
    const Property *FindProperty(std::string_view propertyName) const;
};

// An event group: a folder of events, and of groups, in a path.
struct Group
{
    std::string name;
    std::vector<Property> properties;
    std::vector<Group> groups;
    std::vector<Event> events;
};

// A set of sound-definition properties, which definitions name by index (xoreos' layout).
struct Template
{
    std::uint32_t playMode =
        0; // 0 sequential, 1 random, 2 random no repeat, 3 sequential no repeat, 4 shuffle, 5 programmer
    std::uint32_t spawnMin = 0;
    std::uint32_t spawnMax = 0;
    std::uint32_t maxSpawned = 0;
    float volume = 0.0F;
    std::array<std::uint32_t, 3> unknown1{};
    float volumeRandomization = 0.0F;
    float pitch = 0.0F;
    std::array<std::uint32_t, 3> unknown2{};
    float pitchRandomization = 0.0F;
    float positionRandomization = 0.0F; // 3-D
};

// A sound definition's entry: a wave of a bank, an oscillator or a silence.
struct Entry
{
    std::uint32_t type = 0; // 0 wave, 1 oscillator, 2 silence
    std::uint32_t weight = 0;
    std::string file;              // wave: "ingame_music/DA_m_fade_a1.wav"
    std::string bank;              // wave: the bank's name, one of Project::banks
    std::uint32_t sampleIndex = 0; // wave: the sample's index in the bank's .fsb
    std::uint32_t lengthMs = 0;    // wave
    std::uint32_t oscillator = 0;  // oscillator: the shape
    float frequency = 0.0F;        // oscillator
};

struct SoundDefinition
{
    std::string name; // "/music_ingame/DA_music_fade_a"
    std::uint32_t templateIndex = 0;
    std::vector<Entry> entries;
};

// A reverb preset of master_reverb.fev, in xoreos' layout, which the bytes agree with.
struct Reverb
{
    std::string name;
    std::int32_t room = 0; // mB
    std::int32_t roomHF = 0;
    float roomRolloff = 0.0F;
    float decayTime = 0.0F;
    float decayHFRatio = 0.0F;
    std::int32_t reflections = 0;
    float reflectDelay = 0.0F;
    std::int32_t reverb = 0;
    float reverbDelay = 0.0F;
    float diffusion = 0.0F;
    float density = 0.0F;
    float hfReference = 0.0F;
    std::int32_t roomLF = 0;
    float lfReference = 0.0F;
    std::array<std::uint32_t, 19> unknown{}; // the 76 bytes xoreos skips
};

// An event with its path.
struct EventPath
{
    std::string path; // "bhm100_jowan/VO/36641_m"
    const Event *event = nullptr;
};

struct Project
{
    std::uint32_t version = 0; // 0x003A0000
    std::array<std::uint32_t, 2> unknown{};
    std::string name;
    std::vector<Bank> banks;
    Category categories; // the root, "master"
    std::vector<Group> groups;
    std::vector<Template> templates;
    std::vector<SoundDefinition> soundDefinitions;
    std::vector<Reverb> reverbs;
    std::vector<std::byte> trailer; // what follows the reverbs: 8 bytes in 9,019 of the 9,020 projects

    // Every event with its path, the project's name first, groups in file order.
    std::vector<EventPath> Events() const;

    // The event at that path; null if none. The pointer is into this project.
    const Event *FindEvent(std::string_view path) const;

    // The sound definition of that name; null if none.
    const SoundDefinition *FindSoundDefinition(std::string_view definition) const;
};

// A parameter name's order to the game: "#duck:ambient,1000,66,2000,-4" is kind "duck" and the fields
// "ambient" to "-4", each trimmed. The kinds in use are duck, pause and music.
struct Directive
{
    std::string kind;
    std::vector<std::string> fields;
};

// The order a parameter's name carries, if it starts with '#' and has a ':'.
std::optional<Directive> DirectiveOf(std::string_view parameterName);

// A project out of a .fev's bytes; none if it is not FEV1 version 0x003A0000 or is cut short.
std::optional<Project> ProjectOf(std::span<const std::byte> bytes);

} // namespace dao::core::fev
