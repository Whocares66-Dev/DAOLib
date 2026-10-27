// Example: character creation's choices (dao/CharGen.h).
//
// Each time the creation screen opens, its offers are logged as it shows them, enabled or greyed
// out, the hero's own marked; what it sees goes to example_chargen.log.
#include "dao/CharGen.h"
#include "dao/Gui.h"
#include "dao/Log.h"
#include "dao/Plugin.h"
#include "dao/String.h"

#include <spdlog/spdlog.h>

#include <string>
#include <vector>

namespace
{

namespace chargen = dao::chargen;

void LogChoices(const char *what, const std::vector<chargen::Choice> &choices)
{
    std::string line;
    for (const chargen::Choice &choice : choices)
    {
        line += " " + std::to_string(choice.id) + " " + dao::Utf8(choice.name) + (choice.enabled ? "" : " (greyed)") +
                (choice.chosen ? " (chosen)" : "") + ";";
    }
    spdlog::info("{}:{}", what, line);
}

void Log()
{
    LogChoices("genders", chargen::Genders());
    LogChoices("races", chargen::Races());
    LogChoices("classes", chargen::Classes());
    LogChoices("backgrounds", chargen::Backgrounds());
    std::string voices;
    for (const chargen::Voice &voice : chargen::Voices())
    {
        voices += " " + dao::Utf8(voice.name) + " (" + dao::Utf8(voice.soundSet) + ");";
    }
    spdlog::info("voices:{}", voices);
}

} // namespace

extern "C" __declspec(dllexport) bool __cdecl DAOLib_Load(const dao::LoaderInfo *info)
{
    dao::OpenLog("example_chargen");
    if (info == nullptr || info->apiVersion != dao::kPluginApiVersion || !info->knownExecutable)
    {
        spdlog::error("not the loader or the game this was built for");
        return false;
    }
    dao::gui::OnFullScreen([](std::wstring_view name, bool shown) {
        spdlog::info("screen {} {}", dao::Utf8(std::wstring(name)), shown);
        if (shown && chargen::Hero())
        {
            Log();
        }
    });
    return true;
}
