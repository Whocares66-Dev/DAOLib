#include "dao/core/Fev.h"

#include "dao/core/Bytes.h"

#include <algorithm>
#include <cstring>
#include <utility>

namespace dao::core::fev
{

namespace
{

constexpr std::uint32_t kVersion = 0x003A0000;
constexpr std::uint32_t kComplexEvent = 8;
constexpr std::uint32_t kSimpleEvent = 16;
// Categories and groups nest a few deep in the game's files; a file that goes deeper than this is not one.
constexpr int kMaxDepth = 64;

// A cursor over the bytes that fails once and stays failed: every read after a short one gives zero.
class Cursor
{
  public:
    explicit Cursor(std::span<const std::byte> bytes) : bytes_(bytes)
    {
    }

    bool Ok() const
    {
        return ok_;
    }

    std::size_t Position() const
    {
        return position_;
    }

    std::size_t Remaining() const
    {
        return bytes_.size() - position_;
    }

    template <class T> T Read()
    {
        const auto value = ReadAt<T>(bytes_, position_);
        if (!ok_ || !value)
        {
            ok_ = false;
            return T{};
        }
        position_ += sizeof(T);
        return *value;
    }

    std::uint32_t U32()
    {
        return Read<std::uint32_t>();
    }

    std::int32_t I32()
    {
        return Read<std::int32_t>();
    }

    float F32()
    {
        return Read<float>();
    }

    // A count of things each at least `each` bytes long, which the rest of the file must hold.
    std::size_t Count(std::size_t each)
    {
        const std::uint32_t n = U32();
        if (ok_ && (each != 0 && n > Remaining() / each))
        {
            ok_ = false;
        }
        return ok_ ? n : 0;
    }

    // A string: a length counting the NUL, then the bytes, which end at the first NUL.
    std::string String()
    {
        const std::size_t n = Count(1);
        if (!ok_)
        {
            return {};
        }
        std::string text(reinterpret_cast<const char *>(bytes_.data() + position_), n);
        position_ += n;
        text.resize(text.find('\0') == std::string::npos ? text.size() : text.find('\0'));
        return text;
    }

    template <class T, std::size_t N> std::array<T, N> Array()
    {
        std::array<T, N> out{};
        for (T &value : out)
        {
            value = Read<T>();
        }
        return out;
    }

    std::vector<std::byte> Rest()
    {
        const auto first = bytes_.begin() + static_cast<std::ptrdiff_t>(position_);
        std::vector<std::byte> out(first, bytes_.end());
        position_ = bytes_.size();
        return out;
    }

    void Fail()
    {
        ok_ = false;
    }

  private:
    std::span<const std::byte> bytes_;
    std::size_t position_ = 0;
    bool ok_ = true;
};

Category ReadCategory(Cursor &in, int depth)
{
    Category category;
    category.name = in.String();
    category.volume = in.F32();
    category.pitch = in.F32();
    category.unknown = in.Array<std::uint32_t, 2>();
    const std::size_t children = in.Count(24);
    if (depth > kMaxDepth)
    {
        in.Fail();
        return category;
    }
    for (std::size_t i = 0; i < children && in.Ok(); ++i)
    {
        category.children.push_back(ReadCategory(in, depth + 1));
    }
    return category;
}

std::vector<Property> ReadProperties(Cursor &in)
{
    std::vector<Property> out;
    const std::size_t n = in.Count(12);
    for (std::size_t i = 0; i < n && in.Ok(); ++i)
    {
        Property property;
        property.name = in.String();
        property.type = in.U32();
        switch (property.type)
        {
        case 0:
            property.integer = in.I32();
            break;
        case 1:
            property.real = in.F32();
            break;
        case 2:
            property.text = in.String();
            break;
        default:
            in.Fail();
            break;
        }
        out.push_back(std::move(property));
    }
    return out;
}

Instance ReadInstance(Cursor &in)
{
    Instance instance;
    instance.soundDefinition = in.Read<std::uint16_t>();
    instance.start = in.F32();
    instance.length = in.F32();
    instance.loopMode = in.U32();
    instance.unknown1 = in.U32();
    instance.loopCount = in.I32();
    instance.unknown2 = in.Array<std::uint32_t, 4>();
    instance.unknown3 = in.Array<float, 3>();
    instance.fadeInCurve = in.U32();
    instance.fadeOutCurve = in.U32();
    return instance;
}

constexpr std::size_t kInstanceSize = 58;

Envelope ReadEnvelope(Cursor &in)
{
    Envelope envelope;
    envelope.parameter = in.I32();
    envelope.dsp = in.String();
    envelope.unknown1 = in.U32();
    envelope.flags = in.U32();
    envelope.unknown2 = in.U32();
    const std::size_t points = in.Count(12);
    for (std::size_t i = 0; i < points && in.Ok(); ++i)
    {
        EnvelopePoint point;
        point.x = in.F32();
        point.y = in.F32();
        point.curve = in.U32();
        envelope.points.push_back(point);
    }
    envelope.unknown3 = in.Array<std::uint32_t, 2>();
    return envelope;
}

Layer ReadLayer(Cursor &in)
{
    Layer layer;
    layer.flags = in.Read<std::uint16_t>();
    layer.priority = in.Read<std::int16_t>();
    layer.parameter = in.Read<std::int16_t>();
    const std::size_t instances = in.Read<std::uint16_t>();
    const std::size_t envelopes = in.Read<std::uint16_t>();
    if (in.Ok() && instances > in.Remaining() / kInstanceSize)
    {
        in.Fail();
    }
    for (std::size_t i = 0; i < instances && in.Ok(); ++i)
    {
        layer.instances.push_back(ReadInstance(in));
    }
    for (std::size_t i = 0; i < envelopes && in.Ok(); ++i)
    {
        layer.envelopes.push_back(ReadEnvelope(in));
    }
    return layer;
}

Parameter ReadParameter(Cursor &in)
{
    Parameter parameter;
    parameter.name = in.String();
    parameter.velocity = in.F32();
    parameter.min = in.F32();
    parameter.max = in.F32();
    parameter.flags = in.U32();
    parameter.seekSpeed = in.F32();
    parameter.unknown = in.Array<std::uint32_t, 2>();
    return parameter;
}

// The fixed properties after an event's GUID, up to its layer count.
void ReadEventProperties(Cursor &in, Event &event)
{
    event.volume = in.F32();
    event.pitch = in.F32();
    event.pitchRandomization = in.F32();
    event.volumeRandomization = in.F32();
    event.unknown16 = in.U32();
    event.maxPlaybacks = in.U32();
    event.unknown24 = in.U32();
    event.mode = in.U32();
    event.minDistance = in.F32();
    event.maxDistance = in.F32();
    event.flags2 = in.U32();
    event.speakers = in.Array<float, 8>();
    event.coneInside = in.F32();
    event.coneOutside = in.F32();
    event.coneOutsideVolume = in.F32();
    event.maxPlaybacksBehaviour = in.U32();
    event.doppler = in.F32();
    event.reverbDry = in.F32();
    event.reverbWet = in.F32();
    event.spread = in.F32();
    event.fadeInMs = in.U32();
    event.fadeOutMs = in.U32();
    event.spawnIntensity = in.F32();
    event.spawnIntensityRandomization = in.F32();
    event.panLevel = in.F32();
    event.positionRandomization = in.U32();
}

Event ReadEvent(Cursor &in)
{
    Event event;
    const std::uint32_t type = in.U32();
    event.name = in.String();
    event.guid = in.Array<std::uint8_t, 16>();
    ReadEventProperties(in, event);
    const std::size_t layers = in.U32();
    if (type == kComplexEvent)
    {
        if (layers > in.Remaining() / 10)
        {
            in.Fail();
        }
        for (std::size_t i = 0; i < layers && in.Ok(); ++i)
        {
            event.layers.push_back(ReadLayer(in));
        }
        const std::size_t parameters = in.Count(32);
        for (std::size_t i = 0; i < parameters && in.Ok(); ++i)
        {
            event.parameters.push_back(ReadParameter(in));
        }
        event.properties = ReadProperties(in);
    }
    else if (type == kSimpleEvent && layers == 1)
    {
        event.simple = true;
        event.layers.push_back(
            Layer{.flags = 0, .priority = 0, .parameter = -1, .instances = {ReadInstance(in)}, .envelopes = {}});
    }
    else
    {
        in.Fail();
    }
    event.unknownTail = in.U32();
    event.category = in.String();
    return event;
}

Group ReadGroup(Cursor &in, int depth)
{
    Group group;
    group.name = in.String();
    group.properties = ReadProperties(in);
    const std::size_t subgroups = in.U32();
    const std::size_t events = in.U32();
    if (depth > kMaxDepth || subgroups + events > in.Remaining())
    {
        in.Fail();
    }
    for (std::size_t i = 0; i < subgroups && in.Ok(); ++i)
    {
        group.groups.push_back(ReadGroup(in, depth + 1));
    }
    for (std::size_t i = 0; i < events && in.Ok(); ++i)
    {
        group.events.push_back(ReadEvent(in));
    }
    return group;
}

Template ReadTemplate(Cursor &in)
{
    Template t;
    t.playMode = in.U32();
    t.spawnMin = in.U32();
    t.spawnMax = in.U32();
    t.maxSpawned = in.U32();
    t.volume = in.F32();
    t.unknown1 = in.Array<std::uint32_t, 3>();
    t.volumeRandomization = in.F32();
    t.pitch = in.F32();
    t.unknown2 = in.Array<std::uint32_t, 3>();
    t.pitchRandomization = in.F32();
    t.positionRandomization = in.F32();
    return t;
}

Entry ReadEntry(Cursor &in)
{
    Entry entry;
    entry.type = in.U32();
    entry.weight = in.U32();
    switch (entry.type)
    {
    case 0:
        entry.file = in.String();
        entry.bank = in.String();
        entry.sampleIndex = in.U32();
        entry.lengthMs = in.U32();
        break;
    case 1:
        entry.oscillator = in.U32();
        entry.frequency = in.F32();
        break;
    case 2:
        break;
    default:
        in.Fail();
        break;
    }
    return entry;
}

SoundDefinition ReadSoundDefinition(Cursor &in)
{
    SoundDefinition definition;
    definition.name = in.String();
    definition.templateIndex = in.U32();
    const std::size_t entries = in.Count(8);
    for (std::size_t i = 0; i < entries && in.Ok(); ++i)
    {
        definition.entries.push_back(ReadEntry(in));
    }
    return definition;
}

Reverb ReadReverb(Cursor &in)
{
    Reverb r;
    r.name = in.String();
    r.room = in.I32();
    r.roomHF = in.I32();
    r.roomRolloff = in.F32();
    r.decayTime = in.F32();
    r.decayHFRatio = in.F32();
    r.reflections = in.I32();
    r.reflectDelay = in.F32();
    r.reverb = in.I32();
    r.reverbDelay = in.F32();
    r.diffusion = in.F32();
    r.density = in.F32();
    r.hfReference = in.F32();
    r.roomLF = in.I32();
    r.lfReference = in.F32();
    r.unknown = in.Array<std::uint32_t, 19>();
    return r;
}

void CollectEvents(const Group &group, const std::string &parent, std::vector<EventPath> &out)
{
    const std::string path = parent + "/" + group.name;
    for (const Group &sub : group.groups)
    {
        CollectEvents(sub, path, out);
    }
    for (const Event &event : group.events)
    {
        out.push_back({.path = path + "/" + event.name, .event = &event});
    }
}

std::string_view Trim(std::string_view text)
{
    while (!text.empty() && (text.front() == ' ' || text.front() == '\t'))
    {
        text.remove_prefix(1);
    }
    while (!text.empty() && (text.back() == ' ' || text.back() == '\t'))
    {
        text.remove_suffix(1);
    }
    return text;
}

} // namespace

const Category *Category::Find(std::string_view node) const
{
    if (name == node)
    {
        return this;
    }
    for (const Category &child : children)
    {
        if (const Category *found = child.Find(node))
        {
            return found;
        }
    }
    return nullptr;
}

const Property *Event::FindProperty(std::string_view propertyName) const
{
    const auto it = std::ranges::find(properties, propertyName, &Property::name);
    return it == properties.end() ? nullptr : &*it;
}

std::vector<EventPath> Project::Events() const
{
    std::vector<EventPath> out;
    for (const Group &group : groups)
    {
        CollectEvents(group, name, out);
    }
    return out;
}

const Event *Project::FindEvent(std::string_view path) const
{
    for (const EventPath &entry : Events())
    {
        if (entry.path == path)
        {
            return entry.event;
        }
    }
    return nullptr;
}

const SoundDefinition *Project::FindSoundDefinition(std::string_view definition) const
{
    const auto it = std::ranges::find(soundDefinitions, definition, &SoundDefinition::name);
    return it == soundDefinitions.end() ? nullptr : &*it;
}

std::optional<Directive> DirectiveOf(std::string_view parameterName)
{
    const std::size_t colon = parameterName.find(':');
    if (parameterName.empty() || parameterName.front() != '#' || colon == std::string_view::npos)
    {
        return std::nullopt;
    }
    Directive directive;
    directive.kind = std::string(Trim(parameterName.substr(1, colon - 1)));
    std::string_view rest = parameterName.substr(colon + 1);
    for (;;)
    {
        const std::size_t comma = rest.find(',');
        directive.fields.emplace_back(Trim(rest.substr(0, comma)));
        if (comma == std::string_view::npos)
        {
            break;
        }
        rest.remove_prefix(comma + 1);
    }
    return directive;
}

std::optional<Project> ProjectOf(std::span<const std::byte> bytes)
{
    Cursor in(bytes);
    if (bytes.size() < 8 || std::memcmp(bytes.data(), "FEV1", 4) != 0)
    {
        return std::nullopt;
    }
    in.Read<std::uint32_t>();
    Project project;
    project.version = in.U32();
    if (project.version != kVersion)
    {
        return std::nullopt;
    }
    project.unknown = in.Array<std::uint32_t, 2>();
    project.name = in.String();
    const std::size_t banks = in.Count(12);
    for (std::size_t i = 0; i < banks && in.Ok(); ++i)
    {
        Bank bank;
        bank.flags = in.U32();
        bank.unknown = in.U32();
        bank.name = in.String();
        project.banks.push_back(std::move(bank));
    }
    project.categories = ReadCategory(in, 0);
    const std::size_t groups = in.Count(12);
    for (std::size_t i = 0; i < groups && in.Ok(); ++i)
    {
        project.groups.push_back(ReadGroup(in, 0));
    }
    const std::size_t templates = in.Count(60);
    for (std::size_t i = 0; i < templates && in.Ok(); ++i)
    {
        project.templates.push_back(ReadTemplate(in));
    }
    const std::size_t definitions = in.Count(12);
    for (std::size_t i = 0; i < definitions && in.Ok(); ++i)
    {
        project.soundDefinitions.push_back(ReadSoundDefinition(in));
    }
    const std::size_t reverbs = in.Count(4 + 56 + 76);
    for (std::size_t i = 0; i < reverbs && in.Ok(); ++i)
    {
        project.reverbs.push_back(ReadReverb(in));
    }
    if (!in.Ok())
    {
        return std::nullopt;
    }
    project.trailer = in.Rest();
    return project;
}

} // namespace dao::core::fev
