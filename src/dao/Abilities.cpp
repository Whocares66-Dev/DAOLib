#include "dao/Abilities.h"

#include "dao/Gda.h"
#include "dao/Tlk.h"

namespace dao::abilities
{

namespace
{
constexpr auto kTable = static_cast<script::Table>(script::constants::TABLE_ABILITIES_SPELLS);
} // namespace

std::optional<std::wstring> Label(std::int32_t ability)
{
    return gda::GetString(kTable, ability, "Label");
}

std::optional<std::wstring> Name(std::int32_t ability)
{
    const auto strref = gda::GetInt(kTable, ability, "NameStrRef");
    if (strref && *strref > 0)
    {
        if (std::wstring text = tlk::String(static_cast<std::uint32_t>(*strref)); !text.empty())
        {
            return text;
        }
    }
    return Label(ability);
}

} // namespace dao::abilities
