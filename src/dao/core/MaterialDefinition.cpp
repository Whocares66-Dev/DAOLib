// dao/core/MaterialDefinition.h: a .mat.xml's elements read one tag at a time (dao/core/XmlTags.h),
// each put where the elements still open say it is, as the engine's builder (MaterialBuilderXML2,
// 0x0050D090 for an element's start, 0x0050C7E0 for its end) puts it. dev/MATERIALS.md has the reading.
#include "dao/core/MaterialDefinition.h"

#include "dao/core/XmlTags.h"

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <deque>
#include <system_error>
#include <utility>

namespace dao::core::mat
{

namespace
{

using xml::ForEachAttribute;

bool EqualsIgnoringCase(std::string_view left, std::string_view right)
{
    const auto lower = [](char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c; };
    return std::ranges::equal(left, right, {}, lower, lower);
}

// The value of an attribute, its name matched ignoring case (every temp of the builder does, 0x0050D8D3),
// the last of them where it is written twice, as each pair is set in turn.
std::optional<std::string> Attr(std::string_view tag, std::string_view key)
{
    std::optional<std::string> found;
    ForEachAttribute(tag, [&](std::string_view name, const std::string &value) {
        if (EqualsIgnoringCase(name, key))
        {
            found = value;
        }
    });
    return found;
}

std::string Named(std::string_view tag, std::string_view key)
{
    return Attr(tag, key).value_or("");
}

// A name given as `key` or as `ID`, whichever is written last (the setters of 0x0050C500).
std::string Identified(std::string_view tag, std::string_view key)
{
    std::string found;
    ForEachAttribute(tag, [&](std::string_view name, const std::string &value) {
        if (EqualsIgnoringCase(name, key) || EqualsIgnoringCase(name, "ID"))
        {
            found = value;
        }
    });
    return found;
}

bool IsSpace(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r';
}

// Reads a number from `at` as a wide stream's `>> float` does, white space skipped first, and moves `at`
// past it; false (and `at` left) where no number starts there.
bool ReadFloat(std::string_view text, std::size_t &at, float &out)
{
    std::size_t p = at;
    while (p < text.size() && IsSpace(text[p]))
    {
        ++p;
    }
    bool negative = false;
    if (p < text.size() && (text[p] == '+' || text[p] == '-'))
    {
        negative = text[p] == '-';
        ++p;
    }
    if (p >= text.size() || (text[p] != '.' && (text[p] < '0' || text[p] > '9')))
    {
        return false;
    }
    float value = 0;
    const auto done = std::from_chars(text.data() + p, text.data() + text.size(), value);
    if (done.ec != std::errc())
    {
        return false;
    }
    out = negative ? -value : value;
    at = static_cast<std::size_t>(done.ptr - text.data());
    return true;
}

// The leading number of a string, 0 where it has none (the script's string to float, 0x004C09D0).
float LeadingFloat(std::string_view text)
{
    std::size_t at = 0;
    float value = 0;
    return ReadFloat(text, at, value) ? value : 0.0F;
}

// The leading whole number of a string as wcstol reads it, 0 where it has none.
long LeadingInteger(std::string_view text)
{
    std::size_t p = 0;
    while (p < text.size() && IsSpace(text[p]))
    {
        ++p;
    }
    bool negative = false;
    if (p < text.size() && (text[p] == '+' || text[p] == '-'))
    {
        negative = text[p] == '-';
        ++p;
    }
    long value = 0;
    std::from_chars(text.data() + p, text.data() + text.size(), value);
    return negative ? -value : value;
}

constexpr Vector4 kZero = {0, 0, 0, 0};

// The engine's names for what a parameter's Type makes, matched ignoring case (CreateParam 0x0050B360).
bool InternalTypeKnown(std::string_view type)
{
    constexpr std::string_view kTypes[] = {"Texture", "Matrix4x4f", "InternalDataReference", "Float",
                                           "Color4f", "Vector4f",   "Vector4fArray"};
    return std::ranges::any_of(kTypes, [type](std::string_view known) { return EqualsIgnoringCase(type, known); });
}

Parameter ParameterOf(std::string_view tag, bool internal)
{
    Parameter parameter{.internal = internal,
                        .fromSet = false,
                        .type = Named(tag, "Type"),
                        .name = Identified(tag, "Name"),
                        .sampler = Named(tag, "SamplerName"),
                        .defaultValue = Attr(tag, "Default"),
                        .usage = Named(tag, "Usage"),
                        .stage = Named(tag, "Stage"),
                        .size = Named(tag, "Size")};
    if (const auto min = Attr(tag, "Min"))
    {
        parameter.min = *min;
    }
    if (const auto max = Attr(tag, "Max"))
    {
        parameter.max = *max;
    }
    return parameter;
}

// One LogicTest's word, attribute by attribute in the order written, as ShaderModel (0x005095A0) and Detail
// (0x0050ADA0) leave it: shader model 3 sets 1; 2 sets 2 and clears any detail already there; a detail sets
// 4, 8 or 0x10, and does nothing once the word has 2.
Test TestOf(std::string_view tag)
{
    Test test;
    ForEachAttribute(tag, [&](std::string_view name, const std::string &value) {
        if (EqualsIgnoringCase(name, "ShaderModel"))
        {
            test.shaderModel = static_cast<int>(LeadingInteger(value));
            if (test.shaderModel == 3)
            {
                test.flags |= 1;
            }
            else if (test.shaderModel == 2)
            {
                test.flags |= 2;
                test.flags &= ~0x1CU;
            }
        }
        else if (EqualsIgnoringCase(name, "Detail"))
        {
            test.detail = value;
            if ((test.flags & 2) == 0)
            {
                if (EqualsIgnoringCase(value, "HIGH"))
                {
                    test.flags |= 4;
                }
                else if (EqualsIgnoringCase(value, "MEDIUM"))
                {
                    test.flags |= 8;
                }
                else if (EqualsIgnoringCase(value, "LOW"))
                {
                    test.flags |= 0x10;
                }
            }
        }
    });
    return test;
}

// The kind of an element once its parent is known: Unknown for one the builder makes nothing of.
enum class Element
{
    Material,
    Semantic,
    Logic,
    LogicTest,
    Technique,
    Pass,
    Parameter,
    InternalParameter,
    EffectParameter,
    RequiredStream,
    Stage,
    ParameterSet,
    ParameterSetRef,
    Unknown,
};

Element Classify(std::string_view name)
{
    struct Entry
    {
        std::string_view name;
        Element element;
    };
    constexpr Entry kNames[] = {
        {"Material", Element::Material},
        {"Semantic", Element::Semantic},
        {"Logic", Element::Logic},
        {"LogicTest", Element::LogicTest},
        {"Technique", Element::Technique},
        {"Pass", Element::Pass},
        {"Parameter", Element::Parameter},
        {"ArtParameter", Element::Parameter},
        {"InternalParameter", Element::InternalParameter},
        {"EffectParameter", Element::EffectParameter},
        {"RequiredStreamData", Element::RequiredStream},
        {"Stage", Element::Stage},
        {"ParameterSet", Element::ParameterSet},
        {"ParameterSetRef", Element::ParameterSetRef},
    };
    for (const Entry &entry : kNames)
    {
        if (EqualsIgnoringCase(name, entry.name))
        {
            return entry.element;
        }
    }
    return Element::Unknown;
}

// Whether an element may sit under the one open, by the kind tests of StartElement (0x0050D259 to 0x0050D883).
bool Fits(Element child, Element parent)
{
    switch (child)
    {
    case Element::Semantic:
    case Element::ParameterSet:
        return parent == Element::Material;
    case Element::Logic:
        return parent == Element::Semantic || parent == Element::Logic;
    case Element::LogicTest:
    case Element::Technique:
        return parent == Element::Logic;
    case Element::Pass:
        return parent == Element::Technique;
    case Element::Parameter:
    case Element::InternalParameter:
        return parent == Element::Pass || parent == Element::ParameterSet;
    case Element::EffectParameter:
    case Element::RequiredStream:
    case Element::Stage:
    case Element::ParameterSetRef:
        return parent == Element::Pass;
    case Element::Material:
    case Element::Unknown:
        break;
    }
    return false;
}

// An element open, and what it is filling: only the one its kind says is set.
struct Frame
{
    Element kind = Element::Unknown;
    Semantic *semantic = nullptr;
    Block *block = nullptr;
    Technique *technique = nullptr;
    Pass *pass = nullptr;
    ParameterSet *set = nullptr;
};

// A parameter in the pass being read, and whether the pass's name map holds it (Pass.AddParam 0x0050BD20).
struct Held
{
    Parameter parameter;
    bool mapped = false;
};

// The elements open where a tag is met, and what is being filled.
class Reader
{
  public:
    void Take(std::string_view tag)
    {
        while (!tag.empty() && xml::Space(tag.back()))
        {
            tag.remove_suffix(1);
        }
        const bool closing = tag.starts_with('/');
        const std::string_view element = xml::ElementOf(closing ? tag.substr(1) : tag);
        const bool closes = closing || tag.ends_with('/');
        if (closing)
        {
            Close(element);
            return;
        }
        Open(element, tag);
        if (closes)
        {
            Close(element);
        }
    }

    std::optional<Definition> Done()
    {
        return seen_ ? std::optional(std::move(out_)) : std::nullopt;
    }

  private:
    void Open(std::string_view name, std::string_view tag)
    {
        if (EqualsIgnoringCase(name, "Meta"))
        {
            return; // skipped at both ends, 0x0050D0BA; its children meet whatever is open
        }
        const Element kind = Classify(name);
        if (stack_.empty())
        {
            if (kind == Element::Material)
            {
                // Only on an empty stack; a second one in a file makes a material of its own (0x0050D0FB).
                out_ = Definition{.name = Identified(tag, "Name"), .sets = {}, .semantics = {}};
                seen_ = true;
                stack_.push_back({.kind = Element::Material});
            }
            return; // anything else with nothing open is not pushed (0x0050D223)
        }
        if (kind == Element::Material)
        {
            return; // not pushed either; its end tag pops what is open (0x0050CEE2)
        }
        const Frame parent = stack_.back();
        if (!Fits(kind, parent.kind))
        {
            // A misplaced or unknown element is an inert one, and so is everything under it (0x0050D89B).
            stack_.push_back({.kind = Element::Unknown});
            return;
        }
        stack_.push_back(Make(kind, parent, tag));
    }

    Frame Make(Element kind, const Frame &parent, std::string_view tag)
    {
        Frame frame{.kind = kind};
        switch (kind)
        {
        case Element::Semantic:
            out_.semantics.push_back(
                {.name = Identified(tag, "Name"), .renderClass = Named(tag, "RenderClass"), .logic = std::nullopt});
            frame.semantic = &out_.semantics.back();
            break;
        case Element::ParameterSet:
            out_.sets.push_back({.name = Named(tag, "Name"), .parameters = {}});
            frame.set = &out_.sets.back();
            break;
        case Element::Logic:
            frame.block = NewBlock(parent);
            frame.block->note = Identified(tag, "Note");
            break;
        case Element::LogicTest:
            parent.block->tests.push_back(TestOf(tag));
            break;
        case Element::Technique:
            frame.technique = NewTechnique(*parent.block);
            frame.technique->name = Identified(tag, "Name");
            break;
        case Element::Pass:
            frame.pass = NewPass(*parent.technique, tag);
            break;
        default:
            Fill(kind, parent, tag);
            break;
        }
        return frame;
    }

    // The block a Logic is: a semantic's first is its root, a nested one is added to the block it sits in. A
    // second root is built and not kept (SetLogic 0x00509620 stores only into an empty place).
    Block *NewBlock(const Frame &parent)
    {
        if (parent.kind == Element::Logic)
        {
            return &parent.block->children.emplace_back();
        }
        if (!parent.semantic->logic)
        {
            return &parent.semantic->logic.emplace();
        }
        return &discardedBlocks_.emplace_back();
    }

    // A block's first Technique is its own; a second is built and not kept (SetTechnique 0x005095E0).
    Technique *NewTechnique(Block &block)
    {
        return block.technique ? &discardedTechniques_.emplace_back() : &block.technique.emplace();
    }

    Pass *NewPass(Technique &technique, std::string_view tag)
    {
        technique.passes.push_back({.name = Identified(tag, "Name"),
                                    .vertexShader = Named(tag, "VertexShader"),
                                    .pixelShader = Named(tag, "PixelShader"),
                                    .state = Named(tag, "State"),
                                    .parameters = {},
                                    .sets = {},
                                    .streams = {},
                                    .effects = {},
                                    .effect = Named(tag, "Effect"),
                                    .stages = {}});
        plain_.clear();
        internal_.clear();
        return &technique.passes.back();
    }

    // What a pass or a set holds besides its structure: parameters, a reference to a set, streams and stages.
    void Fill(Element kind, const Frame &parent, std::string_view tag)
    {
        switch (kind)
        {
        case Element::Parameter:
        case Element::InternalParameter: {
            Parameter parameter = ParameterOf(tag, kind == Element::InternalParameter);
            if (parent.set != nullptr)
            {
                parent.set->parameters.push_back(std::move(parameter));
            }
            else
            {
                AddParameter(std::move(parameter), false);
            }
            break;
        }
        case Element::EffectParameter:
            parent.pass->effects.push_back(Named(tag, "EffectName"));
            break;
        case Element::RequiredStream:
            parent.pass->streams.push_back(Identified(tag, "Name"));
            break;
        case Element::Stage:
            parent.pass->stages.push_back(Named(tag, "id"));
            break;
        case Element::ParameterSetRef:
            parent.pass->sets.push_back(Named(tag, "Ref"));
            AddSet(parent.pass->sets.back());
            break;
        default:
            break;
        }
    }

    // Puts a parameter in the pass as Pass.AddParam (0x0050BD20) and AddInternalParam (0x0050BE00) do, the
    // names of the plain and the internal ones kept apart. A parameter of a Type the builder makes nothing
    // of is not made, and so not added. One of the pass's own replaces a set's copy of its name, but only
    // if written after the reference: a copy is in the pass's name map and an own parameter never is.
    void AddParameter(Parameter parameter, bool fromSet)
    {
        const bool known = parameter.internal ? InternalTypeKnown(parameter.type) : parameter.KindOf() != Kind::Unknown;
        if (!known)
        {
            return;
        }
        std::vector<Held> &list = parameter.internal ? internal_ : plain_;
        const auto holder = std::ranges::find_if(
            list, [&](const Held &held) { return held.mapped && held.parameter.name == parameter.name; });
        if (fromSet)
        {
            list.push_back({.parameter = std::move(parameter), .mapped = holder == list.end()});
            return;
        }
        if (holder != list.end())
        {
            // RemoveAtSwap: the last takes the removed one's place.
            if (&*holder != &list.back())
            {
                *holder = std::move(list.back());
            }
            list.pop_back();
        }
        list.push_back({.parameter = std::move(parameter), .mapped = false});
    }

    // A ParameterSetRef: fresh copies of the first set of that name, ignoring case, internal ones first
    // (0x0050CD07). A name no set read so far has adds nothing.
    void AddSet(std::string_view name)
    {
        const auto found = std::ranges::find_if(
            out_.sets, [name](const ParameterSet &set) { return EqualsIgnoringCase(set.name, name); });
        if (found == out_.sets.end())
        {
            return;
        }
        for (const bool internal : {true, false})
        {
            for (const Parameter &parameter : found->parameters)
            {
                if (parameter.internal == internal)
                {
                    Parameter copy = parameter;
                    copy.fromSet = true;
                    AddParameter(std::move(copy), true);
                }
            }
        }
    }

    // A pass is whole at its end: with an Effect it keeps only that and its stages (BuildEffectPass 0x0050B0C0
    // reads none of the rest), otherwise its parameters are what the references left.
    void FinishPass(Pass &pass)
    {
        if (!pass.effect.empty())
        {
            pass.vertexShader.clear();
            pass.pixelShader.clear();
            pass.state.clear();
            pass.sets.clear();
            pass.streams.clear();
            pass.effects.clear();
            return;
        }
        for (const std::vector<Held> *list : {&plain_, &internal_})
        {
            for (const Held &held : *list)
            {
                pass.parameters.push_back(held.parameter);
            }
        }
    }

    void Close(std::string_view name)
    {
        if (EqualsIgnoringCase(name, "Meta") || stack_.empty())
        {
            return;
        }
        // Every end tag pops the top (0x0050CEE2), whatever its name.
        const Frame closed = stack_.back();
        stack_.pop_back();
        if (closed.kind == Element::Pass)
        {
            FinishPass(*closed.pass);
        }
    }

    Definition out_;
    std::vector<Frame> stack_;
    std::vector<Held> plain_;
    std::vector<Held> internal_;
    std::deque<Block> discardedBlocks_;
    std::deque<Technique> discardedTechniques_;
    bool seen_ = false;
};

template <class Item> const Item *NamedIn(const std::vector<Item> &items, std::string_view wanted)
{
    const auto found = std::ranges::find(items, wanted, &Item::name);
    return found != items.end() ? &*found : nullptr;
}

// Whether AddSemantic (0x00503520) drops a semantic: one whose name is exactly one already there.
bool Dropped(const std::vector<Semantic> &semantics, std::size_t at)
{
    const std::string &own = semantics[at].name;
    return std::any_of(semantics.begin(), semantics.begin() + static_cast<std::ptrdiff_t>(at),
                       [&own](const Semantic &earlier) { return earlier.name == own; });
}

bool PassValid(const Pass &pass)
{
    if (!pass.effect.empty())
    {
        return true; // valid when its effect is found, which is the game's to say (0x00505EC0)
    }
    if (pass.vertexShader.empty() || pass.pixelShader.empty())
    {
        return false;
    }
    std::vector<const Parameter *> plain;
    for (const Parameter &parameter : pass.parameters)
    {
        if (!parameter.internal)
        {
            plain.push_back(&parameter);
        }
    }
    for (std::size_t i = 0; i < plain.size(); ++i)
    {
        if (!plain[i]->Valid())
        {
            return false;
        }
        for (std::size_t j = i + 1; j < plain.size(); ++j)
        {
            const bool sameStage = plain[i]->KindOf() == Kind::Texture && plain[j]->KindOf() == Kind::Texture &&
                                   plain[i]->StageNumber() && plain[i]->StageNumber() == plain[j]->StageNumber();
            if (plain[i]->name == plain[j]->name || sameStage)
            {
                return false;
            }
        }
    }
    return true;
}

bool TechniqueValid(const Technique &technique)
{
    return !technique.passes.empty() && std::ranges::all_of(technique.passes, PassValid);
}

bool BlockValid(const Block &block)
{
    if (block.technique && (!block.children.empty() || !TechniqueValid(*block.technique)))
    {
        return false;
    }
    return std::ranges::all_of(block.children, BlockValid);
}

std::uint32_t FlagsOf(const Block &block)
{
    std::uint32_t flags = 0;
    for (const Test &test : block.tests)
    {
        flags |= test.flags;
    }
    return flags;
}

// MaterialLogicBlock::TestsPass (0x00507280). A device is taken to have the one shader model its capabilities
// say exactly (0x0086E090 compares the shader versions for equality), so shader model 2 is a device without 3.
bool TestsPass(std::uint32_t flags, int level, bool shaderModel3)
{
    bool pass = true;
    if ((flags & 3) != 0)
    {
        if ((flags & 2) != 0 && level != 0)
        {
            pass = !shaderModel3;
        }
        if ((flags & 1) != 0)
        {
            if (level == 0)
            {
                return false;
            }
            pass = pass && shaderModel3;
        }
        if (!pass)
        {
            return false;
        }
    }
    if ((flags & 0x1C) != 0)
    {
        std::uint32_t admits = 0;
        switch (level)
        {
        case 3:
            admits |= flags & 4;
            [[fallthrough]];
        case 2:
            admits |= flags & 8;
            [[fallthrough]];
        case 1:
            admits |= flags & 0x10;
            break;
        default:
            break;
        }
        pass = admits != 0;
    }
    return pass;
}

// MaterialLogicBlock::SelectTechnique (0x005073C0): a block whose tests fail gives nothing, with its blocks;
// one with a technique gives it; else the first block inside that gives one.
const Technique *Select(const Block &block, int level, bool shaderModel3)
{
    if (!TestsPass(FlagsOf(block), level, shaderModel3))
    {
        return nullptr;
    }
    if (block.technique)
    {
        return &*block.technique;
    }
    for (const Block &child : block.children)
    {
        if (const Technique *chosen = Select(child, level, shaderModel3))
        {
            return chosen;
        }
    }
    return nullptr;
}

} // namespace

Kind Parameter::KindOf() const
{
    if (EqualsIgnoringCase(type, "Texture"))
    {
        return Kind::Texture;
    }
    if (EqualsIgnoringCase(type, "Float"))
    {
        return Kind::Float;
    }
    if (EqualsIgnoringCase(type, "Vector4f") || EqualsIgnoringCase(type, "Vector4fArray"))
    {
        return Count() > 1 ? Kind::Vector4Array : Kind::Vector4;
    }
    return EqualsIgnoringCase(type, "Matrix4x4f") ? Kind::Matrix : Kind::Unknown;
}

// MaterialParamTemp::CreateArt (0x005093D0) hands the Stage (a word, 0xFFFF where not written) on with
// SetStage, and the texture's resolver looks the SamplerName up only while it is 0xFFFF (0x005056A5).
std::optional<int> Parameter::StageNumber() const
{
    if (stage.empty())
    {
        return std::nullopt;
    }
    const int word = static_cast<int>(LeadingInteger(stage) & 0xFFFF);
    return word == 0xFFFF ? std::nullopt : std::optional(word);
}

// The Size is read with wcstol into a dword that starts at 1 (0x0050AA90), and for a Vector4f is the
// count the type is told by (0x0050F0B0, 2 + (count > 1)).
int Parameter::Count() const
{
    return size.empty() ? 1 : static_cast<int>(LeadingInteger(size));
}

// MaterialArtParameter::Validate: the base check (0x0050F2A0) fails a parameter with no name, and a Float
// (0x0050F2E0) one outside [Min, Max]. CreateArt always gives a Float its Min and Max (0x0050954D, 0x00509555), by
// default -10000 and 10000 (0x0050AB32, 0x0050AB53), so the 0 to 1 of the Float's constructor is not the range.
// The compare is unordered, so a NaN is outside it.
bool Parameter::Valid() const
{
    if (name.empty())
    {
        return false;
    }
    if (KindOf() != Kind::Float)
    {
        return true;
    }
    const float value = FloatValue();
    return LeadingFloat(min) <= value && value <= LeadingFloat(max);
}

// SetDefault is called whether or not there is a Default (0x00509546), with the empty text for none, which
// reads as 0 (0x0050F180, string to float 0x004C09D0).
float Parameter::FloatValue() const
{
    return LeadingFloat(defaultValue.value_or(""));
}

// MaterialArtVector4f::SetDefault (0x0050F540): while the stream is not at its end, a group of four zeros and
// four `>> float` into it, pushed. A number read to the text's end sets the end, so the group in hand is the last;
// white space after the last number does not, and makes one more group, read as a failure. A text with no
// number is one group of zeros. The empty text makes none.
std::vector<Vector4> Parameter::Vector4Groups() const
{
    std::vector<Vector4> groups;
    const std::string_view text = defaultValue ? std::string_view(*defaultValue) : std::string_view();
    std::size_t at = 0;
    bool ended = text.empty();
    while (!ended)
    {
        Vector4 group = kZero;
        for (float &component : group)
        {
            if (!ReadFloat(text, at, component) || at >= text.size())
            {
                ended = true;
                break;
            }
        }
        groups.push_back(group);
    }
    return groups;
}

// MaterialArtMatrix4x4f::SetDefault (0x0050F670): with text, sixteen `>> float` in file order. A text with
// fewer leaves the rest of the engine's matrix unwritten (the constructor does not set them), read here as 0.
std::optional<Matrix> Parameter::MatrixDefault() const
{
    if (!defaultValue || defaultValue->empty())
    {
        return std::nullopt;
    }
    Matrix matrix{};
    std::size_t at = 0;
    for (float &element : matrix)
    {
        if (!ReadFloat(*defaultValue, at, element))
        {
            break;
        }
    }
    return matrix;
}

// MaterialObject::ApplyParamDefaults (0x005028C0) for a Vector4: the Default's group when it has exactly one
// (0x00502A23), else zeros (0x00502A3F).
Vector4 Parameter::Vector4Value() const
{
    const std::vector<Vector4> groups = Vector4Groups();
    return groups.size() == 1 ? groups.front() : kZero;
}

// For an array (0x00502AE7): the Default cut to Size when it has at least that many, else a new vector of Size
// elements (0x00502B05), whose contents the engine leaves unwritten; zeros here.
std::vector<Vector4> Parameter::Vector4ArrayValue() const
{
    std::vector<Vector4> groups = Vector4Groups();
    const auto wanted = static_cast<std::size_t>(std::max(Count(), 0));
    if (groups.size() < wanted)
    {
        return std::vector<Vector4>(wanted, kZero);
    }
    groups.resize(wanted);
    return groups;
}

// For a matrix (0x00502B7C): the Default, else the identity (0x00502B80).
Matrix Parameter::MatrixValue() const
{
    constexpr Matrix kIdentity = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    return MatrixDefault().value_or(kIdentity);
}

// As the engine has it (read 2026-11-01 in GOG's exe for DAOLib #296, from DAOParagonEngine #351's render-16
// and render-21). Material::AddSemantic (0x00503520) drops a semantic whose name is exactly one it has, compared
// with case (0x004C0DE0, a wstring compare equal to 0), so the first of those is the one kept. Then
// Material::FindSemantic (0x00503140) walks what is kept from the last to the first, comparing names ignoring
// case (0x004C0A50). This keeps the file's list as written, so it skips what AddSemantic would have dropped.
const Semantic *Definition::SemanticNamed(std::string_view wanted) const
{
    for (std::size_t at = semantics.size(); at-- > 0;)
    {
        if (!Dropped(semantics, at) && EqualsIgnoringCase(semantics[at].name, wanted))
        {
            return &semantics[at];
        }
    }
    return nullptr;
}

// AddSemantic (0x00503520) makes a semantic the default when its name is exactly "Default" (the global string
// 0x00C31108) or when it is the first added (0x0050359E to 0x005035B5), so the first is the default until a
// "Default" comes, and a second "Default" is dropped as a repeat.
const Semantic *Definition::DefaultSemantic() const
{
    const Semantic *first = nullptr;
    for (std::size_t at = 0; at < semantics.size(); ++at)
    {
        if (Dropped(semantics, at))
        {
            continue;
        }
        if (semantics[at].name == "Default")
        {
            return &semantics[at];
        }
        first = first != nullptr ? first : &semantics[at];
    }
    return first;
}

// MaterialBuilderXML2::FindParamSet (0x0050B510): the first set whose name equals the reference's, compared
// ignoring case with _wcsicmp.
const ParameterSet *Definition::SetNamed(std::string_view wanted) const
{
    const auto found =
        std::ranges::find_if(sets, [wanted](const ParameterSet &set) { return EqualsIgnoringCase(set.name, wanted); });
    return found != sets.end() ? &*found : nullptr;
}

const Parameter *Definition::ParameterIn(const Pass &pass, std::string_view wanted)
{
    return NamedIn(pass.parameters, wanted);
}

std::optional<Definition> Read(std::string_view text)
{
    Reader reader;
    xml::ForEachTag(text, [&](std::string_view tag) { reader.Take(tag); });
    return reader.Done();
}

// Read in the GOG exe 2026-11-01 for DAOLib #298, from DAOParagonEngine #351's readings render-13, 17, 18,
// 19, 21 and 24, the test, the walk and the validity re-read in the disassembly. A semantic is built in
// MaterialSemanticTemp::Build (0x00509630): MaterialLogicBlock::Validate (0x00507330) over its tree, then
// SelectTechnique (0x004F6AB0, the walk 0x005073C0), then MaterialSemantic::Validate (0x004F6CD0), which
// clears the semantic for an empty name, no tree, a root whose valid byte is clear and no technique chosen.
// Validate fails a block with a technique and blocks inside, and one whose technique is not valid
// (0x00506F00: no pass, or a pass whose valid byte 0x00503970 is clear), and a failing child fails every
// block above it. A pass is cleared (MaterialPass::Validate 0x005040F0) for an empty vertex or pixel shader,
// no state block, a parameter that is not valid, two of one name, and two textures on one stage.
// The state block, and whether a shader loads, are the files'; nothing here looks at them.
const Technique *SelectTechnique(const Definition &definition, std::string_view semantic, int detailLevel,
                                 bool shaderModel3)
{
    const Semantic *chosen = semantic.empty() ? definition.DefaultSemantic() : definition.SemanticNamed(semantic);
    if (chosen == nullptr || chosen->name.empty() || !chosen->logic || !BlockValid(*chosen->logic))
    {
        return nullptr;
    }
    return Select(*chosen->logic, detailLevel, shaderModel3);
}

} // namespace dao::core::mat
