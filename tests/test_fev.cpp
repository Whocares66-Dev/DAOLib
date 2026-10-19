// dao/core/Fev.h over a project made here, laid out as the game's are (the
// layout is in Fev.h), and over the game's own in game/audio when there
// (DAOLib #192).
#include "dao/core/Fev.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

using Catch::Approx;
namespace fev = dao::core::fev;

namespace
{

class Writer
{
  public:
    Writer &Raw(const char *text, std::size_t size)
    {
        for (std::size_t i = 0; i < size; ++i)
        {
            bytes_.push_back(static_cast<std::byte>(text[i]));
        }
        return *this;
    }

    template <class T> Writer &Put(T value)
    {
        std::byte raw[sizeof(T)];
        std::memcpy(raw, &value, sizeof(T));
        bytes_.insert(bytes_.end(), raw, raw + sizeof(T));
        return *this;
    }

    Writer &U32(std::uint32_t value)
    {
        return Put(value);
    }

    Writer &F32(float value)
    {
        return Put(value);
    }

    Writer &Str(const std::string &text)
    {
        U32(static_cast<std::uint32_t>(text.size() + 1));
        Raw(text.c_str(), text.size() + 1);
        return *this;
    }

    Writer &Zeros(std::size_t n)
    {
        bytes_.insert(bytes_.end(), n, std::byte{0});
        return *this;
    }

    const std::vector<std::byte> &Bytes() const
    {
        return bytes_;
    }

  private:
    std::vector<std::byte> bytes_;
};

void Category(Writer &w, const std::string &name, float volume, std::uint32_t children)
{
    w.Str(name).F32(volume).F32(0.0F).U32(1).U32(2).U32(children);
}

void Instance(Writer &w, std::uint16_t definition, float start, float length)
{
    w.Put<std::uint16_t>(definition).F32(start).F32(length);
    w.U32(1).U32(2).Put<std::int32_t>(-1);
    w.U32(0).U32(0).U32(0).U32(0);
    w.F32(1.0F).F32(-1.0F).F32(-1.0F);
    w.U32(2).U32(2);
}

// The fixed properties of an event, each a different number so a swap shows.
void EventHeader(Writer &w, std::uint32_t type, const std::string &name, std::uint32_t layers)
{
    w.U32(type).Str(name);
    for (int i = 0; i < 16; ++i)
    {
        w.Put<std::uint8_t>(static_cast<std::uint8_t>(0xA0 + i));
    }
    w.F32(0.5F).F32(0.25F).F32(0.125F).F32(0.0625F); // volume, pitch, pitch rand, volume rand
    w.U32(41).U32(3).U32(10000).U32(0x00180008);
    w.F32(1.0F).F32(10000.0F).U32(0x00080000);
    for (int i = 0; i < 8; ++i)
    {
        w.F32(static_cast<float>(i) / 8.0F);
    }
    w.F32(360.0F).F32(270.0F).F32(0.75F).U32(1);
    w.F32(1.5F).F32(0.8F).F32(0.2F).F32(0.3F); // doppler, dry, wet, spread
    w.U32(300).U32(400);
    w.F32(2.0F).F32(3.0F).F32(0.9F).U32(7);
    w.U32(layers);
}

void Template(Writer &w)
{
    w.U32(5).U32(1).U32(2).U32(3).F32(0.5F).U32(0).U32(0).U32(0).F32(0.1F);
    w.F32(1.5F).U32(0).U32(0).U32(0).F32(0.2F).F32(0.3F);
}

// "fx": bank "fx_bank" (stream), categories master/music(0.5), group "g" (an integer property) with
// subgroup "s" holding the simple event "ping", and the complex event "swell" with two layers and
// three parameters; two sound definitions, one reverb, an 8-byte trailer.
std::vector<std::byte> Project()
{
    Writer w;
    w.Raw("FEV1", 4).U32(0x003A0000).U32(20).U32(24).Str("fx");
    w.U32(1).U32(0x80).U32(4).Str("fx_bank");
    Category(w, "master", 1.0F, 1);
    Category(w, "music", 0.5F, 0);
    w.U32(1); // groups
    w.Str("g").U32(1).Str("tag").U32(0).Put<std::int32_t>(-5);
    w.U32(1).U32(1); // one subgroup, one event
    w.Str("s").U32(2).Str("note").U32(2).Str("hello").Str("gain").U32(1).F32(2.5F).U32(0).U32(1);
    EventHeader(w, 16, "ping", 1);
    Instance(w, 1, 0.0F, 1.0F);
    w.U32(1).Str("music");
    EventHeader(w, 8, "swell", 2);
    // layer 0: two instances along parameter 1, one envelope over parameter 2
    w.Put<std::uint16_t>(2).Put<std::int16_t>(-1).Put<std::int16_t>(1).Put<std::uint16_t>(2).Put<std::uint16_t>(1);
    Instance(w, 0, 0.0F, 0.5F);
    Instance(w, 1, 0.5F, 0.5F);
    w.Put<std::int32_t>(2).Str("FMOD Lowpass Simple").U32(7).U32(0x204).U32(9);
    w.U32(2).F32(0.0F).F32(1.0F).U32(1).F32(1.0F).F32(0.25F).U32(2);
    w.U32(4).U32(5);
    // layer 1: no parameter, no envelope
    w.Put<std::uint16_t>(2).Put<std::int16_t>(3).Put<std::int16_t>(-1).Put<std::uint16_t>(1).Put<std::uint16_t>(0);
    Instance(w, 1, 0.0F, 1.0F);
    w.U32(3);
    w.Str("intensity").F32(0.0F).F32(0.0F).F32(4.0F).U32(3).F32(10.0F).U32(0).U32(1);
    w.Str("#duck:ambient, 500,0,1000,-6").F32(0.0F).F32(0.0F).F32(1.0F).U32(2).F32(0.0F).U32(0).U32(0);
    w.Str("#music:track,1").F32(0.0F).F32(0.0F).F32(1.0F).U32(2).F32(0.0F).U32(0).U32(0);
    w.U32(1).Str("tag").U32(0).Put<std::int32_t>(8); // the event's user properties
    w.U32(1).Str("music");
    w.U32(1); // templates
    Template(w);
    w.U32(2); // sound definitions
    w.Str("/pings").U32(0).U32(2);
    w.U32(0).U32(100).Str("fx/ping.wav").Str("fx_bank").U32(12).U32(2500);
    w.U32(1).U32(50).U32(2).F32(440.0F);
    w.Str("/quiet").U32(0).U32(1);
    w.U32(2).U32(10);
    w.U32(1); // reverbs
    w.Str("cave").Put<std::int32_t>(-1000).Put<std::int32_t>(-300).F32(0.0F).F32(2.5F).F32(0.8F);
    w.Put<std::int32_t>(-200).F32(0.01F).Put<std::int32_t>(100).F32(0.02F).F32(90.0F).F32(80.0F).F32(5000.0F);
    w.Put<std::int32_t>(-250).F32(250.0F);
    for (std::uint32_t i = 0; i < 19; ++i)
    {
        w.U32(i + 1);
    }
    w.U32(8).Raw("comp", 4);
    return w.Bytes();
}

std::vector<std::byte> FromDisk(const std::filesystem::path &path)
{
    std::ifstream in(path, std::ios::binary);
    const std::vector<char> chars{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
    std::vector<std::byte> out(chars.size());
    std::memcpy(out.data(), chars.data(), chars.size());
    return out;
}

std::filesystem::path AudioFolder()
{
    return std::filesystem::path(__FILE__).parent_path().parent_path() / "game" / "audio";
}

} // namespace

TEST_CASE("a project's name, banks and category tree", "[fev]")
{
    const auto project = fev::ProjectOf(Project());
    REQUIRE(project);
    CHECK(project->version == 0x003A0000);
    CHECK(project->unknown[0] == 20);
    CHECK(project->unknown[1] == 24);
    CHECK(project->name == "fx");
    REQUIRE(project->banks.size() == 1);
    CHECK(project->banks[0].name == "fx_bank");
    CHECK(project->banks[0].flags == 0x80);
    CHECK(project->banks[0].unknown == 4);
    CHECK(project->categories.name == "master");
    REQUIRE(project->categories.children.size() == 1);
    CHECK(project->categories.children[0].volume == Approx(0.5F));
    CHECK(project->categories.Find("music") == project->categories.children.data());
    CHECK(project->categories.Find("nothing") == nullptr);
}

TEST_CASE("events are found by path, and groups and events keep their user properties", "[fev]")
{
    const auto project = fev::ProjectOf(Project());
    REQUIRE(project);
    const auto events = project->Events();
    REQUIRE(events.size() == 2);
    CHECK(events[0].path == "fx/g/s/ping"); // the subgroup's events before the group's own
    CHECK(events[1].path == "fx/g/swell");
    CHECK(project->FindEvent("fx/g/swell") == events[1].event);
    CHECK(project->FindEvent("fx/g/other") == nullptr);

    REQUIRE(project->groups.size() == 1);
    const fev::Group &group = project->groups[0];
    REQUIRE(group.properties.size() == 1);
    CHECK(group.properties[0].name == "tag");
    CHECK(group.properties[0].type == 0);
    CHECK(group.properties[0].integer == -5);
    REQUIRE(group.groups.size() == 1);
    const fev::Group &sub = group.groups[0];
    REQUIRE(sub.properties.size() == 2);
    CHECK(sub.properties[0].type == 2);
    CHECK(sub.properties[0].text == "hello");
    CHECK(sub.properties[1].type == 1);
    CHECK(sub.properties[1].real == Approx(2.5F));
}

TEST_CASE("a simple event is one instance and its category", "[fev]")
{
    const auto project = fev::ProjectOf(Project());
    REQUIRE(project);
    const fev::Event *ping = project->FindEvent("fx/g/s/ping");
    REQUIRE(ping);
    CHECK(ping->simple);
    CHECK(ping->category == "music");
    CHECK(ping->parameters.empty());
    CHECK(ping->properties.empty());
    REQUIRE(ping->layers.size() == 1);
    REQUIRE(ping->layers[0].instances.size() == 1);
    CHECK(ping->layers[0].instances[0].soundDefinition == 1);
    CHECK(ping->layers[0].instances[0].length == Approx(1.0F));
}

TEST_CASE("an event's fixed properties are each their own", "[fev]")
{
    const auto project = fev::ProjectOf(Project());
    REQUIRE(project);
    const fev::Event *swell = project->FindEvent("fx/g/swell");
    REQUIRE(swell);
    CHECK_FALSE(swell->simple);
    CHECK(swell->guid[0] == 0xA0);
    CHECK(swell->guid[15] == 0xAF);
    CHECK(swell->volume == Approx(0.5F));
    CHECK(swell->pitch == Approx(0.25F));
    CHECK(swell->pitchRandomization == Approx(0.125F));
    CHECK(swell->volumeRandomization == Approx(0.0625F));
    CHECK(swell->unknown16 == 41);
    CHECK(swell->maxPlaybacks == 3);
    CHECK(swell->unknown24 == 10000);
    CHECK(swell->mode == 0x00180008);
    CHECK(swell->minDistance == Approx(1.0F));
    CHECK(swell->maxDistance == Approx(10000.0F));
    CHECK(swell->flags2 == 0x00080000);
    CHECK(swell->speakers[7] == Approx(7.0F / 8.0F));
    CHECK(swell->coneInside == Approx(360.0F));
    CHECK(swell->coneOutside == Approx(270.0F));
    CHECK(swell->coneOutsideVolume == Approx(0.75F));
    CHECK(swell->maxPlaybacksBehaviour == 1);
    CHECK(swell->doppler == Approx(1.5F));
    CHECK(swell->reverbDry == Approx(0.8F));
    CHECK(swell->reverbWet == Approx(0.2F));
    CHECK(swell->spread == Approx(0.3F));
    CHECK(swell->fadeInMs == 300);
    CHECK(swell->fadeOutMs == 400);
    CHECK(swell->spawnIntensity == Approx(2.0F));
    CHECK(swell->spawnIntensityRandomization == Approx(3.0F));
    CHECK(swell->panLevel == Approx(0.9F));
    CHECK(swell->positionRandomization == 7);
    CHECK(swell->unknownTail == 1);
    CHECK(swell->category == "music");
}

TEST_CASE("a complex event's layers, instances and envelopes", "[fev]")
{
    const auto project = fev::ProjectOf(Project());
    REQUIRE(project);
    const fev::Event *swell = project->FindEvent("fx/g/swell");
    REQUIRE(swell);
    REQUIRE(swell->layers.size() == 2);
    const fev::Layer &first = swell->layers[0];
    CHECK(first.flags == 2);
    CHECK(first.priority == -1);
    CHECK(first.parameter == 1);
    REQUIRE(first.instances.size() == 2);
    CHECK(first.instances[0].soundDefinition == 0);
    CHECK(first.instances[1].soundDefinition == 1);
    CHECK(first.instances[1].start == Approx(0.5F));
    CHECK(first.instances[1].length == Approx(0.5F));
    CHECK(first.instances[1].loopMode == 1);
    CHECK(first.instances[1].unknown1 == 2);
    CHECK(first.instances[1].loopCount == -1);
    CHECK(first.instances[1].unknown3[1] == Approx(-1.0F));
    CHECK(first.instances[1].fadeInCurve == 2);
    REQUIRE(first.envelopes.size() == 1);
    const fev::Envelope &envelope = first.envelopes[0];
    CHECK(envelope.parameter == 2);
    CHECK(envelope.dsp == "FMOD Lowpass Simple");
    CHECK(envelope.unknown1 == 7);
    CHECK(envelope.flags == 0x204);
    CHECK(envelope.unknown2 == 9);
    REQUIRE(envelope.points.size() == 2);
    CHECK(envelope.points[1].x == Approx(1.0F));
    CHECK(envelope.points[1].y == Approx(0.25F));
    CHECK(envelope.points[1].curve == 2);
    CHECK(envelope.unknown3[0] == 4);
    CHECK(envelope.unknown3[1] == 5);
    const fev::Layer &second = swell->layers[1];
    CHECK(second.priority == 3);
    CHECK(second.parameter == -1);
    CHECK(second.instances.size() == 1);
    CHECK(second.envelopes.empty());
}

TEST_CASE("an event's parameters and user properties follow its layers", "[fev]")
{
    const auto project = fev::ProjectOf(Project());
    REQUIRE(project);
    const fev::Event *swell = project->FindEvent("fx/g/swell");
    REQUIRE(swell);
    REQUIRE(swell->parameters.size() == 3);
    CHECK(swell->parameters[0].name == "intensity");
    CHECK(swell->parameters[0].max == Approx(4.0F));
    CHECK(swell->parameters[0].flags == 3);
    CHECK(swell->parameters[0].seekSpeed == Approx(10.0F));
    CHECK(swell->parameters[0].unknown[1] == 1);
    CHECK(swell->parameters[1].name == "#duck:ambient, 500,0,1000,-6");
    CHECK(swell->parameters[1].flags == 2);
    REQUIRE(swell->properties.size() == 1);
    CHECK(swell->FindProperty("tag")->integer == 8);
    CHECK(swell->FindProperty("other") == nullptr);
}

TEST_CASE("a parameter's name that starts with '#' is an order to the game", "[fev]")
{
    const auto duck = fev::DirectiveOf("#duck:ambient, 500,0,1000,-6");
    REQUIRE(duck);
    CHECK(duck->kind == "duck");
    CHECK(duck->fields == std::vector<std::string>{"ambient", "500", "0", "1000", "-6"});
    const auto pause = fev::DirectiveOf("#pause:footsteps");
    REQUIRE(pause);
    CHECK(pause->kind == "pause");
    CHECK(pause->fields == std::vector<std::string>{"footsteps"});
    const auto music = fev::DirectiveOf("#music:bhm_mage_end,1,4000");
    REQUIRE(music);
    CHECK(music->fields.size() == 3);
    CHECK_FALSE(fev::DirectiveOf("music_state"));
    CHECK_FALSE(fev::DirectiveOf("#nocolon"));
    CHECK_FALSE(fev::DirectiveOf(""));
}

TEST_CASE("sound definitions have their templates and entries, a wave with its bank and sample", "[fev]")
{
    const auto project = fev::ProjectOf(Project());
    REQUIRE(project);
    REQUIRE(project->templates.size() == 1);
    CHECK(project->templates[0].playMode == 5);
    CHECK(project->templates[0].spawnMax == 2);
    CHECK(project->templates[0].volume == Approx(0.5F));
    CHECK(project->templates[0].volumeRandomization == Approx(0.1F));
    CHECK(project->templates[0].pitch == Approx(1.5F));
    CHECK(project->templates[0].pitchRandomization == Approx(0.2F));
    CHECK(project->templates[0].positionRandomization == Approx(0.3F));
    REQUIRE(project->soundDefinitions.size() == 2);
    const fev::SoundDefinition *pings = project->FindSoundDefinition("/pings");
    REQUIRE(pings);
    CHECK(pings == project->soundDefinitions.data());
    REQUIRE(pings->entries.size() == 2);
    CHECK(pings->entries[0].type == 0);
    CHECK(pings->entries[0].weight == 100);
    CHECK(pings->entries[0].file == "fx/ping.wav");
    CHECK(pings->entries[0].bank == "fx_bank");
    CHECK(pings->entries[0].sampleIndex == 12);
    CHECK(pings->entries[0].lengthMs == 2500);
    CHECK(pings->entries[1].type == 1);
    CHECK(pings->entries[1].oscillator == 2);
    CHECK(pings->entries[1].frequency == Approx(440.0F));
    CHECK(project->soundDefinitions[1].entries[0].type == 2);
    CHECK(project->soundDefinitions[1].entries[0].weight == 10);
    CHECK(project->FindSoundDefinition("/none") == nullptr);
}

TEST_CASE("reverb presets are read in xoreos' layout, and what follows is kept", "[fev]")
{
    const auto project = fev::ProjectOf(Project());
    REQUIRE(project);
    REQUIRE(project->reverbs.size() == 1);
    const fev::Reverb &cave = project->reverbs[0];
    CHECK(cave.name == "cave");
    CHECK(cave.room == -1000);
    CHECK(cave.roomHF == -300);
    CHECK(cave.decayTime == Approx(2.5F));
    CHECK(cave.decayHFRatio == Approx(0.8F));
    CHECK(cave.reflections == -200);
    CHECK(cave.reflectDelay == Approx(0.01F));
    CHECK(cave.reverb == 100);
    CHECK(cave.reverbDelay == Approx(0.02F));
    CHECK(cave.diffusion == Approx(90.0F));
    CHECK(cave.density == Approx(80.0F));
    CHECK(cave.hfReference == Approx(5000.0F));
    CHECK(cave.roomLF == -250);
    CHECK(cave.lfReference == Approx(250.0F));
    CHECK(cave.unknown[18] == 19);
    CHECK(project->trailer.size() == 8);
}

TEST_CASE("a project that is not FEV1, or is cut short anywhere, is none", "[fev]")
{
    const std::vector<std::byte> whole = Project();
    REQUIRE(fev::ProjectOf(whole));
    for (std::size_t size = 0; size < whole.size(); ++size)
    {
        const std::span<const std::byte> cut(whole.data(), size);
        const auto read = fev::ProjectOf(cut);
        // Cut in the trailer, the project is whole; anywhere before, it is not.
        CHECK(read.has_value() == (size >= whole.size() - 8));
    }
    std::vector<std::byte> wrong = whole;
    wrong[0] = std::byte{'X'};
    CHECK_FALSE(fev::ProjectOf(wrong));
    wrong = whole;
    wrong[5] = std::byte{1};
    CHECK_FALSE(fev::ProjectOf(wrong));
}

TEST_CASE("a count bigger than the file is none, not an allocation", "[fev]")
{
    std::vector<std::byte> bytes = Project();
    // The bank count follows the name "fx" (a length of 3, the bytes, a NUL) after the 16-byte header.
    const std::size_t at = 16 + 4 + 3;
    const std::uint32_t huge = 0xFFFFFFF0;
    std::memcpy(bytes.data() + at, &huge, sizeof(huge));
    CHECK_FALSE(fev::ProjectOf(bytes));
}

TEST_CASE("an event of a type that is neither simple nor complex is none", "[fev]")
{
    std::vector<std::byte> bytes = Project();
    // The first event, "ping": its type is the u32 before its name's length of 5.
    const char name[] = {5, 0, 0, 0, 'p', 'i', 'n', 'g', 0};
    const auto it = std::search(bytes.begin(), bytes.end(), reinterpret_cast<const std::byte *>(name),
                                reinterpret_cast<const std::byte *>(name) + sizeof(name));
    REQUIRE(it != bytes.end());
    const std::uint32_t odd = 9;
    std::memcpy(&*(it - 4), &odd, sizeof(odd));
    CHECK_FALSE(fev::ProjectOf(bytes));
}

TEST_CASE("the game's projects in game/audio, when there, read whole", "[fev][game]")
{
    const auto folder = AudioFolder();
    if (!std::filesystem::exists(folder))
    {
        SKIP("no game/audio (copies of a VO project, music.fev, audio_categories.fev, glo_gui.fev and "
             "master_reverb.fev)");
    }
    std::size_t read = 0;
    for (const auto &entry : std::filesystem::directory_iterator(folder))
    {
        if (entry.path().extension() != ".fev")
        {
            continue;
        }
        const auto bytes = FromDisk(entry.path());
        const auto project = fev::ProjectOf(bytes);
        INFO(entry.path().filename().string());
        REQUIRE(project);
        ++read;
        for (const fev::SoundDefinition &definition : project->soundDefinitions)
        {
            REQUIRE(definition.templateIndex < project->templates.size());
            for (const fev::Entry &e : definition.entries)
            {
                if (e.type == 0)
                {
                    CHECK(std::ranges::any_of(project->banks, [&](const fev::Bank &b) { return b.name == e.bank; }));
                }
            }
        }
        for (const auto &event : project->Events())
        {
            for (const fev::Layer &layer : event.event->layers)
            {
                for (const fev::Instance &instance : layer.instances)
                {
                    CHECK(instance.soundDefinition < project->soundDefinitions.size());
                }
            }
            CHECK(project->categories.Find(event.event->category));
        }
    }
    CHECK(read >= 1);
}

TEST_CASE("a VO project: its line event, its music order and the samples of its sound definitions", "[fev][game]")
{
    const auto path = AudioFolder() / "bhm100_jowan.fev";
    if (!std::filesystem::exists(path))
    {
        SKIP("no game/audio/bhm100_jowan.fev");
    }
    const auto project = fev::ProjectOf(FromDisk(path));
    REQUIRE(project);
    CHECK(project->name == "bhm100_jowan");
    REQUIRE(project->banks.size() == 1);
    CHECK(project->banks[0].name == "bhm100_jowan");
    const fev::Event *line = project->FindEvent("bhm100_jowan/VO/36641_m");
    REQUIRE(line);
    const auto it = std::ranges::find_if(line->parameters,
                                         [](const fev::Parameter &p) { return fev::DirectiveOf(p.name).has_value(); });
    REQUIRE(it != line->parameters.end());
    CHECK(fev::DirectiveOf(it->name)->kind == "music");
    const fev::SoundDefinition *conversation = project->FindSoundDefinition("/VOconversation");
    REQUIRE(conversation);
    CHECK(conversation->entries.size() == 204);
    CHECK(project->templates[conversation->templateIndex].playMode == 5);
    for (const fev::Entry &e : conversation->entries)
    {
        CHECK(e.bank == "bhm100_jowan");
        CHECK(e.sampleIndex < 214);
    }
    const fev::SoundDefinition *own = project->FindSoundDefinition("/36641_m");
    REQUIRE(own);
    CHECK(own->entries.size() == 1);
    CHECK(project->templates[own->templateIndex].playMode == 3);
}

TEST_CASE("the interface's project and the cutscene music's, when in game/audio", "[fev][game]")
{
    const auto folder = AudioFolder();
    if (!std::filesystem::exists(folder / "glo_gui.fev") || !std::filesystem::exists(folder / "music_cutscene.fev"))
    {
        SKIP("no game/audio/glo_gui.fev and music_cutscene.fev");
    }
    const auto gui = fev::ProjectOf(FromDisk(folder / "glo_gui.fev"));
    REQUIRE(gui);
    REQUIRE(gui->banks.size() == 2);
    CHECK(gui->banks[0].name == "gui");
    CHECK(gui->banks[1].name == "party_picker_bank06");
    CHECK(gui->Events().size() == 87);
    CHECK(gui->soundDefinitions.size() == 87);
    // The cutscene project has events and no sound: each carries a #music order among its parameters.
    const auto cutscene = fev::ProjectOf(FromDisk(folder / "music_cutscene.fev"));
    REQUIRE(cutscene);
    CHECK(cutscene->banks.empty());
    CHECK(cutscene->soundDefinitions.empty());
    const auto events = cutscene->Events();
    CHECK(events.size() == 49);
    for (const fev::EventPath &entry : events)
    {
        INFO(entry.path);
        CHECK(std::ranges::any_of(entry.event->parameters, [](const fev::Parameter &p) {
            const auto order = fev::DirectiveOf(p.name);
            return order && order->kind == "music";
        }));
    }
}

TEST_CASE("music.fev, audio_categories.fev and master_reverb.fev, when in game/audio", "[fev][game]")
{
    const auto folder = AudioFolder();
    if (!std::filesystem::exists(folder / "music.fev") || !std::filesystem::exists(folder / "audio_categories.fev") ||
        !std::filesystem::exists(folder / "master_reverb.fev"))
    {
        SKIP("no game/audio/music.fev, audio_categories.fev and master_reverb.fev");
    }
    const auto music = fev::ProjectOf(FromDisk(folder / "music.fev"));
    REQUIRE(music);
    CHECK(music->Events().size() == 782);
    CHECK(music->soundDefinitions.size() == 268);
    REQUIRE(music->banks.size() == 3);
    CHECK(music->banks[0].name == "music_notstream_sting");
    CHECK(music->banks[0].flags == 0x200);
    CHECK(music->banks[1].name == "music_bank_strm_new");
    CHECK(music->banks[1].flags == 0x80);
    CHECK(music->templates.size() == 65);
    CHECK(music->unknown[0] == 20040);
    CHECK(music->unknown[1] == 24224);

    const auto categories = fev::ProjectOf(FromDisk(folder / "audio_categories.fev"));
    REQUIRE(categories);
    CHECK(categories->categories.children.size() == 20);
    const fev::Category *quiet = categories->categories.Find("VOambient_ss");
    REQUIRE(quiet);
    CHECK(quiet->volume == Approx(0.708F).margin(0.001F));
    const fev::Event *cutscene = categories->FindEvent("audio_categories/ducking/OnCutscene");
    REQUIRE(cutscene);
    CHECK(cutscene->category == "master");
    CHECK(cutscene->parameters.size() == 9);
    const fev::Event *combat = categories->FindEvent("audio_categories/ducking/OnCombatMusic");
    REQUIRE(combat);
    REQUIRE(combat->parameters.size() == 3);
    const auto duck = fev::DirectiveOf(combat->parameters[2].name);
    REQUIRE(duck);
    CHECK(duck->kind == "duck");
    CHECK(duck->fields[0] == "vfxamb_combat");
    CHECK(duck->fields[4] == "-60");
    const fev::Event *pause = categories->FindEvent("audio_categories/ducking/OnGamePause");
    REQUIRE(pause);
    CHECK(std::ranges::any_of(pause->parameters, [](const fev::Parameter &p) {
        return fev::DirectiveOf(p.name) && fev::DirectiveOf(p.name)->kind == "pause";
    }));

    const auto reverbs = fev::ProjectOf(FromDisk(folder / "master_reverb.fev"));
    REQUIRE(reverbs);
    REQUIRE(reverbs->reverbs.size() == 38);
    CHECK(reverbs->reverbs[0].name == "Padded cell");
    CHECK(reverbs->reverbs[37].name == "DA_parking_lot");
    CHECK(reverbs->reverbs[1].name == "Generic");
    CHECK(reverbs->reverbs[1].room == -1000);
    CHECK(reverbs->reverbs[1].roomHF == -100);
    CHECK(reverbs->reverbs[1].decayTime == Approx(1.49F));
    CHECK(reverbs->reverbs[1].decayHFRatio == Approx(0.83F));
    CHECK(reverbs->reverbs[1].hfReference == Approx(5000.0F));
    CHECK(reverbs->reverbs[1].lfReference == Approx(250.0F));
    CHECK(reverbs->reverbs[30].name == "DA custom cathedral");
    CHECK(reverbs->reverbs[30].room == -200);
    CHECK(reverbs->reverbs[30].hfReference == Approx(2798.0F));
    CHECK(reverbs->trailer.size() == 8);
    CHECK(music->reverbs.empty());
    CHECK(music->trailer.size() == 8);
    for (const fev::Reverb &reverb : reverbs->reverbs)
    {
        CHECK_FALSE(reverb.name.empty());
        CHECK(reverb.decayTime > 0.0F);
    }
}
