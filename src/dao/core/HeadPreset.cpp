#include "dao/core/HeadPreset.h"

#include <algorithm>
#include <bit>
#include <cwctype>
#include <iterator>
#include <limits>
#include <utility>

namespace dao::core::preset
{

namespace
{

// The fields' labels (dev/gff4_fields.csv).
constexpr std::uint32_t kParts = 23000;
constexpr std::uint32_t kTints = 23001;
constexpr std::uint32_t kName = 23008;
constexpr std::uint32_t kNodeName = 23009;
constexpr std::uint32_t kParamName = 23010;
constexpr std::uint32_t kParamIndex = 23011;
constexpr std::uint32_t kParamValue = 23012;
constexpr std::uint32_t kParamVector = 23013;
constexpr std::uint32_t kValues = 23014;
constexpr std::uint32_t kVectors = 23015;
constexpr std::uint32_t kModelName = 23016;
constexpr std::uint32_t kModelValue = 23017;
constexpr std::uint32_t kModels = 23018;
constexpr std::uint32_t kTexNodeName = 23019;
constexpr std::uint32_t kTexParamName = 23020;
constexpr std::uint32_t kTexName = 23021;
constexpr std::uint32_t kTextures = 23022;

std::wstring Text(const gff4::Struct &from, std::uint32_t label)
{
    return from.String(label).value_or(L"");
}

// An S32 in every file read, 0 to 23.
std::int32_t IndexOf(const gff4::Struct &from)
{
    return from.Get<std::int32_t>(kParamIndex).value_or(0);
}

} // namespace

std::optional<Preset> PresetOf(const gff4::File &file)
{
    const std::optional<gff4::Struct> root = file.Root();
    if (file.Type() != "MOP" || !root)
    {
        return std::nullopt;
    }
    Preset out;
    out.name = Text(*root, kName);
    out.parts = root->Strings(kParts);
    out.tints = root->Strings(kTints);
    for (const gff4::Struct &model : root->Structs(kModels))
    {
        out.models.push_back({.model = Text(model, kModelName), .weight = model.Get<float>(kModelValue).value_or(0)});
    }
    for (const gff4::Struct &value : root->Structs(kValues))
    {
        out.values.push_back({.node = Text(value, kNodeName),
                              .parameter = Text(value, kParamName),
                              .index = IndexOf(value),
                              .value = value.Get<float>(kParamValue).value_or(0)});
    }
    for (const gff4::Struct &vector : root->Structs(kVectors))
    {
        MaterialVector read{
            .node = Text(vector, kNodeName), .parameter = Text(vector, kParamName), .index = IndexOf(vector)};
        const std::vector<float> floats = vector.Floats(kParamVector);
        for (std::size_t i = 0; i < read.value.size() && i < floats.size(); ++i)
        {
            read.value.at(i) = floats[i];
        }
        out.vectors.push_back(std::move(read));
    }
    for (const gff4::Struct &texture : root->Structs(kTextures))
    {
        out.textures.push_back({.node = Text(texture, kTexNodeName),
                                .parameter = Text(texture, kTexParamName),
                                .texture = Text(texture, kTexName)});
    }
    return out;
}

// Notes on ComposeHead, 2026-10-18. The blend, a mesh's part plus each target's difference from
// its mesh's base by its weight, is inferred from the weights (each base takes minus the sum of
// its targets'), not read in the engine's blend code; lib-agent-2 read that the engine composes
// the head from the editor's state (GUI::ComposeMorphTask, 0x00802E60) and writes an ordinary
// morph of it (GFF4 "MORP V0.1", the morph writer 0x0056FA20), kept as "Head_Morph" and a
// counter, so the result is a .mor's shape. The base entries are left out of the sum: with their
// weights as written the two are the same.
//
// The part, its base and its targets do not share a vertex order or count (hm_uhm_bas_0.msh's
// face 2,282 vertices, uh_uhm_bas_0.msh's 2,208, its targets' 2,205 to 2,247), but each has the
// same 1,847 texture coordinates, so a vertex is matched to another's by its texture coordinate,
// and among several with one coordinate (about 240 a face, a seam's sides) by the nearest place.
// Checked against the shipped morphs made from presets, hm_pcc_b02 to b04.mor: 1,628, 1,691 and
// 1,698 of FaceM1's 2,282 vertices come out within 0.1 mm of the morph's; the rest, about the nose
// and mouth, are off by up to 1.6 cm, by no combination of the 245 face targets the game has. Those
// morphs' EyesM1 has 291 vertices where hm_uem_bas_0.msh has 373: they were baked from other
// meshes than the game ships now, so they bound the reading and do not settle it. Bones are not
// composed: the targets' face bones move near the morph's but not to them (27 of hm_pcc_b02.mor's
// 39 off by up to 4 mm), and their body bones, which no morph lists, move too.

namespace
{

std::wstring Lower(std::wstring_view text)
{
    std::wstring out(text);
    std::ranges::transform(out, out.begin(), [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
    return out;
}

// The face, the eyes and the lashes: the part's slot, the mesh node, the base and its targets'
// prefixes, in lower case.
struct MeshKind
{
    std::size_t part;
    std::wstring_view node;
    std::wstring_view base;
    std::array<std::wstring_view, 2> prefixes;
};

constexpr std::array<MeshKind, 3> kMeshes{{
    {.part = 0, .node = L"FaceM1", .base = L"uh_uhm_basa_0", .prefixes = {L"uh_", L"uhm_"}},
    {.part = 1, .node = L"EyesM1", .base = L"uh_uem_basa_0", .prefixes = {L"ue_", L"uem_"}},
    {.part = 5, .node = L"LashesM1", .base = L"uh_ulm_basa_0", .prefixes = {L"ul_", L"ulm_"}},
}};

bool IsBase(std::wstring_view lower)
{
    return std::ranges::any_of(kMeshes, [lower](const MeshKind &kind) { return lower == kind.base; });
}

// A mesh's vertices by their texture coordinates.
class ByUv
{
  public:
    explicit ByUv(MeshVertices mesh) : mesh_(std::move(mesh))
    {
        const std::size_t count = std::min(mesh_.positions.size(), mesh_.uvs.size());
        index_.reserve(count);
        for (std::size_t i = 0; i < count; ++i)
        {
            index_.emplace_back(Key(mesh_.uvs[i]), i);
        }
        std::ranges::sort(index_);
    }

    // Returns the place of the vertex with this coordinate nearest to `near`; nothing for none.
    std::optional<std::array<float, 3>> Nearest(const std::array<float, 2> &uv, const std::array<float, 3> &near) const
    {
        const std::uint64_t key = Key(uv);
        const auto first = std::ranges::lower_bound(index_, std::pair<std::uint64_t, std::size_t>{key, 0});
        if (first == index_.end() || first->first != key)
        {
            return std::nullopt;
        }
        std::size_t best = first->second;
        float bestDistance = std::numeric_limits<float>::max();
        for (auto at = first; at != index_.end() && at->first == key; ++at)
        {
            const std::size_t i = at->second;
            const std::array<float, 3> &p = mesh_.positions[i];
            const float dx = p[0] - near[0];
            const float dy = p[1] - near[1];
            const float dz = p[2] - near[2];
            const float distance = (dx * dx) + (dy * dy) + (dz * dz);
            if (distance < bestDistance)
            {
                bestDistance = distance;
                best = i;
            }
        }
        return mesh_.positions[best];
    }

  private:
    static std::uint64_t Key(const std::array<float, 2> &uv)
    {
        return (std::uint64_t{std::bit_cast<std::uint32_t>(uv[0])} << 32) | std::bit_cast<std::uint32_t>(uv[1]);
    }

    MeshVertices mesh_;
    std::vector<std::pair<std::uint64_t, std::size_t>> index_; // a coordinate and a vertex, sorted
};

// A mesh's targets that are found, each with its weight.
std::vector<std::pair<float, ByUv>> TargetsOf(const Preset &preset, const MeshKind &kind, const MeshOfModel &meshOf)
{
    std::vector<std::pair<float, ByUv>> targets;
    for (const ModelWeight &model : preset.models)
    {
        const std::wstring lower = Lower(model.model);
        const bool ours = std::ranges::any_of(kind.prefixes,
                                              [&lower](std::wstring_view prefix) { return lower.starts_with(prefix); });
        if (!ours || IsBase(lower))
        {
            continue;
        }
        if (std::optional<MeshVertices> target = meshOf(model.model))
        {
            targets.emplace_back(model.weight, ByUv(std::move(*target)));
        }
    }
    return targets;
}

// One mesh's vertices placed: the part's, each moved by the targets' differences from the base.
std::vector<std::array<float, 4>> Places(const Preset &preset, const MeshKind &kind, const MeshOfModel &meshOf)
{
    if (kind.part >= preset.parts.size() || preset.parts[kind.part].empty())
    {
        return {};
    }
    const std::optional<MeshVertices> part = meshOf(preset.parts[kind.part]);
    if (!part)
    {
        return {};
    }
    std::optional<MeshVertices> baseMesh = meshOf(kind.base);
    const std::optional<ByUv> base = baseMesh ? std::optional<ByUv>(ByUv(std::move(*baseMesh))) : std::nullopt;
    const std::vector<std::pair<float, ByUv>> targets =
        base ? TargetsOf(preset, kind, meshOf) : std::vector<std::pair<float, ByUv>>{};
    const std::size_t count = std::min(part->positions.size(), part->uvs.size());
    std::vector<std::array<float, 4>> places(count);
    for (std::size_t i = 0; i < count; ++i)
    {
        const std::array<float, 3> &at = part->positions[i];
        places[i] = {at[0], at[1], at[2], 1.0F};
        const std::optional<std::array<float, 3>> from = base ? base->Nearest(part->uvs[i], at) : std::nullopt;
        if (!from)
        {
            continue;
        }
        for (const auto &[weight, target] : targets)
        {
            if (const std::optional<std::array<float, 3>> to = target.Nearest(part->uvs[i], *from))
            {
                for (std::size_t k = 0; k < 3; ++k)
                {
                    places[i].at(k) += weight * (to->at(k) - from->at(k));
                }
            }
        }
    }
    return places;
}

morph::Node &NodeNamed(morph::Morph &head, std::wstring_view name)
{
    const auto found = std::ranges::find(head.nodes, name, &morph::Node::name);
    if (found != head.nodes.end())
    {
        return *found;
    }
    morph::Node node;
    node.name = name;
    head.nodes.push_back(std::move(node));
    return head.nodes.back();
}

// A node's vector parameter by name, with at least `count` vectors.
std::vector<std::array<float, 4>> &VectorsOf(morph::Node &node, std::wstring_view name, std::size_t count)
{
    auto found = std::ranges::find(node.vectors, name, &morph::VectorParameter::name);
    if (found == node.vectors.end())
    {
        node.vectors.push_back({.name = std::wstring(name), .values = {}});
        found = std::prev(node.vectors.end());
    }
    if (found->values.size() < count)
    {
        found->values.resize(count, std::array<float, 4>{});
    }
    return found->values;
}

// The preset's material values on their nodes: its vectors whole, then its single values into
// their components, then its textures.
void MaterialsOf(const Preset &preset, morph::Morph &head)
{
    for (const MaterialVector &vector : preset.vectors)
    {
        if (vector.index >= 0)
        {
            const auto at = static_cast<std::size_t>(vector.index);
            VectorsOf(NodeNamed(head, vector.node), vector.parameter, at + 1).at(at) = vector.value;
        }
    }
    for (const MaterialValue &value : preset.values)
    {
        if (value.index >= 0)
        {
            const auto at = static_cast<std::size_t>(value.index);
            VectorsOf(NodeNamed(head, value.node), value.parameter, (at / 4) + 1).at(at / 4).at(at % 4) = value.value;
        }
    }
    for (const Texture &texture : preset.textures)
    {
        NodeNamed(head, texture.node).textures.push_back({.name = texture.parameter, .texture = texture.texture});
    }
}

} // namespace

morph::Morph ComposeHead(const Preset &preset, const MeshOfModel &meshOf)
{
    morph::Morph head;
    head.name = preset.name;
    head.parts = preset.parts;
    head.tints = preset.tints;
    for (const MeshKind &kind : kMeshes)
    {
        NodeNamed(head, kind.node).places = Places(preset, kind, meshOf);
    }
    MaterialsOf(preset, head);
    return head;
}

} // namespace dao::core::preset
