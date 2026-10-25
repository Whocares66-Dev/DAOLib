// Example: crafting's recipes (dao/Crafting.h).
//
// Each time the crafting screen opens, the party's recipes of each craft are logged with what each
// needs and has, and whether the controlled creature could make it; what it sees goes to
// example_crafting.log.
#include "dao/Crafting.h"
#include "dao/Gui.h"
#include "dao/Log.h"
#include "dao/Plugin.h"
#include "dao/String.h"
#include "dao/generated/ScriptApi.h"

#include <spdlog/fmt/fmt.h>
#include <spdlog/spdlog.h>

#include <string>
#include <string_view>

namespace
{

namespace api = dao::script::api;
namespace crafting = dao::crafting;

void Log()
{
    const dao::ObjectId who = api::GetMainControlled();
    for (const auto craft :
         {crafting::Craft::Traps, crafting::Craft::Herbalism, crafting::Craft::Poisons, crafting::Craft::Runes})
    {
        for (const crafting::Recipe &recipe : crafting::Recipes(craft, who))
        {
            std::string needs;
            for (const crafting::Ingredient &ingredient : recipe.ingredients)
            {
                needs += fmt::format(" {} {}/{}", dao::Utf8(ingredient.tag), ingredient.have, ingredient.need);
            }
            std::string_view verdict;
            if (recipe.makeable)
            {
                verdict = ", makeable";
            }
            else if (!recipe.rank)
            {
                verdict = ", not the rank";
            }
            spdlog::info("{} {}: {}{}{}", static_cast<int>(craft), recipe.id, dao::Utf8(recipe.product), needs,
                         verdict);
        }
    }
}

} // namespace

extern "C" __declspec(dllexport) bool __cdecl DAOLib_Load(const dao::LoaderInfo *info)
{
    dao::OpenLog("example_crafting");
    if (info == nullptr || info->apiVersion != dao::kPluginApiVersion || !info->knownExecutable)
    {
        spdlog::error("not the loader or the game this was built for");
        return false;
    }
    dao::gui::OnFullScreen([](std::wstring_view name, bool shown) {
        if (shown && name == L"Crafting")
        {
            Log();
        }
    });
    return true;
}
