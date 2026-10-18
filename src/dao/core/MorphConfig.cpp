// dao/core/MorphConfig.h: chargenmorphcfg.xml read a tag at a time (dao/core/XmlTags.h). A
// <resource> belongs to the list its first enclosing element under <morph_config> names, and to
// the race and gender of a second one where there is one.
#include "dao/core/MorphConfig.h"

#include "dao/core/XmlTags.h"

#include <array>
#include <utility>

namespace dao::core::preset
{

namespace
{

using xml::Attribute;
using xml::ElementOf;

// The elements open under <morph_config>, and the lists read.
class Reader
{
  public:
    void Take(std::string_view tag)
    {
        while (!tag.empty() && xml::Space(tag.back()))
        {
            tag.remove_suffix(1);
        }
        if (tag.starts_with('/'))
        {
            Close(ElementOf(tag.substr(1)));
            return;
        }
        const std::string_view element = ElementOf(tag);
        const bool closes = tag.ends_with('/');
        if (!inside_)
        {
            inside_ = element == "morph_config" && !closes;
            return;
        }
        if (element == "resource" && !open_.empty())
        {
            Add(tag);
            return;
        }
        if (!closes)
        {
            open_.emplace_back(element);
        }
    }

    Config Done()
    {
        return std::move(config_);
    }

  private:
    void Close(std::string_view element)
    {
        if (!open_.empty() && open_.back() == element)
        {
            open_.pop_back();
        }
        else if (element == "morph_config")
        {
            inside_ = false;
            open_.clear();
        }
    }

    void Add(std::string_view tag)
    {
        const std::string &kind = open_.front();
        const std::string group = open_.size() > 1 ? open_[1] : std::string();
        if (config_.lists.empty() || config_.lists.back().kind != kind || config_.lists.back().group != group)
        {
            config_.lists.push_back({.kind = kind, .group = group, .resources = {}});
        }
        config_.lists.back().resources.push_back(
            {.name = Attribute(tag, "name").value_or(""), .cut = Attribute(tag, "cut").value_or("")});
    }

    bool inside_ = false;
    std::vector<std::string> open_;
    Config config_;
};

} // namespace

std::string_view GroupOf(RaceGender raceGender)
{
    static constexpr std::array<std::string_view, 6> kGroups{"human_male",   "human_female", "dwarf_male",
                                                             "dwarf_female", "elf_male",     "elf_female"};
    return kGroups.at(static_cast<std::size_t>(raceGender));
}

const List *Config::Find(std::string_view kind, RaceGender raceGender) const
{
    return Find(kind, GroupOf(raceGender));
}

std::vector<HairChoice> HairChoices(const List &hairs)
{
    std::vector<HairChoice> choices;
    for (const Resource &hair : hairs.resources)
    {
        if (hair.cut.empty())
        {
            choices.push_back({.name = hair.name, .cut = 0});
            continue;
        }
        for (const char digit : hair.cut)
        {
            if (digit >= '0' && digit <= '4')
            {
                choices.push_back({.name = hair.name, .cut = digit - '0'});
            }
        }
    }
    return choices;
}

const List *Config::Find(std::string_view kind, std::string_view group) const
{
    for (const List &list : lists)
    {
        if (list.kind == kind && list.group == group)
        {
            return &list;
        }
    }
    return nullptr;
}

Config ReadMorphConfig(std::string_view xml)
{
    Reader reader;
    xml::ForEachTag(xml, [&reader](std::string_view tag) { reader.Take(tag); });
    return reader.Done();
}

} // namespace dao::core::preset
