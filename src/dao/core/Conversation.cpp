#include "dao/core/Conversation.h"

namespace dao::core
{

namespace
{

PlotCheck ReadCheck(const std::optional<gff4::Struct> &check)
{
    PlotCheck out;
    if (!check)
    {
        return out;
    }
    out.plot = check->String(ConversationFile::kPlotGuid).value_or(L"");
    // LineCondPlotFlag (0x007EF740) and GetLineActionPlotFlag (0x007EFA00) read the flag with 0 for one
    // the struct lacks (0x007EF790, 0x007EFA50), and answer 0 where the line has no such struct
    // (0x007EF7B9) (read 2026-11-10 from paragon-agent's Conversation audit, checked by lib-agent).
    out.flag = check->Get<std::int32_t>(ConversationFile::kPlotFlag).value_or(0);
    out.test = check->Get<std::uint8_t>(ConversationFile::kPlotTest).value_or(0xFF);
    out.script = check->String(ConversationFile::kScript).value_or(L"");
    out.parameter = check->Get<std::int32_t>(ConversationFile::kScriptParameter).value_or(0);
    return out;
}

} // namespace

std::optional<ConversationFile> ConversationFile::Parse(std::span<const std::byte> bytes)
{
    auto file = gff4::File::Parse(bytes);
    if (!file || file->Type() != "CONV")
    {
        return std::nullopt;
    }
    return ConversationFile(std::move(*file));
}

ConversationFile::ConversationFile(gff4::File file) : file_(std::move(file))
{
    // Line() walking the whole list for each line made reading a
    // conversation of thousands of lines quadratic: a minute over the
    // game's 1,721 in a debug build (2026-10-18).
    const auto root = file_.Root();
    if (!root)
    {
        return;
    }
    for (const gff4::Struct &line : root->Structs(kLineList))
    {
        lines_.emplace_back(line.Index(), line.Offset());
    }
}

std::size_t ConversationFile::LineCount() const
{
    return lines_.size();
}

std::optional<gff4::Struct> ConversationFile::LineStruct(std::uint32_t index) const
{
    if (index >= lines_.size())
    {
        return std::nullopt;
    }
    return gff4::Struct(file_, lines_[index].first, lines_[index].second);
}

std::optional<ConversationLine> ConversationFile::Line(std::uint32_t index) const
{
    const auto found = LineStruct(index);
    if (!found)
    {
        return std::nullopt;
    }
    const gff4::Struct &line = *found;
    ConversationLine out;
    out.index = index;
    if (line.Label() == "RPLY")
    {
        out.kind = ConversationLine::Kind::Player;
    }
    else if (line.Label() != "NTRY")
    {
        return std::nullopt;
    }
    if (const auto text = line.Tlk(kLineText))
    {
        out.text = text->strref;
    }
    out.speaker = line.String(kLineSpeaker).value_or(L"");
    out.listener = line.String(kLineListener).value_or(L"");
    out.icon = line.Get<std::int32_t>(kLineIcon).value_or(0);
    out.condition = ReadCheck(line.Child(kLineCondition));
    out.action = ReadCheck(line.Child(kLineAction));
    out.children = line.Numbers(kLineChildren);
    out.fastPath = line.Get<std::uint8_t>(kLineFastPath).value_or(0) != 0;
    out.skip = line.Get<std::uint8_t>(kLineSkip).value_or(0) != 0;
    out.visibility = line.Get<std::int32_t>(kLineVisibility).value_or(0);
    out.ambient = line.Get<std::uint8_t>(kLineAmbient).value_or(0) != 0;
    out.noVoInGame = line.Get<std::uint8_t>(kLineNoVoInGame).value_or(0) != 0;
    out.revertAnimation = line.Get<std::uint8_t>(kLineRevertAnimation).value_or(0) != 0;
    out.animation = line.Get<std::uint32_t>(kLineAnimation).value_or(0xFFFFFFFF);
    out.slideShowTexture = line.String(kLineSlideShowTexture).value_or(L"");
    // The generic field holds a CUTR, a cutscene by name, or a CUT, one of its own.
    // LineHasCut (0x007EFC30) is true where the field holds either (setne at 0x007EFC59).
    if (const auto cut = line.Child(kLineCut))
    {
        out.hasCut = true;
        out.cutscene = cut->String(kLineCutResRef).value_or(L"");
        out.cutsceneInLine = cut->Label() == "CUT";
    }
    // The stage's fields are the line's own, not the cut's (dev/PLOTS.md, "A line's cut").
    out.stageName = line.String(kLineStageName).value_or(L"");
    out.stageAtCurrentLocation = line.Get<std::uint8_t>(kLineStageAtCurrentLocation).value_or(0) != 0;
    for (const gff4::Struct &entry : line.Structs(kLineActive))
    {
        out.switches.push_back({entry.String(kSwitchTag).value_or(L""),
                                static_cast<LineSwitch::Switch>(entry.Get<std::int32_t>(kSwitchValue).value_or(0))});
    }
    return out;
}

std::optional<cutscene::Cutscene> ConversationFile::LineCutscene(std::uint32_t index) const
{
    const auto line = LineStruct(index);
    const auto cut = line ? line->Child(kLineCut) : std::nullopt;
    if (!cut || cut->Label() != "CUT")
    {
        return std::nullopt;
    }
    return cutscene::CutsceneOf(*cut);
}

std::vector<std::uint32_t> ConversationFile::Starts() const
{
    const auto root = file_.Root();
    return root ? root->Numbers(kStartingList) : std::vector<std::uint32_t>();
}

std::wstring ConversationFile::VoiceBank() const
{
    const auto root = file_.Root();
    return root ? root->String(kVoiceBank).value_or(L"") : std::wstring();
}

// The end action's fields and their defaults, read in the exe 2026-10-31
// (0x007EEF90 to 0x007EF290, DAOLib #288): with no 12003 struct, or a field
// missing from it, the plot and script are empty, the flag 0, the value true
// and the parameter 0. The script's default is null in the exe (0x007EF05B);
// empty here. Field 12302 is the value written to the flag when the
// conversation ends (setnz at 0x007EEE96, then SetPlotFlag), not a test.
ConversationEnd ConversationFile::End() const
{
    ConversationEnd out;
    const auto root = file_.Root();
    const auto end = root ? root->Child(kEnd) : std::nullopt;
    if (!end)
    {
        return out;
    }
    out.plot = end->String(kPlotGuid).value_or(L"");
    out.flag = end->Get<std::int32_t>(kPlotFlag).value_or(0);
    out.value = end->Get<std::uint8_t>(kPlotTest).value_or(1) != 0;
    out.script = end->String(kScript).value_or(L"");
    out.parameter = end->Get<std::int32_t>(kScriptParameter).value_or(0);
    return out;
}

} // namespace dao::core
