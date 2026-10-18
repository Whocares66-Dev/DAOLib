#include "dao/core/Morph.h"

#include "dao/core/Bytes.h"

#include <cstring>

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

} // namespace dao::core::morph
