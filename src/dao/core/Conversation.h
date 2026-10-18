// A conversation as its file holds it: the lines, NPC and player, each
// with its text, who speaks it, what must be true for it to be offered,
// what it does, and the lines that may follow it. dao/Dialogue.h reads the
// one the engine is playing through this.
//
// Read 2026-09-30 from the game's conversation files (.dlg in the module
// archives, 3,925 of them; tools/gff4.py --templates FILE), GFF4 of type
// "CONV" version "V0.1", with the field names xoreos gives
// (dev/gff4_fields.csv). The top struct holds the lines in a generic list
// (12002), each an NTRY, a line the NPC speaks, or an RPLY, one the player
// may choose; the lines that may open the conversation by their index
// (12000); what runs when it ends (12003); its voice bank (12004). A line's
// children (12400) are indices into the same list: the replies an NPC line
// offers, or the NPC lines that may answer a reply, and the engine takes
// the first whose condition holds (0x007F1100). The engine reads the same
// fields by the same labels (0x007F1A40 walks 12400 and tests each child's
// template for "RPLY").
//
// A line's switches and what it plays, read 2026-10-18 over the 1,721
// conversations ghilana writes out of the single-player campaign's copies
// (123,686 lines, 63,133 NTRY and 60,553 RPLY; every line has every field;
// DAOLib #174): ConversationLineAmbient (12207, U8) is 255 on 33,764 lines
// in 1,279 files and 0 on the rest; Skip (12214, U8) 255 on 388; FastPath
// (12215, U8) 1 on 2,287; NoVOIngame (12216, U8) 255 on 1,629; RevertAnim
// (12217, U8) 255 on 279; Icon (12205, S32) other than 0 on 1,395;
// Visibility (12206, S32) 1 on 1,213 and 2 on 2,787; Animation (12213, U32)
// a row of the Animations table (anim_*.gda: 820 "Talk Sad", 824
// "Headshake") on 350, 0xFFFFFFFF on the rest. ConversationLineCut (12211)
// is a generic field by reference: empty on 88,632 lines, a CUTR on 2,766,
// whose ConversationLineCutResRef (12210) names a .cut resource (1,505 of
// them the line's string reference, "390762"; the rest that and a word,
// "95427_give_amulet"), and on 32,288 a CUT, a whole cutscene held in the
// line, of the .cut file's fields and templates (dao/core/Cutscene.h; run
// times 0 to 23 seconds). bhm600_mouse_new.dlg's lines 27 to 31 are
// ambient, two of them its starts.
//
// A line's objects switched, read 2026-10-18 over 1,721 conversations
// (DAOLib #178): ConversationLineActive (12500) is a list in place of IMAP
// structs (8 bytes: ConversationKeyTag, 12102, a string; ConversationValueTag,
// 12103, S32), on the NTRY and RPLY templates at 0x58. The list is filled on
// 36,639 lines in 698 files, 240,148 entries: 239,436 of them 0, 263 1, 12 2,
// 17 3 and 420 4, the switching ones on 400 lines in 123 files; 36,250
// entries have an empty tag, 43 of them not 0. The key is an object's tag;
// the value, as lib-agent-2 read the engine (2026-10-18), activates it (1)
// or deactivates it (3) at the line's start, or does each after the line
// (2, 4), and the start's switches are applied before the line's speaker is
// resolved. bhm600_mouse_new.dlg activates bhm600cr_mouse_human, Mouse's
// human shape, on lines 1, 2 and 32 to 34 (the tag has a 0 on the others),
// and its line 102 deactivates bhm600cr_mouse after the line.
#pragma once

#include "dao/core/Cutscene.h"
#include "dao/core/Gff4.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace dao::core
{

// A plot check or action (a PLOT or ENDA struct): a plot's flag tested or
// set, or a script run with a parameter. Empty when the line has none.
struct PlotCheck
{
    std::wstring plot;        // the plot's GUID, as 32 hex digits
    std::int32_t flag = -1;   // the flag, -1 for none
    std::uint8_t test = 0xFF; // how the flag is tested, as the file has it; 255 by default
    std::wstring script;      // a script's resource name
    std::int32_t parameter = 0;

    bool Empty() const
    {
        return plot.empty() && flag == -1 && script.empty();
    }
};

// An object a line activates or deactivates (ConversationLine::switches).
struct LineSwitch
{
    // When the object is switched, and which way; the file's number.
    enum class Switch : std::int32_t
    {
        None = 0,              // left as it is
        ActivateAtStart = 1,   // activated as the line starts
        ActivateAfter = 2,     // activated once the line has played
        DeactivateAtStart = 3, // deactivated as the line starts
        DeactivateAfter = 4,   // deactivated once the line has played
    };

    std::wstring tag; // the object's tag; may be empty
    Switch value = Switch::None;
};

struct ConversationLine
{
    enum class Kind
    {
        Npc,    // NTRY
        Player, // RPLY
    };

    std::uint32_t index = 0; // in the conversation's list of lines
    Kind kind = Kind::Npc;
    std::uint32_t text = 0xFFFFFFFF; // a string reference (dao/Tlk.h)
    std::wstring speaker;            // a tag, or OWNER or PLAYER
    std::wstring listener;           // the same
    std::int32_t icon = 0;           // the reply's icon: persuade, intimidate, ...
    std::int32_t visibility = 0;     // 0, 1 or 2 as the file has it; what it changes is unread
    PlotCheck condition;             // offered only if this holds
    PlotCheck action;                // done when the line is spoken
    std::vector<std::uint32_t> children;
    bool fastPath = false;        // the engine offers a fast-path reply first (0x00690B70)
    bool skip = false;            // passed over when played (inferred from the name)
    bool ambient = false;         // spoken in the world, as a bark, not in dialogue mode (inferred from the name)
    bool noVoInGame = false;      // played without its voice-over in the game (inferred from the name)
    bool revertAnimation = false; // the speaker's animation undone when the line ends (inferred from the name)
    std::uint32_t animation = 0xFFFFFFFF; // a row of the Animations table played with the line; 0xFFFFFFFF for none
    std::wstring cutscene;                // the cutscene (.cut) the line plays, by name; empty for none
    bool cutsceneInLine = false;          // the line holds its own cutscene: ConversationFile::LineCutscene
    std::vector<LineSwitch> switches;     // objects activated or deactivated with the line, in the file's order
};

class ConversationFile
{
  public:
    // The conversation in `bytes`, which must outlive it; nothing if they
    // are not a CONV file.
    static std::optional<ConversationFile> Parse(std::span<const std::byte> bytes);

    std::size_t LineCount() const;
    std::optional<ConversationLine> Line(std::uint32_t index) const;
    // The lines the conversation may open with, first to last.
    std::vector<std::uint32_t> Starts() const;
    std::wstring VoiceBank() const;
    // Returns the cutscene a line holds in the file (ConversationLine::cutsceneInLine).
    // @return the cutscene, or nothing for a line past the last, or one that holds none
    std::optional<cutscene::Cutscene> LineCutscene(std::uint32_t index) const;

    // The field labels read, xoreos's names for them.
    static constexpr std::uint32_t kStartingList = 12000;
    static constexpr std::uint32_t kLineList = 12002;
    static constexpr std::uint32_t kEnd = 12003;
    static constexpr std::uint32_t kVoiceBank = 12004;
    static constexpr std::uint32_t kLineText = 12201;
    static constexpr std::uint32_t kLineSpeaker = 12202;
    static constexpr std::uint32_t kLineListener = 12203;
    static constexpr std::uint32_t kLineIcon = 12205;
    static constexpr std::uint32_t kLineVisibility = 12206;
    static constexpr std::uint32_t kLineAmbient = 12207;
    static constexpr std::uint32_t kLineCondition = 12208;
    static constexpr std::uint32_t kLineAction = 12209;
    static constexpr std::uint32_t kLineCutResRef = 12210;
    static constexpr std::uint32_t kLineCut = 12211;
    static constexpr std::uint32_t kLineAnimation = 12213;
    static constexpr std::uint32_t kLineSkip = 12214;
    static constexpr std::uint32_t kLineFastPath = 12215;
    static constexpr std::uint32_t kLineNoVoInGame = 12216;
    static constexpr std::uint32_t kLineRevertAnimation = 12217;
    static constexpr std::uint32_t kPlotGuid = 12300;
    static constexpr std::uint32_t kPlotFlag = 12301;
    static constexpr std::uint32_t kPlotTest = 12302;
    static constexpr std::uint32_t kScript = 12303;
    static constexpr std::uint32_t kScriptParameter = 12304;
    static constexpr std::uint32_t kLineChildren = 12400;
    static constexpr std::uint32_t kLineActive = 12500;
    static constexpr std::uint32_t kSwitchTag = 12102;
    static constexpr std::uint32_t kSwitchValue = 12103;

  private:
    explicit ConversationFile(gff4::File file);
    std::optional<gff4::Struct> LineStruct(std::uint32_t index) const;

    gff4::File file_;
    // Each line's template and offset, found once: a Struct points at the
    // file, which moves with this object, so the lines are made again from these.
    std::vector<std::pair<std::size_t, std::size_t>> lines_;
};

} // namespace dao::core
