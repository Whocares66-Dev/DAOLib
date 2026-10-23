// How long animations, attacks and abilities take (dao/Animation.h).
//
// Read 2026-10-02. A creature's animation set (its +0x154) answers how long
// an animation is, in milliseconds (0x00694E50, __thiscall (id), `ret 4`),
// and whether it has one (0x00694E70). An animation's id is a row of the
// anim_ tables (anim_combat for an attack's).
//
// An attack's swing (0x006A41A0, the step that plays it): the scripts set
// its time for a humanoid with a weapon, after the attack is rolled
// (combat_h.nss, SetAttackDuration with core_h.nss's CalculateAttackTiming;
// SetAimLoopDuration for a bow's aim), and the engine keeps it in the
// creature's combat record (+0xC60) in milliseconds, +0x64 the attack's and
// +0x60 the aim's. With one set the swing's animation is sped or slowed to
// take that time, after a blend of a fifth of it; with none (a creature
// that is not humanoid, or no weapon) it takes the animation's own length,
// less a blend of a fifth of that. An attack that has no animation of its
// own plays kAttack.
//
// Seen in play 2026-10-02 (examples/animation, two fights in the GOG game,
// one in the turn-based mod's turns and one in real time, the same figures
// in both; build\playtest-requests6-10-02-animation-results):
// - An animation-timed action's command ends at about four fifths of the
//   animation, the blend: giant spiders' attacks, Length(kAttack) 1.833 s,
//   took a median 1.49 s (with a second group near 1.15 s, another attack
//   animation of theirs); Dirty Fighting's cast, 1.800 s, took 1.51 s;
//   a spider's Web, 2.433 s, took 2.07 s.
// - A humanoid's scripted swing takes the scripts' time: Lyna's daggers,
//   the scripts' 1.000 s, took 1.02 s, 46 times running.
// - Bows: Length(kAttack) 1.500 s says nothing of a shot, which took 1.39 to
//   1.42 s for a follower and 1.58 to 1.62 s for the player.
// - AimDuration read nothing at any time, a bow's shot included.
// - An ability whose row has no CastAnim (Pinning Shot, a spider's
//   Overwhelm) reads nothing here, though it plays something (1.7 s, 2.4 s).
//   Read after: the use-ability step (0x007A7440) sends the ability's
//   script EVENT_TYPE_ABILITY_CAST_START, and then, with a CastAnim, plays
//   it and lets the impact come at its point; with none, it sends
//   EVENT_TYPE_ABILITY_CAST_IMPACT at once and plays nothing. What such an
//   ability takes is what its script does then: an archery talent's is a
//   shot (Pinning Shot's 1.7 s, a shot's 1.4 to 1.6 s and the command's
//   ends), a monster's special its own.
//
// A shot's steps, read 2026-10-10 (DAOLib #51). A
// ranged attack's sub-action 50 queues the turn to face (39) and the shot
// (2, 0x007A6490), which sends EVENT_TYPE_COMMAND_PENDING; when the scripts
// answer success, and the shooter has a line of sight, 0x007A4DD0 queues
// three steps (a UseAbility queues the same three where its ability's
// Flags in abi_base has bit 0, 0x007A83F0):
// - 37 playing animation 249 ("Aim"), with a speed of its Length over the
//   combat record's attack time (+0x64) where that is above 0, so the step
//   takes the scripts' attack time; else at its own speed;
// - 6, the aim (0x007A2410), whose duration is the combat record's aim time
//   (+0x60) as it starts, done at once at 0; it fails without a line of
//   sight or with the target more than 67.5 degrees off the facing;
// - 37 playing animation 243 ("Fire") at its own speed.
// The animation step (37, 0x007A4390) takes its duration from the step's
// length (dao/Addresses.h, AnimationStepLength) over its speed. The scripts
// set both times in the COMMAND_PENDING handler, for humanoids alone
// (combat_h, HandleCommandAttack, in the toolset's source: the attack time
// 0.3 s for a staff, for a bow by follower, armour and Master Archer; the
// aim time core_h's GetCreatureRangedDrawSpeed, the ranged aim speed
// property's total and itemstats' BaseAimDelay, not below 0). The attack
// speed modifier is in the melee timing alone. The attack command's finish
// (0x00746900, at 0x00746A74) zeroes both times, so they are set only from
// COMMAND_PENDING to the command's end.
//
// Seen in play 2026-10-10 (examples/attack, the turn-based mod's session,
// Awakening's Wending Wood in real time): a follower's bow, shot after
// shot, step 39, then 37 given 800 ms, then 37 given 766 ms, each running
// its time, with no aim seen (its time 0) and both script times read as
// none at each shot's start; his Length(kAim) 1.566 s and Length(kFire)
// 0.433 s, so the Fire step is the step's length, not the animation's. A
// mage's staff: 39, then 6 given 100 ms, then 37 given 1,533 ms, the
// creature having no Aim animation (its step takes no time) and a Fire of
// 1.000 s. StepLength and OfShot were written after that run, and seen the
// same day in two more of its runs: read as each shot's command ended,
// OfShot's three parts were the steps' given times in 64 plain shots of 64
// that ran to their end (the bow 0.800, 0 and 0.766 s with an attack time
// of 0.800 s and no aim time; the staff 0, 0.100 and 1.533 s with 0.300
// and 0.100 s), and its fire before each shot was the Fire step's. The
// four shots a cleared command cut short showed fewer steps. An archery
// talent's shot (Arrow of Slaying and others, by the archer's tactics) ran
// the Aim animation at its own 1,566 ms, an aim of 1,500 ms and the same
// 766 ms: no attack time, and the scripts' aim time for an ability.
#include "dao/Animation.h"

#include "dao/Addresses.h"
#include "dao/ClientExoApp.h"
#include "dao/Gda.h"
#include "dao/Guard.h"
#include "dao/Hook.h"
#include "dao/Module.h"
#include "dao/Objects.h"
#include "dao/SafeRead.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <utility>
#include <vector>

namespace dao::animation
{

namespace
{

constexpr std::size_t kCombat = 0xC60;  // the creature's combat record
constexpr std::size_t kAimMs = 0x60;    // SetAimLoopDuration's milliseconds
constexpr std::size_t kAttackMs = 0x64; // SetAttackDuration's milliseconds
constexpr float kMillisecond = 0.001F;

// Observers of a creature's play call, a list replaced whole on each add so that the hook, which
// runs for every animation, takes a pointer under the lock and walks the list without one.
using PlayedObserver = std::function<void(ObjectId, std::int32_t, float, float, bool)>;
std::mutex g_playedLock;
std::shared_ptr<const std::vector<PlayedObserver>> g_played = std::make_shared<const std::vector<PlayedObserver>>();

// CCCreature slot 44: __thiscall (animation, speed, blend, flag), `ret 0x10`, returns nothing.
using PlayFn = void(__fastcall *)(CCCreature *, int, std::int32_t, float, float, std::uint8_t);
Detour<PlayFn> g_play;

void __fastcall Play(CCCreature *self, int edx, std::int32_t animation, float speed, float blend, std::uint8_t flag)
{
    g_play.Original()(self, edx, animation, speed, blend, flag);
    std::shared_ptr<const std::vector<PlayedObserver>> observers;
    {
        const std::lock_guard lock(g_playedLock);
        observers = g_played;
    }
    const ObjectId id = self != nullptr ? self->Id() : kInvalidObject;
    for (const auto &observer : *observers)
    {
        detail::Guard("an animation played callback", [&] { observer(id, animation, speed, blend, flag != 0); });
    }
}

CCCreature *CreatureOf(ObjectId creature)
{
    CClientExoApp *app = CClientExoApp::Get();
    return app != nullptr ? app->GetCreatureById(creature) : nullptr;
}

std::optional<float> Milliseconds(ObjectId creature, std::size_t field)
{
    const CCCreature *object = CreatureOf(creature);
    if (object == nullptr)
    {
        return std::nullopt;
    }
    const auto record = TryRead<std::uintptr_t>(reinterpret_cast<std::uintptr_t>(object) + kCombat);
    if (!record || *record == 0)
    {
        return std::nullopt;
    }
    const auto ms = TryRead<std::int32_t>(*record + field);
    if (!ms || *ms <= 0)
    {
        return std::nullopt;
    }
    return static_cast<float>(*ms) * kMillisecond;
}

} // namespace

namespace
{

// One of the engine's two lengths of an animation, AnimationLength or
// AnimationStepLength, each __thiscall (id), `ret 4`, in milliseconds;
// nothing for an animation the creature lacks.
std::optional<float> EngineLength(ObjectId creature, std::int32_t animation, AddressId length)
{
    CCCreature *object = CreatureOf(creature);
    if (object == nullptr)
    {
        return std::nullopt;
    }
    using HasFn = bool(__fastcall *)(CCCreature *, int, std::int32_t);
    if (!reinterpret_cast<HasFn>(Module::Address(addr::kCCCreature_HasAnimation))(object, 0, animation))
    {
        return std::nullopt;
    }
    using LengthFn = std::int32_t(__fastcall *)(CCCreature *, int, std::int32_t);
    const std::int32_t ms = reinterpret_cast<LengthFn>(Module::Address(length))(object, 0, animation);
    if (ms <= 0)
    {
        return std::nullopt;
    }
    return static_cast<float>(ms) * kMillisecond;
}

} // namespace

std::optional<float> Length(ObjectId creature, std::int32_t animation)
{
    return EngineLength(creature, animation, addr::kCCCreature_AnimationLength);
}

std::optional<float> StepLength(ObjectId creature, std::int32_t animation)
{
    return EngineLength(creature, animation, addr::kCCCreature_AnimationStepLength);
}

// AttackDuration: set when an attack is rolled and zeroed by the command's
// finish (the file's head). In play it read nothing at an attack's start
// and the scripts' value at its end (1.000 s for daggers, 0.600 s for
// bows): set while the attack is under way, so a cost reckoned ahead of an
// attack takes the scripts' formula instead.
std::optional<float> AttackDuration(ObjectId creature)
{
    return Milliseconds(creature, kAttackMs);
}

std::optional<float> AimDuration(ObjectId creature)
{
    return Milliseconds(creature, kAimMs);
}

// The three steps of the file's head, each as its handler would time it:
// the Aim step its step length over the speed the engine gives it, the
// animation's Length over the attack time; nothing for a creature without
// the animation, whose step has no length.
std::optional<Shot> OfShot(ObjectId creature, std::optional<float> attackTime, std::optional<float> aimTime)
{
    const auto fire = StepLength(creature, kFire);
    if (!fire)
    {
        return std::nullopt;
    }
    Shot shot{.aimLoop = aimTime ? *aimTime : AimDuration(creature).value_or(0), .fire = *fire};
    const auto step = StepLength(creature, kAim);
    const auto own = Length(creature, kAim);
    if (step && own)
    {
        const auto scripted = attackTime ? attackTime : AttackDuration(creature);
        shot.aim = scripted ? *step * *scripted / *own : *step;
    }
    return shot;
}

namespace
{

// An animation's length, if it is one the creature has; nothing for 0, the
// tables' "none".
std::optional<float> LengthOf(ObjectId creature, std::optional<std::int32_t> animation)
{
    return animation && *animation > 0 ? Length(creature, *animation) : std::nullopt;
}

std::optional<std::int32_t> TransitionOf(std::optional<std::int32_t> animation)
{
    if (!animation || *animation <= 0)
    {
        return std::nullopt;
    }
    return gda::GetInt(script::Table::BaseAnimations, *animation, "TransitionTo");
}

} // namespace

// From the abilities table (TABLE_ABILITIES_SPELLS, ConjureAnim and
// CastAnim) and the animations table's TransitionTo (TABLE_BASE_ANIMATIONS),
// which says what each animation passes into: a spell's conjure start
// (244, "Conjure Direct start") into its loop (264), its cast (246, "Cast
// Direct") into its end (290); a talent has a cast alone (Dirty Fighting's
// 146), a potion its drink (257). How many times the loop plays is the
// caller's to know (the scripts' conjure tables count loops, not seconds).
// Read 2026-10-02 from the tables; the casts seen in play the same day
// (the file's head), no conjure yet.
Ability OfAbility(ObjectId creature, std::int32_t ability)
{
    const auto conjure = gda::GetInt(script::Table::AbilitiesSpells, ability, "ConjureAnim");
    const auto cast = gda::GetInt(script::Table::AbilitiesSpells, ability, "CastAnim");
    // TransitionTo 1 is the idle ("Pause"), not a part of the ability.
    const auto loop = TransitionOf(conjure);
    const auto end = TransitionOf(cast);
    return Ability{.conjure = LengthOf(creature, conjure),
                   .conjureLoop = loop && *loop > 1 ? LengthOf(creature, loop) : std::nullopt,
                   .cast = LengthOf(creature, cast),
                   .castEnd = end && *end > 1 ? LengthOf(creature, end) : std::nullopt};
}

// One object's animation held and sped, read 2026-10-17 in GOG's exe and
// seen in play the same day (DAOLib #159, for DAOTurnBased's issue 119, an
// ogre's grab held between turns; the probe's Hold.cpp).
//
// A graphics object (GraphicsStructures::Object) has its animation's speed
// as a double at +0x2E8, 1 from its constructor; Animation::AnimationTask
// (0x008154D0) multiplies each frame's time by it before it advances the
// object's clips. Its setter is the object's slot 36 (0x0054F830), __thiscall
// (double), `ret 8`. The engine's model over it keeps the same speed as a
// float at +0xEC, and its pause (0x00A5D820, __thiscall (bool)) sets both to
// 0 or 1. The model reaches the graphics object through its +0x18, then
// +4, whose slot 19 (no arguments) answers it. An object's
// interface at +0x70 gives its model from its slot 24 and pauses it from its
// slot 19 (CCObject's 0x00695380); CCCreature's slot 19 (0x0069F590) also
// pauses (0x006C9E60) or resumes (0x006CAB00) its controller at +0x768.
// Called through the vtables, so no address is needed.
//
// Seen in play (turn-based-agent's runs, GOG, Witch Hunt, an ogre grabbing
// Lyna): the paused ogre raised no animation event for 8 s and landed no
// punch, its spent grab step (51, 2100 of 2100 ms) unmoved; the victim was
// not paused with it, her model's own, until she was paused too, and both
// resumed in one frame went on in step with no snap. A stun or a death on a
// paused ogre started its command, whose step's time ran (37, 3707 ms;
// 51, 5400 ms), and showed nothing until the resume. In the turn-based
// mod's fights the same evening an ogre's grab, the pair paused at the end
// of the ogre's turns and resumed at its next, ran its whole 12.1 s over
// three turns; Stonefist on the paused ogre ended its grab command without
// starting another (the knockdown did not take), so OnEnd, not OnStart, is
// what let the pair go. SetRate seen in play by examples/hold (a 40 s
// fight: 11 attacks slowed to 0.25 and back, 8 pauses each resumed, no
// warning).
namespace
{

constexpr std::size_t kInterface = 0x70;
constexpr std::size_t kPauseSlot = 19;
constexpr std::size_t kModelSlot = 24;
constexpr std::size_t kModelRate = 0xEC;
constexpr std::size_t kModelGraphics = 0x18;
constexpr std::size_t kGraphicsOwner = 4;
constexpr std::size_t kGraphicsSlot = 19;
constexpr std::size_t kSetRateSlot = 36;

void **VtableOf(void *self)
{
    return *static_cast<void ***>(self);
}

void *InterfaceOf(CCCreature *creature)
{
    return reinterpret_cast<char *>(creature) + kInterface;
}

std::uintptr_t ModelOf(CCCreature *creature)
{
    using Fn = void *(__fastcall *)(void *self, int edx);
    void *self = InterfaceOf(creature);
    return reinterpret_cast<std::uintptr_t>(reinterpret_cast<Fn>(VtableOf(self)[kModelSlot])(self, 0));
}

// The graphics object under a model: its +0x18, then +4, whose slot 19 answers it.
void *GraphicsOf(std::uintptr_t model)
{
    const auto graphics = TryRead<std::uintptr_t>(model + kModelGraphics);
    const auto owner = graphics && *graphics != 0 ? TryRead<std::uintptr_t>(*graphics + kGraphicsOwner) : std::nullopt;
    if (!owner || *owner == 0)
    {
        return nullptr;
    }
    using GetFn = void *(__fastcall *)(void *self, int edx);
    void *from = reinterpret_cast<void *>(*owner);
    return reinterpret_cast<GetFn>(VtableOf(from)[kGraphicsSlot])(from, 0);
}

} // namespace

bool Pause(ObjectId creature, bool paused)
{
    CCCreature *object = CreatureOf(creature);
    if (object == nullptr)
    {
        return false;
    }
    using Fn = void(__fastcall *)(void *self, int edx, bool paused);
    void *self = InterfaceOf(object);
    reinterpret_cast<Fn>(VtableOf(self)[kPauseSlot])(self, 0, paused);
    return true;
}

bool SetRate(ObjectId creature, float rate)
{
    CCCreature *object = CreatureOf(creature);
    const std::uintptr_t model = object != nullptr ? ModelOf(object) : 0;
    if (model == 0)
    {
        return false;
    }
    // As the model's pause does: the graphics object's speed by its setter,
    // and the model's own copy.
    if (void *graphics = GraphicsOf(model))
    {
        using SetFn = void(__fastcall *)(void *self, int edx, double rate);
        reinterpret_cast<SetFn>(VtableOf(graphics)[kSetRateSlot])(graphics, 0, rate);
    }
    *reinterpret_cast<float *>(model + kModelRate) = rate;
    return true;
}

// Read 2026-10-15 for DAOLib #131 and checked 2026-10-23 (dao/Addresses.h,
// kModelAnimationTree_Vftable): the graphics object keeps its animations in a
// vector at +0x17C, and a ModelAnimationTree's nodes are a vector at its +0x10,
// 0x34 bytes each, a node's place in its cycle the 16-bit word at +0x2E (0x10000
// the whole cycle) and its last place at +0x30. Both are set, so the next step
// blends from where the node is now set and not from where it was.
bool SetPhase(ObjectId creature, float phase)
{
    constexpr std::size_t kAnimationsBegin = 0x180;
    constexpr std::size_t kAnimationsEnd = 0x184;
    constexpr std::size_t kNodesBegin = 0x14;
    constexpr std::size_t kNodesEnd = 0x18;
    constexpr std::size_t kNodeSize = 0x34;
    constexpr std::size_t kNodePlace = 0x2E;
    constexpr std::size_t kNodeLastPlace = 0x30;
    constexpr float kCycle = 65536.0F;
    CCCreature *object = CreatureOf(creature);
    const std::uintptr_t model = object != nullptr ? ModelOf(object) : 0;
    const auto graphics = reinterpret_cast<std::uintptr_t>(model != 0 ? GraphicsOf(model) : nullptr);
    if (graphics == 0)
    {
        return false;
    }
    const auto place = static_cast<std::uint16_t>(std::min(std::clamp(phase, 0.0F, 1.0F) * kCycle, 65535.0F));
    const std::uintptr_t treeTable = Module::Address(addr::kModelAnimationTree_Vftable);
    const auto begin = TryRead<std::uintptr_t>(graphics + kAnimationsBegin).value_or(0);
    const auto end = TryRead<std::uintptr_t>(graphics + kAnimationsEnd).value_or(0);
    bool any = false;
    for (std::uintptr_t slot = begin; begin != 0 && slot < end; slot += sizeof(std::uintptr_t))
    {
        const auto tree = TryRead<std::uintptr_t>(slot).value_or(0);
        if (tree == 0 || TryRead<std::uintptr_t>(tree).value_or(0) != treeTable)
        {
            continue;
        }
        const auto first = TryRead<std::uintptr_t>(tree + kNodesBegin).value_or(0);
        const auto last = TryRead<std::uintptr_t>(tree + kNodesEnd).value_or(0);
        for (std::uintptr_t node = first; first != 0 && node + kNodeSize <= last; node += kNodeSize)
        {
            *reinterpret_cast<std::uint16_t *>(node + kNodePlace) = place;
            *reinterpret_cast<std::uint16_t *>(node + kNodeLastPlace) = place;
            any = true;
        }
    }
    return any;
}

std::optional<float> Rate(ObjectId creature)
{
    CCCreature *object = CreatureOf(creature);
    const std::uintptr_t model = object != nullptr ? ModelOf(object) : 0;
    if (model == 0)
    {
        return std::nullopt;
    }
    return TryRead<float>(model + kModelRate);
}

void OnPlayed(
    std::function<void(ObjectId creature, std::int32_t animation, float speed, float blend, bool flag)> observer)
{
    static std::once_flag once;
    std::call_once(once, [] { g_play.Attach(addr::kCCCreature_Play, &Play); });
    const std::lock_guard lock(g_playedLock);
    auto next = std::make_shared<std::vector<PlayedObserver>>(*g_played);
    next->push_back(std::move(observer));
    g_played = std::move(next);
}

} // namespace dao::animation
