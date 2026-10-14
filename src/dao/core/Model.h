// A model out of the game's files, whole, over bytes: every vertex element
// of a mesh's chunks as their Direct3D declarations name them, and the
// model's node tree with its bones, transforms and materials (DAOLib #116),
// for a model taken out to an editor and put back (DAOEluvianMCP #132).
// dao/core/Level.h reads what a lightmap's bake needs of the same files.
//
// Read 2026-10-14 over all 12,139 .msh the game takes (tools/gff4.py, every
// chunk and declarator) and the ogre's body (cn_bdy_ogr_0.msh,
// cn_bdy_ogra_0.mmh) decoded by hand:
//
//   <model>.msh: one vertex buffer and one index buffer for the whole file,
//     each chunk a slice of them (its vertex offset in bytes, its start
//     index and base vertex) with a D3DVERTEXELEMENT9 declaration of its
//     own, ended by D3DDECL_END. Every element is in stream 0, and no chunk
//     has an additional stream. Positions are Float3, Float4 or Float16_4;
//     normals Float3 or Float16_4; texture coordinates Float2 or Float16_2;
//     tangents and binormals Float16_4, always both; blend weights
//     Float16_4, four that sum to 1; blend indices Short4; colours Float3 or
//     Float16_4. Indices are 16 bits but in 3 chunks, which say 32 in the
//     chunk's index format; the file's own index format is junk in 91 files.
//   <model>.mmh: a tree of nodes, each a struct with a name and children.
//     A node's children are its own transform (a "trsl" translation, a
//     "rota" rotation as x, y, z, w, a "scal" scale), local to its parent,
//     its animation exports, and the nodes under it. There is no bind pose
//     or inverse bind matrix in the file: the nodes' own transforms are it.
//     Seen 2026-10-14: the ogre taken to glTF by them (ghilana,
//     DAOEluvianMCP #132) passed Khronos' validator and came into Blender
//     skinned in its A-pose, all 9652 vertices and 99 bones, nothing
//     mangled; and its triangles face their normals counter-clockwise, as
//     glTF reads them. A bone node has its index among
//     the model's bones, -1 on a node that is none. A mesh node ("mshh")
//     names its chunk, its material (a .mao) and the bones its chunk's blend
//     indices count in: index i is the model's bone bonesUsed[i].
#pragma once

#include "dao/core/Gff4.h"
#include "dao/core/RayCast.h"

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace dao::core::model
{

using Quaternion = std::array<float, 4>; // x, y, z, w
using Value = std::array<float, 4>;

// Returns a half float widened to a float.
float Half(std::uint16_t half);

// What a vertex element is for: Direct3D's D3DDECLUSAGE.
enum class Usage : std::uint8_t
{
    Position = 0,
    BlendWeight = 1,
    BlendIndices = 2,
    Normal = 3,
    PointSize = 4,
    TexCoord = 5,
    Tangent = 6,
    Binormal = 7,
    TessFactor = 8,
    PositionT = 9,
    Color = 10,
    Fog = 11,
    Depth = 12,
    Sample = 13,
};

// How a vertex element is kept: Direct3D's D3DDECLTYPE.
enum class Format : std::uint8_t
{
    Float1 = 0,
    Float2 = 1,
    Float3 = 2,
    Float4 = 3,
    D3DColor = 4,
    UByte4 = 5,
    Short2 = 6,
    Short4 = 7,
    UByte4N = 8,
    Short2N = 9,
    Short4N = 10,
    UShort2N = 11,
    UShort4N = 12,
    UDec3 = 13,
    Dec3N = 14,
    Float16x2 = 15,
    Float16x4 = 16,
};

// Returns how many bytes an element of a format takes.
std::size_t SizeOf(Format format);

// One element of a chunk's vertex declaration, as the file has it.
struct Element
{
    std::uint16_t stream = 0;
    std::uint16_t offset = 0; // in the vertex
    Format format = Format::Float3;
    std::uint8_t method = 0;
    Usage usage = Usage::Position;
    std::uint8_t usageIndex = 0;
};

// One chunk of a mesh: its declaration, its counts and offsets as the file
// has them, and its slices of the file's buffers.
struct Chunk
{
    std::wstring name;
    std::vector<Element> declaration; // without the end marker
    std::uint32_t vertexSize = 0;
    std::uint32_t vertexCount = 0;
    std::uint32_t indexCount = 0;
    std::uint32_t primitive = 0;   // 0, a triangle list, in every chunk the game has
    std::uint32_t indexFormat = 0; // 0 for 16-bit indices, 1 for 32-bit
    std::uint32_t baseVertex = 0;
    std::uint32_t vertexOffset = 0; // in bytes, into the file's vertices
    std::uint32_t minIndex = 0;
    std::uint32_t verticesReferenced = 0;
    std::uint32_t startIndex = 0;        // in indices, into the file's indices
    std::span<const std::byte> vertices; // this chunk's: vertexCount of vertexSize bytes
    std::span<const std::byte> indices;  // this chunk's: indexCount of 2 or 4 bytes

    // Returns the element for a usage and its index, or null with none.
    const Element *Find(Usage usage, std::uint8_t usageIndex = 0) const;
};

struct Mesh
{
    std::wstring name;
    std::vector<Chunk> chunks;
};

// Returns a mesh file's chunks (.msh), or nothing for a file that is no
// mesh. A chunk whose vertices or indices run out of the file is left out.
// The spans point into the bytes the File was parsed from.
std::optional<Mesh> MeshOf(const gff4::File &mesh);

// Returns an element's value for each vertex of the chunk as up to four
// floats, the rest filled as Direct3D fills them (0, 0, 0, 1). Normalised
// formats are scaled to [0, 1] or [-1, 1], D3DColor is red, green, blue,
// alpha. Empty if the vertices stop short.
std::vector<Value> Floats(const Chunk &chunk, const Element &element);

// Returns an element of Short2 or Short4 for each vertex as four integers,
// the rest 0: the blend indices. Empty for another format.
std::vector<std::array<std::int16_t, 4>> Shorts(const Chunk &chunk, const Element &element);

// Returns the chunk's indices into its own vertices: the base vertex added.
std::vector<std::uint32_t> Indices(const Chunk &chunk);

// A mesh node's mesh: its chunk, its material and its bones.
struct MeshRef
{
    std::wstring chunk;                  // the chunk's name in the model's .msh
    std::wstring material;               // the .mao, with no extension
    std::wstring part;                   // its id, what a room's lightmap list knows it by
    std::vector<std::int32_t> bonesUsed; // a blend index's bone in the model's bones
};

struct Node
{
    std::wstring name;
    std::string kind; // its struct's label: "node", "mshh", "nplt" ...
    int parent = -1;
    std::vector<int> children;
    Point translation{};
    Quaternion rotation{0, 0, 0, 1};
    std::optional<float> scale;
    std::optional<int> bone; // its index among the model's bones, if it is one
    std::optional<MeshRef> mesh;
};

struct Hierarchy
{
    std::wstring name;
    std::wstring meshFile; // the .msh, as the file names it
    std::uint32_t totalBones = 0;
    std::vector<Node> nodes; // in the file's order, each before its children; the tops have parent -1
    std::vector<int> bones;  // a bone index's node, -1 for one no node carries
};

// Returns a model's node tree (.mmh), or nothing for a file that is no
// model hierarchy.
std::optional<Hierarchy> HierarchyOf(const gff4::File &hierarchy);

// The most bones one chunk's blend indices may count in: the skinned shaders'
// palette, bsm_bones, is 201 registers of three a bone. The game's own meshes
// use up to this many (cn_bdy_glkb_0 and others).
inline constexpr std::size_t kMostBonesInChunk = 67;

// A chunk's new vertices and triangles, for WriteMesh.
struct ChunkData
{
    std::wstring name;                // a chunk of the original, matched by name
    std::optional<std::size_t> index; // or by its place in Mesh::chunks, where names repeat
    std::vector<Element> declaration; // empty for the original chunk's
    // One vector a declaration element, in its order, each a value a vertex;
    // blend indices as Values too, rounded to whole numbers.
    std::vector<std::vector<Value>> values;
    std::vector<std::uint32_t> indices; // a triangle list into this chunk's vertices
};

// A file written, or why it was not.
struct Written
{
    std::vector<std::byte> bytes;
    std::string error; // empty when written
};

// Returns a mesh file (.msh) with the given chunks' vertices and triangles
// replaced and the rest kept. The shared buffers are packed again in the
// chunks' order, each chunk's counts, offsets, index format and bounds
// written to match; a chunk not given keeps its vertices and triangles.
// With no chunk given, or each given as it was read, the original's bytes.
// Fails on an unknown name or index, a name several chunks have and no index,
// a value count that is not the vertex count, an
// index past the vertices, a blend index of kMostBonesInChunk or more, more
// than 65535 vertices in a mesh of several chunks, or a declaration whose
// elements overlap or leave the vertex.
Written WriteMesh(std::span<const std::byte> original, std::span<const ChunkData> chunks);

// A change to one node of a hierarchy, for WriteHierarchy.
struct NodeEdit
{
    int node = -1; // its index in Hierarchy::nodes
    std::optional<Point> translation;
    std::optional<Quaternion> rotation;
    std::optional<float> scale;
    std::optional<std::wstring> material;               // a mesh node's .mao, with no extension
    std::optional<std::vector<std::int32_t>> bonesUsed; // a mesh node's, the whole list
};

// Returns a hierarchy file (.mmh) with the given nodes changed. With
// `written`, the mesh as written for it, each bounding box in the file is
// made again: the box of the chunks of the mesh nodes under its node,
// through the transforms between. Fails on a node that is not in the file,
// a transform the node does not already keep, a material or bone list on a
// node that is no mesh, more than kMostBonesInChunk bones, or a model that
// keeps mesh data of its own ("msgr", the banners').
Written WriteHierarchy(std::span<const std::byte> original, std::span<const NodeEdit> edits,
                       const Mesh *written = nullptr);

} // namespace dao::core::model
