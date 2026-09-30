#include "dao/core/Rtti.h"

#include <vector>

namespace dao::core
{

std::string UndecorateClassName(std::string_view decorated)
{
    // ".?AV" is a class, ".?AU" a struct, ".?AW4" an enum; each ends in "@@".
    constexpr std::string_view kPrefix = ".?A";
    if (!decorated.starts_with(kPrefix) || !decorated.ends_with("@@"))
    {
        return std::string(decorated);
    }
    // A template's name needs a real parser (tools/msvc_names.py is one);
    // split on '@' like the rest it would come out as nonsense, so it comes
    // back decorated, which is at least exact.
    if (decorated.substr(kPrefix.size() + 1).starts_with("?$"))
    {
        return std::string(decorated);
    }
    std::string_view body = decorated.substr(kPrefix.size());
    body.remove_suffix(2);
    if (body.starts_with("W4"))
    {
        body.remove_prefix(2);
    }
    else if (!body.empty())
    {
        body.remove_prefix(1);
    }

    std::vector<std::string_view> parts;
    while (!body.empty())
    {
        const auto at = body.find('@');
        parts.push_back(body.substr(0, at));
        if (at == std::string_view::npos)
        {
            break;
        }
        body.remove_prefix(at + 1);
    }

    std::string out;
    for (auto it = parts.rbegin(); it != parts.rend(); ++it)
    {
        if (!out.empty())
        {
            out += "::";
        }
        out += *it;
    }
    return out;
}

} // namespace dao::core
