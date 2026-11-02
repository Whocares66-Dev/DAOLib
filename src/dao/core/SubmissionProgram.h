// The renderer's draw-order program (a .sub.xml, DAOLib #293) out of the game's files, over its text, and the
// engine's rule for which of its groups takes a renderable. A camera draws through the program the graphics
// settings name (`dadefaultsub`, in packages/core/data/subqueuefiles.erf): a list of groups in the file's order,
// each group a list of classes and a list of exclusions, each class a set of includes (affinity directives) and
// how to draw what they take. For drawing a scene as the game does (DAOParagonEngine), through Classify; the
// order the groups are drawn in is the file's.
//
// The names are the file's own: Renderer::SubmissionQueueProgram holds groups (Renderer::SubmissionGroup,
// ctor 0x0046CA90), a group holds classes (the 0x7C-byte object of ctor 0x00472280, which has no RTTI, so
// "class" is the builder's element name for it, [I]), a class holds directives (Renderer::IAffinityDirective
// and its ten kinds). Read in the exe 2026-11-01 from the builder (SQProgramBuilderXML2, 0x00472800..0x00474000)
// and the classification (0x0046EF40); the reading is DAOParagonEngine's render-08 and render-09. [C] marks
// what was read at an address, [I] what was inferred; every address is GOG's.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace dao::core::sub
{

// The 38 draw categories a material is of, in the engine's order: category i is bit i of a node's 64-bit class
// mask (the table of Renderer::ADClassAffinity's class names, 0x004F6660, read as code: a case-insensitive
// _wcsicmp chain from "transparent" = 1 to "LowLOD" = 0x2000000000).
inline constexpr std::size_t kCategoryCount = 38;
const std::array<std::string_view, kCategoryCount> &CategoryNames();

// Returns the mask of a category named as the file writes it (case-insensitive), or nullopt where the engine's
// table has none.
std::optional<std::uint64_t> CategoryMask(std::string_view name);

// Bits of a node's attributes (0x00472670, case-insensitive names): what the file's `attribute` names.
namespace attribute
{
inline constexpr std::uint32_t kOccluder = 0x1;
inline constexpr std::uint32_t kCutout = 0x2;
inline constexpr std::uint32_t kToolsWireframe = 0x4;
inline constexpr std::uint32_t kFadeoutObject = 0x10;
inline constexpr std::uint32_t kFadeoutPreEmitter = 0x80000;
inline constexpr std::uint32_t kLodFade = 0x20;
inline constexpr std::uint32_t kTransparent = 0x40;
inline constexpr std::uint32_t kNoDepthTest = 0x80;
inline constexpr std::uint32_t kHidden = 0x400;
inline constexpr std::uint32_t kSortedEmitter = 0x8000;
inline constexpr std::uint32_t kInvisibleCaster = 0x10000;
inline constexpr std::uint32_t kUnsupportedCaster = 0x20000;
inline constexpr std::uint32_t kIgnoreDistortion = 0x40000;
} // namespace attribute

// Returns the bit of an attribute named as the file writes it (case-insensitive), or nullopt.
std::optional<std::uint32_t> AttributeBit(std::string_view name);

// The kind of an affinity directive, as Renderer::CreateAffinityDirective (0x0046E770) numbers them; GetType
// returns the same.
enum class DirectiveKind : int
{
    Always = 0,                // `all`: takes everything
    Class = 1,                 // `class <category>`: the node's class mask meets the mask
    Attribute = 2,             // `attribute <name>`: the node's attribute bits meet the mask
    MaterialName = 3,          // `material <name>`: the node's material is of that name
    SemanticName = 4,          // `semantic <name>`: the node's material is of that semantic
    IncludesSemantic = 5,      // the material has that semantic among its own; no keyword makes it
    NoShadowCast = 6,          // the node casts no shadow; no keyword makes it
    ShadowCaster = 7,          // `shadowcaster <n>`: the node is in shadow partition n
    InvisibleShadowCaster = 8, // `invisibleshadowcaster <n>`: in partition n and of the invisiblecaster attribute
    MultiPass = 9,             // `supportsmultipass true|false`: the node's multipass support is that
};

// One affinity directive. Only the fields of its kind mean anything.
struct Directive
{
    DirectiveKind kind = DirectiveKind::Always;
    std::uint64_t categories = 0; // Class: the OR of the categories named
    std::uint32_t bits = 0;       // Attribute: the OR of the attributes named; ShadowCaster kinds: the partitions' mask
    std::string name;             // MaterialName, SemanticName, IncludesSemantic
    bool flag = false;            // MultiPass: what the node's support must be

    bool operator==(const Directive &) const = default;
};

// The renderable a program sorts, as the directives look at it (the node of 0x0046E2F0..0x0046E9FA).
struct Node
{
    std::uint64_t categories = 0;       // its material's class mask; the OR of the categories it is of
    std::uint32_t attributes = 0;       // render-modifier attribute bits
    std::string material;               // the material's name
    std::string semantic;               // the semantic the material is drawn with
    std::vector<std::string> semantics; // every semantic the material has (kind 5 only)
    std::uint32_t partitions = 0;       // shadow partitions it falls in, a bit each
    bool castsShadow = true;
    bool supportsMultipass = true;
    bool hasMaterial = true; // false: a node with no material object; no class takes it [C 0x00470920]

    bool operator==(const Node &) const = default;
};

// Returns whether a directive takes a node (the directive's Matches, slot 1 of its vftable).
[[nodiscard]] bool Matches(const Directive &directive, const Node &node);

// How a class draws what it takes: its mode (`render`), 0..4 in the engine [C 0x0046EAA0 dispatches on the
// field at +0x70 by that number]. The names are the keyword's, [I] for what each does.
enum class RenderMode : int
{
    Disabled = 0,  // `render disabled`: taken and not drawn
    Batched = 1,   // `render batched`
    Pass = 2,      // `render pass <n>`
    Multipass = 3, // `render multipass`
    Plain = 4,     // no `render` line: the default
};

// The operations a class runs, as the factory Renderer::ISubmissionOperation::Create (0x004718E0) numbers them
// [C jump table 0x00471B8C]; each is made by one line of the file.
enum class OperationType : int
{
    // The sorts order the class's nodes before they are drawn. Distance is the square of the distance from the eye
    // to the node's origin (the translation of its transform), not its depth in the view [C 0x0046F470]; mode 0
    // draws nearest first and mode 1 farthest first [C 0x0046FB60]. Material orders by shader, then material,
    // then the object's address [C 0x0046F760], so a tie between two materials is the engine's memory, not a rule.
    DistanceSortNearest = 0,  // `sort by distance nearest`; a distance other than `farthest` is this one
    DistanceSortFarthest = 1, // `sort by distance farthest`
    MaterialSort = 2,         // `sort by material`
    SemanticOverride = 3,     // `force semantic <name>`: each node is drawn with that semantic; `first` is the name
    MaterialOverride = 4,     // `force material <library>.<object>`; `first` is the library, `second` the object
    EventTrigger = 5,         // `trigger <event>`; `number` is the frame-buffer effect event
    EffectStage = 6,          // `effect <effect>.<stage>`; `first` and `second`
    StartPartition = 7,       // `shadows partitiongroup <n>`, with a StopPartition after; `number` is n
    StopPartition = 8,
    BlurPartition = 9,          // `shadows blurpartition [n]`; `number` is n, 0 where none
    BindBackbuffer = 10,        // `shadows bindbackbuffer`
    ResetInvisibleCasters = 11, // `shadows resetinvisiblecasters`
    RenderFadedSpeedTrees = 12, // `speedtree renderAllFadedTrees`
};

// When an operation runs [C 0x00472370 files them in three lists of the class, +0x20, +0x40, +0x30]: Before runs
// before the class's nodes are drawn, After runs after, PerObject on each node (nothing in the engine was
// found to call those, only the order is read: 0x00470C00 runs the first and the last).
enum class OperationWhen : int
{
    Before = 0,
    PerObject = 1,
    After = 2,
};

// One operation of a class, in the order the file's lines made them.
struct Operation
{
    OperationType type = OperationType::MaterialSort;
    OperationWhen when = OperationWhen::Before;
    int number = 0;    // a partition, or a trigger's event number (see TriggerEvent)
    std::string first; // see OperationType
    std::string second;

    bool operator==(const Operation &) const = default;
};

// Returns the frame-buffer effect event a `trigger` names, 0..10, or -1 for a word the engine refuses
// [C 0x00455980]: start 0, stop 1, beginframe 2, endframe 3, staticgeomstart (or staticgeom_start) 5,
// staticgeomend (or staticgeom_end) 6, motiongeometrystart 7, motiongeometryend 8, distortionmaskend 9,
// userdefined 10.
int TriggerEvent(std::string_view name);

// What a class needs before it is drawn (`requires`, the ISubmissionSwitch of 0x0046F680). A second `requires`
// replaces the first [C 0x0046F110].
enum class Gate : int
{
    None,
    ShadowsEnabled, // `requires shadows enabled`
    FbEffects,      // `requires fbeffects`
};

struct Class
{
    std::string name;                // `in <name>` or `select <name>`; empty for the group's first, unnamed
    std::vector<Directive> includes; // the same kind merged: two `include class` lines are one with the masks ORed
    bool keepCopy = false;           // `keep copy`: a node it takes goes on to the next class, and group
    bool forceDraw = false;          // `forcedraw`; the shipped files have none [C 0x00473D10]
    RenderMode mode = RenderMode::Plain;
    int pass = 0; // `render pass <n>`'s n
    Gate gate = Gate::None;
    std::vector<Operation> operations;

    // Returns the operations that run at a time, in order.
    [[nodiscard]] std::vector<const Operation *> OperationsAt(OperationWhen when) const;
    // Returns whether any include was given (bit 0x10 of the engine's class flags, set when one is added).
    [[nodiscard]] bool HasIncludes() const
    {
        return !includes.empty();
    }
    // Returns whether the class takes the node: it has a material and any include matches [C 0x00470920].
    [[nodiscard]] bool Accepts(const Node &node) const;
};

// Where a node went: the group and class (indices into the program's and the group's lists).
struct Placement
{
    std::size_t group = 0;
    std::size_t cls = 0;

    bool operator==(const Placement &) const = default;
};

struct Group
{
    std::string id;                    // the element's `id`; empty where it has none
    std::vector<Class> classes;        // in the file's order
    std::vector<Directive> exclusions; // `exclude` and `except`, whatever class they came after, merged by kind

    // Returns whether any class has an include. The engine leaves a group without out of the list it
    // classifies by [C 0x0046EBA0, I for what the list is used for].
    [[nodiscard]] bool HasIncludes() const;
    // Offers a node to the group as Renderer::SubmissionGroup::Classify does [C 0x0046EF40]: refused whole if
    // any exclusion matches; else each class in order takes it if it accepts, and a class that does not keep a
    // copy ends the search. Appends the classes that took it to `into`, as Placement{group, class}.
    // @return whether the group claimed the node (some class took it and the last to take it kept no copy)
    bool Classify(const Node &node, std::size_t groupIndex, std::vector<Placement> &into) const;
};

// A whole program: its groups in the file's order, which is the order they are drawn in.
struct Program
{
    std::string id; // the SubmissionInstruction's `id`
    std::vector<Group> groups;

    // Offers a node to each group in order until one claims it, as the queue does [C 0x0046CB64, render-07]. The
    // result lists every class it was put in, in order: the `keep copy` ones before the group that claimed it.
    [[nodiscard]] std::vector<Placement> Classify(const Node &node) const;
};

// Reads a program from its file's text. The engine's builder finishes with an error count; a file with one is
// thrown away whole [C 0x0046CF50]. Those errors are an element out of place, a line of 256 characters or
// more, an unknown word that continues no `include`, `exclude`, `effect` or `requires`, a malformed include
// or exclude, a class or attribute name the engine does not have, and a missing keyword value; and what the
// XML parser refuses: here, tags that do not close.
// @return nullopt where the engine would not take the file
std::optional<Program> Read(std::string_view text);

// The program the engine draws with when the file is missing or bad [C 0x0046CE65..0x0046CE89]: one group of one
// class that includes everything and draws by the plain mode.
Program Fallback();

// A program as the engine ends up with it: the file's, or the fallback.
struct Loaded
{
    Program program;
    bool fellBack = false; // the file was missing or bad, and program is Fallback()
};

// Reads the program a camera would load from the file's text, nullopt where the file is not found.
Loaded ReadOrFallback(std::optional<std::string_view> text);

} // namespace dao::core::sub
