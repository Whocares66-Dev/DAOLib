#include "dao/core/Campaign.h"

namespace dao::core::campaign
{

namespace
{

constexpr std::uint32_t kRoot = 3290;
constexpr std::uint32_t kArchives = 3291;
constexpr std::uint32_t kChildren = 3292;
constexpr std::uint32_t kTag = 3293;
constexpr std::uint32_t kArea = 3294;
constexpr std::uint32_t kVariables = 17000;
constexpr std::uint32_t kVariableName = 17001;
constexpr std::uint32_t kVariableType = 17002;
constexpr std::uint32_t kVariableValue = 17003;
constexpr std::uint32_t kEntryAreaList = 17100;
constexpr std::uint32_t kEntryArea = 17101;
constexpr std::uint32_t kEntryPosition = 17102;
constexpr std::uint32_t kEntryOrientation = 17103;
constexpr std::uint32_t kScript = 17104;
constexpr std::uint32_t kClientScript = 17105;
constexpr std::uint32_t kDisplayName = 17106; // English; 17107 to 17114 the others
constexpr std::uint32_t kPackages = 17124;
// A tree deeper than this is a loop in the file.
constexpr int kDeepest = 32;

Node NodeOf(const gff4::Struct &in, int depth)
{
    Node node{.tag = in.String(kTag).value_or(L""),
              .area = in.String(kArea).value_or(L""),
              .archives = in.Strings(kArchives),
              .children = {}};
    if (depth < kDeepest)
    {
        for (const gff4::Struct &child : in.Structs(kChildren))
        {
            node.children.push_back(NodeOf(child, depth + 1));
        }
    }
    return node;
}

std::array<float, 3> Three(const gff4::Struct &in, std::uint32_t label)
{
    const std::vector<float> floats = in.Floats(label);
    std::array<float, 3> out{};
    for (std::size_t i = 0; i < out.size() && i < floats.size(); ++i)
    {
        out[i] = floats[i];
    }
    return out;
}

// A variable's value is a generic field of the type the value has, read as
// it is whatever the variable's own type says.
Variable VariableOf(const gff4::Struct &in)
{
    Variable variable{.name = in.String(kVariableName).value_or(L""),
                      .type = in.Get<std::uint8_t>(kVariableType).value_or(0),
                      .value = std::monostate{}};
    if (const auto whole = in.Get<std::int32_t>(kVariableValue))
    {
        variable.value = *whole;
    }
    else if (const auto real = in.Get<float>(kVariableValue))
    {
        variable.value = *real;
    }
    else if (auto text = in.String(kVariableValue))
    {
        variable.value = std::move(*text);
    }
    return variable;
}

} // namespace

std::optional<Campaign> CampaignOf(const gff4::File &file)
{
    const std::optional<gff4::Struct> root = file.Root();
    if (file.Type() != "CIF" || !root)
    {
        return std::nullopt;
    }
    Campaign campaign{
        .script = root->String(kScript).value_or(L""),
        .clientScript = root->String(kClientScript).value_or(L""),
        .entryArea = root->String(kEntryArea).value_or(L""),
        .entryAreaList = root->String(kEntryAreaList).value_or(L""),
        .entryPosition = Three(*root, kEntryPosition),
        .entryOrientation = Three(*root, kEntryOrientation),
        .displayName = root->String(kDisplayName).value_or(L""),
        .variables = {},
        .areas = {},
        .packages = root->Strings(kPackages),
    };
    for (const gff4::Struct &variable : root->Structs(kVariables))
    {
        campaign.variables.push_back(VariableOf(variable));
    }
    if (const auto tree = root->Child(kRoot))
    {
        campaign.areas = NodeOf(*tree, 0);
    }
    return campaign;
}

} // namespace dao::core::campaign
