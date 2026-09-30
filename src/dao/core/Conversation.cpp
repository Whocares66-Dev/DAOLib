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

std::vector<gff4::Struct> ConversationFile::Lines() const
{
    const auto root = file_.Root();
    return root ? root->Structs(kLineList) : std::vector<gff4::Struct>();
}

std::size_t ConversationFile::LineCount() const
{
    return Lines().size();
}

std::optional<ConversationLine> ConversationFile::Line(std::uint32_t index) const
{
    const auto lines = Lines();
    if (index >= lines.size())
    {
        return std::nullopt;
    }
    const gff4::Struct &line = lines[index];
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
    return out;
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
