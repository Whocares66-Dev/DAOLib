#include "dao/core/Level.h"

#include "dao/core/Bytes.h"
#include "dao/core/Model.h"

#include <algorithm>
#include <cstring>
#include <functional>
#include <string>
#include <string_view>
#include <utility>

namespace dao::core::level
{

namespace
{

// The fields' labels (dev/gff4_fields.csv).
constexpr std::uint32_t kName = 2;
constexpr std::uint32_t kPosition = 4;
constexpr std::uint32_t kOrientation = 5;
constexpr std::uint32_t kRoomSees = 3047;
constexpr std::uint32_t kRoomModels = 3050;
constexpr std::uint32_t kRoomConnects = 3099;
constexpr std::uint32_t kModelScale = 3059;
constexpr std::uint32_t kModelFile = 3063;
// A placed model's user params (DAOLib #212): the level loader hands them to the model
// (0x0053A510, its slot 0x138 at 0x005462F0), whose emitters take a value by name
// (dev/EFFECTS.md, "An effect a level places").
constexpr std::uint32_t kModelUserParams = 3506;
constexpr std::uint32_t kModelCutAwayOverride = 3109; // a byte in the game's files
constexpr std::uint32_t kUserParamName = 3507;
constexpr std::uint32_t kUserParamValue = 3508;
constexpr std::uint32_t kProbeCube = 3104;
constexpr std::uint32_t kLightmapPart = 3235;
constexpr std::uint32_t kRoomTrees = 3354;
constexpr std::uint32_t kTreeScale = 3356;
constexpr std::uint32_t kTreeModel = 3357;
constexpr std::uint32_t kTreeTint = 3375;
constexpr std::uint32_t kTreeDrawDistance = 3379;
constexpr std::uint32_t kLightmapAtlas = 3323;
constexpr std::uint32_t kLightmapOffsetScale = 3324;
constexpr std::uint32_t kModelLightmaps = 3326;
constexpr std::uint32_t kNodeName = 6000;
constexpr std::uint32_t kNodeId = 6004;
constexpr std::uint32_t kModelMeshFile = 6005;
constexpr std::uint32_t kNodeMeshChunk = 6006;
constexpr std::uint32_t kLampColour = 6007;
constexpr std::uint32_t kLampRadius = 6008;
constexpr std::uint32_t kLampStatic = 6009;
constexpr std::uint32_t kAmbientColour = 6010;
constexpr std::uint32_t kTranslation = 6047;
constexpr std::uint32_t kRotation = 6048;
constexpr std::uint32_t kLampVariation = 6249;
constexpr std::uint32_t kLampPeriod = 6250;
constexpr std::uint32_t kLampPeriodDelta = 6251;
constexpr std::uint32_t kIrradianceFile = 6253;
constexpr std::uint32_t kLampDomain = 6296;
constexpr std::uint32_t kIrradianceRed = 6336; // green and blue follow
constexpr std::uint32_t kLampOccluded = 6339;
constexpr std::uint32_t kVariationTint = 6340;
constexpr std::uint32_t kLampVersion = 6345;
constexpr std::uint32_t kChildren = 6999;
constexpr std::uint32_t kChunkVertexSize = 8000;
constexpr std::uint32_t kChunkVertexCount = 8001;
constexpr std::uint32_t kChunkIndexCount = 8002;
constexpr std::uint32_t kChunkBaseVertex = 8005;
constexpr std::uint32_t kChunkVertexOffset = 8006;
constexpr std::uint32_t kChunkStartIndex = 8009;
constexpr std::uint32_t kMeshChunks = 8021;
constexpr std::uint32_t kMeshVertices = 8022;
constexpr std::uint32_t kMeshIndices = 8023;
constexpr std::uint32_t kChunkDeclarators = 8025;
constexpr std::uint32_t kDeclaratorStream = 8026;
constexpr std::uint32_t kDeclaratorOffset = 8027;
constexpr std::uint32_t kDeclaratorType = 8028;
constexpr std::uint32_t kDeclaratorUsage = 8029;
constexpr std::uint32_t kDeclaratorUsageIndex = 8030;

// Direct3D's D3DDECLUSAGE and D3DDECLTYPE, as far as the meshes use them.
constexpr std::uint32_t kUsagePosition = 0;
constexpr std::uint32_t kUsageTexcoord = 5;
constexpr std::uint32_t kTypeFloat2 = 1;
constexpr std::uint32_t kTypeFloat3 = 2;
constexpr std::uint32_t kTypeFloat4 = 3;
constexpr std::uint32_t kTypeHalf2 = 15;
constexpr std::uint32_t kTypeHalf4 = 16;

constexpr std::size_t kNodesDeep = 64; // a model's tree deeper than this is not one

// How many floats a vertex element of a type holds; 0 for a type that is not floats.
std::size_t FloatsOf(std::uint32_t type)
{
    switch (type)
    {
    case kTypeFloat2:
    case kTypeHalf2:
        return 2;
    case kTypeFloat3:
        return 3;
    case kTypeFloat4:
    case kTypeHalf4:
        return 4;
    default:
        return 0;
    }
}

// `count` floats of a vertex element of the given type at `at`, or false for
// a type that has not that many or bytes that stop short.
bool Element(std::span<const std::byte> vertices, std::size_t at, std::uint32_t type, std::size_t count, float *to)
{
    const bool half = type == kTypeHalf2 || type == kTypeHalf4;
    const std::size_t has = FloatsOf(type);
    if (has < count)
    {
        return false;
    }
    for (std::size_t i = 0; i < count; ++i)
    {
        if (half)
        {
            const auto value = ReadAt<std::uint16_t>(vertices, at + (i * 2));
            if (!value)
            {
                return false;
            }
            to[i] = model::Half(*value);
        }
        else
        {
            const auto value = ReadAt<float>(vertices, at + (i * 4));
            if (!value)
            {
                return false;
            }
            to[i] = *value;
        }
    }
    return true;
}

Point PointOf(const std::vector<float> &floats)
{
    return floats.size() >= 3 ? Point{floats[0], floats[1], floats[2]} : Point{};
}

Quaternion QuaternionOf(const std::vector<float> &floats)
{
    return floats.size() >= 4 ? Quaternion{floats[0], floats[1], floats[2], floats[3]} : Quaternion{0, 0, 0, 1};
}

// Where in a vertex an element is, and its type.
using Declared = std::optional<std::pair<std::size_t, std::uint32_t>>;

// Where a chunk's vertices have their place and the lightmap's coordinates. The lightmap's are
// the second set of texture coordinates, or the first where the chunk has no second: a terrain's
// ground patch has one set and it is the lightmap's (read in Redcliffe's lak100d_14_0.msh, whose
// tile's .rml lists the patch with a lightmap's part and atlas, 2026-10-11; with the second set
// alone asked for, the 52 terrain levels came out with no ground). Whether a part is lightmapped
// at all is the room's list's to say, not this. The first set is also the material's own: where
// its texture paints the chunk (DAOLib #96).
struct Elements
{
    Declared position;
    Declared light;
    Declared paint;
};

Elements DeclaredIn(const gff4::Struct &chunk)
{
    Declared position;
    Declared first;
    Declared second;
    for (const gff4::Struct &declarator : chunk.Structs(kChunkDeclarators))
    {
        if (declarator.Get<std::int32_t>(kDeclaratorStream).value_or(-1) != 0)
        {
            continue;
        }
        const auto offset = static_cast<std::size_t>(declarator.Get<std::int32_t>(kDeclaratorOffset).value_or(0));
        const std::uint32_t type = declarator.Get<std::uint32_t>(kDeclaratorType).value_or(~0U);
        const std::uint32_t usage = declarator.Get<std::uint32_t>(kDeclaratorUsage).value_or(~0U);
        const std::uint32_t which = declarator.Get<std::uint32_t>(kDeclaratorUsageIndex).value_or(0);
        if (usage == kUsagePosition && which == 0)
        {
            position = {offset, type};
        }
        else if (usage == kUsageTexcoord && which == 0)
        {
            first = {offset, type};
        }
        else if (usage == kUsageTexcoord && which == 1)
        {
            second = {offset, type};
        }
    }
    return {.position = position, .light = second ? second : first, .paint = first};
}

std::optional<MeshChunk> ChunkOf(const gff4::Struct &chunk, std::span<const std::byte> vertices,
                                 std::span<const std::byte> indices)
{
    MeshChunk out;
    out.name = chunk.String(kName).value_or(L"");
    const std::size_t size = chunk.Get<std::uint32_t>(kChunkVertexSize).value_or(0);
    const std::size_t count = chunk.Get<std::uint32_t>(kChunkVertexCount).value_or(0);
    const std::size_t start = chunk.Get<std::uint32_t>(kChunkVertexOffset).value_or(0);
    if (size == 0 || start > vertices.size() || count > (vertices.size() - start) / size)
    {
        return std::nullopt;
    }
    const auto [position, light, declaredPaint] = DeclaredIn(chunk);
    if (!position)
    {
        return std::nullopt;
    }
    // A first set that is not floats (a few chunks keep theirs as shorts) is one the chunk has
    // not, here: its places and its lightmap's coordinates are read all the same.
    const Declared paint = declaredPaint && FloatsOf(declaredPaint->second) >= 2 ? declaredPaint : std::nullopt;
    out.positions.resize(count);
    out.lightUv.resize(light ? count : 0);
    out.paintUv.resize(paint ? count : 0);
    for (std::size_t i = 0; i < count; ++i)
    {
        const std::size_t at = start + (i * size);
        if (!Element(vertices, at + position->first, position->second, 3, out.positions[i].data()) ||
            (light && !Element(vertices, at + light->first, light->second, 2, out.lightUv[i].data())))
        {
            return std::nullopt;
        }
        // The texture's coordinates stopping short cost the chunk those alone: its places and its
        // lightmap's are whole, and a lightmap's reader has no use for the rest.
        if (paint && !out.paintUv.empty() &&
            !Element(vertices, at + paint->first, paint->second, 2, out.paintUv[i].data()))
        {
            out.paintUv.clear();
        }
    }
    const std::size_t first = chunk.Get<std::uint32_t>(kChunkStartIndex).value_or(0);
    const std::size_t used = chunk.Get<std::uint32_t>(kChunkIndexCount).value_or(0);
    const std::uint32_t base = chunk.Get<std::uint32_t>(kChunkBaseVertex).value_or(0);
    if (first > indices.size() / 2 || used > (indices.size() / 2) - first)
    {
        return std::nullopt;
    }
    out.indices.reserve(used - (used % 3));
    for (std::size_t i = 0; i + 2 < used; i += 3)
    {
        std::array<std::uint32_t, 3> corner{};
        bool inside = true;
        for (std::size_t k = 0; k < 3; ++k)
        {
            corner.at(k) = base + ReadAt<std::uint16_t>(indices, (first + i + k) * 2).value_or(0);
            inside = inside && corner.at(k) < count;
        }
        if (inside)
        {
            out.indices.insert(out.indices.end(), corner.begin(), corner.end());
        }
    }
    return out;
}

// A node's own translation and rotation: its children of those kinds.
void PlaceOf(const gff4::Struct &node, Point &at, Quaternion &turn)
{
    for (const gff4::Struct &child : node.Structs(kChildren))
    {
        if (child.Label() == "trsl")
        {
            at = PointOf(child.Floats(kTranslation));
        }
        else if (child.Label() == "rota")
        {
            turn = QuaternionOf(child.Floats(kRotation));
        }
    }
}

void MeshNodes(const gff4::Struct &node, const Point &parentAt, const Quaternion &parentTurn, std::size_t deep,
               std::vector<MeshNode> &out)
{
    if (deep > kNodesDeep)
    {
        return;
    }
    Point own{};
    Quaternion ownTurn{0, 0, 0, 1};
    PlaceOf(node, own, ownTurn);
    const Point turned = Turned(parentTurn, own);
    const Point at{parentAt[0] + turned[0], parentAt[1] + turned[1], parentAt[2] + turned[2]};
    const Quaternion turn = Then(ownTurn, parentTurn);
    if (const auto chunk = node.String(kNodeMeshChunk); chunk && !chunk->empty())
    {
        out.push_back({*chunk, node.String(kNodeId).value_or(L""), at, turn});
    }
    for (const gff4::Struct &child : node.Structs(kChildren))
    {
        if (child.Shape().Find(kChildren) != nullptr)
        {
            MeshNodes(child, at, turn, deep + 1, out);
        }
    }
}

Colour ColourOf(const std::vector<float> &floats)
{
    return floats.size() >= 3 ? Colour{floats[0], floats[1], floats[2]} : Colour{};
}

bool Flag(const gff4::Struct &node, std::uint32_t label)
{
    return node.Get<std::uint8_t>(label).value_or(0) != 0;
}

Lamp LampOf(const gff4::Struct &node, const Point &at)
{
    Lamp lamp;
    lamp.name = node.String(kNodeName).value_or(L"");
    lamp.at = at;
    lamp.colour = ColourOf(node.Floats(kLampColour));
    lamp.radius = node.Get<float>(kLampRadius).value_or(0.0F);
    lamp.isStatic = Flag(node, kLampStatic);
    lamp.variation = node.Get<float>(kLampVariation).value_or(0.0F);
    lamp.period = node.Get<float>(kLampPeriod).value_or(0.0F);
    lamp.periodDelta = node.Get<float>(kLampPeriodDelta).value_or(0.0F);
    lamp.affectDomain = node.Get<std::uint8_t>(kLampDomain).value_or(0);
    lamp.canBeOccluded = Flag(node, kLampOccluded);
    lamp.useVariationTint = Flag(node, kVariationTint);
    lamp.version = node.Get<std::uint8_t>(kLampVersion).value_or(0);
    return lamp;
}

Probe ProbeOf(const gff4::Struct &node, const Point &at)
{
    Probe probe;
    probe.name = node.String(kNodeName).value_or(L"");
    probe.at = at;
    for (const gff4::Struct &part : node.Structs(kChildren))
    {
        if (part.Label() == "nrfl")
        {
            probe.cube = part.String(kProbeCube).value_or(L"");
        }
        else if (part.Label() == "nirr")
        {
            probe.irradianceFile = part.String(kIrradianceFile).value_or(L"");
            for (std::uint32_t channel = 0; channel < 3; ++channel)
            {
                if (const std::vector<float> matrix = part.Floats(kIrradianceRed + channel); matrix.size() == 16)
                {
                    std::ranges::copy(matrix, probe.irradiance.begin() + (static_cast<std::ptrdiff_t>(channel) * 16));
                }
            }
        }
    }
    return probe;
}

// The lights under a node of a layout, each placed through the nodes above it.
void LightsUnder(const gff4::Struct &node, const Point &parentAt, const Quaternion &parentTurn, std::size_t deep,
                 Layout &out)
{
    if (deep > kNodesDeep)
    {
        return;
    }
    for (const gff4::Struct &child : node.Structs(kChildren))
    {
        Point own{};
        Quaternion ownTurn{0, 0, 0, 1};
        PlaceOf(child, own, ownTurn);
        const Point turned = Turned(parentTurn, own);
        const Point at{parentAt[0] + turned[0], parentAt[1] + turned[1], parentAt[2] + turned[2]};
        const std::string &label = child.Label();
        if (label == "nplt")
        {
            out.lamps.push_back(LampOf(child, at));
        }
        else if (label == "nalt")
        {
            if (!out.ambient)
            {
                out.ambient = Ambient{.name = child.String(kNodeName).value_or(L""),
                                      .at = at,
                                      .colour = ColourOf(child.Floats(kAmbientColour)),
                                      .useVariationTint = Flag(child, kVariationTint)};
            }
        }
        else if (label == "nlpr")
        {
            out.probes.push_back(ProbeOf(child, at));
        }
        else if (child.Shape().Find(kChildren) != nullptr)
        {
            LightsUnder(child, at, Then(ownTurn, parentTurn), deep + 1, out);
        }
    }
}

// A level layout's fields (dev/gff4_fields.csv).
constexpr std::uint32_t kAreaId = 3011;
constexpr std::uint32_t kAreaName = 3012;
constexpr std::uint32_t kAreaRooms = 3016;
constexpr std::uint32_t kAreaCenter = 3024;
constexpr std::uint32_t kAreaSkyDome = 3025;
constexpr std::uint32_t kAreaEffects = 3026;
constexpr std::uint32_t kAreaGlobalWind = 3027;
constexpr std::uint32_t kAreaLocalWinds = 3028;
constexpr std::uint32_t kAreaRoomId = 3031;
constexpr std::uint32_t kAreaRoomFile = 3033;
constexpr std::uint32_t kAreaIsChunked = 3122;
constexpr std::uint32_t kAreaChunkRows = 3123;
constexpr std::uint32_t kAreaChunkColumns = 3124;
constexpr std::uint32_t kAreaChunkWidth = 3125;
constexpr std::uint32_t kAreaChunkHeight = 3126;
constexpr std::uint32_t kAreaLayoutName = 3127;
constexpr std::uint32_t kAreaStartPoint = 3128;
constexpr std::uint32_t kAreaCutoffHeight = 3129;
constexpr std::uint32_t kAreaCutoffEnabled = 3134;
constexpr std::uint32_t kAreaMinimapLower = 3138;
constexpr std::uint32_t kAreaMinimapUpper = 3139;
constexpr std::uint32_t kAreaRoomLower = 3140;
constexpr std::uint32_t kAreaRoomUpper = 3141;
constexpr std::uint32_t kAreaForceCharacterLighting = 3142;
constexpr std::uint32_t kAreaSunOccludedOnCharacters = 3148;
constexpr std::uint32_t kAreaSunColourOnCharacters = 3149;
constexpr std::uint32_t kAreaSunDirection = 3150;
constexpr std::uint32_t kAreaSunColour = 3152;
constexpr std::uint32_t kAreaTreeFile = 3353;
constexpr std::uint32_t kAreaTrees = 3355;
constexpr std::uint32_t kAreaTreeTint = 3377;
constexpr std::uint32_t kAreaLightSubsets = 3800;
constexpr std::uint32_t kAreaLightSubsetEntry = 3801;
constexpr std::uint32_t kEffectFile = 5520;
constexpr std::uint32_t kEffectName = 5521;
constexpr std::uint32_t kEffectParameters = 5522;
constexpr std::uint32_t kEffectParameterName = 5523;
constexpr std::uint32_t kEffectParameterValue = 5524;
constexpr std::uint32_t kWindFile = 22001;
constexpr std::uint32_t kWindRadius = 22002;
constexpr std::uint32_t kAtmosphere = 22500;
constexpr std::uint32_t kClouds = 22600;

using Named = std::pair<std::uint32_t, std::string_view>;

// The atmosphere's and the clouds' fields by name, as xoreos names them (dev/gff4_fields.csv).
constexpr std::array<Named, 24> kSkyNames{{
    {22519, "AtmoSunColor"},
    {22520, "AtmoSunIntensity"},
    {22521, "AtmoTurbidity"},
    {22522, "AtmoEarthReflectance"},
    {22523, "AtmoMieMultiplier"},
    {22524, "AtmoRayleighMultiplier"},
    {22526, "AtmoDistanceMultiplier"},
    {22528, "AtmoAlpha"},
    {22529, "AtmoFogColor"},
    {22530, "AtmoFogIntensity"},
    {22531, "AtmoFogCap"},
    {22532, "AtmoFogZenith"},
    {22533, "AtmoFogWaterIntensity"},
    {22534, "AtmoFogWaterCap"},
    {22535, "AtmoFogTacticalMultiplier"},
    {22620, "CloudDensity"},
    {22621, "CloudSharpness"},
    {22622, "CloudDepth"},
    {22623, "CloudRangeMultiplier1"},
    {22624, "CloudRangeMultiplier2"},
    {22625, "CloudColor"},
    {22700, "MoonScale"},
    {22701, "MoonAlpha"},
    {22703, "MoonRotation"},
}};

// A struct's floats, each field by its name, in the file's order; a field not named above keeps
// its number.
std::vector<NamedFloats> FloatsByName(const gff4::Struct &values)
{
    std::vector<NamedFloats> out;
    for (const gff4::Field &field : values.Shape().fields)
    {
        const auto named = std::ranges::find(kSkyNames, field.label, &Named::first);
        out.push_back({.name = named != kSkyNames.end() ? std::string(named->second) : std::to_string(field.label),
                       .floats = values.Floats(field.label)});
    }
    return out;
}

std::array<float, 4> Four(const std::vector<float> &floats, std::array<float, 4> otherwise)
{
    if (floats.size() >= 4)
    {
        std::ranges::copy_n(floats.begin(), 4, otherwise.begin());
    }
    return otherwise;
}

FrameEffect EffectOf(const gff4::Struct &effect)
{
    FrameEffect out{.file = effect.String(kEffectFile).value_or(L""),
                    .name = effect.String(kEffectName).value_or(L""),
                    .parameters = {}};
    for (const gff4::Struct &parameter : effect.Structs(kEffectParameters))
    {
        const std::vector<float> value = parameter.Floats(kEffectParameterValue);
        out.parameters.emplace_back(parameter.String(kEffectParameterName).value_or(L""),
                                    value.empty() ? 0.0F : value[0]);
    }
    return out;
}

// A layout's lists: its rooms, effects, trees, winds and light subsets.
void ListsOf(const gff4::Struct &root, LevelLayout &out)
{
    for (const gff4::Struct &room : root.Structs(kAreaRooms))
    {
        out.rooms.push_back({.id = room.Get<std::int32_t>(kAreaRoomId).value_or(0),
                             .file = room.String(kAreaRoomFile).value_or(L""),
                             .lower = PointOf(room.Floats(kAreaRoomLower)),
                             .upper = PointOf(room.Floats(kAreaRoomUpper))});
    }
    for (const gff4::Struct &effect : root.Structs(kAreaEffects))
    {
        if (FrameEffect read = EffectOf(effect); !read.name.empty() || !read.file.empty())
        {
            out.effects.push_back(std::move(read));
        }
    }
    for (const gff4::Struct &tree : root.Structs(kAreaTrees))
    {
        out.trees.push_back(tree.String(kAreaTreeFile).value_or(L""));
    }
    for (const gff4::Struct &wind : root.Structs(kAreaLocalWinds))
    {
        out.localWinds.push_back({.at = PointOf(wind.Floats(kPosition)),
                                  .file = wind.String(kWindFile).value_or(L""),
                                  .radius = wind.Get<float>(kWindRadius).value_or(0.0F)});
    }
    for (const gff4::Struct &subset : root.Structs(kAreaLightSubsets))
    {
        out.lightSubsets.push_back(subset.Numbers(kAreaLightSubsetEntry));
    }
}

} // namespace

Point Turned(const Quaternion &turn, const Point &point)
{
    // point + 2 u x (u x point + w point), u the quaternion's vector part.
    const Point u{turn[0], turn[1], turn[2]};
    const Point inner{(u[1] * point[2]) - (u[2] * point[1]) + (turn[3] * point[0]),
                      (u[2] * point[0]) - (u[0] * point[2]) + (turn[3] * point[1]),
                      (u[0] * point[1]) - (u[1] * point[0]) + (turn[3] * point[2])};
    return {point[0] + (2 * ((u[1] * inner[2]) - (u[2] * inner[1]))),
            point[1] + (2 * ((u[2] * inner[0]) - (u[0] * inner[2]))),
            point[2] + (2 * ((u[0] * inner[1]) - (u[1] * inner[0])))};
}

Quaternion Then(const Quaternion &inner, const Quaternion &outer)
{
    // outer times inner: inner's turn first.
    return {(outer[3] * inner[0]) + (outer[0] * inner[3]) + (outer[1] * inner[2]) - (outer[2] * inner[1]),
            (outer[3] * inner[1]) - (outer[0] * inner[2]) + (outer[1] * inner[3]) + (outer[2] * inner[0]),
            (outer[3] * inner[2]) + (outer[0] * inner[1]) - (outer[1] * inner[0]) + (outer[2] * inner[3]),
            (outer[3] * inner[3]) - (outer[0] * inner[0]) - (outer[1] * inner[1]) - (outer[2] * inner[2])};
}

std::vector<MeshChunk> Meshes(const gff4::File &mesh)
{
    std::vector<MeshChunk> chunks;
    const std::optional<gff4::Struct> root = mesh.Root();
    if (!root)
    {
        return chunks;
    }
    const std::span<const std::byte> vertices = root->ListBytes(kMeshVertices, gff4::Type::U8);
    // The indices are 16 bits each, in a list the file calls bytes as it does the vertices.
    const std::span<const std::byte> indices = root->ListBytes(kMeshIndices, gff4::Type::U8);
    for (const gff4::Struct &chunk : root->Structs(kMeshChunks))
    {
        if (std::optional<MeshChunk> read = ChunkOf(chunk, vertices, indices))
        {
            chunks.push_back(std::move(*read));
        }
    }
    return chunks;
}

Model ModelOf(const gff4::File &hierarchy)
{
    Model model;
    const std::optional<gff4::Struct> root = hierarchy.Root();
    if (!root)
    {
        return model;
    }
    model.meshFile = root->String(kModelMeshFile).value_or(L"");
    for (const gff4::Struct &top : root->Structs(kChildren))
    {
        MeshNodes(top, {}, {0, 0, 0, 1}, 0, model.nodes);
    }
    return model;
}

Layout LayoutOf(const gff4::File &layout)
{
    Layout out;
    if (const std::optional<gff4::Struct> root = layout.Root())
    {
        LightsUnder(*root, {}, {0, 0, 0, 1}, 0, out);
    }
    return out;
}

std::optional<LevelLayout> LevelLayoutOf(const gff4::File &layout)
{
    const std::optional<gff4::Struct> root = layout.Root();
    if (!root || root->Shape().Find(kAreaRooms) == nullptr || root->Shape().Find(kAtmosphere) == nullptr)
    {
        return std::nullopt;
    }
    LevelLayout out;
    out.id = root->Get<std::int32_t>(kAreaId).value_or(0);
    out.name = root->String(kAreaName).value_or(L"");
    out.layoutName = root->String(kAreaLayoutName).value_or(L"");
    out.center = PointOf(root->Floats(kAreaCenter));
    out.skyDome = root->String(kAreaSkyDome).value_or(L"");
    out.sunDirection = PointOf(root->Floats(kAreaSunDirection));
    out.sunColour = Four(root->Floats(kAreaSunColour), {});
    if (const std::vector<float> colour = root->Floats(kAreaSunColourOnCharacters); colour.size() >= 4)
    {
        out.sunColourOnCharacters = Four(colour, {});
    }
    if (const auto occluded = root->Get<std::uint8_t>(kAreaSunOccludedOnCharacters))
    {
        out.sunOccludedOnCharacters = *occluded != 0;
    }
    out.forceCharacterLighting = root->Get<std::int8_t>(kAreaForceCharacterLighting).value_or(0) != 0;
    if (const std::optional<gff4::Struct> atmosphere = root->Child(kAtmosphere))
    {
        out.atmosphere = FloatsByName(*atmosphere);
    }
    if (const std::optional<gff4::Struct> clouds = root->Child(kClouds))
    {
        out.clouds = FloatsByName(*clouds);
    }
    out.treeTint = Four(root->Floats(kAreaTreeTint), {1, 1, 1, 1});
    out.globalWind = root->String(kAreaGlobalWind).value_or(L"");
    if (root->Get<std::int8_t>(kAreaIsChunked).value_or(0) != 0)
    {
        out.chunks = Chunks{.rows = root->Get<std::int32_t>(kAreaChunkRows).value_or(0),
                            .columns = root->Get<std::int32_t>(kAreaChunkColumns).value_or(0),
                            .width = root->Get<float>(kAreaChunkWidth).value_or(0.0F),
                            .height = root->Get<float>(kAreaChunkHeight).value_or(0.0F)};
    }
    out.startPoint = root->String(kAreaStartPoint).value_or(L"");
    out.cutoffHeight = root->Get<std::int32_t>(kAreaCutoffHeight);
    if (const auto enabled = root->Get<std::int8_t>(kAreaCutoffEnabled))
    {
        out.cutoffEnabled = *enabled != 0;
    }
    out.minimapLower = PointOf(root->Floats(kAreaMinimapLower));
    out.minimapUpper = PointOf(root->Floats(kAreaMinimapUpper));
    ListsOf(*root, out);
    return out;
}

// A room's number and connections (DAOLib #186), read 2026-10-18 in the game's files (ghilana's
// extract of every .rml, 9,553 by name; tools/gff4.py): no .rml has EnvRoomID (3031) or another
// number of its own. The layout's room list (.arl, 3016) gives it, the same number the file's
// name ends in (lak302d_1325) and, for a terrain level's chunk, column << 16 | row (oth000d_2_0
// is 131072). EnvRoomVisibilityList (3047) and EnvRoomConnectivityList (3099) are both int32
// lists of those numbers. 987 rooms connect, to 1 to 35 rooms each (2,340 in all), none to
// itself; 30 (shl100d's among them) connect to a room they do not see. The Circle's basement,
// lak302d, connects every room; the Fade, oth000d, none. The engine keeps the list at CRoom
// +0xD0 and walks it room to room for perception (dev/COMBAT.md, "Perception through doors").
// A creature's room (+0x41C, its number +0x150) is the room of the static geometry a ray down
// from it hits, from the .arl's EnvStatPhys (3744, 3745), whose actors carry these numbers; the
// walk grid's cells do not (lib-agent-2's reading, dev/COLLISION.md, section 3).
std::optional<Room> RoomOf(const gff4::File &definition)
{
    const std::optional<gff4::Struct> root = definition.Root();
    if (!root || root->Shape().Find(kRoomModels) == nullptr)
    {
        return std::nullopt;
    }
    Room room;
    room.at = PointOf(root->Floats(kPosition));
    room.turn = QuaternionOf(root->Floats(kOrientation));
    room.sees = root->Numbers(kRoomSees);
    room.connects = root->Numbers(kRoomConnects);
    for (const gff4::Struct &model : root->Structs(kRoomModels))
    {
        Placed placed;
        placed.file = model.String(kModelFile).value_or(L"");
        placed.at = PointOf(model.Floats(kPosition));
        placed.turn = QuaternionOf(model.Floats(kOrientation));
        placed.scale = model.Get<float>(kModelScale).value_or(1.0F);
        placed.cutAwayOverride = model.Get<std::uint8_t>(kModelCutAwayOverride).value_or(0);
        for (const gff4::Struct &part : model.Structs(kModelLightmaps))
        {
            LightmapPart lightmap;
            lightmap.part = part.String(kLightmapPart).value_or(L"");
            lightmap.atlas = part.String(kLightmapAtlas).value_or(L"");
            if (const std::vector<float> place = part.Floats(kLightmapOffsetScale); place.size() == 4)
            {
                std::ranges::copy(place, lightmap.offsetScale.begin());
            }
            placed.parts.push_back(std::move(lightmap));
        }
        for (const gff4::Struct &param : model.Structs(kModelUserParams))
        {
            UserParam user;
            user.name = param.String(kUserParamName).value_or(L"");
            if (const std::vector<float> value = param.Floats(kUserParamValue); value.size() == 4)
            {
                std::ranges::copy(value, user.value.begin());
            }
            placed.userParams.push_back(std::move(user));
        }
        room.models.push_back(std::move(placed));
    }
    // A room's trees (DAOLib #182), read 2026-10-18 by lib-agent-2 in the exe: the room reader
    // (0x00A28D50) keeps each as a 0x50-byte record; an instance (0x005CBF21) is placed at the
    // room's place plus the room's turn of the tree's, turned about z alone, scaled uniformly and
    // tinted; the draw distance indexes the factors {1, 1.5, 2, 0.001 or 0.004} (0x00BF73E0) on
    // its level of detail's distances. Its types read 2026-10-18 in 6,738 of the game's .rml
    // (tools/gff4.py --templates; the Fade's oth000d among them): Position a vector3,
    // Orientation a quaternion, the scale a float, the model an int32, the tint a vector4,
    // the draw distance an int8; 175,929 trees, every turn of length 1, scales 0.07 to 0.61,
    // models 0 to 25, draw distances 0 to 3. 20 files (lak519d_0 among them) lack the draw
    // distance.
    for (const gff4::Struct &node : root->Structs(kRoomTrees))
    {
        room.trees.push_back({.at = PointOf(node.Floats(kPosition)),
                              .turn = QuaternionOf(node.Floats(kOrientation)),
                              .scale = node.Get<float>(kTreeScale).value_or(1.0F),
                              .tree = node.Get<std::int32_t>(kTreeModel).value_or(0),
                              .tint = Four(node.Floats(kTreeTint), {}),
                              .drawDistance = node.Get<std::int8_t>(kTreeDrawDistance).value_or(0)});
    }
    return room;
}

Point InLevel(const Room &room, const Placed &placed, const Point &point)
{
    const Point inRoom =
        Turned(placed.turn, {point[0] * placed.scale, point[1] * placed.scale, point[2] * placed.scale});
    const Point turned =
        Turned(room.turn, {inRoom[0] + placed.at[0], inRoom[1] + placed.at[1], inRoom[2] + placed.at[2]});
    return {turned[0] + room.at[0], turned[1] + room.at[1], turned[2] + room.at[2]};
}

void AddTriangles(const Room &room, const Placed &placed, const Model &model, std::span<const MeshChunk> chunks,
                  const std::function<std::uint32_t(const std::wstring &)> &atlasOf, std::vector<LitTriangle> &out)
{
    for (const MeshNode &node : model.nodes)
    {
        const auto part = std::ranges::find(placed.parts, node.part, &LightmapPart::part);
        const auto chunk = std::ranges::find(chunks, node.chunk, &MeshChunk::name);
        if (part == placed.parts.end() || chunk == chunks.end() || chunk->lightUv.empty())
        {
            continue;
        }
        const std::uint32_t atlas = atlasOf(part->atlas);
        const auto corner = [&](std::uint32_t index) {
            const Point inModel = Turned(node.turn, chunk->positions[index]);
            return InLevel(room, placed, {inModel[0] + node.at[0], inModel[1] + node.at[1], inModel[2] + node.at[2]});
        };
        const auto mapped = [&](std::uint32_t index) {
            const Uv &uv = chunk->lightUv[index];
            return Uv{(uv[0] * part->offsetScale[2]) + part->offsetScale[0],
                      (uv[1] * part->offsetScale[3]) + part->offsetScale[1]};
        };
        for (std::size_t i = 0; i + 2 < chunk->indices.size(); i += 3)
        {
            const std::uint32_t a = chunk->indices[i];
            const std::uint32_t b = chunk->indices[i + 1];
            const std::uint32_t c = chunk->indices[i + 2];
            out.push_back({{corner(a), corner(b), corner(c)}, {mapped(a), mapped(b), mapped(c)}, atlas});
        }
    }
}

} // namespace dao::core::level
