// dao/core/MaterialDefinition.h: a .mat.xml's elements read one tag at a time (dao/core/XmlTags.h),
// each put where the elements still open say it is.
#include "dao/core/MaterialDefinition.h"

#include "dao/core/XmlTags.h"

#include <algorithm>
#include <charconv>
#include <utility>

namespace dao::core::mat
{

namespace
{

using xml::Attribute;
using xml::ElementOf;

std::string Named(std::string_view tag, std::string_view key)
{
    return Attribute(tag, key).value_or("");
}

Parameter ParameterOf(std::string_view tag, bool internal)
{
    return {.internal = internal,
            .type = Named(tag, "Type"),
            .name = Named(tag, "Name"),
            .sampler = Named(tag, "SamplerName"),
            .defaultValue = Attribute(tag, "Default"),
            .usage = Named(tag, "Usage"),
            .stage = Named(tag, "Stage"),
            .size = Named(tag, "Size")};
}

Test TestOf(std::string_view tag)
{
    Test test;
    const std::string model = Named(tag, "ShaderModel");
    std::from_chars(model.data(), model.data() + model.size(), test.shaderModel);
    test.detail = Named(tag, "Detail");
    return test;
}

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
        const std::string_view element = ElementOf(closing ? tag.substr(1) : tag);
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
    struct Logic
    {
        std::string note;
        bool hasBlock = false; // whether its tests and techniques have a block yet
    };

    void Open(std::string_view element, std::string_view tag)
    {
        if (element == "Material")
        {
            seen_ = true;
            out_.name = Named(tag, "Name");
        }
        else if (element == "ParameterSet")
        {
            out_.sets.push_back({.name = Named(tag, "Name"), .parameters = {}, .effects = {}});
            inSet_ = true;
        }
        else if (element == "Semantic")
        {
            out_.semantics.push_back(
                {.name = Named(tag, "Name"), .renderClass = Named(tag, "RenderClass"), .blocks = {}});
            inSemantic_ = true;
        }
        else if (element == "Logic" && inSemantic_)
        {
            logic_.push_back({.note = Named(tag, "Note"), .hasBlock = false});
        }
        else if (element == "LogicTest" && OpenBlock())
        {
            out_.semantics.back().blocks.back().tests.push_back(TestOf(tag));
        }
        else if (element == "Technique" && OpenBlock())
        {
            out_.semantics.back().blocks.back().techniques.push_back({.name = Named(tag, "Name"), .passes = {}});
            inTechnique_ = true;
        }
        else if (element == "Pass" && inTechnique_)
        {
            out_.semantics.back().blocks.back().techniques.back().passes.push_back(
                {.name = Named(tag, "Name"),
                 .vertexShader = Named(tag, "VertexShader"),
                 .pixelShader = Named(tag, "PixelShader"),
                 .state = Named(tag, "State"),
                 .parameters = {},
                 .sets = {},
                 .streams = {},
                 .effects = {}});
            inPass_ = true;
        }
        else
        {
            Item(element, tag);
        }
    }

    // A parameter, a set's reference, a stream or an effect: the open pass's, or the open set's.
    void Item(std::string_view element, std::string_view tag)
    {
        const bool parameter = element == "Parameter" || element == "InternalParameter";
        if (inPass_)
        {
            Pass &pass = out_.semantics.back().blocks.back().techniques.back().passes.back();
            if (parameter)
            {
                pass.parameters.push_back(ParameterOf(tag, element == "InternalParameter"));
            }
            else if (element == "ParameterSetRef")
            {
                pass.sets.push_back(Named(tag, "Ref"));
            }
            else if (element == "RequiredStreamData")
            {
                pass.streams.push_back(Named(tag, "Name"));
            }
            else if (element == "EffectParameter")
            {
                pass.effects.push_back(Named(tag, "EffectName"));
            }
        }
        else if (inSet_)
        {
            ParameterSet &set = out_.sets.back();
            if (parameter)
            {
                set.parameters.push_back(ParameterOf(tag, element == "InternalParameter"));
            }
            else if (element == "EffectParameter")
            {
                set.effects.push_back(Named(tag, "EffectName"));
            }
        }
    }

    // Makes the innermost open logic block's block, the first time it has a test or a technique;
    // false outside one.
    bool OpenBlock()
    {
        if (logic_.empty())
        {
            return false;
        }
        if (!logic_.back().hasBlock)
        {
            out_.semantics.back().blocks.push_back({.note = logic_.back().note, .tests = {}, .techniques = {}});
            logic_.back().hasBlock = true;
        }
        return true;
    }

    void Close(std::string_view element)
    {
        if (element == "ParameterSet")
        {
            inSet_ = false;
        }
        else if (element == "Semantic")
        {
            inSemantic_ = false;
            logic_.clear();
        }
        else if (element == "Logic" && !logic_.empty())
        {
            logic_.pop_back();
        }
        else if (element == "Technique")
        {
            inTechnique_ = false;
        }
        else if (element == "Pass")
        {
            inPass_ = false;
        }
    }

    Definition out_;
    std::vector<Logic> logic_;
    bool seen_ = false;
    bool inSet_ = false;
    bool inSemantic_ = false;
    bool inTechnique_ = false;
    bool inPass_ = false;
};

template <class Item> const Item *NamedIn(const std::vector<Item> &items, std::string_view wanted)
{
    const auto found = std::ranges::find(items, wanted, &Item::name);
    return found != items.end() ? &*found : nullptr;
}

} // namespace

const Semantic *Definition::SemanticNamed(std::string_view wanted) const
{
    return NamedIn(semantics, wanted);
}

const ParameterSet *Definition::SetNamed(std::string_view wanted) const
{
    return NamedIn(sets, wanted);
}

const Parameter *Definition::ParameterIn(const Pass &pass, std::string_view wanted) const
{
    if (const Parameter *own = NamedIn(pass.parameters, wanted))
    {
        return own;
    }
    for (const std::string &set : pass.sets)
    {
        if (const ParameterSet *found = SetNamed(set))
        {
            if (const Parameter *shared = NamedIn(found->parameters, wanted))
            {
                return shared;
            }
        }
    }
    return nullptr;
}

std::optional<Definition> Read(std::string_view text)
{
    Reader reader;
    xml::ForEachTag(text, [&](std::string_view tag) { reader.Take(tag); });
    return reader.Done();
}

} // namespace dao::core::mat
