#include "dao/Party.h"

#include "dao/ClientExoApp.h"
#include "dao/SafeRead.h"
#include "dao/core/Msvc8.h"

#include <cstddef>
#include <cstdint>

namespace dao::party
{

namespace
{
constexpr std::size_t kPartyHolder = 0xD0; // in the application
constexpr std::size_t kParty = 0x58;       // in what it holds
constexpr std::size_t kSelection = 0x184;  // in the party: a std::vector of ids
} // namespace

std::vector<ObjectId> Selected()
{
    const CClientExoApp *app = CClientExoApp::Get();
    if (app == nullptr)
    {
        return {};
    }
    const auto holder = TryRead<std::uintptr_t>(reinterpret_cast<std::uintptr_t>(app) + kPartyHolder);
    const auto party = holder && *holder != 0 ? TryRead<std::uintptr_t>(*holder + kParty) : std::nullopt;
    if (!party || *party == 0)
    {
        return {};
    }
    const auto selection = TryRead<core::VectorRep<ObjectId>>(*party + kSelection);
    if (!selection)
    {
        return {};
    }
    const auto ids = selection->View();
    if (ids.size() > 64)
    {
        return {}; // not a party's selection
    }
    return {ids.begin(), ids.end()};
}

} // namespace dao::party
