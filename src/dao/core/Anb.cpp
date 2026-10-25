#include "dao/core/Anb.h"

#include "dao/core/XmlTags.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cwctype>
#include <filesystem>
#include <ranges>
#include <variant>

namespace dao::core::anb
{

namespace
{

constexpr std::uint32_t kNodes = 4021;
constexpr std::uint32_t kTreeName = 4022;
constexpr std::uint32_t kName = 4024;
constexpr std::uint32_t kFile = 4025;
constexpr std::uint32_t kWeight = 4026;
constexpr std::uint32_t kFlags = 4027;
constexpr std::uint32_t kFirstChild = 4028;
constexpr std::uint32_t kChildren = 4029;
constexpr std::uint32_t kParent = 4030;
// A replacement chain longer than this is a loop in the table.
constexpr int kMostReplacements = 16;

bool SameName(std::wstring_view a, std::wstring_view b)
{
    return std::ranges::equal(a, b, [](wchar_t x, wchar_t y) { return std::towlower(x) == std::towlower(y); });
}

} // namespace

const Node *Tree::Find(std::wstring_view node) const
{
    const auto found = std::ranges::find_if(nodes, [&](const Node &n) { return SameName(n.name, node); });
    return found != nodes.end() ? &*found : nullptr;
}

// Read 2026-10-24 (DAOLib #262) over the 21 level .anb files of hrt000d, lak100c/d/n, lak300d and
// lak400d: an XML ModelAnimationTree named for its room, holding one AnimationTree (VectorBlend,
// Vector3), whose RootAnimationNode holds the nodes with a clip, each Name, Animation (the .ani),
// WeightType Dynamic, StartWeight and EndWeight 1, Speed 1 and NoLoop False in all 27. A placed
// model's EnvModelDefaultAnimation names a node by its Name. The nodes with no clip are left out.
const RoomNode *RoomTree::Find(std::wstring_view node) const
{
    const auto found = std::ranges::find_if(nodes, [&](const RoomNode &n) { return SameName(n.name, node); });
    return found != nodes.end() ? &*found : nullptr;
}

std::optional<RoomTree> RoomTreeOf(std::string_view xml)
{
    const auto wide = [](const std::string &utf8) {
        return std::filesystem::path(std::u8string(utf8.begin(), utf8.end())).wstring();
    };
    std::optional<RoomTree> tree;
    xml::ForEachTag(xml, [&](std::string_view tag) {
        const std::string_view element = xml::ElementOf(tag);
        if (element == "ModelAnimationTree")
        {
            tree = RoomTree{.name = wide(xml::Attribute(tag, "Name").value_or("")), .nodes = {}};
        }
        else if (element == "AnimationNode" && tree)
        {
            if (const auto file = xml::Attribute(tag, "Animation"))
            {
                std::string loop = xml::Attribute(tag, "NoLoop").value_or("");
                std::ranges::transform(loop, loop.begin(), [](unsigned char c) { return std::tolower(c); });
                float speed = 1;
                const std::string given = xml::Attribute(tag, "Speed").value_or("1");
                std::from_chars(given.data(), given.data() + given.size(), speed);
                tree->nodes.push_back(RoomNode{.name = wide(xml::Attribute(tag, "Name").value_or("")),
                                               .file = wide(*file),
                                               .speed = speed,
                                               .noLoop = loop == "true"});
            }
        }
    });
    return tree;
}

std::optional<Tree> TreeOf(const gff4::File &file)
{
    const std::optional<gff4::Struct> root = file.Root();
    if (file.Type() != "ANBT" || !root)
    {
        return std::nullopt;
    }
    Tree tree{.name = root->String(kTreeName).value_or(L""), .nodes = {}};
    for (const gff4::Struct &node : root->Structs(kNodes))
    {
        tree.nodes.push_back({
            .name = node.String(kName).value_or(L""),
            .file = node.String(kFile).value_or(L""),
            .weight = node.Get<float>(kWeight).value_or(1.0F),
            .flags = node.Get<std::uint32_t>(kFlags).value_or(0),
            .firstChild = node.Get<std::uint32_t>(kFirstChild).value_or(0),
            .children = node.Get<std::uint32_t>(kChildren).value_or(0),
            .parent = node.Get<std::int32_t>(kParent).value_or(-1),
        });
    }
    return tree;
}

// 0x006A1470, read 2026-10-17: for a creature of gender 2 the tree's name
// is cut at its last '.' and "_fem.anb" put after it, and the resource
// manager asked whether there is one; if there is it is taken.
std::wstring FemaleTree(std::wstring_view tree)
{
    const std::size_t dot = tree.rfind(L'.');
    std::wstring female(tree.substr(0, dot));
    female += L"_fem.anb";
    return female;
}

// A placeable's clip has no fallback: its tree's slot 0x10 answers 0 (lib-agent-2's reading,
// 2026-10-18, DAOLib #177), so a missing node plays nothing.
std::optional<Clip> ClipOf(const m2da::Merged &animations, std::int64_t animation, std::span<const Tree *const> trees,
                           bool replacements)
{
    std::int64_t id = animation;
    for (int hop = 0; hop < kMostReplacements; ++hop)
    {
        const m2da::Row *row = m2da::Taken(animations, id);
        if (row == nullptr)
        {
            return std::nullopt;
        }
        const GdaFile::Cell *name = m2da::CellOf(animations, *row, "AnimName");
        if (const auto *text = name != nullptr ? std::get_if<std::wstring>(name) : nullptr; text != nullptr)
        {
            for (std::size_t i = 0; i < trees.size(); ++i)
            {
                if (const Node *node = trees[i] != nullptr ? trees[i]->Find(*text) : nullptr)
                {
                    return Clip{.animation = id, .node = node, .tree = i};
                }
            }
        }
        if (!replacements)
        {
            return std::nullopt;
        }
        const GdaFile::Cell *next = m2da::CellOf(animations, *row, "Replacement");
        const auto *replacement = next != nullptr ? std::get_if<std::int64_t>(next) : nullptr;
        if (replacement == nullptr || *replacement == 0 || *replacement == id)
        {
            return std::nullopt;
        }
        id = *replacement;
    }
    return std::nullopt;
}

} // namespace dao::core::anb
