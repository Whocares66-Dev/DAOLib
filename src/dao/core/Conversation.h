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
#pragma once

#include "dao/core/Gff4.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
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
    PlotCheck condition;             // offered only if this holds
    PlotCheck action;                // done when the line is spoken
    std::vector<std::uint32_t> children;
    bool fastPath = false; // the engine offers a fast-path reply first (0x00690B70)
    bool skip = false;
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

    // The field labels read, xoreos's names for them.
    static constexpr std::uint32_t kStartingList = 12000;
    static constexpr std::uint32_t kLineList = 12002;
    static constexpr std::uint32_t kEnd = 12003;
    static constexpr std::uint32_t kVoiceBank = 12004;
    static constexpr std::uint32_t kLineText = 12201;
    static constexpr std::uint32_t kLineSpeaker = 12202;
    static constexpr std::uint32_t kLineListener = 12203;
    static constexpr std::uint32_t kLineIcon = 12205;
    static constexpr std::uint32_t kLineCondition = 12208;
    static constexpr std::uint32_t kLineAction = 12209;
    static constexpr std::uint32_t kLineSkip = 12214;
    static constexpr std::uint32_t kLineFastPath = 12215;
    static constexpr std::uint32_t kPlotGuid = 12300;
    static constexpr std::uint32_t kPlotFlag = 12301;
    static constexpr std::uint32_t kPlotTest = 12302;
    static constexpr std::uint32_t kScript = 12303;
    static constexpr std::uint32_t kScriptParameter = 12304;
    static constexpr std::uint32_t kLineChildren = 12400;

  private:
    explicit ConversationFile(gff4::File file) : file_(std::move(file))
    {
    }
    std::vector<gff4::Struct> Lines() const;

    gff4::File file_;
};

} // namespace dao::core
