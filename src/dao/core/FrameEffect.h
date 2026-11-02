// A frame effect (.ffx.xml) out of the game's files, over its text (DAOLib #292): the effects a
// file holds, each with its id, layer, minimum detail, availability, resources, the effects it
// requires and its stages by event, each stage's filters holding the instructions their lines make.
// The post-process the engine runs at an area's end (dev/FRAME_EFFECTS.md) is these.
//
// Read as the engine's builder reads them, quirks and all (FBEffectBuilderXML2, 0x004698E0..0x0046A430,
// and the filter line parser, 0x00461570), so what is returned is what the engine would hold: an effect
// the engine drops is not returned, a line it ignores makes no instruction.
#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace dao::core::ffx
{

// A resource element's kind, by the engine's number for it. The engine has a texture kind (2) too,
// but no element makes one.
enum class ResourceKind
{
    RenderTarget = 0, // <target>
    BackBuffer = 1,   // <backbuffer>
    Float = 3,        // <float>
    Vector4f = 4,     // <vector4f>
    Runtime = 5,      // <runtime>
};

struct Attribute
{
    std::string name;
    std::string value;
};

// An effect's resource. Its attributes after the id are kept as written, in order.
struct Resource
{
    ResourceKind kind = ResourceKind::RenderTarget;
    std::string id;
    std::vector<Attribute> attributes;
    int index = 1; // a back buffer's own index (its `index` attribute, else 1); unused by the others
};

// <requires id=".." effectid=".."/>: the effect asks for another to be loaded and kept on with it.
struct Requirement
{
    std::string id;
    std::string effectId;
};

// When a stage runs, by the engine's number. A name the engine does not know is Other (11): the
// stage is read and never run. Start and Stop are the enable and disable events; a Stop stage never
// runs either, since the effect is already off when the event is sent (0x0045FA10).
enum class Event
{
    Start = 0,
    Stop = 1,
    BeginFrame = 2,
    EndFrame = 3,
    StaticGeomStart = 5,
    StaticGeomEnd = 6,
    MotionGeometryStart = 7,
    MotionGeometryEnd = 8,
    DistortionMaskEnd = 9,
    UserDefined = 10,
    Other = 11,
};

// How a bind sets the viewport.
enum class ViewportMode
{
    Original = 0,
    Recalculate = 1,
    Stored = 2,
};

// `Name = 0.5`: sets a float resource of the effect to a literal.
struct SetResource
{
    std::string resource;
    float value = 0;
};

// `param = Name`: the filter's shader parameter takes a resource's value (a float, a vector or a
// runtime value) each time the filter runs.
struct ParameterFromResource
{
    std::string parameter;
    std::string resource;
    ResourceKind kind = ResourceKind::Float;
};

// `Decal0 = Target`: the shader's texture is a render target or the back buffer.
struct TextureFromResource
{
    std::string parameter;
    std::string resource;
};

// `param = 0.5`: a shader parameter set to a literal.
struct ParameterValue
{
    std::string parameter;
    float value = 0;
};

// `param = Target.width` or `.height`: a shader parameter set to a target's size as it was
// when the file was read.
struct ParameterSize
{
    std::string parameter;
    std::string target;
    bool height = false; // else width
};

// `copy Source to Destination [filter point|linear]`.
struct Copy
{
    std::string source;      // a render target's id, or "backbuffer"
    std::string destination; // a render target or back buffer resource
    int filter = 5;          // 5 by default, 6 point, 7 linear
    bool fromBackBuffer = false;
};

// `bind ...`: what a render target slot is drawn to.
struct Bind
{
    enum class Subject
    {
        Resource,   // a render target, or a back buffer resource (then `index` is its own)
        BackBuffer, // the word `backbuffer`
        Unbind,     // the word `null`
    };
    Subject subject = Subject::Resource;
    std::string resource; // the render target's or back buffer resource's id
    int index = 0;        // the slot, `at N`
    ViewportMode viewport = ViewportMode::Original;
};

// `run semantic Name`, or `run` / `run preferred` for the material's default semantic.
struct Run
{
    std::string semantic; // empty for the default
};

// `dump Target [path]`: a debug aid that writes a target to a file.
struct Dump
{
    std::string target;
    std::string path; // the target's id and ".dds" where none is given
};

using Instruction = std::variant<SetResource, ParameterFromResource, TextureFromResource, ParameterValue, ParameterSize,
                                 Copy, Bind, Run, Dump>;

struct Filter
{
    std::string id;
    std::string material; // the material's name; empty where none
    std::vector<Instruction> instructions;
};

struct Stage
{
    std::string id;
    Event event = Event::Other;
    std::vector<Filter> filters;
};

struct Effect
{
    std::string id;
    int layer = 0;           // the draw order, ascending
    int minLod = 0;          // not run below this detail setting
    bool cinematics = false; // domain="cinematics" and read with the engine's flag off: it has no resources or stages
    // True if the `availability` attribute was a number above 0. The engine draws an effect only if
    // this is set (while its global 0x00C1E8B5 is 0), so an effect with no attribute is not drawn.
    bool available = false;
    std::vector<Resource> resources;
    std::vector<Requirement> requirements;
    std::vector<Stage> stages;

    // Returns the resource of an id (the engine's lookup is exact), or nullptr.
    [[nodiscard]] const Resource *ResourceNamed(std::string_view wanted) const;
};

struct Options
{
    // The engine's command-line option (0x00C1E8B7) that lets cinematics-only effects load whole.
    // Off, as the engine runs unless asked.
    bool cinematicsAllowed = false;
};

// Reads the effects of a .ffx.xml's text, in file order.
// An effect the engine would drop is not returned; at the first error the engine stops the file and
// the effects before it stand. A domain="cinematics" effect read with the option off is returned empty,
// and the file's effects after it are lost.
std::vector<Effect> Read(std::string_view text, const Options &options = {});

} // namespace dao::core::ffx
