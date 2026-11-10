// The creature restore joins the party before restoring stats, so SetHero's surname lookup
// sees the template's class/background. Refresh the name after the original restore finishes.
// Read 2026-11-10 in the GOG exe; dev/LAST_NAME_TOKEN.md records the call order (DAOLib #328).
#include "fix/LastName.h"

#include "dao/Addresses.h"
#include "dao/ClientExoApp.h"
#include "dao/Hook.h"
#include "dao/Log.h"

namespace fix
{

namespace
{

using LoadFn = int(__fastcall *)(dao::CCCreature *creature, int edx, void *structure);
using GetPartyFn = void *(__fastcall *)(dao::CClientExoApp * app, int edx);
using GetHeroFn = dao::ObjectId(__fastcall *)(void *party, int edx);
using SetHeroFn = void(__fastcall *)(void *party, int edx, dao::CCCreature *creature);

dao::Detour<LoadFn> g_load;
GetPartyFn g_getParty = nullptr;
GetHeroFn g_getHero = nullptr;
SetHeroFn g_setHero = nullptr;

int __fastcall Load(dao::CCCreature *creature, int edx, void *structure)
{
    const int result = g_load.Original()(creature, edx, structure);
    auto *app = dao::CClientExoApp::Get();
    if (result == 0 || app == nullptr)
    {
        return result;
    }
    void *party = g_getParty(app, 0);
    if (party != nullptr && g_getHero(party, 0) == creature->Id())
    {
        g_setHero(party, 0, creature);
    }
    return result;
}

} // namespace

void InstallLastNameFix()
{
    g_getParty = reinterpret_cast<GetPartyFn>(dao::Module::Address(dao::addr::kCClientExoApp_GetParty));
    g_getHero = reinterpret_cast<GetHeroFn>(dao::Module::Address(dao::addr::kCCParty_GetHero));
    g_setHero = reinterpret_cast<SetHeroFn>(dao::Module::Address(dao::addr::kCCParty_SetHero));
    if (g_getParty == nullptr || g_getHero == nullptr || g_setHero == nullptr ||
        !g_load.Attach(dao::addr::kCCCreature_LoadFromStruct, &Load))
    {
        spdlog::warn("fixes: the hero's surname is not refreshed after loading: the detour could not be placed "
                     "(issue #328)");
        return;
    }
    spdlog::info("fixes: the hero's surname is refreshed after loading the saved stats (issue #328)");
}

} // namespace fix
