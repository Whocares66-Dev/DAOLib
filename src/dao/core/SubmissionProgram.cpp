// dao/core/SubmissionProgram.h: a .sub.xml read the way the engine's builder (SQProgramBuilderXML2,
// 0x00472800..0x00474000) reads it: the XML's elements one at a time, and the text inside a
// SubmissionGroup a line at a time, a line cut at its commas and each piece at its spaces. [C] is read at an
// address of GOG's exe (2026-11-01), [I] inferred.
#include "dao/core/SubmissionProgram.h"

#include "dao/core/XmlTags.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <utility>

namespace dao::core::sub
{

namespace
{

constexpr std::array<std::string_view, kCategoryCount> kCategories = {"transparent",
                                                                      "additive",
                                                                      "opaque",
                                                                      "punchthrough",
                                                                      "debug",
                                                                      "primitive",
                                                                      "emitter",
                                                                      "vista",
                                                                      "Occluder",
                                                                      "terrain",
                                                                      "character",
                                                                      "shadowreceiver",
                                                                      "cloth",
                                                                      "speedtree",
                                                                      "static1",
                                                                      "static2",
                                                                      "static3",
                                                                      "static0",
                                                                      "eye",
                                                                      "hair",
                                                                      "weapon",
                                                                      "sky",
                                                                      "tooloverlay",
                                                                      "3dgui",
                                                                      "face",
                                                                      "closeup",
                                                                      "2dgui",
                                                                      "distortion",
                                                                      "prop",
                                                                      "emittergroup",
                                                                      "MultipassHair",
                                                                      "Black",
                                                                      "distortion_diffuse",
                                                                      "emittersplat",
                                                                      "water",
                                                                      "transparent_body",
                                                                      "StaticBlendFlat",
                                                                      "LowLOD"};

struct AttributeName
{
    std::string_view name;
    std::uint32_t bit;
};

constexpr AttributeName kAttributes[] = {
    {"occluder", attribute::kOccluder},
    {"cutout", attribute::kCutout},
    {"toolswireframe", attribute::kToolsWireframe},
    {"fadeoutobject", attribute::kFadeoutObject},
    {"fadeoutpreemitter", attribute::kFadeoutPreEmitter},
    {"LODFade", attribute::kLodFade},
    {"transparent", attribute::kTransparent},
    {"noDepthTest", attribute::kNoDepthTest},
    {"hidden", attribute::kHidden},
    {"sorted_emitter", attribute::kSortedEmitter},
    {"invisiblecaster", attribute::kInvisibleCaster},
    {"unsupportedcaster", attribute::kUnsupportedCaster},
    {"ignore_distortion", attribute::kIgnoreDistortion},
};

bool Same(std::string_view a, std::string_view b)
{
    return std::ranges::equal(a, b, [](char x, char y) {
        return std::tolower(static_cast<unsigned char>(x)) == std::tolower(static_cast<unsigned char>(y));
    });
}

std::string Lower(std::string_view text)
{
    std::string out(text);
    std::ranges::transform(out, out.begin(),
                           [](char c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); });
    return out;
}

// The C runtime's _wtoi (0x008E643E): leading blanks, a sign, then digits; anything else reads as 0.
int ToInt(std::string_view text)
{
    std::size_t at = 0;
    while (at < text.size() && xml::Space(text[at]))
    {
        ++at;
    }
    bool negative = false;
    if (at < text.size() && (text[at] == '-' || text[at] == '+'))
    {
        negative = text[at] == '-';
        ++at;
    }
    int value = 0;
    while (at < text.size() && text[at] >= '0' && text[at] <= '9')
    {
        value = (value * 10) + (text[at] - '0');
        ++at;
    }
    return negative ? -value : value;
}

// ShadowMaps::GetPartitionMask (0x004518A0): -1 is every partition, else one bit; x86's shift takes the count
// modulo 32.
std::uint32_t PartitionMask(int partition)
{
    return partition == -1 ? 0xFFFFFFFFu : 1u << (static_cast<unsigned>(partition) & 31u);
}

// The directive kinds that merge with one of their kind already in a list (CanMerge, slot 2 [C 0x00AE80C8..]).
bool CanMerge(DirectiveKind kind)
{
    return kind == DirectiveKind::Always || kind == DirectiveKind::Class || kind == DirectiveKind::Attribute ||
           kind == DirectiveKind::NoShadowCast || kind == DirectiveKind::ShadowCaster;
}

// Puts a directive in a list as the engine's add does (0x004720C0, 0x0046ED50): into the one of its kind, if the
// kind merges, ORing the masks (the others have nothing to merge), else at the end.
void AddDirective(std::vector<Directive> &list, Directive directive)
{
    if (CanMerge(directive.kind))
    {
        for (Directive &present : list)
        {
            if (present.kind == directive.kind)
            {
                present.categories |= directive.categories;
                present.bits |= directive.bits;
                return;
            }
        }
    }
    list.push_back(std::move(directive));
}

// Cuts a line at its spaces and tabs: wcstok with " \t" (0x00ADEE0C).
std::vector<std::string_view> Words(std::string_view piece)
{
    std::vector<std::string_view> out;
    std::size_t at = 0;
    while (at < piece.size())
    {
        while (at < piece.size() && (piece[at] == ' ' || piece[at] == '\t'))
        {
            ++at;
        }
        const std::size_t start = at;
        while (at < piece.size() && piece[at] != ' ' && piece[at] != '\t')
        {
            ++at;
        }
        if (at > start)
        {
            out.push_back(piece.substr(start, at - start));
        }
    }
    return out;
}

// What a line's chunk is cut into before the builder sees it: past a '#' is a comment (0x00472580), the rest cut at its
// commas (0x004724B0) into at most 16 pieces, each without its leading blanks. (The engine also skips a piece of more
// than 256 characters [C 0x0047251E], which cannot happen: a line of 256 is refused whole first.)
std::vector<std::string_view> Pieces(std::string_view line)
{
    constexpr std::size_t kPieces = 16; // the statement buffer is 0x4000 bytes of 0x400 [C 0x004724D9, 0x00472553]
    line = line.substr(0, line.find('#'));
    std::vector<std::string_view> out;
    std::size_t at = 0;
    while (at <= line.size() && out.size() < kPieces)
    {
        std::size_t end = line.find(',', at);
        if (end == std::string_view::npos)
        {
            end = line.size();
        }
        std::string_view piece = line.substr(at, end - at);
        while (!piece.empty() && (piece.front() == ' ' || piece.front() == '\t'))
        {
            piece.remove_prefix(1);
        }
        if (!piece.empty())
        {
            out.push_back(piece);
        }
        at = end + 1;
    }
    return out;
}

// The builder: a program being made from a file's elements and text.
class Builder
{
  public:
    bool Failed() const
    {
        return failed_;
    }

    Program Take()
    {
        return std::move(program_);
    }

    void OpenInstruction(std::string_view tag)
    {
        program_.id = xml::Attribute(tag, "id").value_or("");
    }

    // A SubmissionGroup, or a SubmissionClass, which the builder makes the same [C 0x00472C95, 0x00472D53]: allowed
    // only directly inside the instruction and with no other open [C 0x00472C72, 0x00472C77].
    void OpenGroup(std::string_view tag, bool directlyInside)
    {
        if (!directlyInside || open_)
        {
            failed_ = true;
            return;
        }
        open_ = true;
        cls_.reset();
        last_.clear();
        program_.groups.push_back({.id = xml::Attribute(tag, "id").value_or(""), .classes = {}, .exclusions = {}});
    }

    void CloseGroup()
    {
        open_ = false;
        cls_.reset();
    }

    // The text between two tags, inside a group: each line a chunk of its own (expat gives it a line at a time).
    void Text(std::string_view text)
    {
        if (!open_)
        {
            return;
        }
        for (std::size_t at = 0; at <= text.size();)
        {
            std::size_t end = text.find('\n', at);
            if (end == std::string_view::npos)
            {
                end = text.size();
            }
            Chunk(text.substr(at, end - at));
            at = end + 1;
        }
    }

  private:
    Group &CurrentGroup()
    {
        return program_.groups.back();
    }

    void Chunk(std::string_view line)
    {
        constexpr std::size_t kLongest = 255; // 0x1FE bytes; a chunk past it is an error [C 0x00473FB9..0x00473FD7]
        while (!line.empty() && (xml::Space(line.front())))
        {
            line.remove_prefix(1);
        }
        while (!line.empty() && line.back() == '\r')
        {
            line.remove_suffix(1);
        }
        if (line.size() > kLongest)
        {
            failed_ = true;
            return;
        }
        const std::string text = xml::Unescape(line); // the pieces point into it
        for (const std::string_view piece : Pieces(text))
        {
            Statement(Words(piece));
            if (failed_)
            {
                return;
            }
        }
    }

    // The class the builder is filling, made if none is [C 0x00472800 with no name].
    Class &SelectClass(std::string_view name = {})
    {
        Group &group = CurrentGroup();
        if (name.empty())
        {
            if (!cls_)
            {
                group.classes.emplace_back();
                cls_ = group.classes.size() - 1;
            }
            return group.classes[*cls_];
        }
        const auto found = std::ranges::find_if(group.classes, [&](const Class &c) { return Same(c.name, name); });
        if (found != group.classes.end())
        {
            cls_ = static_cast<std::size_t>(found - group.classes.begin());
        }
        else
        {
            group.classes.emplace_back();
            group.classes.back().name = std::string(name);
            cls_ = group.classes.size() - 1;
        }
        return group.classes[*cls_];
    }

    // The dispatch on a piece's first word [C 0x00473C80].
    void Statement(const std::vector<std::string_view> &words)
    {
        if (words.empty())
        {
            return;
        }
        const std::string word = Lower(words[0]);
        const std::vector<std::string_view> rest(words.begin() + 1, words.end());
        if (word == "include")
        {
            Include(rest);
        }
        else if (word == "exclude" || word == "except")
        {
            Exclude(rest);
        }
        else if (word == "force")
        {
            Force(rest);
        }
        else if (word == "trigger")
        {
            Trigger(rest);
        }
        else if (word == "effect")
        {
            Effect(rest);
        }
        else if (word == "requires")
        {
            Requires(rest);
        }
        else if (word == "shadows")
        {
            Shadows(rest);
        }
        else if (word == "speedtree")
        {
            if (rest.empty() || !Same(rest[0], "renderAllFadedTrees"))
            {
                failed_ = true; // [C 0x004736D0]
                return;
            }
            Add(OperationType::RenderFadedSpeedTrees, OperationWhen::After, 0, {}, {});
        }
        else if (word == "forcedraw")
        {
            // A class must be there; the engine does not make one [C 0x00472850], and a statement before any would
            // write through a null class. A class is made here.
            SelectClass().forceDraw = true;
            last_.clear();
        }
        else if (word == "select")
        {
            if (rest.empty())
            {
                failed_ = true; // [C 0x0047358F: error 1]
                return;
            }
            SelectClass(rest[0]);
            last_.clear();
        }
        else if (word == "sort")
        {
            Sort(rest);
        }
        else if (word == "depth")
        {
            // Known and ignored [C 0x00473D8A, to 0x00473E8E].
        }
        else if (word == "keep")
        {
            Keep(rest);
        }
        else if (word == "render")
        {
            Render(rest);
        }
        else
        {
            Continue(words);
        }
    }

    // A piece whose first word is no keyword goes on with the last `include`, `exclude`, `effect` or `requires`
    // line [C 0x00473E56..0x00473F37]; with none, an error [C 0x00473F3D]; after another keyword, nothing.
    void Continue(const std::vector<std::string_view> &words)
    {
        if (last_.empty())
        {
            failed_ = true;
            return;
        }
        if (last_ == "include")
        {
            Include(words);
        }
        else if (last_ == "exclude")
        {
            Exclude(words);
        }
        else if (last_ == "effect")
        {
            Effect(words);
        }
        else if (last_ == "requires")
        {
            Requires(words);
        }
    }

    // Returns the kind a directive's word names, or nothing [C 0x004738D0, 10 for none].
    static std::optional<DirectiveKind> KindOf(std::string_view word)
    {
        const std::string lower = Lower(word);
        if (lower == "class")
        {
            return DirectiveKind::Class;
        }
        if (lower == "material")
        {
            return DirectiveKind::MaterialName;
        }
        if (lower == "semantic")
        {
            return DirectiveKind::SemanticName;
        }
        if (lower == "attribute")
        {
            return DirectiveKind::Attribute;
        }
        if (lower == "shadowcaster")
        {
            return DirectiveKind::ShadowCaster;
        }
        if (lower == "invisibleshadowcaster")
        {
            return DirectiveKind::InvisibleShadowCaster;
        }
        if (lower == "supportsmultipass")
        {
            return DirectiveKind::MultiPass;
        }
        return std::nullopt;
    }

    // Makes a directive of a kind from its value [C 0x00472E00]. A class or attribute the engine has no name
    // for is an error [C 0x00472E4E, 0x00472EA4].
    std::optional<Directive> Value(DirectiveKind kind, std::string_view word)
    {
        Directive directive{.kind = kind, .categories = 0, .bits = 0, .name = {}, .flag = false};
        switch (kind)
        {
        case DirectiveKind::Class: {
            const auto mask = CategoryMask(word);
            if (!mask)
            {
                failed_ = true;
                return std::nullopt;
            }
            directive.categories = *mask;
            break;
        }
        case DirectiveKind::Attribute: {
            const auto bit = AttributeBit(word);
            if (!bit)
            {
                failed_ = true;
                return std::nullopt;
            }
            directive.bits = *bit;
            break;
        }
        case DirectiveKind::ShadowCaster:
        case DirectiveKind::InvisibleShadowCaster:
            directive.bits = PartitionMask(ToInt(word));
            break;
        case DirectiveKind::MultiPass:
            directive.flag = Same(word, "true");
            break;
        default:
            directive.name = std::string(word);
            break;
        }
        return directive;
    }

    // `include all|<kind> <value> [in <class>]` [C 0x004739F0]: states 0 (the kind), 1 (its value), 2 (made; an
    // `in` may follow), 3 (the class's name). A directive that cannot be made leaves the statement dropped.
    void Include(const std::vector<std::string_view> &words)
    {
        last_ = "include";
        enum State
        {
            Kind,
            Wanted,
            Made,
            ClassName,
            Done
        } state = Kind;
        std::optional<Directive> directive;
        std::optional<DirectiveKind> kind;
        std::string className;
        for (const std::string_view word : words)
        {
            if (state == Done)
            {
                break;
            }
            if (state == Kind)
            {
                if (Same(word, "all"))
                {
                    directive = Directive{};
                    state = Made;
                    continue;
                }
                kind = KindOf(word);
                if (!kind)
                {
                    return;
                }
                state = Wanted;
            }
            else if (state == Wanted)
            {
                directive = Value(*kind, word);
                if (!directive)
                {
                    return;
                }
                state = Made;
            }
            else if (state == Made)
            {
                if (!Same(word, "in"))
                {
                    failed_ = true; // [C 0x00473AF3]
                    return;
                }
                state = ClassName;
            }
            else
            {
                className = std::string(word);
                state = Done;
            }
        }
        if (!directive || (state != Made && state != Done))
        {
            failed_ = true; // ran out of words with no directive, or after `in` [C 0x00473AEC, 0x00473B18]
            return;
        }
        AddDirective(SelectClass(className).includes, std::move(*directive));
    }

    // `exclude|except all|<kind> <value>` [C 0x00473B50]: into the group, whichever class is being filled.
    void Exclude(const std::vector<std::string_view> &words)
    {
        last_ = "exclude";
        std::optional<Directive> directive;
        std::optional<DirectiveKind> kind;
        for (const std::string_view word : words)
        {
            if (directive)
            {
                break; // the words after it are not read [C 0x00473B93]
            }
            if (!kind)
            {
                if (Same(word, "all"))
                {
                    directive = Directive{};
                    continue;
                }
                kind = KindOf(word);
                if (!kind)
                {
                    return;
                }
            }
            else
            {
                directive = Value(*kind, word);
                if (!directive)
                {
                    return;
                }
            }
        }
        if (!directive)
        {
            failed_ = true;
            return;
        }
        AddDirective(CurrentGroup().exclusions, std::move(*directive));
    }

    // Puts an operation on the class being filled, made if none is [C 0x00472370].
    void Add(OperationType type, OperationWhen when, int number, std::string_view first, std::string_view second)
    {
        SelectClass().operations.push_back(
            {.type = type, .when = when, .number = number, .first = std::string(first), .second = std::string(second)});
    }

    // `force semantic <name>` and `force material <library>.<object>` [C 0x00472F60]. The material's object is
    // read and thrown away by the engine: only the library is loaded [C 0x0046F2D6]. A material with no '.' makes
    // no operation, and neither does any other word.
    void Force(const std::vector<std::string_view> &words)
    {
        last_.clear(); // [C 0x00472FBF]
        if (words.size() < 2)
        {
            return;
        }
        if (Same(words[0], "semantic"))
        {
            Add(OperationType::SemanticOverride, OperationWhen::PerObject, 0, words[1], {});
        }
        else if (Same(words[0], "material"))
        {
            const std::size_t dot = words[1].find('.');
            if (dot != std::string_view::npos)
            {
                Add(OperationType::MaterialOverride, OperationWhen::PerObject, 0, words[1].substr(0, dot),
                    words[1].substr(dot + 1));
            }
        }
    }

    // `trigger <event>` [C 0x00473360]: an event the engine does not know is an error; the endframe,
    // staticgeomend and motiongeometryend events run after the class's nodes, the rest before. The words after
    // the first, and a piece after a comma, are not read.
    void Trigger(const std::vector<std::string_view> &words)
    {
        last_ = "trigger";
        const int event = words.empty() ? -1 : TriggerEvent(words[0]);
        if (event < 0)
        {
            failed_ = true; // [C 0x004733B7]
            return;
        }
        const bool after = event == 3 || event == 6 || event == 8;
        Add(OperationType::EventTrigger, after ? OperationWhen::After : OperationWhen::Before, event, {}, {});
    }

    // `effect <effect>.<stage>` [C 0x00473110]: no '.' is an error.
    void Effect(const std::vector<std::string_view> &words)
    {
        last_ = "effect";
        const std::size_t dot = words.empty() ? std::string_view::npos : words[0].find('.');
        if (dot == std::string_view::npos)
        {
            failed_ = true; // [C 0x004731EC]
            return;
        }
        Add(OperationType::EffectStage, OperationWhen::Before, 0, words[0].substr(0, dot), words[0].substr(dot + 1));
    }

    // `requires shadows enabled` and `requires fbeffects` [C 0x00473210]: the class is drawn only when they hold. A
    // second one replaces the first [C 0x0046F110]. Anything else is an error.
    void Requires(const std::vector<std::string_view> &words)
    {
        last_ = "requires";
        Gate gate = Gate::None;
        if (words.size() >= 2 && Same(words[0], "shadows") && Same(words[1], "enabled"))
        {
            gate = Gate::ShadowsEnabled;
        }
        else if (!words.empty() && Same(words[0], "fbeffects"))
        {
            gate = Gate::FbEffects;
        }
        if (gate == Gate::None)
        {
            failed_ = true;
            return;
        }
        SelectClass().gate = gate;
    }

    // `shadows partitiongroup <n>|blurpartition [n]|bindbackbuffer|resetinvisiblecasters` [C 0x00473730]. A
    // partition group is two operations, its start before the nodes and its stop after; it needs its n.
    void Shadows(const std::vector<std::string_view> &words)
    {
        if (words.empty())
        {
            failed_ = true;
            return;
        }
        const std::optional<int> number = words.size() > 1 ? std::optional(ToInt(words[1])) : std::nullopt;
        if (Same(words[0], "partitiongroup"))
        {
            if (!number)
            {
                failed_ = true; // [C 0x004737AF]
                return;
            }
            Add(OperationType::StartPartition, OperationWhen::Before, *number, {}, {});
            Add(OperationType::StopPartition, OperationWhen::After, *number, {}, {});
        }
        else if (Same(words[0], "blurpartition"))
        {
            Add(OperationType::BlurPartition, OperationWhen::Before, number.value_or(0), {}, {});
        }
        else if (Same(words[0], "bindbackbuffer"))
        {
            Add(OperationType::BindBackbuffer, OperationWhen::After, 0, {}, {});
        }
        else if (Same(words[0], "resetinvisiblecasters"))
        {
            Add(OperationType::ResetInvisibleCasters, OperationWhen::PerObject, 0, {}, {});
        }
        else
        {
            failed_ = true; // [C 0x00473899]
        }
    }

    // `sort by material` and `sort by distance nearest|farthest` [C 0x00473420].
    void Sort(const std::vector<std::string_view> &words)
    {
        if (words.size() < 2 || !Same(words[0], "by"))
        {
            return;
        }
        if (Same(words[1], "material"))
        {
            Add(OperationType::MaterialSort, OperationWhen::Before, 0, {}, {});
        }
        else if (Same(words[1], "distance") && words.size() > 2)
        {
            Add(Same(words[2], "farthest") ? OperationType::DistanceSortFarthest : OperationType::DistanceSortNearest,
                OperationWhen::Before, 0, {}, {});
        }
    }

    // `keep copy|original` [C 0x00472870].
    void Keep(const std::vector<std::string_view> &words)
    {
        if (words.empty())
        {
            return;
        }
        Class &target = SelectClass();
        if (Same(words[0], "copy"))
        {
            target.keepCopy = true;
        }
        else if (Same(words[0], "original"))
        {
            target.keepCopy = false;
        }
    }

    // `render [with] disabled|batched|multipass|pass <n>` [C 0x004735C0, mode by 0x004725F0].
    void Render(const std::vector<std::string_view> &words)
    {
        std::size_t at = 0;
        if (at < words.size() && Same(words[at], "with"))
        {
            ++at;
        }
        if (at >= words.size())
        {
            failed_ = true;
            return;
        }
        Class &target = SelectClass();
        const std::string_view mode = words[at++];
        if (Same(mode, "disabled"))
        {
            target.mode = RenderMode::Disabled;
        }
        else if (Same(mode, "batched"))
        {
            target.mode = RenderMode::Batched;
        }
        else if (Same(mode, "multipass"))
        {
            target.mode = RenderMode::Multipass;
        }
        else if (Same(mode, "pass"))
        {
            target.mode = RenderMode::Pass;
            if (at >= words.size())
            {
                failed_ = true;
                return;
            }
            target.pass = ToInt(words[at]);
        }
        else
        {
            target.mode = RenderMode::Plain;
        }
    }

    Program program_;
    bool open_ = false;              // a group is open
    std::optional<std::size_t> cls_; // the class being filled, in the open group
    std::string last_;               // the word a piece with no keyword goes on with [C builder+0x24]
    bool failed_ = false;
};

bool IsGroup(std::string_view element)
{
    return Same(element, "SubmissionGroup") || Same(element, "SubmissionClass");
}

// Where the comment, processing instruction or declaration at the start of rest ends, npos with no closer.
std::size_t PastMarkup(std::string_view rest)
{
    const auto past = [rest](std::string_view ending, std::size_t from) {
        const std::size_t end = rest.find(ending, from);
        return end == std::string_view::npos ? end : end + ending.size();
    };
    if (rest.starts_with("<!--"))
    {
        return past("-->", 4);
    }
    return rest.starts_with("<?") ? past("?>", 2) : past(">", 2);
}

// An end tag, without its `/`: false if it closes no open element.
bool GiveEndTag(std::string_view tag, std::vector<std::string> &open, Builder &builder)
{
    const std::string_view element = xml::ElementOf(tag);
    if (open.empty() || open.back() != element)
    {
        return false;
    }
    if (IsGroup(element))
    {
        builder.CloseGroup();
    }
    open.pop_back();
    return true;
}

// A start tag: false for a second root, no name, or what the builder refuses.
bool GiveStartTag(std::string_view tag, std::vector<std::string> &open, bool &root, Builder &builder)
{
    const bool closes = tag.ends_with('/');
    const std::string_view element = xml::ElementOf(tag);
    if (element.empty() || (open.empty() && root))
    {
        return false;
    }
    const bool directlyInside = open.size() == 1 && Same(open[0], "SubmissionInstruction");
    root = true;
    if (Same(element, "SubmissionInstruction"))
    {
        builder.OpenInstruction(tag);
    }
    else if (IsGroup(element))
    {
        builder.OpenGroup(tag, directlyInside);
    }
    if (builder.Failed())
    {
        return false;
    }
    if (!closes)
    {
        open.emplace_back(element);
    }
    else if (IsGroup(element))
    {
        builder.CloseGroup();
    }
    return true;
}

// The elements of the text, in order: calls the builder for each, true if the tags are well formed.
bool Walk(std::string_view text, Builder &builder)
{
    std::vector<std::string> open;
    bool root = false;
    std::size_t at = 0;
    while (true)
    {
        const std::size_t lt = text.find('<', at);
        builder.Text(text.substr(at, lt == std::string_view::npos ? std::string_view::npos : lt - at));
        if (lt == std::string_view::npos)
        {
            break;
        }
        const std::string_view rest = text.substr(lt);
        if (rest.starts_with("<?") || rest.starts_with("<!"))
        {
            const std::size_t past = PastMarkup(rest);
            if (past == std::string_view::npos)
            {
                return false;
            }
            at = lt + past;
            continue;
        }
        const std::size_t gt = rest.find('>');
        if (gt == std::string_view::npos)
        {
            return false;
        }
        std::string_view tag = rest.substr(1, gt - 1);
        at = lt + gt + 1;
        while (!tag.empty() && xml::Space(tag.back()))
        {
            tag.remove_suffix(1);
        }
        const bool taken =
            tag.starts_with('/') ? GiveEndTag(tag.substr(1), open, builder) : GiveStartTag(tag, open, root, builder);
        if (!taken)
        {
            return false;
        }
    }
    return root && open.empty() && !builder.Failed();
}

} // namespace

const std::array<std::string_view, kCategoryCount> &CategoryNames()
{
    return kCategories;
}

std::optional<std::uint64_t> CategoryMask(std::string_view name)
{
    for (std::size_t i = 0; i < kCategories.size(); ++i)
    {
        if (Same(name, kCategories[i]))
        {
            return std::uint64_t{1} << i;
        }
    }
    return std::nullopt;
}

std::optional<std::uint32_t> AttributeBit(std::string_view name)
{
    for (const AttributeName &entry : kAttributes)
    {
        if (Same(name, entry.name))
        {
            return entry.bit;
        }
    }
    return std::nullopt;
}

bool Matches(const Directive &directive, const Node &node)
{
    switch (directive.kind)
    {
    case DirectiveKind::Always:
        return true; // [C 0x00722AE0]
    case DirectiveKind::Class:
        return node.hasMaterial && (node.categories & directive.categories) != 0; // [C 0x0046E290]
    case DirectiveKind::Attribute:
        return (node.attributes & directive.bits) != 0; // [C 0x0046E9F0]
    case DirectiveKind::MaterialName:
        return node.hasMaterial && Same(node.material, directive.name); // [C 0x0046E320]
    case DirectiveKind::SemanticName:
        return node.hasMaterial && Same(node.semantic, directive.name); // [C 0x0046E350]
    case DirectiveKind::IncludesSemantic:
        return node.hasMaterial &&
               std::ranges::any_of(node.semantics, [&](const std::string &s) { return Same(s, directive.name); });
    case DirectiveKind::NoShadowCast:
        return !node.castsShadow; // [C 0x0046E650]
    case DirectiveKind::ShadowCaster:
        return (directive.bits & node.partitions) != 0; // [C 0x0046E6F0]
    case DirectiveKind::InvisibleShadowCaster:
        return (directive.bits & node.partitions) != 0 &&
               (node.attributes & attribute::kInvisibleCaster) != 0; // [C 0x0046E750]
    case DirectiveKind::MultiPass:
        return node.supportsMultipass == directive.flag; // [C 0x0046E670]
    }
    return false;
}

int TriggerEvent(std::string_view name)
{
    struct Event
    {
        std::string_view name;
        int number;
    };
    static constexpr Event kEvents[] = {
        {"start", 0},
        {"stop", 1},
        {"beginframe", 2},
        {"endframe", 3},
        {"staticgeomstart", 5},
        {"staticgeom_start", 5},
        {"staticgeomend", 6},
        {"staticgeom_end", 6},
        {"motiongeometrystart", 7},
        {"motiongeometryend", 8},
        {"distortionmaskend", 9},
        {"userdefined", 10},
    };
    for (const Event &event : kEvents)
    {
        if (Same(name, event.name))
        {
            return event.number;
        }
    }
    return -1;
}

std::vector<const Operation *> Class::OperationsAt(OperationWhen when) const
{
    std::vector<const Operation *> out;
    for (const Operation &operation : operations)
    {
        if (operation.when == when)
        {
            out.push_back(&operation);
        }
    }
    return out;
}

bool Class::Accepts(const Node &node) const
{
    return node.hasMaterial && std::ranges::any_of(includes, [&](const Directive &d) { return Matches(d, node); });
}

bool Group::HasIncludes() const
{
    return std::ranges::any_of(classes, [](const Class &c) { return c.HasIncludes(); });
}

bool Group::Classify(const Node &node, std::size_t groupIndex, std::vector<Placement> &into) const
{
    if (std::ranges::any_of(exclusions, [&](const Directive &d) { return Matches(d, node); }))
    {
        return false;
    }
    for (std::size_t i = 0; i < classes.size(); ++i)
    {
        if (classes[i].Accepts(node))
        {
            into.push_back({.group = groupIndex, .cls = i});
            if (!classes[i].keepCopy)
            {
                return true;
            }
        }
    }
    return false;
}

std::vector<Placement> Program::Classify(const Node &node) const
{
    std::vector<Placement> placed;
    for (std::size_t i = 0; i < groups.size(); ++i)
    {
        if (groups[i].Classify(node, i, placed))
        {
            break;
        }
    }
    return placed;
}

std::optional<Program> Read(std::string_view text)
{
    Builder builder;
    if (!Walk(text, builder))
    {
        return std::nullopt;
    }
    return builder.Take();
}

Program Fallback()
{
    Class everything;
    everything.includes.push_back(Directive{});
    Group group;
    group.classes.push_back(std::move(everything));
    Program program;
    program.groups.push_back(std::move(group));
    return program;
}

Loaded ReadOrFallback(std::optional<std::string_view> text)
{
    if (text)
    {
        if (auto program = Read(*text))
        {
            return {.program = std::move(*program), .fellBack = false};
        }
    }
    return {.program = Fallback(), .fellBack = true};
}

} // namespace dao::core::sub
