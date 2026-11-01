// dao/core/FrameEffect.h: the XML scanned into start, end and text events (attributes in order,
// which the engine's builder depends on), a state machine over them as FBEffectBuilderXML2 is, and
// the filter lines parsed as FBParseFilterLine is. Addresses are GOG's, read in the exe 2026-11-01
// (DAOParagonEngine #351's reading, render-04 and render-07, with 0x004610A0 and 0x0045E080 read here).
#include "dao/core/FrameEffect.h"

#include "dao/core/XmlTags.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdint>
#include <iterator>
#include <utility>

namespace dao::core::ffx
{

namespace
{

// The engine compares names with __wcsicmp (0x008E687B), which folds A-Z. Attribute names are
// compared the same way [I: the compare at each attribute was not looked at one by one].
bool Same(std::string_view a, std::string_view b)
{
    return std::ranges::equal(a, b, [](char x, char y) {
        return std::tolower(static_cast<unsigned char>(x)) == std::tolower(static_cast<unsigned char>(y));
    });
}

// _wtoi: leading blanks, a sign, digits; anything else is 0.
int Atoi(std::string_view text)
{
    std::size_t at = 0;
    while (at < text.size() && std::isspace(static_cast<unsigned char>(text[at])))
    {
        ++at;
    }
    bool negative = false;
    if (at < text.size() && (text[at] == '-' || text[at] == '+'))
    {
        negative = text[at] == '-';
        ++at;
    }
    std::uint32_t value = 0;
    for (; at < text.size() && std::isdigit(static_cast<unsigned char>(text[at])); ++at)
    {
        value = value * 10 + static_cast<std::uint32_t>(text[at] - '0');
    }
    return negative ? -static_cast<int>(value) : static_cast<int>(value);
}

bool StartsWithDigit(std::string_view text)
{
    return !text.empty() && std::isdigit(static_cast<unsigned char>(text.front()));
}

// _wtof of a text that starts with a digit.
float Number(std::string_view text)
{
    double value = 0;
    std::from_chars(text.data(), text.data() + text.size(), value);
    return static_cast<float>(value);
}

Event EventNamed(std::string_view name)
{
    // The names of 0x00455980, without case. There is no 4.
    static constexpr std::pair<std::string_view, Event> kNames[] = {{"start", Event::Start},
                                                                    {"stop", Event::Stop},
                                                                    {"beginframe", Event::BeginFrame},
                                                                    {"endframe", Event::EndFrame},
                                                                    {"staticgeomstart", Event::StaticGeomStart},
                                                                    {"staticgeom_start", Event::StaticGeomStart},
                                                                    {"staticgeomend", Event::StaticGeomEnd},
                                                                    {"staticgeom_end", Event::StaticGeomEnd},
                                                                    {"motiongeometrystart", Event::MotionGeometryStart},
                                                                    {"motiongeometryend", Event::MotionGeometryEnd},
                                                                    {"distortionmaskend", Event::DistortionMaskEnd},
                                                                    {"userdefined", Event::UserDefined}};
    for (const auto &[text, event] : kNames)
    {
        if (Same(name, text))
        {
            return event;
        }
    }
    return Event::Other;
}

bool IsTarget(const Resource *resource)
{
    return resource != nullptr && resource->kind == ResourceKind::RenderTarget;
}

bool IsSurface(const Resource *resource)
{
    return resource != nullptr &&
           (resource->kind == ResourceKind::RenderTarget || resource->kind == ResourceKind::BackBuffer);
}

// wcstok(line, " \t") and wcstok(NULL, " \t"): the engine's line parsers share its state.
class Tokens
{
  public:
    explicit Tokens(std::string_view line) : rest_(line)
    {
    }
    std::optional<std::string_view> Next()
    {
        const auto start = rest_.find_first_not_of(" \t");
        if (start == std::string_view::npos)
        {
            rest_ = {};
            return std::nullopt;
        }
        rest_.remove_prefix(start);
        const auto end = rest_.find_first_of(" \t");
        const std::string_view token = rest_.substr(0, end);
        rest_.remove_prefix(end == std::string_view::npos ? rest_.size() : end);
        return token;
    }

  private:
    std::string_view rest_;
};

// The line parsers over the effect so far and the filter being filled.
class LineParser
{
  public:
    LineParser(const Effect &effect, Filter &filter) : effect_(effect), filter_(filter)
    {
    }

    // FBParseFilterLine, 0x00461570: the first word picks the parser, any other is an assignment.
    // @return false for a line the engine refuses
    bool Parse(std::string_view line)
    {
        Tokens tokens(line);
        const auto first = tokens.Next();
        if (!first)
        {
            return false;
        }
        if (Same(*first, "copy"))
        {
            return ParseCopy(tokens);
        }
        if (Same(*first, "bind"))
        {
            return ParseBind(tokens);
        }
        if (Same(*first, "run"))
        {
            return ParseRun(tokens);
        }
        if (Same(*first, "dump"))
        {
            return ParseDump(tokens);
        }
        return ParseAssignment(*first, tokens);
    }

  private:
    // FBParseAssignment, 0x00460590. Every "=" token is skipped, so "a = b", "a b" and "a = = b" agree.
    bool ParseAssignment(std::string_view lhs, Tokens &tokens)
    {
        std::optional<std::string_view> rhs;
        do
        {
            rhs = tokens.Next();
        } while (rhs && *rhs == "=");
        if (!rhs)
        {
            return false; // the engine reads a null here
        }
        if (effect_.ResourceNamed(lhs) != nullptr)
        {
            // Only a number sets it; anything else is ignored, and the line is accepted (0x00460653).
            if (StartsWithDigit(*rhs))
            {
                filter_.instructions.push_back(SetResource{std::string(lhs), Number(*rhs)});
            }
            return true;
        }
        if (const Resource *source = effect_.ResourceNamed(*rhs))
        {
            if (IsSurface(source))
            {
                filter_.instructions.push_back(TextureFromResource{std::string(lhs), source->id});
            }
            else
            {
                filter_.instructions.push_back(ParameterFromResource{std::string(lhs), source->id, source->kind});
            }
            return true;
        }
        // A number must start with a digit: "-3.5", ".5" and "+1" are not numbers (0x004607ED).
        if (StartsWithDigit(*rhs))
        {
            filter_.instructions.push_back(ParameterValue{std::string(lhs), Number(*rhs)});
            return true;
        }
        // name.width or name.height of a target; a word with no "." fails at its lookup (0x0046085D).
        const auto dot = rhs->find('.');
        if (dot == std::string_view::npos)
        {
            return false;
        }
        const Resource *target = effect_.ResourceNamed(rhs->substr(0, dot));
        const std::string_view member = rhs->substr(dot + 1);
        if (!IsSurface(target) || !(Same(member, "width") || Same(member, "height")))
        {
            return false;
        }
        filter_.instructions.push_back(ParameterSize{std::string(lhs), target->id, Same(member, "height")});
        return true;
    }

    // FBParseCopy, 0x00460A40: copy Source [to] Destination [filter point|linear].
    bool ParseCopy(Tokens &tokens)
    {
        const auto source = tokens.Next(); // the engine crashes on a missing one
        auto destination = tokens.Next();
        if (destination && Same(*destination, "to"))
        {
            destination = tokens.Next();
        }
        if (!source || !destination)
        {
            return false;
        }
        int mode = 5;
        for (auto word = tokens.Next(); word && Same(*word, "filter"); word = tokens.Next())
        {
            const auto kind = tokens.Next();
            if (!kind)
            {
                break;
            }
            mode = Same(*kind, "point") ? 6 : Same(*kind, "linear") ? 7 : 5;
        }
        const Resource *to = effect_.ResourceNamed(*destination);
        const bool fromBackBuffer = Same(*source, "backbuffer");
        const bool allowed = fromBackBuffer ? IsSurface(to) : IsTarget(effect_.ResourceNamed(*source)) && IsTarget(to);
        if (!allowed)
        {
            return false;
        }
        filter_.instructions.push_back(
            Copy{fromBackBuffer ? "backbuffer" : std::string(*source), to->id, mode, fromBackBuffer});
        return true;
    }

    // FBParseBind, 0x00460CE0: a state machine over the words (the engine's table is 0x0046107C).
    // `clear ...` is not a word of it, and `viewport` counts only after `at N` or a back buffer
    // resource: after the subject alone it waits for a second one, so the mode stays original.
    // The words after the mode are not read.
    bool ParseBind(Tokens &tokens)
    {
        Bind bind;
        bool subject = false;
        int state = 0;
        for (auto word = tokens.Next(); word && state != 8; word = tokens.Next())
        {
            switch (state)
            {
            case 0:
                if (const Resource *resource = effect_.ResourceNamed(*word))
                {
                    if (!IsSurface(resource))
                    {
                        return false;
                    }
                    bind.resource = resource->id;
                    subject = true;
                    state = 1;
                    if (!IsTarget(resource))
                    {
                        bind.index = resource->index;
                        state = 3;
                    }
                }
                else if (Same(*word, "at"))
                {
                    state = 2;
                }
                else if (Same(*word, "viewport"))
                {
                    state = 6;
                }
                else if (Same(*word, "backbuffer") || Same(*word, "null"))
                {
                    bind.subject = Same(*word, "null") ? Bind::Subject::Unbind : Bind::Subject::BackBuffer;
                    subject = true;
                }
                break;
            case 1:
                state = Same(*word, "at") ? 2 : Same(*word, "viewport") ? 6 : state;
                break;
            case 2:
                bind.index = Atoi(*word);
                state = 3;
                break;
            case 3:
            case 5:
            case 6:
                state = Same(*word, "viewport") ? 7 : state;
                break;
            default: // 7: the mode
                bind.viewport = Same(*word, "recalculate") ? ViewportMode::Recalculate
                                : Same(*word, "stored")    ? ViewportMode::Stored
                                                           : ViewportMode::Original;
                state = 8;
                break;
            }
        }
        if (!subject)
        {
            return false; // [I: the engine's results are given only for the four subjects]
        }
        if (bind.subject == Bind::Subject::Unbind)
        {
            bind.viewport = ViewportMode::Original; // FBIBindTarget(null, index, 0)
        }
        filter_.instructions.push_back(std::move(bind));
        return true;
    }

    // 0x004610A0: run [preferred | semantic Name]. The semantic is looked up in the filter's
    // material (0x0045D230), so a filter with no material refuses the line. Whether the material
    // has the semantic is for the game's materials; it is not checked here.
    bool ParseRun(Tokens &tokens)
    {
        Run run;
        if (const auto word = tokens.Next(); word && !Same(*word, "preferred"))
        {
            const auto name = Same(*word, "semantic") ? tokens.Next() : std::nullopt;
            if (!name)
            {
                return false;
            }
            run.semantic = *name;
        }
        if (filter_.material.empty())
        {
            return false;
        }
        filter_.instructions.push_back(std::move(run));
        return true;
    }

    // FBParseDump, 0x00461190: dump Target [path]. An unknown target crashes the engine.
    bool ParseDump(Tokens &tokens)
    {
        const auto target = tokens.Next();
        const Resource *resource = target ? effect_.ResourceNamed(*target) : nullptr;
        if (resource == nullptr || !IsTarget(resource))
        {
            return false;
        }
        const auto path = tokens.Next();
        filter_.instructions.push_back(Dump{resource->id, path ? std::string(*path) : resource->id + ".dds"});
        return true;
    }

    const Effect &effect_;
    Filter &filter_;
};

// The builder, FBEffectBuilderXML2 (0x004698E0): a stack of states and the objects being built.
// States: 0 the document, 1 in an Effect, 3 Resources, 4 a resource element, 5 implementation,
// 6 stage, 7 filter, 10 an error (nothing more is read), 11 a skipped cinematics effect.
class Builder
{
  public:
    explicit Builder(const Options &options) : options_(options)
    {
    }

    void Start(std::string_view name, const std::vector<Attribute> &attributes)
    {
        if (State() >= 10)
        {
            return; // 0x0046A1C6
        }
        if (Same(name, "effect"))
        {
            if (State() == 0 && !effect_)
            {
                Push(1);
                ReadEffect(attributes);
            }
            else
            {
                Fail(); // a second Effect before the first is closed, or after a skipped one (0x0046A1EF)
            }
        }
        else if (Same(name, "resources"))
        {
            Enter(State() == 1 && effect_, 3);
        }
        else if (Same(name, "implementation"))
        {
            Enter(State() == 1, 5);
        }
        else if (Same(name, "stage"))
        {
            if (Enter(State() == 5 && !stage_, 6))
            {
                ReadStage(attributes);
            }
        }
        else if (Same(name, "filter"))
        {
            if (Enter(State() == 6 && !filter_, 7))
            {
                ReadFilter(attributes);
            }
        }
        else if (State() == 3 && !resource_ && effect_)
        {
            Push(4);
            ReadResource(name, attributes);
        }
        // Any other element (header, description, author) outside Resources is ignored (0x0046A2F7).
    }

    void End(std::string_view name)
    {
        if (State() == 10)
        {
            return; // 0x00469CC7
        }
        if (Same(name, "effect"))
        {
            if (!effect_)
            {
                Fail();
                return;
            }
            out_.push_back(std::move(*effect_));
            effect_.reset();
            Pop(); // a skipped effect leaves its own state 1 on the stack, not 0 (0x00469D36)
            return;
        }
        if (State() == 11)
        {
            return; // 0x00469D41
        }
        if (Same(name, "resources") || Same(name, "implementation"))
        {
            Pop();
        }
        else if (Same(name, "stage"))
        {
            if (stage_ && effect_ && stageHasFilter_)
            {
                effect_->stages.push_back(std::move(*stage_));
                stage_.reset();
                Pop();
            }
            else
            {
                Fail(); // a stage with no filter (0x00469D88)
            }
        }
        else if (Same(name, "filter"))
        {
            if (filter_ && stage_)
            {
                stage_->filters.push_back(std::move(*filter_));
                filter_.reset();
                stageHasFilter_ = true;
                Pop();
            }
            else
            {
                Fail();
            }
        }
        else if (State() == 4)
        {
            Pop();
            if (resource_)
            {
                // A duplicate id is dropped (FrameBufferEffect::AddResource, 0x00460280).
                if (effect_->ResourceNamed(resource_->id) == nullptr)
                {
                    effect_->resources.push_back(std::move(*resource_));
                }
                resource_.reset();
            }
            // A requires has no resource and ends quietly (0x00469E16).
        }
    }

    // One chunk of character data, which expat gives a line at a time (OnCharacterData, 0x00469BA0).
    void Text(std::string_view chunk)
    {
        if (State() != 7)
        {
            return;
        }
        // Leading tab, LF and space are skipped; the rest is cut at 255 characters, then at '#'.
        const auto start = chunk.find_first_not_of(" \t\n");
        if (start == std::string_view::npos)
        {
            return;
        }
        std::string_view line = chunk.substr(start);
        line = line.substr(0, Characters(line, 255));
        line = line.substr(0, line.find('#'));
        if (line.empty())
        {
            return; // 0x00472580 refuses it, and the line is passed over
        }
        if (!LineParser(*effect_, *filter_).Parse(line))
        {
            Fail(); // error 5 (0x00469C53)
        }
    }

    std::vector<Effect> Done()
    {
        return std::move(out_);
    }

  private:
    int State() const
    {
        return states_.back();
    }
    void Push(int state)
    {
        states_.push_back(state);
    }
    void Pop()
    {
        if (states_.size() > 1)
        {
            states_.pop_back();
        }
    }
    void Fail()
    {
        Push(10);
    }
    // Pushes a state where the element is in place, else fails (a known element out of place, 0x0046A324).
    bool Enter(bool inPlace, int state)
    {
        inPlace ? Push(state) : Fail();
        return inPlace;
    }

    // The byte length of the first `most` characters of UTF-8 text.
    static std::size_t Characters(std::string_view text, std::size_t most)
    {
        std::size_t count = 0;
        for (std::size_t at = 0; at < text.size(); ++at)
        {
            if ((static_cast<unsigned char>(text[at]) & 0xC0) != 0x80 && count++ == most)
            {
                return at;
            }
        }
        return text.size();
    }

    // ReadElementAttributes, state 1 (0x0046A05F..0x0046A186): `id` must be the first attribute; then
    // layer, domain, minlod and availability from the second on. The attributes after domain are
    // still read.
    void ReadEffect(const std::vector<Attribute> &attributes)
    {
        if (attributes.empty() || !Same(attributes[0].name, "id"))
        {
            Fail(); // error 4
            return;
        }
        effect_ = Effect{};
        effect_->id = attributes[0].value;
        for (std::size_t i = 1; i < attributes.size(); ++i)
        {
            const Attribute &attribute = attributes[i];
            if (Same(attribute.name, "layer"))
            {
                effect_->layer = Atoi(attribute.value);
            }
            else if (Same(attribute.name, "domain"))
            {
                if (Same(attribute.value, "cinematics") && !options_.cinematicsAllowed)
                {
                    effect_->cinematics = true;
                    Push(11); // 0x0046A0E9..0x0046A11E
                }
            }
            else if (Same(attribute.name, "minlod"))
            {
                effect_->minLod = Atoi(attribute.value);
            }
            else if (Same(attribute.name, "availability"))
            {
                effect_->available = Atoi(attribute.value) > 0; // setg, 0x0046A16F
            }
        }
    }

    // State 6: `id` first and `event` second, else error 4 (0x00469FAB..0x0046A027).
    void ReadStage(const std::vector<Attribute> &attributes)
    {
        if (attributes.size() < 2 || !Same(attributes[0].name, "id") || !Same(attributes[1].name, "event"))
        {
            Fail();
            return;
        }
        stage_ = Stage{.id = attributes[0].value, .event = EventNamed(attributes[1].value), .filters = {}};
        stageHasFilter_ = false;
    }

    // State 7: `id` first, `material` as the second or third attribute only (the engine looks at
    // entries 2 and 4 of its list, 0x00469F18..0x00469F3C).
    void ReadFilter(const std::vector<Attribute> &attributes)
    {
        if (attributes.empty() || !Same(attributes[0].name, "id"))
        {
            Fail();
            return;
        }
        filter_ = Filter{.id = attributes[0].value, .material = {}, .instructions = {}};
        for (std::size_t i = 1; i < std::min<std::size_t>(attributes.size(), 3); ++i)
        {
            if (Same(attributes[i].name, "material"))
            {
                filter_->material = attributes[i].value;
                break;
            }
        }
    }

    // StartResourceElement (0x00469980): `id` first, else error 3; the element picks the kind; a
    // `requires` needs `effectid` as its second attribute; any other element is error 2. Whether a
    // resource is valid (the engine's byte at +0x28, which ends in error 6) is not checked [I].
    void ReadResource(std::string_view name, const std::vector<Attribute> &attributes)
    {
        if (attributes.empty() || !Same(attributes[0].name, "id"))
        {
            Fail();
            return;
        }
        if (Same(name, "requires"))
        {
            if (attributes.size() < 2 || !Same(attributes[1].name, "effectid"))
            {
                Fail();
                return;
            }
            effect_->requirements.push_back({.id = attributes[0].value, .effectId = attributes[1].value});
            return;
        }
        static constexpr std::pair<std::string_view, ResourceKind> kKinds[] = {{"target", ResourceKind::RenderTarget},
                                                                               {"backbuffer", ResourceKind::BackBuffer},
                                                                               {"float", ResourceKind::Float},
                                                                               {"vector4f", ResourceKind::Vector4f},
                                                                               {"runtime", ResourceKind::Runtime}};
        const auto kind = std::ranges::find_if(kKinds, [&](const auto &entry) { return Same(name, entry.first); });
        if (kind == std::end(kKinds))
        {
            Fail();
            return;
        }
        Resource resource{.kind = kind->second, .id = attributes[0].value, .attributes = {}, .index = 1};
        for (std::size_t i = 1; i < attributes.size(); ++i)
        {
            resource.attributes.push_back(attributes[i]);
            if (resource.kind == ResourceKind::BackBuffer && Same(attributes[i].name, "index"))
            {
                resource.index = Atoi(attributes[i].value); // FBBackbufferResource slot 0, 0x0045E080
            }
        }
        resource_ = std::move(resource);
    }

    Options options_;
    std::vector<int> states_{0};
    std::vector<Effect> out_;
    std::optional<Effect> effect_;
    std::optional<Stage> stage_;
    std::optional<Filter> filter_;
    std::optional<Resource> resource_;
    bool stageHasFilter_ = false;
};

std::string_view Trimmed(std::string_view text)
{
    while (!text.empty() && xml::Space(text.back()))
    {
        text.remove_suffix(1);
    }
    return text;
}

// A start tag's text after its '<': the name, then name="value" pairs in order.
// @return false for a tag expat would refuse
bool ParseStart(std::string_view tag, std::string_view &name, std::vector<Attribute> &attributes)
{
    std::size_t at = 0;
    while (at < tag.size() && !xml::Space(tag[at]))
    {
        ++at;
    }
    name = tag.substr(0, at);
    if (name.empty())
    {
        return false;
    }
    while (true)
    {
        while (at < tag.size() && xml::Space(tag[at]))
        {
            ++at;
        }
        if (at >= tag.size())
        {
            return true;
        }
        const std::size_t equals = tag.find('=', at);
        if (equals == std::string_view::npos)
        {
            return false;
        }
        const std::string_view key = Trimmed(tag.substr(at, equals - at));
        at = equals + 1;
        while (at < tag.size() && xml::Space(tag[at]))
        {
            ++at;
        }
        if (key.empty() || at >= tag.size() || (tag[at] != '"' && tag[at] != '\''))
        {
            return false;
        }
        const std::size_t end = tag.find(tag[at], at + 1);
        if (end == std::string_view::npos)
        {
            return false;
        }
        attributes.push_back({std::string(key), xml::Unescape(tag.substr(at + 1, end - at - 1))});
        at = end + 1;
    }
}

// Expat reports line ends as LF whatever the file's.
std::string Normalized(std::string_view text)
{
    const std::string raw = xml::Unescape(text);
    std::string lines;
    for (std::size_t i = 0; i < raw.size(); ++i)
    {
        if (raw[i] == '\r')
        {
            lines += '\n';
            if (i + 1 < raw.size() && raw[i + 1] == '\n')
            {
                ++i;
            }
        }
        else
        {
            lines += raw[i];
        }
    }
    return lines;
}

// Gives the builder a run of text a line at a time, as expat does.
void GiveText(Builder &builder, std::string_view text)
{
    const std::string lines = Normalized(text);
    for (std::size_t at = 0; at < lines.size();)
    {
        const std::size_t end = std::min(lines.find('\n', at), lines.size());
        builder.Text(std::string_view(lines).substr(at, end - at));
        at = end + 1;
    }
}

} // namespace

const Resource *Effect::ResourceNamed(std::string_view wanted) const
{
    const auto found = std::ranges::find(resources, wanted, &Resource::id);
    return found != resources.end() ? &*found : nullptr;
}

std::vector<Effect> Read(std::string_view text, const Options &options)
{
    Builder builder(options);
    std::vector<std::string> open; // element names, which expat requires to match exactly
    std::size_t at = 0;
    while (at < text.size())
    {
        const std::size_t lt = text.find('<', at);
        GiveText(builder, text.substr(at, lt == std::string_view::npos ? std::string_view::npos : lt - at));
        if (lt == std::string_view::npos)
        {
            break;
        }
        const std::string_view rest = text.substr(lt);
        // Each skipped kind ends at its closer; one with none ends the file.
        const auto skip = [&](std::string_view opener, std::string_view closer) {
            const std::size_t end = rest.find(closer, opener.size());
            return end == std::string_view::npos ? text.size() : lt + end + closer.size();
        };
        if (rest.starts_with("<!--"))
        {
            at = skip("<!--", "-->");
        }
        else if (rest.starts_with("<?"))
        {
            at = skip("<?", "?>");
        }
        else if (rest.starts_with("<![CDATA["))
        {
            const std::size_t end = rest.find("]]>");
            if (end == std::string_view::npos)
            {
                break;
            }
            GiveText(builder, rest.substr(9, end - 9));
            at = lt + end + 3;
        }
        else if (rest.starts_with("<!"))
        {
            at = skip("<!", ">");
        }
        else
        {
            const std::size_t close = rest.find('>');
            if (close == std::string_view::npos)
            {
                break;
            }
            std::string_view tag = rest.substr(1, close - 1);
            at = lt + close + 1;
            if (tag.starts_with('/'))
            {
                const std::string_view name = Trimmed(tag.substr(1));
                if (open.empty() || open.back() != name)
                {
                    break; // not well formed: expat stops here
                }
                open.pop_back();
                builder.End(name);
                continue;
            }
            const bool selfClosing = tag.ends_with('/');
            if (selfClosing)
            {
                tag.remove_suffix(1);
            }
            std::string_view name;
            std::vector<Attribute> attributes;
            if (!ParseStart(tag, name, attributes))
            {
                break;
            }
            builder.Start(name, attributes);
            if (selfClosing)
            {
                builder.End(name);
            }
            else
            {
                open.emplace_back(name);
            }
        }
    }
    return builder.Done();
}

} // namespace dao::core::ffx
