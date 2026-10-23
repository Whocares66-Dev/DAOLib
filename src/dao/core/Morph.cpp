#include "dao/core/Morph.h"

#include "dao/core/Bytes.h"

#include <cstdint>
#include <cstring>
#include <span>

namespace dao::core::morph
{

namespace
{

// The fields' labels (dev/gff4_fields.csv).
constexpr std::uint32_t kName = 2;
constexpr std::uint32_t kPosition = 4;
constexpr std::uint32_t kVertexCount = 8013;
constexpr std::uint32_t kVertexData = 8024;
constexpr std::uint32_t kParts = 23000;
constexpr std::uint32_t kTints = 23001;
constexpr std::uint32_t kNodes = 23002;
constexpr std::uint32_t kVectors = 17; // Vector4fList
constexpr std::uint32_t kTextureName = 23003;
constexpr std::uint32_t kTextureParameters = 23004;
constexpr std::uint32_t kVectorParameters = 23005;
constexpr std::uint32_t kFloatParameters = 23006;
constexpr std::uint32_t kFloatValue = 23007;

void ParametersOf(const gff4::Struct &from, Node &node)
{
    for (const gff4::Struct &parameter : from.Structs(kTextureParameters))
    {
        node.textures.push_back(
            {.name = parameter.String(kName).value_or(L""), .texture = parameter.String(kTextureName).value_or(L"")});
    }
    for (const gff4::Struct &parameter : from.Structs(kVectorParameters))
    {
        VectorParameter vector{.name = parameter.String(kName).value_or(L""), .values = {}};
        const std::span<const std::byte> values = parameter.ListBytes(kVectors, gff4::Type::Vector4);
        vector.values.resize(values.size() / 16);
        if (!vector.values.empty())
        {
            std::memcpy(vector.values.data(), values.data(), vector.values.size() * 16);
        }
        node.vectors.push_back(std::move(vector));
    }
    for (const gff4::Struct &parameter : from.Structs(kFloatParameters))
    {
        node.floats.push_back(
            {.name = parameter.String(kName).value_or(L""), .value = parameter.Get<float>(kFloatValue).value_or(0.0F)});
    }
}

Node NodeOf(const gff4::Struct &from)
{
    Node node;
    node.name = from.String(kName).value_or(L"");
    const std::vector<float> position = from.Floats(kPosition);
    for (std::size_t i = 0; i < 3 && i < position.size(); ++i)
    {
        node.position.at(i) = position[i];
    }
    const std::uint32_t count = from.Get<std::uint32_t>(kVertexCount).value_or(0);
    const std::span<const std::byte> data = from.ListBytes(kVertexData, gff4::Type::F32);
    if (count != 0 && data.size() >= std::size_t{count} * 16)
    {
        node.places.resize(count);
        std::memcpy(node.places.data(), data.data(), std::size_t{count} * 16);
    }
    ParametersOf(from, node);
    return node;
}

} // namespace

std::optional<Morph> MorphOf(const gff4::File &file)
{
    const std::optional<gff4::Struct> root = file.Root();
    if (file.Type() != "MORP" || !root)
    {
        return std::nullopt;
    }
    Morph out;
    out.name = root->String(kName).value_or(L"");
    out.parts = root->Strings(kParts);
    out.tints = root->Strings(kTints);
    for (const gff4::Struct &node : root->Structs(kNodes))
    {
        out.nodes.push_back(NodeOf(node));
    }
    return out;
}

// Notes on the writing, 2026-10-22 (DAOLib #237, for a head made in Paragon's character creation).
//
// The game's .mor files share one set of five templates (morp, tprm, vprm, fprm, node; the four of
// game/heads read with tools/gff4.py --templates are the same bytes), and each holds every field the
// reader knows, so any of them can carry another head. The writer edits a copy of that file through
// Gff4's Editor: the root's name and lists are set afresh and every node is a new struct, built from a
// blank of its template (Editor::New, every list and string null) copied into the list. A list of structs
// in place (the nodes, a node's parameters) names its template by its field's type. What was in the base
// stays as dead bytes. MeshStreamVertexCount is the count of the places, which is what the reader takes
// the data's length from; a bone's node has none.
namespace
{

// The template of a list of structs held in place: its field's type is the template's index.
std::optional<std::size_t> ListTemplate(const gff4::File &file, const gff4::Struct &in, std::uint32_t label)
{
    const auto &templates = file.Templates();
    const gff4::Field *field = in.Shape().Find(label);
    if (field == nullptr || (field->flags & gff4::kList) == 0 || (field->flags & gff4::kStruct) == 0 ||
        (field->flags & gff4::kReference) != 0 || field->type >= templates.size())
    {
        return std::nullopt;
    }
    return field->type;
}

// Sets a list of structs in place to `count` new blanks of its template, and returns them.
std::optional<std::vector<gff4::Struct>> NewList(gff4::Editor &editor, const gff4::File &file, const gff4::Struct &in,
                                                 std::uint32_t label, std::size_t count)
{
    const auto index = ListTemplate(file, in, label);
    if (!index)
    {
        return std::nullopt;
    }
    if (count == 0)
    {
        return editor.CopyList(in, label, {});
    }
    const auto blank = editor.New(*index);
    if (!blank)
    {
        return std::nullopt;
    }
    const std::vector<gff4::Struct> like(count, *blank);
    return editor.CopyList(in, label, like);
}

template <class T> std::span<const std::byte> BytesOf(const std::vector<T> &values)
{
    return std::as_bytes(std::span(values));
}

bool WriteParameters(gff4::Editor &editor, const gff4::File &file, const gff4::Struct &at, const Node &node)
{
    const auto textures = NewList(editor, file, at, kTextureParameters, node.textures.size());
    if (!textures)
    {
        return false;
    }
    for (std::size_t i = 0; i < node.textures.size(); ++i)
    {
        if (!editor.SetString((*textures)[i], kName, node.textures[i].name) ||
            !editor.SetString((*textures)[i], kTextureName, node.textures[i].texture))
        {
            return false;
        }
    }
    const auto vectors = NewList(editor, file, at, kVectorParameters, node.vectors.size());
    if (!vectors)
    {
        return false;
    }
    for (std::size_t i = 0; i < node.vectors.size(); ++i)
    {
        const auto &values = node.vectors[i].values;
        if (!editor.SetString((*vectors)[i], kName, node.vectors[i].name) ||
            !editor.SetList((*vectors)[i], kVectors, BytesOf(values), static_cast<std::uint32_t>(values.size()),
                            gff4::Place::Append))
        {
            return false;
        }
    }
    const auto floats = NewList(editor, file, at, kFloatParameters, node.floats.size());
    if (!floats)
    {
        return false;
    }
    for (std::size_t i = 0; i < node.floats.size(); ++i)
    {
        const float value = node.floats[i].value;
        if (!editor.SetString((*floats)[i], kName, node.floats[i].name) ||
            !editor.SetFloats((*floats)[i], kFloatValue, std::span(&value, 1)))
        {
            return false;
        }
    }
    return true;
}

bool WriteNode(gff4::Editor &editor, const gff4::File &file, const gff4::Struct &at, const Node &node)
{
    return editor.SetString(at, kName, node.name) && editor.SetFloats(at, kPosition, node.position) &&
           editor.SetNumber(at, kVertexCount, static_cast<std::int64_t>(node.places.size())) &&
           editor.SetList(at, kVertexData, BytesOf(node.places), static_cast<std::uint32_t>(node.places.size() * 4),
                          gff4::Place::Append) &&
           WriteParameters(editor, file, at, node);
}

} // namespace

std::optional<std::vector<std::byte>> Write(const gff4::File &base, const Morph &morph)
{
    const std::optional<gff4::Struct> root = MorphOf(base) ? base.Root() : std::nullopt;
    if (!root)
    {
        return std::nullopt;
    }
    gff4::Editor editor(base);
    if (!editor.SetString(*root, kName, morph.name) || !editor.SetStrings(*root, kParts, morph.parts) ||
        !editor.SetStrings(*root, kTints, morph.tints))
    {
        return std::nullopt;
    }
    const auto nodes = NewList(editor, base, *root, kNodes, morph.nodes.size());
    if (!nodes)
    {
        return std::nullopt;
    }
    for (std::size_t i = 0; i < morph.nodes.size(); ++i)
    {
        if (!WriteNode(editor, base, (*nodes)[i], morph.nodes[i]))
        {
            return std::nullopt;
        }
    }
    return editor.Bytes();
}

} // namespace dao::core::morph
