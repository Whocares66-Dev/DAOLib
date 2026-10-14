// A model's files written back from what was read and what was changed
// (DAOLib #117): a mesh with chunks' vertices and triangles replaced, and a
// hierarchy with nodes' transforms, materials and bones changed and its
// bounding boxes made again. Both go through gff4::Editor, so nothing they do
// not change moves. New chunks and mesh nodes are added too (DAOLib #118), for
// a part split past a chunk's bones or a piece made in an editor: each a copy
// of a struct the file already has, renamed and put at the end of its list.
//
// What a new chunk and a new mesh node must agree with, read 2026-10-14 over
// all 17,792 .mmh the game takes and their .msh: a mesh node names its chunk
// by name (MMHMeshGroupName); in 13,651 files the mesh nodes and the chunks
// are in the same order, in 34 not, so the order is not what binds them. A
// mesh node's id (MMHID) repeats within 14 files. Its children are its
// attributes and its transform, and in 7,090 of them animation exports too,
// which a new node does not take. Every child in the children lists is a struct (flags
// 0x4000), none null.
//
// The layout a mesh is packed in again is the game's, read 2026-10-14 over
// 3,000 of the .msh it takes (tools/gff4.py): the chunks' vertices in the
// chunks' order, each chunk's followed by 16 - (end % 16) bytes of nothing,
// 1 to 16, the last's too; the indices one after another from start index 0,
// followed the same way. A chunk's bounds are the box of its places and the
// sphere about the box's centre through its corners (the ogre's body:
// centre (0, -0.0481, 1.684), radius 2.7862, its box's half diagonal). The
// three chunks with 32-bit indices are each their file's only chunk.
#include "dao/core/Level.h"
#include "dao/core/Model.h"
#include "dao/core/ModelNodes.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <iterator>
#include <limits>
#include <string>
#include <utility>

namespace dao::core::model
{

namespace
{

constexpr std::size_t kAlign = 16;
constexpr std::uint32_t kMost16BitVertices = 65535;

// A float rounded to the nearest half float, ties to even; Half widens it back exactly.
std::uint16_t ToHalf(float value)
{
    const auto bits = std::bit_cast<std::uint32_t>(value);
    const auto sign = static_cast<std::uint16_t>((bits >> 16) & 0x8000U);
    const std::uint32_t exponent = (bits >> 23) & 0xFFU;
    std::uint32_t mantissa = bits & 0x7FFFFFU;
    if (exponent == 0xFF)
    {
        return static_cast<std::uint16_t>(sign | 0x7C00U | (mantissa != 0 ? 0x200U : 0U));
    }
    const int unbiased = static_cast<int>(exponent) - 127;
    if (unbiased > 15)
    {
        return static_cast<std::uint16_t>(sign | 0x7C00U); // too large: infinity
    }
    if (unbiased >= -14)
    {
        // A normal half: 10 bits of mantissa, the 13 below them rounded.
        std::uint32_t half = (static_cast<std::uint32_t>(unbiased + 15) << 10) | (mantissa >> 13);
        const std::uint32_t rest = mantissa & 0x1FFFU;
        if (rest > 0x1000U || (rest == 0x1000U && (half & 1U) != 0))
        {
            ++half; // a carry into the exponent is right, to infinity at the top
        }
        return static_cast<std::uint16_t>(sign | half);
    }
    if (unbiased < -25)
    {
        return sign; // below half the least subnormal: zero
    }
    // A subnormal half: the mantissa with its leading 1, shifted down and rounded.
    mantissa |= 0x800000U;
    const auto shift = static_cast<std::uint32_t>(-14 - unbiased + 13);
    std::uint32_t half = mantissa >> shift;
    const std::uint32_t rest = mantissa & ((1U << shift) - 1U);
    const std::uint32_t midway = 1U << (shift - 1U);
    if (rest > midway || (rest == midway && (half & 1U) != 0))
    {
        ++half;
    }
    return static_cast<std::uint16_t>(sign | half);
}

// A chunk's name for a message: the game's are ASCII.
std::string Narrow(const std::wstring &name)
{
    std::string out;
    out.reserve(name.size());
    for (const wchar_t c : name)
    {
        out.push_back(c < 0x80 ? static_cast<char>(c) : '?');
    }
    return out;
}

template <class T> void Put(std::vector<std::byte> &out, std::size_t at, T value)
{
    std::memcpy(out.data() + at, &value, sizeof(T));
}

std::int16_t ToShort(float value)
{
    return static_cast<std::int16_t>(std::clamp(std::lround(value), -32768L, 32767L));
}

std::uint8_t ToByte(float value)
{
    return static_cast<std::uint8_t>(std::clamp(std::lround(value), 0L, 255L));
}

// One element's value written into a vertex, the inverse of what Floats reads.
void Encode(std::vector<std::byte> &out, std::size_t at, Format format, const Value &value)
{
    switch (format)
    {
    case Format::Float1:
    case Format::Float2:
    case Format::Float3:
    case Format::Float4:
        for (std::size_t i = 0; i * 4 < SizeOf(format); ++i)
        {
            Put(out, at + (i * 4), value.at(i));
        }
        return;
    case Format::Float16x2:
    case Format::Float16x4:
        for (std::size_t i = 0; i * 2 < SizeOf(format); ++i)
        {
            Put(out, at + (i * 2), ToHalf(value.at(i)));
        }
        return;
    case Format::Short2:
    case Format::Short4:
        for (std::size_t i = 0; i * 2 < SizeOf(format); ++i)
        {
            Put(out, at + (i * 2), ToShort(value.at(i)));
        }
        return;
    case Format::Short2N:
    case Format::Short4N:
        for (std::size_t i = 0; i * 2 < SizeOf(format); ++i)
        {
            Put(out, at + (i * 2), ToShort(std::clamp(value.at(i), -1.0F, 1.0F) * 32767.0F));
        }
        return;
    case Format::UShort2N:
    case Format::UShort4N:
        for (std::size_t i = 0; i * 2 < SizeOf(format); ++i)
        {
            const long scaled = std::lround(std::clamp(value.at(i), 0.0F, 1.0F) * 65535.0F);
            Put(out, at + (i * 2), static_cast<std::uint16_t>(scaled));
        }
        return;
    case Format::UByte4:
        for (std::size_t i = 0; i < 4; ++i)
        {
            Put(out, at + i, ToByte(value.at(i)));
        }
        return;
    case Format::UByte4N:
        for (std::size_t i = 0; i < 4; ++i)
        {
            Put(out, at + i, ToByte(std::clamp(value.at(i), 0.0F, 1.0F) * 255.0F));
        }
        return;
    case Format::D3DColor: {
        constexpr std::array<std::size_t, 4> kByteOf{2, 1, 0, 3}; // red, green, blue, alpha in BGRA
        for (std::size_t i = 0; i < 4; ++i)
        {
            Put(out, at + kByteOf.at(i), ToByte(std::clamp(value.at(i), 0.0F, 1.0F) * 255.0F));
        }
        return;
    }
    case Format::UDec3:
    case Format::Dec3N: {
        std::uint32_t word = 0;
        for (std::size_t i = 0; i < 3; ++i)
        {
            const long bits = format == Format::UDec3 ? std::clamp(std::lround(value.at(i)), 0L, 1023L)
                                                      : std::lround(std::clamp(value.at(i), -1.0F, 1.0F) * 511.0F);
            word |= (static_cast<std::uint32_t>(bits) & 0x3FFU) << (10 * i);
        }
        Put(out, at, word);
        return;
    }
    }
}

// Whether a value given is the one read: each component equal, or both not numbers.
bool Same(const Value &read, const Value &given)
{
    return std::ranges::equal(read, given, [](float a, float b) { return a == b || (std::isnan(a) && std::isnan(b)); });
}

// A chunk as it will be written: its vertices' bytes and its indices, and the fields made of them.
struct Packed
{
    std::vector<Element> declaration;
    bool declarationChanged = false;
    std::uint32_t vertexSize = 0;
    std::uint32_t vertexCount = 0;
    std::vector<std::byte> vertices;
    std::vector<std::uint32_t> indices; // as the file keeps them, before the base vertex
    std::uint32_t indexFormat = 0;
    std::uint32_t baseVertex = 0;
    std::uint32_t minIndex = 0;
    std::uint32_t verticesReferenced = 0;
    bool changed = false;
    std::optional<std::wstring> added; // a new chunk's name
};

Packed AsItWas(const Chunk &chunk)
{
    Packed out;
    out.declaration = chunk.declaration;
    out.vertexSize = chunk.vertexSize;
    out.vertexCount = chunk.vertexCount;
    out.vertices.assign(chunk.vertices.begin(), chunk.vertices.end());
    for (const std::uint32_t index : Indices(chunk))
    {
        out.indices.push_back(index - chunk.baseVertex);
    }
    out.indexFormat = chunk.indexFormat;
    out.baseVertex = chunk.baseVertex;
    out.minIndex = chunk.minIndex;
    out.verticesReferenced = chunk.verticesReferenced;
    return out;
}

// The vertex a declaration makes, or the reason it makes none: elements that overlap.
std::string VertexSizeOf(const std::vector<Element> &declaration, std::uint32_t &size)
{
    std::vector<std::pair<std::size_t, std::size_t>> spans;
    for (const Element &element : declaration)
    {
        if (element.stream != 0)
        {
            return "an element in a stream other than 0, which no chunk of the game's has";
        }
        spans.emplace_back(element.offset, element.offset + SizeOf(element.format));
    }
    std::ranges::sort(spans);
    for (std::size_t i = 1; i < spans.size(); ++i)
    {
        if (spans[i].first < spans[i - 1].second)
        {
            return "two elements of the declaration overlap";
        }
    }
    size = spans.empty() ? 0 : static_cast<std::uint32_t>(spans.back().second);
    return {};
}

// The chunk's declaration and the vertex it makes.
std::string Layout(const Chunk &original, const ChunkData &given, Packed &out)
{
    out.declaration = given.declaration.empty() ? original.declaration : given.declaration;
    out.vertexSize = original.vertexSize;
    if (!given.declaration.empty())
    {
        if (std::string why = VertexSizeOf(out.declaration, out.vertexSize); !why.empty())
        {
            return why;
        }
    }
    else if (std::uint32_t needed = 0; !VertexSizeOf(out.declaration, needed).empty() || needed > out.vertexSize)
    {
        return "the original chunk's declaration does not fit its vertex";
    }
    if (given.values.size() != out.declaration.size())
    {
        return "a vector of values is wanted for each of the declaration's " + std::to_string(out.declaration.size()) +
               " elements, and " + std::to_string(given.values.size()) + " were given";
    }
    return {};
}

// The chunk's vertices, each element's values written at its place in each vertex.
std::string Vertices(const Chunk &original, const ChunkData &given, Packed &out)
{
    out.vertexCount = given.values.empty() ? 0 : static_cast<std::uint32_t>(given.values[0].size());
    // With the original's layout, its bytes are kept wherever a value is given back as it was
    // read: a NaN's payload, and any bytes between elements, stay as they were.
    const bool sameLayout = given.declaration.empty() && out.vertexSize == original.vertexSize;
    out.vertices.assign(std::size_t{out.vertexCount} * out.vertexSize, std::byte{0});
    if (sameLayout)
    {
        const std::size_t kept = std::min(out.vertices.size(), original.vertices.size());
        std::memcpy(out.vertices.data(), original.vertices.data(), kept);
    }
    for (std::size_t e = 0; e < out.declaration.size(); ++e)
    {
        const Element &element = out.declaration[e];
        const std::vector<Value> was = sameLayout ? Floats(original, element) : std::vector<Value>{};
        if (given.values[e].size() != out.vertexCount)
        {
            return "element " + std::to_string(e) + " has " + std::to_string(given.values[e].size()) + " values for " +
                   std::to_string(out.vertexCount) + " vertices";
        }
        for (std::size_t v = 0; v < out.vertexCount; ++v)
        {
            const Value &value = given.values[e][v];
            if (element.usage == Usage::BlendIndices && std::ranges::any_of(value, [](float i) {
                    return std::lround(i) >= static_cast<long>(kMostBonesInChunk);
                }))
            {
                return "a blend index of " + std::to_string(kMostBonesInChunk) +
                       " or more; the game's own meshes use at most " + std::to_string(kMostBonesInChunk) +
                       " bones in one chunk";
            }
            if (v < was.size() && Same(was[v], value))
            {
                continue;
            }
            Encode(out.vertices, (v * out.vertexSize) + element.offset, element.format, value);
        }
    }
    return {};
}

// The chunk's triangles, and the fields made of them.
std::string Triangles(const Chunk &original, const ChunkData &given, Packed &out)
{
    if (given.indices.size() % 3 != 0)
    {
        return "the indices are no whole number of triangles";
    }
    out.indices = given.indices;
    if (std::ranges::any_of(out.indices, [&](std::uint32_t i) { return i >= out.vertexCount; }))
    {
        return "an index past the chunk's " + std::to_string(out.vertexCount) + " vertices";
    }
    out.indexFormat = out.vertexCount > kMost16BitVertices ? 1 : original.indexFormat;
    out.baseVertex = 0;
    const auto [low, high] = std::ranges::minmax(out.indices.empty() ? std::vector<std::uint32_t>{0} : out.indices);
    out.minIndex = low;
    out.verticesReferenced = out.indices.empty() ? 0 : high - low + 1;
    return {};
}

std::string Pack(const Chunk &original, const ChunkData &given, Packed &out)
{
    std::string why = Layout(original, given, out);
    why = why.empty() ? Vertices(original, given, out) : why;
    why = why.empty() ? Triangles(original, given, out) : why;
    if (!why.empty())
    {
        return why;
    }
    out.declarationChanged =
        out.declaration.size() != original.declaration.size() ||
        !std::ranges::equal(out.declaration, original.declaration, [](const Element &a, const Element &b) {
            return a.stream == b.stream && a.offset == b.offset && a.format == b.format && a.method == b.method &&
                   a.usage == b.usage && a.usageIndex == b.usageIndex;
        });
    // Given as it was read: the original's every field kept, so the file is the same bytes.
    const Packed was = AsItWas(original);
    out.changed = out.declarationChanged || out.vertexSize != was.vertexSize || out.vertices != was.vertices ||
                  out.indices != was.indices;
    if (!out.changed)
    {
        out = was;
    }
    return {};
}

// A new chunk, packed from nothing.
std::string PackNew(const ChunkData &given, Packed &out)
{
    if (given.declaration.empty() || std::ranges::none_of(given.declaration, [](const Element &e) {
            return e.usage == Usage::Position && e.usageIndex == 0;
        }))
    {
        return "a new chunk needs a declaration with a Position";
    }
    const Chunk none;
    std::string why = Layout(none, given, out);
    why = why.empty() ? Vertices(none, given, out) : why;
    why = why.empty() ? Triangles(none, given, out) : why;
    out.declarationChanged = true;
    out.changed = true;
    out.added = given.name;
    return why;
}

// The chunk a ChunkData is for: by its index, or by its name where no other chunk has it.
std::string Match(const Mesh &mesh, const ChunkData &chunk, std::vector<const ChunkData *> &given)
{
    std::size_t at = chunk.index.value_or(mesh.chunks.size());
    if (chunk.index)
    {
        if (at >= mesh.chunks.size() || (!chunk.name.empty() && mesh.chunks[at].name != chunk.name))
        {
            return "no chunk of that index and name in the original";
        }
    }
    else
    {
        const auto named = [&](const Chunk &c) { return c.name == chunk.name; };
        if (std::ranges::count_if(mesh.chunks, named) > 1)
        {
            return "several chunks have that name; give the chunk's index";
        }
        at = static_cast<std::size_t>(std::ranges::find_if(mesh.chunks, named) - mesh.chunks.begin());
        if (at == mesh.chunks.size())
        {
            return "no chunk of that name in the original";
        }
    }
    if (given[at] != nullptr)
    {
        return "a chunk given twice";
    }
    given[at] = &chunk;
    return {};
}

std::size_t Padded(std::size_t end)
{
    return end + (kAlign - (end % kAlign));
}

// The declarators' list as the file keeps it: each a struct of the declarator template, in place,
// then the end marker.
std::vector<std::byte> Declarators(const gff4::Template &shape, const std::vector<Element> &declaration)
{
    std::vector<std::byte> out((declaration.size() + 1) * shape.size, std::byte{0});
    const auto put = [&](std::size_t row, std::uint32_t label, std::uint32_t value) {
        if (const gff4::Field *field = shape.Find(label); field != nullptr && field->offset + 4 <= shape.size)
        {
            Put(out, (row * shape.size) + field->offset, value);
        }
    };
    for (std::size_t row = 0; row < declaration.size(); ++row)
    {
        const Element &e = declaration[row];
        put(row, kDeclaratorStream, e.stream);
        put(row, kDeclaratorOffset, e.offset);
        put(row, kDeclaratorType, static_cast<std::uint32_t>(e.format));
        put(row, kDeclaratorUsage, static_cast<std::uint32_t>(e.usage));
        put(row, kDeclaratorUsageIndex, e.usageIndex);
        put(row, kDeclaratorMethod, e.method);
    }
    // D3DDECL_END as the game's files keep it: stream -1, type and usage all ones.
    put(declaration.size(), kDeclaratorStream, 0xFFFFFFFFU);
    put(declaration.size(), kDeclaratorType, 0xFFFFFFFFU);
    put(declaration.size(), kDeclaratorUsage, 0xFFFFFFFFU);
    return out;
}

struct Box
{
    Point low{std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max()};
    Point high{std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(),
               std::numeric_limits<float>::lowest()};

    void Add(const Point &p)
    {
        for (std::size_t i = 0; i < 3; ++i)
        {
            low.at(i) = std::min(low.at(i), p.at(i));
            high.at(i) = std::max(high.at(i), p.at(i));
        }
    }
    bool Empty() const
    {
        return low[0] > high[0];
    }
};

Box BoxOf(const Chunk &chunk)
{
    Box box;
    if (const Element *position = chunk.Find(Usage::Position))
    {
        for (const Value &v : Floats(chunk, *position))
        {
            box.Add({v[0], v[1], v[2]});
        }
    }
    return box;
}

std::string WriteBounds(gff4::Editor &editor, const gff4::Struct &chunk, const Box &box)
{
    const std::optional<gff4::Struct> bounds = chunk.Child(kChunkBounds);
    if (!bounds || box.Empty())
    {
        return {};
    }
    const Point centre{(box.low[0] + box.high[0]) / 2, (box.low[1] + box.high[1]) / 2, (box.low[2] + box.high[2]) / 2};
    const float radius = std::sqrt(((box.high[0] - centre[0]) * (box.high[0] - centre[0])) +
                                   ((box.high[1] - centre[1]) * (box.high[1] - centre[1])) +
                                   ((box.high[2] - centre[2]) * (box.high[2] - centre[2])));
    const std::array<float, 4> low{box.low[0], box.low[1], box.low[2], 1};
    const std::array<float, 4> high{box.high[0], box.high[1], box.high[2], 1};
    const std::array<float, 4> sphere{centre[0], centre[1], centre[2], radius};
    if (!editor.SetFloats(*bounds, kBoundsBoxMin, low) || !editor.SetFloats(*bounds, kBoundsBoxMax, high) ||
        !editor.SetFloats(*bounds, kBoundsSphere, sphere))
    {
        return "a chunk's bounds are not kept as the game's are";
    }
    return {};
}

std::string WriteChunk(gff4::Editor &editor, const gff4::File &file, const gff4::Struct &chunk, const Packed &packed,
                       std::size_t vertexOffset, std::size_t startIndex)
{
    bool ok = editor.SetNumber(chunk, kChunkVertexSize, packed.vertexSize) &&
              editor.SetNumber(chunk, kChunkVertexCount, packed.vertexCount) &&
              editor.SetNumber(chunk, kChunkIndexCount, static_cast<std::int64_t>(packed.indices.size())) &&
              editor.SetNumber(chunk, kChunkIndexFormat, packed.indexFormat) &&
              editor.SetNumber(chunk, kChunkBaseVertex, packed.baseVertex) &&
              editor.SetNumber(chunk, kChunkVertexOffset, static_cast<std::int64_t>(vertexOffset)) &&
              editor.SetNumber(chunk, kChunkMinIndex, packed.minIndex) &&
              editor.SetNumber(chunk, kChunkVerticesReferenced, packed.verticesReferenced) &&
              editor.SetNumber(chunk, kChunkStartIndex, static_cast<std::int64_t>(startIndex));
    if (!ok)
    {
        return "a chunk's counts are not kept as the game's are";
    }
    if (packed.declarationChanged)
    {
        const gff4::Field *field = chunk.Shape().Find(kChunkDeclarators);
        if (field == nullptr || (field->flags & gff4::kReference) != 0 || field->type >= file.Templates().size())
        {
            return "a chunk's declaration is not kept as the game's is";
        }
        const std::vector<std::byte> rows = Declarators(file.Templates()[field->type], packed.declaration);
        ok = editor.SetList(chunk, kChunkDeclarators, rows, static_cast<std::uint32_t>(packed.declaration.size() + 1),
                            gff4::Place::Append);
    }
    return ok ? std::string{} : "a chunk's declaration could not be written";
}

} // namespace

namespace
{

// Each chunk as it will be written: those given packed from their values, the rest as they were.
std::string PackAll(const Mesh &mesh, std::span<const ChunkData> chunks, std::vector<Packed> &packed)
{
    std::vector<const ChunkData *> given(mesh.chunks.size(), nullptr);
    std::vector<std::wstring> names;
    std::ranges::transform(mesh.chunks, std::back_inserter(names), &Chunk::name);
    for (const ChunkData &chunk : chunks)
    {
        if (chunk.added && (chunk.index || std::ranges::find(names, chunk.name) != names.end()))
        {
            return "a new chunk needs a name no other chunk has, and no index: " + Narrow(chunk.name);
        }
        if (chunk.added)
        {
            names.push_back(chunk.name);
        }
        else if (std::string why = Match(mesh, chunk, given); !why.empty())
        {
            return why + ": " + Narrow(chunk.name);
        }
    }
    for (std::size_t i = 0; i < mesh.chunks.size(); ++i)
    {
        const Chunk &chunk = mesh.chunks[i];
        if (given[i] == nullptr)
        {
            packed.push_back(AsItWas(chunk));
            continue;
        }
        Packed one;
        if (std::string why = Pack(chunk, *given[i], one); !why.empty())
        {
            return Narrow(chunk.name) + ": " + why;
        }
        packed.push_back(std::move(one));
    }
    for (const ChunkData &chunk : chunks)
    {
        if (!chunk.added)
        {
            continue;
        }
        Packed one;
        if (std::string why = PackNew(chunk, one); !why.empty())
        {
            return Narrow(chunk.name) + ": " + why;
        }
        packed.push_back(std::move(one));
    }
    for (std::size_t i = 0; i < packed.size() && packed.size() > 1; ++i)
    {
        if (packed[i].indexFormat == 1)
        {
            return Narrow(names[i]) + ": more than 65535 vertices, which the game has only in a mesh of one chunk";
        }
    }
    return {};
}

// The file's two buffers, packed again as the game packs them, and where each chunk went.
struct Buffers
{
    std::vector<std::byte> vertices;
    std::vector<std::byte> indices;
    std::vector<std::pair<std::size_t, std::size_t>> places; // each chunk's vertex offset and start index
};

Buffers PackBuffers(const std::vector<Packed> &packed)
{
    Buffers out;
    std::size_t start = 0;
    for (const Packed &p : packed)
    {
        out.places.emplace_back(out.vertices.size(), start);
        out.vertices.insert(out.vertices.end(), p.vertices.begin(), p.vertices.end());
        out.vertices.resize(Padded(out.vertices.size()), std::byte{0});
        const std::size_t width = p.indexFormat == 1 ? 4 : 2;
        const std::size_t at = out.indices.size();
        out.indices.resize(at + (p.indices.size() * width));
        for (std::size_t i = 0; i < p.indices.size(); ++i)
        {
            if (width == 4)
            {
                Put(out.indices, at + (i * 4), p.indices[i]);
            }
            else
            {
                Put(out.indices, at + (i * 2), static_cast<std::uint16_t>(p.indices[i]));
            }
        }
        start += p.indices.size();
    }
    out.indices.resize(Padded(out.indices.size()), std::byte{0});
    return out;
}

// A struct for each new chunk, a copy of the first chunk's with a name of its own, put after the
// original's in the mesh's list of them.
std::string AddChunks(gff4::Editor &editor, const gff4::Struct &root, std::vector<gff4::Struct> &structs,
                      const std::vector<Packed> &packed)
{
    const std::size_t original = structs.size();
    if (original == packed.size())
    {
        return {};
    }
    const gff4::Field *list = root.Shape().Find(kMeshChunks);
    if (original == 0 || list == nullptr || (list->flags & gff4::kReference) == 0)
    {
        return "the mesh has no chunk kept as the game's are to make a new one like";
    }
    std::vector<std::byte> references;
    for (std::size_t i = original; i < packed.size(); ++i)
    {
        const std::optional<gff4::Struct> chunk = editor.Add(structs[0]);
        if (!chunk || !editor.SetString(*chunk, kName, *packed[i].added))
        {
            return "a new chunk could not be made";
        }
        structs.push_back(*chunk);
        references.resize(references.size() + 4);
        Put(references, references.size() - 4, editor.Reference(*chunk));
    }
    if (!editor.Extend(root, kMeshChunks, references, static_cast<std::uint32_t>(packed.size() - original)))
    {
        return "the new chunks could not be listed";
    }
    return {};
}

// The buffers and every chunk's fields written into the file; a changed chunk's bounds made again
// from its vertices as packed.
std::string WriteAll(gff4::Editor &editor, const gff4::File &file, const Mesh &mesh, const std::vector<Packed> &packed,
                     const Buffers &buffers)
{
    const std::optional<gff4::Struct> root = file.Root();
    if (!root ||
        !editor.SetList(*root, kMeshVertices, buffers.vertices, static_cast<std::uint32_t>(buffers.vertices.size()),
                        gff4::Place::InPlaceIfFits) ||
        !editor.SetList(*root, kMeshIndices, buffers.indices, static_cast<std::uint32_t>(buffers.indices.size()),
                        gff4::Place::InPlaceIfFits))
    {
        return "the mesh's buffers are not kept as the game's are";
    }
    std::vector<gff4::Struct> structs = root->Structs(kMeshChunks);
    if (std::string why = AddChunks(editor, *root, structs, packed); !why.empty())
    {
        return why;
    }
    for (std::size_t i = 0; i < packed.size(); ++i)
    {
        const auto [vertexOffset, startIndex] = buffers.places[i];
        std::string why = WriteChunk(editor, file, structs[i], packed[i], vertexOffset, startIndex);
        if (why.empty() && packed[i].changed)
        {
            Chunk written = i < mesh.chunks.size() ? mesh.chunks[i] : Chunk{};
            written.declaration = packed[i].declaration;
            written.vertexSize = packed[i].vertexSize;
            written.vertexCount = packed[i].vertexCount;
            written.vertices = packed[i].vertices;
            why = WriteBounds(editor, structs[i], BoxOf(written));
        }
        if (!why.empty())
        {
            return why;
        }
    }
    return {};
}

} // namespace

Written WriteMesh(std::span<const std::byte> original, std::span<const ChunkData> chunks)
{
    Written out;
    const std::optional<gff4::File> file = gff4::File::Parse(original);
    const std::optional<Mesh> mesh = file ? MeshOf(*file) : std::nullopt;
    const std::optional<gff4::Struct> root = file ? file->Root() : std::nullopt;
    if (!mesh || !root)
    {
        out.error = "the original is no mesh file";
        return out;
    }
    if (root->Structs(kMeshChunks).size() != mesh->chunks.size())
    {
        out.error = "a chunk of the original runs out of its file";
        return out;
    }
    std::vector<Packed> packed;
    if (out.error = PackAll(*mesh, chunks, packed); !out.error.empty())
    {
        return out;
    }
    if (std::ranges::none_of(packed, &Packed::changed))
    {
        out.bytes.assign(original.begin(), original.end());
        return out;
    }
    gff4::Editor editor(*file);
    if (out.error = WriteAll(editor, *file, *mesh, packed, PackBuffers(packed)); !out.error.empty())
    {
        return out;
    }
    out.bytes = editor.Bytes();
    return out;
}

namespace
{

// A node's child struct of a label: its transform's.
std::optional<gff4::Struct> ChildOf(const gff4::Struct &node, std::string_view label)
{
    for (const gff4::Struct &child : node.Structs(kChildren))
    {
        if (child.Label() == label)
        {
            return child;
        }
    }
    return std::nullopt;
}

bool HasBanner(const gff4::Struct &from, std::size_t deep)
{
    if (from.Label() == "msgr")
    {
        return true;
    }
    if (deep > 64)
    {
        return false;
    }
    const auto children = from.Structs(kChildren);
    return std::ranges::any_of(children, [&](const gff4::Struct &child) { return HasBanner(child, deep + 1); });
}

// A mesh node's bones, each one the model has, no more than one chunk's shaders take.
std::string SetBones(gff4::Editor &editor, const gff4::Struct &at, const std::vector<std::int32_t> &bones,
                     std::uint32_t totalBones, gff4::Place place)
{
    if (bones.size() > kMostBonesInChunk)
    {
        return "more than " + std::to_string(kMostBonesInChunk) +
               " bones in one chunk; the game's own meshes use at most " + std::to_string(kMostBonesInChunk);
    }
    if (std::ranges::any_of(bones, [&](std::int32_t b) { return b < 0 || std::cmp_greater_equal(b, totalBones); }))
    {
        return "a bone the model has not; it has " + std::to_string(totalBones);
    }
    std::vector<std::byte> list(bones.size() * 4);
    std::memcpy(list.data(), bones.data(), list.size());
    if (!editor.SetList(at, kBonesUsed, list, static_cast<std::uint32_t>(bones.size()), place))
    {
        return "the node's bones are not kept as the game's are";
    }
    return {};
}

std::string Edit(gff4::Editor &editor, const Node &node, const gff4::Struct &at, const NodeEdit &edit,
                 std::uint32_t totalBones)
{
    const auto transform = [&](std::string_view label, std::uint32_t field, std::span<const float> values) {
        const std::optional<gff4::Struct> child = ChildOf(at, label);
        if (!child)
        {
            return std::string("the node keeps no ") + std::string(label) + " to set";
        }
        return editor.SetFloats(*child, field, values)
                   ? std::string{}
                   : "the node's " + std::string(label) + " is not kept as the game's is";
    };
    std::string why;
    if (edit.translation)
    {
        const std::array<float, 4> t{(*edit.translation)[0], (*edit.translation)[1], (*edit.translation)[2], 1};
        why = transform("trsl", kTranslation, t);
    }
    if (why.empty() && edit.rotation)
    {
        why = transform("rota", kRotation, *edit.rotation);
    }
    if (why.empty() && edit.scale)
    {
        const std::array<float, 1> s{*edit.scale};
        why = transform("scal", kScale, s);
    }
    if (why.empty() && (edit.material || edit.bonesUsed) && !node.mesh)
    {
        why = "a material or bones for a node that is no mesh";
    }
    if (why.empty() && edit.material && !editor.SetString(at, kMaterialObject, *edit.material))
    {
        why = "the node's material is not kept as the game's is";
    }
    if (why.empty() && edit.bonesUsed)
    {
        why = SetBones(editor, at, *edit.bonesUsed, totalBones, gff4::Place::InPlaceIfFits);
    }
    return why;
}

// A new mesh node's struct: a copy of the node it is like, with its own names, bones, and
// transform, and the attributes of the node it is like; no animation exports.
std::optional<gff4::Struct> MakeMeshNode(gff4::Editor &editor, const gff4::Struct &like, const MeshNode &node,
                                         std::uint32_t totalBones, std::string &why)
{
    const std::optional<gff4::Struct> trsl = ChildOf(like, "trsl");
    const std::optional<gff4::Struct> rota = ChildOf(like, "rota");
    std::optional<gff4::Struct> made = editor.Add(like);
    std::optional<gff4::Struct> t = trsl ? editor.Add(*trsl) : std::nullopt;
    std::optional<gff4::Struct> r = rota ? editor.Add(*rota) : std::nullopt;
    const std::array<float, 4> translation{node.translation[0], node.translation[1], node.translation[2], 1};
    if (!made || !t || !r || !editor.SetFloats(*t, kTranslation, translation) ||
        !editor.SetFloats(*r, kRotation, node.rotation) || !editor.SetString(*made, kMmhName, node.name) ||
        !editor.SetString(*made, kMeshGroupName, node.mesh.chunk) ||
        !editor.SetString(*made, kMaterialObject, node.mesh.material) ||
        !editor.SetString(*made, kNodeId, node.mesh.part))
    {
        why = "the mesh node it is like is not kept as the game's are";
        return std::nullopt;
    }
    if (why = SetBones(editor, *made, node.mesh.bonesUsed, totalBones, gff4::Place::Append); !why.empty())
    {
        return std::nullopt;
    }
    // In the order the ogre's Mesh1 keeps them: its attributes, then its transform.
    std::vector<gff4::Struct> kept;
    std::ranges::copy_if(like.Structs(kChildren), std::back_inserter(kept),
                         [](const gff4::Struct &child) { return child.Label() == "attr"; });
    kept.push_back(*t);
    kept.push_back(*r);
    std::vector<std::byte> children(kept.size() * 8);
    for (std::size_t i = 0; i < kept.size(); ++i)
    {
        Put(children, i * 8, static_cast<std::uint32_t>(kept[i].Index()) | (gff4::kStruct << 16));
        Put(children, (i * 8) + 4, editor.Reference(kept[i]));
    }
    if (!editor.SetList(*made, kChildren, children, static_cast<std::uint32_t>(kept.size()), gff4::Place::Append))
    {
        why = "the new node's children could not be written";
        return std::nullopt;
    }
    return made;
}

// Each new mesh node made and put among its parent's children.
std::string AddMeshNodes(gff4::Editor &editor, const gff4::File &file, const Hierarchy &tree,
                         const std::vector<gff4::Struct> &structs, std::span<const MeshNode> added, const Mesh *written)
{
    const auto first = std::ranges::find(tree.nodes, "mshh", &Node::kind);
    for (const MeshNode &node : added)
    {
        const int like = node.like >= 0 ? node.like : static_cast<int>(first - tree.nodes.begin());
        const auto known = [&](int i) { return i >= 0 && static_cast<std::size_t>(i) < structs.size(); };
        const std::string what = "new mesh node " + Narrow(node.name) + ": ";
        if (!known(like) || tree.nodes[static_cast<std::size_t>(like)].kind != "mshh")
        {
            return what + "no mesh node to make it like";
        }
        if (node.parent != -1 && !known(node.parent))
        {
            return what + "no node " + std::to_string(node.parent) + " in the model to put it under";
        }
        if (written != nullptr &&
            std::ranges::find(written->chunks, node.mesh.chunk, &Chunk::name) == written->chunks.end())
        {
            return what + "no chunk " + Narrow(node.mesh.chunk) + " in the mesh";
        }
        std::string why;
        const std::optional<gff4::Struct> made =
            MakeMeshNode(editor, structs[static_cast<std::size_t>(like)], node, tree.totalBones, why);
        if (!made)
        {
            return what + why;
        }
        std::vector<std::byte> entry(8);
        Put(entry, 0, static_cast<std::uint32_t>(made->Index()) | (gff4::kStruct << 16));
        Put(entry, 4, editor.Reference(*made));
        const gff4::Struct parent = node.parent == -1 ? *file.Root() : structs[static_cast<std::size_t>(node.parent)];
        if (!editor.Extend(parent, kChildren, entry, 1))
        {
            return what + "its parent's children could not be written";
        }
    }
    return {};
}

// A point of a mesh node's space carried to the space of its ancestor `top`, through the
// transforms between: the mesh node's own and each one's above it, `top`'s not.
Point Carried(const Hierarchy &tree, int from, int top, Point p)
{
    for (int i = from; i != top && i >= 0; i = tree.nodes[static_cast<std::size_t>(i)].parent)
    {
        const Node &n = tree.nodes[static_cast<std::size_t>(i)];
        const float s = n.scale.value_or(1.0F);
        const Point turned = level::Turned(n.rotation, {p[0] * s, p[1] * s, p[2] * s});
        p = {turned[0] + n.translation[0], turned[1] + n.translation[1], turned[2] + n.translation[2]};
    }
    return p;
}

bool Under(const Hierarchy &tree, int node, int top)
{
    for (int i = node; i >= 0; i = tree.nodes[static_cast<std::size_t>(i)].parent)
    {
        if (i == top)
        {
            return true;
        }
    }
    return false;
}

// The box of the chunks of the mesh nodes under a node, in that node's space.
Box BoxUnder(const Hierarchy &tree, int top, const Mesh &written)
{
    Box box;
    for (std::size_t m = 0; m < tree.nodes.size(); ++m)
    {
        const Node &node = tree.nodes[m];
        if (!node.mesh || !Under(tree, static_cast<int>(m), top))
        {
            continue;
        }
        const auto chunk = std::ranges::find(written.chunks, node.mesh->chunk, &Chunk::name);
        const Box own = chunk == written.chunks.end() ? Box{} : BoxOf(*chunk);
        for (int corner = 0; corner < 8 && !own.Empty(); ++corner)
        {
            const Point p{(corner & 1) != 0 ? own.high[0] : own.low[0], (corner & 2) != 0 ? own.high[1] : own.low[1],
                          (corner & 4) != 0 ? own.high[2] : own.low[2]};
            box.Add(Carried(tree, static_cast<int>(m), top, p));
        }
    }
    return box;
}

std::string WriteBoxes(gff4::Editor &editor, const Hierarchy &tree, const std::vector<gff4::Struct> &structs,
                       const Mesh &written)
{
    for (std::size_t top = 0; top < structs.size(); ++top)
    {
        const std::optional<gff4::Struct> bbox = ChildOf(structs[top], "bbox");
        if (!bbox)
        {
            continue;
        }
        const Box box = BoxUnder(tree, static_cast<int>(top), written);
        if (box.Empty())
        {
            continue;
        }
        const std::array<float, 4> low{box.low[0], box.low[1], box.low[2], 1};
        const std::array<float, 4> high{box.high[0], box.high[1], box.high[2], 1};
        if (!editor.SetFloats(*bbox, kBoxMin, low) || !editor.SetFloats(*bbox, kBoxMax, high))
        {
            return "a bounding box is not kept as the game's are";
        }
    }
    return {};
}

} // namespace

Written WriteHierarchy(std::span<const std::byte> original, std::span<const NodeEdit> edits,
                       std::span<const MeshNode> added, const Mesh *written)
{
    Written out;
    const std::optional<gff4::File> file = gff4::File::Parse(original);
    const std::optional<Hierarchy> tree = file ? HierarchyOf(*file) : std::nullopt;
    if (!tree)
    {
        out.error = "the original is no model hierarchy";
        return out;
    }
    if (HasBanner(*file->Root(), 0))
    {
        out.error = "the model keeps mesh data of its own (msgr), which is not written";
        return out;
    }
    const std::vector<gff4::Struct> structs = detail::NodeStructs(*file);
    gff4::Editor editor(*file);
    for (const NodeEdit &edit : edits)
    {
        if (edit.node < 0 || static_cast<std::size_t>(edit.node) >= structs.size())
        {
            out.error = "no node " + std::to_string(edit.node) + " in the model";
            return out;
        }
        const auto index = static_cast<std::size_t>(edit.node);
        if (std::string why = Edit(editor, tree->nodes[index], structs[index], edit, tree->totalBones); !why.empty())
        {
            out.error = "node " + std::to_string(edit.node) + ": " + why;
            return out;
        }
    }
    if (out.error = AddMeshNodes(editor, *file, *tree, structs, added, written); !out.error.empty())
    {
        return out;
    }
    if (written != nullptr)
    {
        // The boxes are through the transforms as edited, the new nodes' among them.
        Hierarchy edited = *tree;
        for (const NodeEdit &edit : edits)
        {
            Node &node = edited.nodes[static_cast<std::size_t>(edit.node)];
            node.translation = edit.translation.value_or(node.translation);
            node.rotation = edit.rotation.value_or(node.rotation);
            node.scale = edit.scale ? edit.scale : node.scale;
        }
        for (const MeshNode &node : added)
        {
            Node made;
            made.name = node.name;
            made.kind = "mshh";
            made.parent = node.parent;
            made.translation = node.translation;
            made.rotation = node.rotation;
            made.mesh = node.mesh;
            edited.nodes.push_back(std::move(made));
        }
        if (std::string why = WriteBoxes(editor, edited, structs, *written); !why.empty())
        {
            out.error = why;
            return out;
        }
    }
    out.bytes = editor.Bytes();
    return out;
}

} // namespace dao::core::model
