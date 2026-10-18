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
    out.flag = check->Get<std::int32_t>(ConversationFile::kPlotFlag).value_or(-1);
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
    // The generic field holds a CUTR, a cutscene by name, or a CUT, one of its own.
    if (const auto cut = line.Child(kLineCut))
    {
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

} // namespace dao::core
