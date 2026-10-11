#include "dao/core/Level.h"

#include "dao/core/Bytes.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <functional>
#include <limits>
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
constexpr std::uint32_t kModelScale = 3059;
constexpr std::uint32_t kModelFile = 3063;
constexpr std::uint32_t kLightmapPart = 3235;
constexpr std::uint32_t kLightmapAtlas = 3323;
constexpr std::uint32_t kLightmapOffsetScale = 3324;
constexpr std::uint32_t kModelLightmaps = 3326;
constexpr std::uint32_t kNodeId = 6004;
constexpr std::uint32_t kModelMeshFile = 6005;
constexpr std::uint32_t kNodeMeshChunk = 6006;
constexpr std::uint32_t kTranslation = 6047;
constexpr std::uint32_t kRotation = 6048;
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

float Half(std::uint16_t half)
{
    const std::uint32_t sign = (half >> 15) & 1U;
    const std::uint32_t exponent = (half >> 10) & 31U;
    const std::uint32_t fraction = half & 1023U;
    float value = 0;
    if (exponent == 0)
    {
        value = std::ldexp(static_cast<float>(fraction), -24);
    }
    else if (exponent == 31)
    {
        value = fraction == 0 ? std::numeric_limits<float>::infinity() : std::numeric_limits<float>::quiet_NaN();
    }
    else
    {
        value = std::ldexp(static_cast<float>(fraction | 1024U), static_cast<int>(exponent) - 25);
    }
    return sign != 0 ? -value : value;
}

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
            to[i] = Half(*value);
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
// at all is the room's list's to say, not this.
std::pair<Declared, Declared> DeclaredIn(const gff4::Struct &chunk)
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
    return {position, second ? second : first};
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
    const auto [position, light] = DeclaredIn(chunk);
    if (!position)
    {
        return std::nullopt;
    }
    out.positions.resize(count);
    out.lightUv.resize(light ? count : 0);
    for (std::size_t i = 0; i < count; ++i)
    {
        const std::size_t at = start + (i * size);
        if (!Element(vertices, at + position->first, position->second, 3, out.positions[i].data()) ||
            (light && !Element(vertices, at + light->first, light->second, 2, out.lightUv[i].data())))
        {
            return std::nullopt;
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

std::optional<Point> ProbeUnder(const gff4::Struct &node, std::size_t deep)
{
    if (deep > kNodesDeep)
    {
        return std::nullopt;
    }
    for (const gff4::Struct &child : node.Structs(kChildren))
    {
        if (child.Label() == "nlpr")
        {
            Point at{};
            Quaternion turn{0, 0, 0, 1};
            PlaceOf(child, at, turn);
            return at;
        }
        if (child.Shape().Find(kChildren) != nullptr)
        {
            if (const std::optional<Point> found = ProbeUnder(child, deep + 1))
            {
                return found;
            }
        }
    }
    return std::nullopt;
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

std::optional<Point> ProbeOf(const gff4::File &layout)
{
    const std::optional<gff4::Struct> root = layout.Root();
    return root ? ProbeUnder(*root, 0) : std::nullopt;
}

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
    for (const gff4::Struct &model : root->Structs(kRoomModels))
    {
        Placed placed;
        placed.file = model.String(kModelFile).value_or(L"");
        placed.at = PointOf(model.Floats(kPosition));
        placed.turn = QuaternionOf(model.Floats(kOrientation));
        placed.scale = model.Get<float>(kModelScale).value_or(1.0F);
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
        room.models.push_back(std::move(placed));
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
