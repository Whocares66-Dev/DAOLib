// The engine's string, held by a plugin: its std::wstring (MSVC 2005's,
// dao/core/Msvc8.h), in its layout, filled and freed by the engine's own
// functions, since a string longer than seven characters lives on the
// engine's heap.
#include "dao/String.h"

#include "dao/Addresses.h"
#include "dao/Module.h"

#include <Windows.h>

namespace dao
{

WString::WString(std::wstring_view text)
{
    Assign(text);
}

WString::~WString()
{
    // std::wstring::~wstring: frees the heap buffer, if there is one, with
    // the engine's operator delete, and leaves the string empty.
    using Fn = void(__fastcall *)(core::WStringRep *, int);
    reinterpret_cast<Fn>(Module::Address(addr::kStdWString_Destructor))(&rep_, 0);
}

void WString::Assign(std::wstring_view text)
{
    // std::wstring::operator=(const std::wstring&), from a string borrowing
    // the text: the engine copies it and never frees or changes the source.
    const core::WStringRep source = core::WStringRep::Borrow(text);
    using Fn = core::WStringRep *(__fastcall *)(core::WStringRep *, int, const core::WStringRep *);
    reinterpret_cast<Fn>(Module::Address(addr::kStdWString_Assign))(&rep_, 0, &source);
}

std::string Utf8(std::wstring_view text)
{
    if (text.empty())
    {
        return {};
    }
    // The length goes with the pointer, so the text need not end in a 0.
    const wchar_t *characters = text.data(); // NOLINT(bugprone-suspicious-stringview-data-usage)
    const int size = static_cast<int>(text.size());
    const int bytes = WideCharToMultiByte(CP_UTF8, 0, characters, size, nullptr, 0, nullptr, nullptr);
    std::string out(static_cast<std::size_t>(bytes), '\0');
    WideCharToMultiByte(CP_UTF8, 0, characters, size, out.data(), bytes, nullptr, nullptr);
    return out;
}

std::wstring Wide(std::string_view text)
{
    if (text.empty())
    {
        return {};
    }
    const char *characters = text.data(); // NOLINT(bugprone-suspicious-stringview-data-usage)
    const int size = static_cast<int>(text.size());
    const int count = MultiByteToWideChar(CP_UTF8, 0, characters, size, nullptr, 0);
    std::wstring out(static_cast<std::size_t>(count), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, characters, size, out.data(), count);
    return out;
}

} // namespace dao
