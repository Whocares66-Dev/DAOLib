// A model made in a test, as the game lays one out (tools/gff4.py --templates
// on cn_bdy_ogr_0.msh and cn_bdy_ogra_0.mmh): a .msh of two chunks in one
// vertex and one index buffer, and the .mmh of its node tree, with the fields
// dao/core/Model.h reads. Written for DAOLib #116 and Eluvian's ghilana
// (DAOEluvianMCP #132), whose tests include this file too, so a model goes to
// glTF and back from the same bytes DAOLib's tests read.
#pragma once

#include "dao/core/Gff4.h"
#include "gff4_builder.h"

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace dao::test::files
{

// Half floats as the meshes keep them.
inline constexpr std::uint16_t kHalfZero = 0x0000;
inline constexpr std::uint16_t kHalfOne = 0x3C00;
inline constexpr std::uint16_t kHalfTwo = 0x4000;
inline constexpr std::uint16_t kHalfMinusOne = 0xBC00;
inline constexpr std::uint16_t kHalfHalf = 0x3800;
inline constexpr std::uint16_t kHalfQuarter = 0x3400;

// A row of a chunk's vertex declaration: D3DVERTEXELEMENT9's fields as the file keeps them.
struct Declared
{
    std::uint32_t offset;
    std::uint32_t type;  // D3DDECLTYPE
    std::uint32_t usage; // D3DDECLUSAGE
    std::uint32_t usageIndex;
    std::int32_t stream = 0;
};

// The mesh "slab.msh", of two chunks.
//
// "Body" is laid out as the ogre's body is, 52 bytes a vertex, all half
// floats but the blend indices: its place (i, 0, 0, 1) at 0, its texture
// coordinates at 8, its tangent (1, 0, 0, 1) at 12, its binormal (0, -1, 0,
// 1) at 20, so the binormal is cross(normal, tangent), its normal (0, 0, -1,
// 1) at 28, its weights (0.5, 0.25, 0.25, 0) at 36 and its blend indices
// (i, 1, 2, 0) as shorts at 44, for its three vertices i = 0, 1, 2 with the
// texture coordinates (0, 0), (1, 0) and (0, 1); then D3DDECL_END. Its
// triangle is 0, 1, 2.
//
// "Cap" is three vertices of 12 bytes, a place (5, i, 0) as three floats,
// at byte 160 of the buffer; its three indices are the file's fourth to
// sixth, 0, 1, 1, and its base vertex 1. With `cut`, it says it has five
// vertices, which run past the buffer.
//
// The buffers are padded as the game pads them: each chunk's vertices
// followed by 16 - (end % 16) bytes, and the indices the same. Each chunk
// keeps its bounds in place, as the game's do: the box of its places and the
// sphere about the box's centre through its corners.
inline std::vector<std::byte> SlabMesh(bool cut = false)
{
    namespace gff4 = dao::core::gff4;
    dao::test::Builder b = dao::test::Start(
        "MESH",
        {{"mesh",
          16,
          {{2, 14, 0, 0},
           {8021, 2, gff4::kList | gff4::kStruct | gff4::kReference, 4},
           {8022, 0, gff4::kList, 8},
           {8023, 0, gff4::kList, 12}}},
         {"decl",
          24,
          {{8026, 5, 0, 0}, {8027, 5, 0, 4}, {8028, 4, 0, 8}, {8029, 4, 0, 12}, {8030, 4, 0, 16}, {8031, 4, 0, 20}}},
         {"chnk",
          96,
          {{2, 14, 0, 0},
           {8000, 4, 0, 4},
           {8001, 4, 0, 8},
           {8002, 4, 0, 12},
           {8003, 4, 0, 16},
           {8004, 4, 0, 20},
           {8005, 4, 0, 24},
           {8006, 4, 0, 28},
           {8007, 4, 0, 32},
           {8008, 4, 0, 36},
           {8009, 4, 0, 40},
           {8025, 1, gff4::kList | gff4::kStruct, 44},
           {8020, 3, gff4::kStruct, 48}}},
         {"bnds", 48, {{8017, 12, 0, 0}, {8018, 12, 0, 16}, {8019, 12, 0, 32}}}});
    const std::size_t root = dao::test::Struct(b, 16);
    const std::size_t body = dao::test::Struct(b, 96);
    const std::size_t cap = dao::test::Struct(b, 96);
    dao::test::Floats(b, body + 48, {0, 0, 0, 1, 2, 0, 0, 1, 1, 0, 0, 1});
    dao::test::Floats(b, cap + 48, {5, 0, 0, 1, 5, 2, 0, 1, 5, 1, 0, 1});
    std::vector<std::uint8_t> vertices;
    const std::uint16_t places[3] = {kHalfZero, kHalfOne, kHalfTwo};
    const std::uint16_t uvs[3][2] = {{kHalfZero, kHalfZero}, {kHalfOne, kHalfZero}, {kHalfZero, kHalfOne}};
    for (std::int16_t i = 0; i < 3; ++i)
    {
        for (const std::uint16_t half : {places[i], kHalfZero,     kHalfZero,     kHalfOne,   // place
                                         uvs[i][0], uvs[i][1],                                // texture coordinates
                                         kHalfOne,  kHalfZero,     kHalfZero,     kHalfOne,   // tangent
                                         kHalfZero, kHalfMinusOne, kHalfZero,     kHalfOne,   // binormal
                                         kHalfZero, kHalfZero,     kHalfMinusOne, kHalfOne,   // normal
                                         kHalfHalf, kHalfQuarter,  kHalfQuarter,  kHalfZero}) // weights
        {
            dao::test::Append(vertices, half);
        }
        for (const std::int16_t index : {i, std::int16_t{1}, std::int16_t{2}, std::int16_t{0}})
        {
            dao::test::Append(vertices, index);
        }
    }
    vertices.resize(160);
    for (int i = 0; i < 3; ++i)
    {
        dao::test::Append(vertices, 5.0F);
        dao::test::Append(vertices, static_cast<float>(i));
        dao::test::Append(vertices, 0.0F);
    }
    vertices.resize(208);
    b.U32(root + 8, dao::test::Bytes(b, vertices));
    b.U32(root + 12, dao::test::Bytes(b, {0, 0, 1, 0, 2, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0}));
    const std::size_t chunks = b.bytes.size();
    b.U32(chunks, 2);
    b.U32(chunks + 4, static_cast<std::uint32_t>(body - b.data));
    b.U32(chunks + 8, static_cast<std::uint32_t>(cap - b.data));
    b.U32(root + 4, static_cast<std::uint32_t>(chunks - b.data));
    b.U32(root, b.String(u"slab.msh"));
    const auto declare = [&](std::size_t chunk, const std::vector<Declared> &rows) {
        const std::size_t at = b.bytes.size();
        b.U32(at, static_cast<std::uint32_t>(rows.size()));
        for (std::size_t i = 0; i < rows.size(); ++i)
        {
            const std::size_t row = at + 4 + (24 * i);
            b.U32(row, static_cast<std::uint32_t>(rows[i].stream));
            b.U32(row + 4, rows[i].offset);
            b.U32(row + 8, rows[i].type);
            b.U32(row + 12, rows[i].usage);
            b.U32(row + 16, rows[i].usageIndex);
            b.U32(row + 20, 0);
        }
        b.U32(chunk + 44, static_cast<std::uint32_t>(at - b.data));
    };
    const auto counts = [&](std::size_t chunk, const char16_t *name, std::uint32_t size, std::uint32_t count,
                            std::uint32_t base, std::uint32_t offset, std::uint32_t start) {
        b.U32(chunk, b.String(name));
        b.U32(chunk + 4, size);
        b.U32(chunk + 8, count);
        b.U32(chunk + 12, 3);
        b.U32(chunk + 24, base);
        b.U32(chunk + 28, offset);
        b.U32(chunk + 36, count);
        b.U32(chunk + 40, start);
    };
    counts(body, u"Body", 52, 3, 0, 0, 0);
    declare(body, {{0, 16, 0, 0},
                   {8, 15, 5, 0},
                   {12, 16, 6, 0},
                   {20, 16, 7, 0},
                   {28, 16, 3, 0},
                   {36, 16, 1, 0},
                   {44, 7, 2, 0},
                   {0, 0xFFFFFFFF, 0xFFFFFFFF, 0, -1}});
    counts(cap, u"Cap", 12, cut ? 5 : 3, 1, 160, 3);
    declare(cap, {{0, 2, 0, 0}});
    return b.bytes;
}

// The model "slab.mmh" of the mesh "slab.msh", with three bones: GOB, moved
// by (0, 0, 1), holding Bone0 (bone 1, turned a quarter about z, scale 2)
// with Bone1 under it (bone 0, moved by (0, 1, 0)), then the mesh node Mesh1
// (chunk Body, material ogre, part _1, its blend indices counting in bones 1
// and 0), then an animation export, which is no node. GOB keeps a bounding
// box, from (-9, -9, -9) to (9, 9, 9) until one is made again. With
// `banner`, GOB also holds an "msgr", mesh data of the model's own.
inline std::vector<std::byte> SlabModel(bool banner = false)
{
    namespace gff4 = dao::core::gff4;
    const std::uint16_t children = gff4::kList | gff4::kReference;
    dao::test::Builder b = dao::test::Start(
        "MMH ",
        {{"mdlh", 16, {{6000, 14, 0, 0}, {6005, 14, 0, 4}, {6256, 4, 0, 8}, {6999, dao::test::kGeneric, children, 12}}},
         {"node", 12, {{6000, 14, 0, 0}, {6254, 5, 0, 4}, {6999, dao::test::kGeneric, children, 8}}},
         {"mshh",
          24,
          {{6000, 14, 0, 0},
           {6001, 14, 0, 4},
           {6004, 14, 0, 8},
           {6006, 14, 0, 12},
           {6255, 5, gff4::kList, 16},
           {6999, dao::test::kGeneric, children, 20}}},
         {"trsl", 16, {{6047, 12, 0, 0}}},
         {"rota", 16, {{6048, 13, 0, 0}}},
         {"scal", 4, {{6278, 8, 0, 0}}},
         {"xprt", 4, {{6290, 4, 0, 0}}},
         {"bbox", 32, {{6054, 12, 0, 0}, {6055, 12, 0, 16}}},
         {"msgr", 4, {{6000, 14, 0, 0}}}});
    const std::size_t root = dao::test::Struct(b, 16);
    const std::size_t gob = dao::test::Struct(b, 12);
    const std::size_t gobAt = dao::test::Struct(b, 16);
    const std::size_t bone0 = dao::test::Struct(b, 12);
    const std::size_t bone0Turn = dao::test::Struct(b, 16);
    const std::size_t bone0Scale = dao::test::Struct(b, 4);
    const std::size_t bone1 = dao::test::Struct(b, 12);
    const std::size_t bone1At = dao::test::Struct(b, 16);
    const std::size_t mesh = dao::test::Struct(b, 24);
    const std::size_t exported = dao::test::Struct(b, 4);
    const std::size_t box = dao::test::Struct(b, 32);
    const std::size_t meshData = dao::test::Struct(b, 4);
    dao::test::Floats(b, box, {-9, -9, -9, 1, 9, 9, 9, 1});
    b.U32(meshData, 0xFFFFFFFF);
    dao::test::Floats(b, gobAt, {0, 0, 1, 1});
    dao::test::Floats(b, bone0Turn, {0, 0, 0.70710678F, 0.70710678F});
    dao::test::Floats(b, bone0Scale, {2});
    dao::test::Floats(b, bone1At, {0, 1, 0, 1});
    b.U32(root, b.String(u"slab.mmh"));
    b.U32(root + 4, b.String(u"slab.msh"));
    b.U32(root + 8, 3);
    b.U32(root + 12, dao::test::Children(b, {{1, gob}}));
    b.U32(gob, b.String(u"GOB"));
    b.U32(gob + 4, 0xFFFFFFFF);
    std::vector<std::pair<std::uint16_t, std::size_t>> gobChildren{
        {3, gobAt}, {7, box}, {1, bone0}, {2, mesh}, {6, exported}};
    if (banner)
    {
        gobChildren.emplace_back(8, meshData);
    }
    b.U32(gob + 8, dao::test::Children(b, gobChildren));
    b.U32(bone0, b.String(u"Bone0"));
    b.U32(bone0 + 4, 1);
    b.U32(bone0 + 8, dao::test::Children(b, {{4, bone0Turn}, {5, bone0Scale}, {1, bone1}}));
    b.U32(bone1, b.String(u"Bone1"));
    b.U32(bone1 + 4, 0);
    b.U32(bone1 + 8, dao::test::Children(b, {{3, bone1At}}));
    b.U32(mesh, b.String(u"Mesh1"));
    b.U32(mesh + 4, b.String(u"ogre"));
    b.U32(mesh + 8, b.String(u"_1"));
    b.U32(mesh + 12, b.String(u"Body"));
    b.U32(mesh + 16, b.Numbers({1, 0}));
    b.U32(mesh + 20, dao::test::Children(b, {}));
    return b.bytes;
}

} // namespace dao::test::files
