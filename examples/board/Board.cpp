// Example: the Chanter's board (dao/Board.h).
//
// Each time a Chanter's board opens, the quests it offers are logged with whether each is taken;
// what it sees goes to example_board.log.
#include "dao/Board.h"
#include "dao/Gui.h"
#include "dao/Log.h"
#include "dao/Plugin.h"
#include "dao/String.h"

#include <spdlog/spdlog.h>

extern "C" __declspec(dllexport) bool __cdecl DAOLib_Load(const dao::LoaderInfo *info)
{
    dao::OpenLog("example_board");
    if (info == nullptr || info->apiVersion != dao::kPluginApiVersion || !info->knownExecutable)
    {
        spdlog::error("not the loader or the game this was built for");
        return false;
    }
    dao::gui::OnFullScreen([](std::wstring_view name, bool shown) {
        if (!shown || name != L"Chanters")
        {
            return;
        }
        for (const dao::board::Quest &quest : dao::board::Shown())
        {
            spdlog::info("{} ({}){}", dao::Utf8(quest.name), dao::Utf8(quest.plot), quest.accepted ? ", taken" : "");
        }
    });
    return true;
}
