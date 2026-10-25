#include "dao/core/Model.h"

#include "dao/core/Bytes.h"
#include "dao/core/ModelNodes.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace dao::core::model
{

namespace
{

constexpr std::uint32_t kEndType = 0xFFFFFFFF; // D3DDECL_END's type, D3DDECLTYPE_UNUSED as the file keeps it
constexpr std::int64_t kLastFormat = 16;
constexpr std::int64_t kLastUsage = 13;
constexpr std::size_t kNodesDeep = 64; // a model's tree deeper than this is not one

// A whole-number field of whatever width and sign the file gave it.
std::optional<std::int64_t> Number(const gff4::Struct &from, std::uint32_t label)
{
    if (const auto value = from.Get<std::uint32_t>(label))
    {
        return *value;
    }
    if (const auto value = from.Get<std::int32_t>(label))
    {
        return *value;
    }
    if (const auto value = from.Get<std::uint16_t>(label))
    {
        return *value;
    }
    if (const auto value = from.Get<std::int16_t>(label))
    {
        return *value;
    }
    if (const auto value = from.Get<std::uint8_t>(label))
    {
        return *value;
    }
    if (const auto value = from.Get<std::int8_t>(label))
    {
        return *value;
    }
    return std::nullopt;
}

std::uint32_t U32(const gff4::Struct &from, std::uint32_t label)
{
    return static_cast<std::uint32_t>(Number(from, label).value_or(0));
}

std::optional<Element> ElementOf(const gff4::Struct &declarator)
{
    const std::int64_t type = Number(declarator, kDeclaratorType).value_or(kEndType);
    const std::int64_t usage = Number(declarator, kDeclaratorUsage).value_or(kEndType);
    if (type < 0 || type > kLastFormat || usage < 0 || usage > kLastUsage)
    {
        return std::nullopt; // the end marker, or a type no declaration has
    }
    return Element{.stream = static_cast<std::uint16_t>(Number(declarator, kDeclaratorStream).value_or(0)),
                   .offset = static_cast<std::uint16_t>(Number(declarator, kDeclaratorOffset).value_or(0)),
                   .format = static_cast<Format>(type),
                   .method = static_cast<std::uint8_t>(Number(declarator, kDeclaratorMethod).value_or(0)),
                   .usage = static_cast<Usage>(usage),
                   .usageIndex = static_cast<std::uint8_t>(Number(declarator, kDeclaratorUsageIndex).value_or(0))};
}

// The box's corners and the sphere are four floats each, the box's w 1 and the sphere's w its
// radius. The engine places a level chunk by the sphere for its lights: the node's world matrix
// times the centre, with the radius unscaled (dev/EFFECTS.md, "For a level mesh"; DAOLib #208).
std::optional<Bounds> BoundsOf(const gff4::Struct &chunk)
{
    const std::optional<gff4::Struct> bounds = chunk.Child(kChunkBounds);
    if (!bounds)
    {
        return std::nullopt;
    }
    const std::vector<float> low = bounds->Floats(kBoundsBoxMin);
    const std::vector<float> high = bounds->Floats(kBoundsBoxMax);
    const std::vector<float> sphere = bounds->Floats(kBoundsSphere);
    if (low.size() < 3 || high.size() < 3 || sphere.size() < 4)
    {
        return std::nullopt;
    }
    return Bounds{.boxMin = {low[0], low[1], low[2]},
                  .boxMax = {high[0], high[1], high[2]},
                  .centre = {sphere[0], sphere[1], sphere[2]},
                  .radius = sphere[3]};
}

std::optional<Chunk> ChunkOf(const gff4::Struct &from, std::span<const std::byte> vertices,
                             std::span<const std::byte> indices)
{
    Chunk chunk;
    chunk.name = from.String(kName).value_or(L"");
    chunk.vertexSize = U32(from, kChunkVertexSize);
    chunk.vertexCount = U32(from, kChunkVertexCount);
    chunk.indexCount = U32(from, kChunkIndexCount);
    chunk.primitive = U32(from, kChunkPrimitive);
    chunk.indexFormat = U32(from, kChunkIndexFormat);
    chunk.baseVertex = U32(from, kChunkBaseVertex);
    chunk.vertexOffset = U32(from, kChunkVertexOffset);
    chunk.minIndex = U32(from, kChunkMinIndex);
    chunk.verticesReferenced = U32(from, kChunkVerticesReferenced);
    chunk.startIndex = U32(from, kChunkStartIndex);
    chunk.bounds = BoundsOf(from);
    for (const gff4::Struct &declarator : from.Structs(kChunkDeclarators))
    {
        if (const std::optional<Element> element = ElementOf(declarator))
        {
            chunk.declaration.push_back(*element);
        }
    }
    const std::size_t vertexBytes = static_cast<std::size_t>(chunk.vertexSize) * chunk.vertexCount;
    const std::size_t indexSize = chunk.indexFormat == 1 ? 4 : 2;
    const std::size_t indexStart = static_cast<std::size_t>(chunk.startIndex) * indexSize;
    const std::size_t indexBytes = static_cast<std::size_t>(chunk.indexCount) * indexSize;
    if (chunk.vertexOffset > vertices.size() || vertexBytes > vertices.size() - chunk.vertexOffset ||
        indexStart > indices.size() || indexBytes > indices.size() - indexStart)
    {
        return std::nullopt;
    }
    chunk.vertices = vertices.subspan(chunk.vertexOffset, vertexBytes);
    chunk.indices = indices.subspan(indexStart, indexBytes);
    return chunk;
}

// One component of an element: its value as a float, normalised where the format is.
float Component(std::span<const std::byte> vertex, std::size_t at, Format format, std::size_t i)
{
    switch (format)
    {
    case Format::Float1:
    case Format::Float2:
    case Format::Float3:
    case Format::Float4:
        return ReadAt<float>(vertex, at + (i * 4)).value_or(0);
    case Format::Float16x2:
    case Format::Float16x4:
        return Half(ReadAt<std::uint16_t>(vertex, at + (i * 2)).value_or(0));
    case Format::UByte4:
        return ReadAt<std::uint8_t>(vertex, at + i).value_or(0);
    case Format::UByte4N:
        return static_cast<float>(ReadAt<std::uint8_t>(vertex, at + i).value_or(0)) / 255.0F;
    case Format::D3DColor: {
        // A D3DCOLOR is ARGB in a little-endian word: bytes blue, green, red, alpha.
        constexpr std::array<std::size_t, 4> kByteOf{2, 1, 0, 3};
        return static_cast<float>(ReadAt<std::uint8_t>(vertex, at + kByteOf.at(i)).value_or(0)) / 255.0F;
    }
    case Format::Short2:
    case Format::Short4:
        return ReadAt<std::int16_t>(vertex, at + (i * 2)).value_or(0);
    case Format::Short2N:
    case Format::Short4N:
        return std::max(-1.0F, static_cast<float>(ReadAt<std::int16_t>(vertex, at + (i * 2)).value_or(0)) / 32767.0F);
    case Format::UShort2N:
    case Format::UShort4N:
        return static_cast<float>(ReadAt<std::uint16_t>(vertex, at + (i * 2)).value_or(0)) / 65535.0F;
    case Format::UDec3:
    case Format::Dec3N: {
        const std::uint32_t word = ReadAt<std::uint32_t>(vertex, at).value_or(0);
        const std::uint32_t bits = (word >> (10 * i)) & 0x3FFU;
        if (format == Format::UDec3)
        {
            return static_cast<float>(bits);
        }
        const int value = bits >= 512 ? static_cast<int>(bits) - 1024 : static_cast<int>(bits);
        return std::max(-1.0F, static_cast<float>(value) / 511.0F);
    }
    }
    return 0;
}

std::size_t ComponentsOf(Format format)
{
    switch (format)
    {
    case Format::Float1:
        return 1;
    case Format::Float2:
    case Format::Short2:
    case Format::Short2N:
    case Format::UShort2N:
    case Format::Float16x2:
        return 2;
    case Format::Float3:
    case Format::UDec3:
    case Format::Dec3N:
        return 3;
    default:
        return 4;
    }
}

// The node's own transform: its children of those kinds.
void TransformOf(const gff4::Struct &from, Node &node)
{
    for (const gff4::Struct &child : from.Structs(kChildren))
    {
        if (child.Label() == "trsl")
        {
            if (const std::vector<float> at = child.Floats(kTranslation); at.size() >= 3)
            {
                node.translation = {at[0], at[1], at[2]};
            }
        }
        else if (child.Label() == "rota")
        {
            if (const std::vector<float> turn = child.Floats(kRotation); turn.size() >= 4)
            {
                node.rotation = {turn[0], turn[1], turn[2], turn[3]};
            }
        }
        else if (child.Label() == "scal")
        {
            node.scale = child.Get<float>(kScale);
        }
        // A node's exports, read 2026-10-18 (DAOLib #177): plc_smferdrb_01_0.mmh's DoorLow
        // exports doorlow_translation (6) and doorlow_rotation (8), the names
        // ip_door.closetoopen.ani's channels go by; lib-agent-2 read that a placeable's clip
        // drives the node so, rigidly.
        else if (child.Label() == "xprt")
        {
            node.exports.push_back({.name = child.String(kExportName).value_or(L""),
                                    .controllerType = child.Get<std::uint32_t>(kExportControllerType).value_or(0),
                                    .variableType = child.Get<std::uint32_t>(kExportVariableType).value_or(0),
                                    .controllerIndex = child.Get<std::uint32_t>(kExportControllerIndex).value_or(0)});
        }
    }
}

// A float field the file may keep as a whole number.
float Real(const gff4::Struct &from, std::uint32_t label)
{
    if (const auto value = from.Get<float>(label))
    {
        return *value;
    }
    return static_cast<float>(Number(from, label).value_or(0));
}

bool Flag(const gff4::Struct &from, std::uint32_t label)
{
    return Number(from, label).value_or(0) != 0;
}

std::optional<std::array<float, 3>> Triple(const gff4::Struct &from, std::uint32_t label)
{
    const std::vector<float> values = from.Floats(label);
    if (values.size() < 3 || !std::isfinite(values[0]) || !std::isfinite(values[1]) || !std::isfinite(values[2]))
    {
        return std::nullopt;
    }
    return std::array<float, 3>{values[0], values[1], values[2]};
}

// A mesh group of a cloth: a chunk with its own vertex and index bytes. One whose counts are the
// file's null (0xFFFFFFFF), as an unused MMHClothMeshGroupStruct is, is none.
std::optional<Chunk> RestOf(const gff4::Struct &group)
{
    if (group.Label() != "msgr" || U32(group, kChunkVertexCount) == 0xFFFFFFFF || U32(group, kChunkVertexCount) == 0)
    {
        return std::nullopt;
    }
    return ChunkOf(group, group.ListBytes(kMeshVertices, gff4::Type::U8),
                   group.ListBytes(kMeshIndices, gff4::Type::U8));
}

// Read 2026-10-24 (DAOLib #261, asked by paragon-agent for DAOParagonEngine #265) in
// prp_bannerred_04_0.mmh: the node's MMHClothMeshGroupStruct has every field at its null, and its
// mesh at rest is an msgr among its children, with a .msh chunk's fields and its own
// MeshVertexData and MeshIndexData (16 vertices of 72 bytes, 42 32-bit indices). The group field is
// taken where the children have none.
Cloth ClothOf(const gff4::Struct &from)
{
    Cloth cloth;
    cloth.material = from.String(kMaterialObject).value_or(L"");
    cloth.part = from.String(kNodeId).value_or(L"");
    std::array<float *, 11> reals{&cloth.thickness,
                                  &cloth.density,
                                  &cloth.bendingStiffness,
                                  &cloth.stretchingStiffness,
                                  &cloth.dampingCoefficient,
                                  &cloth.friction,
                                  &cloth.pressure,
                                  &cloth.tearFactor,
                                  &cloth.collisionResponseCoefficient,
                                  &cloth.attachmentResponseCoefficient,
                                  &cloth.attachmentTearFactor};
    for (std::size_t i = 0; i < reals.size(); ++i)
    {
        *reals[i] = Real(from, kClothThickness + static_cast<std::uint32_t>(i));
    }
    cloth.solverIterations = U32(from, kClothSolverIterations);
    cloth.externalAcceleration = Triple(from, kClothExternalAcceleration).value_or(std::array<float, 3>{});
    cloth.wakeUpCounter = Real(from, kClothWakeUpCounter);
    cloth.sleepLinearVelocity = Real(from, kClothSleepLinearVelocity);
    std::array<bool *, kClothFlagLast - kClothFlagFirst + 1> flags{
        &cloth.flags.pressure,        &cloth.flags.isStatic,          &cloth.flags.disableCollision,
        &cloth.flags.selfCollision,   &cloth.flags.visualization,     &cloth.flags.gravity,
        &cloth.flags.bending,         &cloth.flags.bendingOrtho,      &cloth.flags.damping,
        &cloth.flags.collisionTwoWay, &cloth.flags.triangleCollision, &cloth.flags.tearable,
        &cloth.flags.hardware,        &cloth.flags.comDamping};
    for (std::size_t i = 0; i < flags.size(); ++i)
    {
        *flags[i] = Flag(from, kClothFlagFirst + static_cast<std::uint32_t>(i));
    }
    for (const gff4::Struct &child : from.Structs(kChildren))
    {
        if (child.Label() == "catl")
        {
            for (const gff4::Struct &held : child.Structs(kChildren))
            {
                cloth.attachments.push_back(ClothAttachment{.type = U32(held, kClothAttachmentType),
                                                            .twoWay = Flag(held, kClothAttachmentTwoWay),
                                                            .tearable = Flag(held, kClothAttachmentTearable),
                                                            .shape = held.String(kClothAttachmentShape).value_or(L""),
                                                            .vertex = U32(held, kClothAttachmentVertex),
                                                            .localPosition = Triple(held, kClothAttachmentLocalPos)});
            }
        }
        else if (!cloth.rest)
        {
            cloth.rest = RestOf(child);
        }
    }
    if (!cloth.rest)
    {
        if (const std::optional<gff4::Struct> group = from.Child(kClothMeshGroup))
        {
            cloth.rest = RestOf(*group);
        }
    }
    return cloth;
}

bool IsNode(const gff4::Struct &from)
{
    return from.Shape().Find(kChildren) != nullptr && from.Shape().Find(kMmhName) != nullptr;
}

void Walk(const gff4::Struct &from, int parent, std::size_t deep, Hierarchy &out, std::vector<gff4::Struct> &structs)
{
    if (deep > kNodesDeep)
    {
        return;
    }
    const int index = static_cast<int>(out.nodes.size());
    Node node;
    node.name = from.String(kMmhName).value_or(L"");
    node.kind = from.Label();
    node.parent = parent;
    TransformOf(from, node);
    if (node.kind == "crst")
    {
        if (const auto hook = Number(from, kCrustHookId))
        {
            node.crustHook = static_cast<std::uint8_t>(*hook);
        }
    }
    if (node.kind == "wtrl")
    {
        node.trail = WeaponTrail{.material = from.String(kMaterialObject).value_or(L""),
                                 .segmentLength = from.Get<float>(kTrailSegmentLength).value_or(0.0F),
                                 .duration = from.Get<float>(kTrailDuration).value_or(0.0F),
                                 .useVariationTint = Number(from, kUseVariationTint).value_or(0) != 0};
    }
    if (node.kind == "nclt")
    {
        node.cloth = ClothOf(from);
    }
    if (const auto bone = Number(from, kBoneIndex); bone && *bone >= 0)
    {
        node.bone = static_cast<int>(*bone);
    }
    if (const auto chunk = from.String(kMeshGroupName); chunk && !chunk->empty())
    {
        MeshRef mesh{.chunk = *chunk,
                     .material = from.String(kMaterialObject).value_or(L""),
                     .part = from.String(kNodeId).value_or(L""),
                     .bonesUsed = {},
                     .cutAway = Number(from, kMeshCutAway).value_or(0) != 0};
        for (const std::uint32_t used : from.Numbers(kBonesUsed))
        {
            mesh.bonesUsed.push_back(static_cast<std::int32_t>(used));
        }
        node.mesh = std::move(mesh);
    }
    out.nodes.push_back(std::move(node));
    structs.push_back(from);
    if (parent >= 0)
    {
        out.nodes[static_cast<std::size_t>(parent)].children.push_back(index);
    }
    for (const gff4::Struct &child : from.Structs(kChildren))
    {
        if (IsNode(child))
        {
            Walk(child, index, deep + 1, out, structs);
        }
    }
}

} // namespace

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

std::size_t SizeOf(Format format)
{
    switch (format)
    {
    case Format::Float1:
    case Format::D3DColor:
    case Format::UByte4:
    case Format::UByte4N:
    case Format::Short2:
    case Format::Short2N:
    case Format::UShort2N:
    case Format::UDec3:
    case Format::Dec3N:
    case Format::Float16x2:
        return 4;
    case Format::Float2:
    case Format::Short4:
    case Format::Short4N:
    case Format::UShort4N:
    case Format::Float16x4:
        return 8;
    case Format::Float3:
        return 12;
    case Format::Float4:
        return 16;
    }
    return 0;
}

const Element *Chunk::Find(Usage usage, std::uint8_t usageIndex) const
{
    const auto found = std::ranges::find_if(declaration, [&](const Element &element) {
        return element.usage == usage && element.usageIndex == usageIndex;
    });
    return found == declaration.end() ? nullptr : &*found;
}

std::optional<Mesh> MeshOf(const gff4::File &mesh)
{
    const std::optional<gff4::Struct> root = mesh.Root();
    if (!root || root->Shape().Find(kMeshChunks) == nullptr)
    {
        return std::nullopt;
    }
    Mesh out;
    out.name = root->String(kName).value_or(L"");
    const std::span<const std::byte> vertices = root->ListBytes(kMeshVertices, gff4::Type::U8);
    // The indices are a list the file calls bytes, as it does the vertices.
    const std::span<const std::byte> indices = root->ListBytes(kMeshIndices, gff4::Type::U8);
    for (const gff4::Struct &chunk : root->Structs(kMeshChunks))
    {
        if (std::optional<Chunk> read = ChunkOf(chunk, vertices, indices))
        {
            out.chunks.push_back(std::move(*read));
        }
    }
    return out;
}

std::vector<Value> Floats(const Chunk &chunk, const Element &element)
{
    std::vector<Value> out;
    if (chunk.vertexSize == 0 || element.offset + SizeOf(element.format) > chunk.vertexSize)
    {
        return out;
    }
    const std::size_t components = ComponentsOf(element.format);
    out.reserve(chunk.vertexCount);
    for (std::size_t v = 0; v < chunk.vertexCount; ++v)
    {
        const std::span<const std::byte> vertex = chunk.vertices.subspan(v * chunk.vertexSize, chunk.vertexSize);
        Value value{0, 0, 0, 1};
        for (std::size_t i = 0; i < components; ++i)
        {
            value.at(i) = Component(vertex, element.offset, element.format, i);
        }
        out.push_back(value);
    }
    return out;
}

std::vector<std::array<std::int16_t, 4>> Shorts(const Chunk &chunk, const Element &element)
{
    std::vector<std::array<std::int16_t, 4>> out;
    if (element.format != Format::Short2 && element.format != Format::Short4)
    {
        return out;
    }
    const std::size_t components = ComponentsOf(element.format);
    if (chunk.vertexSize == 0 || element.offset + (components * 2) > chunk.vertexSize)
    {
        return out;
    }
    out.reserve(chunk.vertexCount);
    for (std::size_t v = 0; v < chunk.vertexCount; ++v)
    {
        std::array<std::int16_t, 4> value{};
        for (std::size_t i = 0; i < components; ++i)
        {
            value.at(i) =
                ReadAt<std::int16_t>(chunk.vertices, (v * chunk.vertexSize) + element.offset + (i * 2)).value_or(0);
        }
        out.push_back(value);
    }
    return out;
}

std::vector<std::uint32_t> Indices(const Chunk &chunk)
{
    std::vector<std::uint32_t> out;
    out.reserve(chunk.indexCount);
    const bool wide = chunk.indexFormat == 1;
    for (std::size_t i = 0; i < chunk.indexCount; ++i)
    {
        const std::uint32_t index = wide ? ReadAt<std::uint32_t>(chunk.indices, i * 4).value_or(0)
                                         : ReadAt<std::uint16_t>(chunk.indices, i * 2).value_or(0);
        out.push_back(index + chunk.baseVertex);
    }
    return out;
}

std::optional<Hierarchy> HierarchyOf(const gff4::File &hierarchy)
{
    const std::optional<gff4::Struct> root = hierarchy.Root();
    if (!root || root->Shape().Find(kModelMeshFile) == nullptr)
    {
        return std::nullopt;
    }
    Hierarchy out;
    out.name = root->String(kMmhName).value_or(L"");
    out.meshFile = root->String(kModelMeshFile).value_or(L"");
    out.totalBones = U32(*root, kTotalBones);
    std::vector<gff4::Struct> structs;
    for (const gff4::Struct &top : root->Structs(kChildren))
    {
        if (IsNode(top))
        {
            Walk(top, -1, 0, out, structs);
        }
    }
    out.bones.assign(out.totalBones, -1);
    for (std::size_t i = 0; i < out.nodes.size(); ++i)
    {
        if (const std::optional<int> bone = out.nodes[i].bone; bone && std::cmp_less(*bone, out.bones.size()))
        {
            out.bones[static_cast<std::size_t>(*bone)] = static_cast<int>(i);
        }
    }
    return out;
}

std::vector<gff4::Struct> detail::NodeStructs(const gff4::File &hierarchy)
{
    std::vector<gff4::Struct> structs;
    const std::optional<gff4::Struct> root = hierarchy.Root();
    if (!root)
    {
        return structs;
    }
    Hierarchy unused;
    for (const gff4::Struct &top : root->Structs(kChildren))
    {
        if (IsNode(top))
        {
            Walk(top, -1, 0, unused, structs);
        }
    }
    return structs;
}

} // namespace dao::core::model
