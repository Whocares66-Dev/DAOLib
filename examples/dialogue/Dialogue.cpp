// Example: conversations (dao/Dialogue.h).
//
// Talk to anyone, and the log follows the conversation: each NPC line with
// its speaker, the replies offered in their order on the screen, and the
// replies the conversation had but did not offer, because their condition
// failed, with the plot flag or script that decided it. The verdicts are
// left as the engine gave them; a plugin that returned something else from
// OnLineAvailable would hide or offer those lines instead.
//
// The text comes from the talk tables (dao/Tlk.h), in the player's
// language. What it sees goes to example_dialogue.log.
#include "dao/Dialogue.h"
#include "dao/Log.h"
#include "dao/Plugin.h"
#include "dao/Tlk.h"

#include <spdlog/spdlog.h>

#include <string>

namespace
{

// Talk-table text as UTF-8 for the log.
std::string Utf8(const std::wstring &text)
{
    std::string out;
    for (const wchar_t c : text)
    {
        const auto u = static_cast<std::uint32_t>(c);
        if (u < 0x80)
        {
            out += static_cast<char>(u);
        }
        else if (u < 0x800)
        {
            out += static_cast<char>(0xC0 | (u >> 6));
            out += static_cast<char>(0x80 | (u & 0x3F));
        }
        else
        {
            out += static_cast<char>(0xE0 | (u >> 12));
            out += static_cast<char>(0x80 | ((u >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (u & 0x3F));
        }
    }
    return out;
}

std::string Text(const dao::dialogue::Line &line)
{
    return Utf8(dao::tlk::String(line.text));
}

std::string Condition(const dao::core::PlotCheck &check)
{
    if (!check.script.empty())
    {
        return "script " + Utf8(check.script) + " (" + std::to_string(check.parameter) + ")";
    }
    return "plot " + Utf8(check.plot) + " flag " + std::to_string(check.flag);
}

} // namespace

extern "C" __declspec(dllexport) bool __cdecl DAOLib_Load(const dao::LoaderInfo *info)
{
    dao::OpenLog("example_dialogue");
    if (info == nullptr || info->apiVersion != dao::kPluginApiVersion || !info->knownExecutable)
    {
        spdlog::error("not the loader or the game this was built for");
        return false;
    }
    dao::dialogue::OnNpcLine([](dao::dialogue::Conversation &, const dao::dialogue::Line &line) {
        spdlog::info("[{}] {} (line {})", Utf8(line.speaker), Text(line), line.index);
    });
    dao::dialogue::OnReplies([](dao::dialogue::Conversation &, std::span<const dao::dialogue::Line> replies) {
        for (std::size_t i = 0; i < replies.size(); ++i)
        {
            spdlog::info("  {}. {} (line {}, icon {})", i + 1, Text(replies[i]), replies[i].index, replies[i].icon);
        }
    });
    dao::dialogue::OnReplyChosen([](dao::dialogue::Conversation &, const dao::dialogue::Line &reply) {
        spdlog::info("  chose: {} (line {})", Text(reply), reply.index);
    });
    dao::dialogue::OnLineAvailable([](dao::dialogue::Conversation &, const dao::dialogue::Line &line, bool available) {
        if (!available && line.kind == dao::dialogue::Line::Kind::Player)
        {
            spdlog::info("  (not offered: {} -- {})", Text(line), Condition(line.condition));
        }
        return available;
    });
    return true;
}
