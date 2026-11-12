// Notes on AddIns.xml, read 2026-10-12 on the GOG install's: an <AddInItem>
// an add-in, its attributes UID (in lower case its folder under Addins\),
// Name, ExtendedModuleUID (the campaign it extends: "Single Player",
// "DAO_PRC_EP_1" for Awakening's, or none), Priority, Enabled, State, Format,
// BioWare, RequiresAuthorization, then a <Title> whose DefaultText is its
// English name; 39 items on the Ultimate Edition. offers.xml is the same with
// <OfferItem> and no Enabled. Neither is parsed as XML in full: the item
// tags' attributes, the title's and the title's text in each language are
// what is read, with XML's five entities (dao/core/XmlTags.h). Written for Eluvian's ghilana and moved here (DAOLib
// #106).
#include "dao/core/AddIns.h"

#include "dao/core/XmlTags.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <utility>

namespace dao::core
{

namespace
{

using xml::Attribute;

std::optional<int> Number(const std::optional<std::string> &text)
{
    if (!text)
    {
        return std::nullopt;
    }
    int value = 0;
    const auto *end = text->data() + text->size();
    const auto [at, error] = std::from_chars(text->data(), end, value);
    if (error != std::errc() || at != end)
    {
        return std::nullopt;
    }
    return value;
}

// The languages AddInsManager::ParseAddInItem (0x005F65C0) reads a Title in, each its own slot of the
// record (en-us +0x17C to hu-hu +0x278).
constexpr std::array<std::string_view, 10> kTitleLanguages{"en-us", "fr-fr", "it-it",  "de-de", "es-es",
                                                           "pl-pl", "ru-ru", "pseudo", "cs-cz", "hu-hu"};

// The text TinyXML's GetText gives an element whose body this is: its first child's, where that is
// text or a CDATA section, else none. Text has its entities replaced and its spaces condensed, as
// TinyXML reads it by default: none at either end, a run of them inside one space.
std::optional<std::string> TextOf(std::string_view body)
{
    std::size_t at = 0;
    while (at < body.size() && xml::Space(body[at]))
    {
        ++at;
    }
    constexpr std::string_view kCdata = "<![CDATA[";
    if (body.substr(at).starts_with(kCdata))
    {
        const std::size_t start = at + kCdata.size();
        const std::size_t end = body.find("]]>", start);
        return std::string(body.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start));
    }
    const std::size_t end = body.find('<', at);
    const std::string_view text = body.substr(at, end == std::string_view::npos ? std::string_view::npos : end - at);
    if (text.empty())
    {
        return std::nullopt;
    }
    std::string condensed;
    bool space = false;
    for (const char letter : text)
    {
        if (xml::Space(letter))
        {
            space = true;
            continue;
        }
        if (space && !condensed.empty())
        {
            condensed.push_back(' ');
        }
        space = false;
        condensed.push_back(letter);
    }
    return xml::Unescape(condensed);
}

// The body of an element's first child of a name: what is between its opening and its closing.
std::optional<std::string_view> ChildBody(std::string_view parent, std::string_view name)
{
    const std::string open = "<" + std::string(name);
    for (std::size_t at = parent.find(open); at != std::string_view::npos; at = parent.find(open, at + 1))
    {
        const std::size_t after = at + open.size();
        if (after >= parent.size() || (parent[after] != '>' && parent[after] != '/' && !xml::Space(parent[after])))
        {
            continue;
        }
        const std::size_t close = parent.find('>', after);
        if (close == std::string_view::npos || parent[close - 1] == '/')
        {
            return std::nullopt;
        }
        const std::size_t end = parent.find("</" + std::string(name), close);
        return parent.substr(close + 1, end == std::string_view::npos ? std::string_view::npos : end - close - 1);
    }
    return std::nullopt;
}

// A Title's text in each of its languages, by the element named for each (ParseAddInItem,
// TiXmlNode::FirstChildElement then TiXmlElement::GetText, a slot left empty where either gives none).
std::vector<std::pair<std::string, std::string>> TitlesOf(std::string_view title)
{
    std::vector<std::pair<std::string, std::string>> titles;
    for (const std::string_view language : kTitleLanguages)
    {
        if (const auto body = ChildBody(title, language))
        {
            if (auto text = TextOf(*body))
            {
                titles.emplace_back(language, std::move(*text));
            }
        }
    }
    return titles;
}

// An item's title: the first <Title ...> after its tag (at from) and before its end, its DefaultText
// and its text in each language.
void ReadTitle(std::string_view xml, std::size_t from, std::size_t end, AddIn &item)
{
    const auto title = xml.find("<Title", from);
    if (title == std::string_view::npos || (end != std::string_view::npos && title >= end))
    {
        return;
    }
    const auto titleClose = xml.find('>', title);
    if (titleClose == std::string_view::npos)
    {
        return;
    }
    item.title = Attribute(xml.substr(title, titleClose - title), "DefaultText").value_or("");
    if (xml[titleClose - 1] != '/')
    {
        const auto titleEnd = xml.find("</Title", titleClose);
        item.titles = TitlesOf(xml.substr(
            titleClose + 1, titleEnd == std::string_view::npos ? std::string_view::npos : titleEnd - titleClose - 1));
    }
}

} // namespace

// AddInRecord::GetLocalizedText (0x005EDEE0) takes DragonAge.ini's Locale (CClientOptions +0x118) in
// lower case, and the slot of the language it names; en-us's for any other. It never falls back to
// the DefaultText. Read 2026-11-11 by lib-agent, from paragon-agent-2's reading.
std::string TitleIn(const AddIn &item, std::string_view locale)
{
    std::string lower(locale);
    for (char &letter : lower)
    {
        if (letter >= 'A' && letter <= 'Z')
        {
            letter = static_cast<char>(letter - 'A' + 'a');
        }
    }
    const bool known = std::ranges::find(kTitleLanguages, lower) != kTitleLanguages.end();
    const std::string_view language = known ? std::string_view(lower) : std::string_view("en-us");
    const auto at = std::ranges::find(item.titles, language, &std::pair<std::string, std::string>::first);
    return at == item.titles.end() ? std::string() : at->second;
}

std::vector<AddIn> ReadAddIns(std::string_view xml, std::string_view tag)
{
    std::vector<AddIn> items;
    const std::string open = "<" + std::string(tag);
    std::size_t at = 0;
    while ((at = xml.find(open, at)) != std::string_view::npos)
    {
        const std::size_t after = at + open.size();
        if (after >= xml.size() ||
            (xml[after] != ' ' && xml[after] != '>' && xml[after] != '\t' && xml[after] != '\r' && xml[after] != '\n'))
        {
            at = after;
            continue;
        }
        const auto close = xml.find('>', after);
        if (close == std::string_view::npos)
        {
            break;
        }
        const std::string_view head = xml.substr(at, close - at);
        AddIn item;
        item.uid = Attribute(head, "UID").value_or("");
        item.name = Attribute(head, "Name").value_or("");
        item.extends = Attribute(head, "ExtendedModuleUID").value_or("");
        item.priority = Number(Attribute(head, "Priority"));
        item.state = Number(Attribute(head, "State"));
        if (const auto enabled = Number(Attribute(head, "Enabled")))
        {
            item.enabled = *enabled != 0;
        }
        item.bioware = Number(Attribute(head, "BioWare")).value_or(0) != 0;
        ReadTitle(xml, close, xml.find("</" + std::string(tag), close), item);
        if (!item.uid.empty())
        {
            items.push_back(std::move(item));
        }
        at = close;
    }
    return items;
}

} // namespace dao::core
