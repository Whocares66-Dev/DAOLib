#include "dao/TypeName.h"

#include "dao/Module.h"
#include "dao/SafeRead.h"
#include "dao/core/Range.h"
#include "dao/core/Rtti.h"

#include <cstdint>
#include <cstring>

namespace dao
{

namespace
{

// Whether a pointer lies inside the exe's image, as the fingerprint sizes it.
bool InImage(std::uintptr_t address, std::size_t bytes)
{
    const auto fingerprint = Module::Fingerprint();
    if (!fingerprint)
    {
        return false;
    }
    const std::uintptr_t base = Module::Base();
    return core::Contains(base, base + fingerprint->sizeOfImage, address, bytes);
}

// Every read goes through TryRead. Until 2026-09-30 the first one, of the
// object's own first word, was a plain memcpy: a hook handed a pointer the
// engine had just freed crashed the game in it (tests/test_saferead.cpp).
std::uint32_t ReadWord(std::uintptr_t at, bool &ok)
{
    const auto value = TryRead<std::uint32_t>(at);
    ok = ok && value.has_value();
    return value.value_or(0);
}

// 32-bit MSVC layouts: pointers are absolute, not RVAs as on x64.
constexpr std::size_t kLocatorTypeDescriptor = 12; // RTTICompleteObjectLocator::pTypeDescriptor
constexpr std::size_t kTypeDescriptorName = 8;     // TypeDescriptor::name, after vftable and spare
constexpr std::size_t kMaxNameLength = 512;

} // namespace

std::string_view DecoratedTypeName(const void *object)
{
    if (object == nullptr)
    {
        return {};
    }
    // The object's first word is its vtable, whose word before is the
    // locator. Each pointer on the chain must land in the image for this to
    // be an object of the game's, and a pointer that does not is the answer
    // "not one", never a fault.
    bool ok = true;
    const auto vtable = ReadWord(reinterpret_cast<std::uintptr_t>(object), ok);
    if (!ok || !InImage(vtable, sizeof(std::uint32_t)) ||
        !InImage(vtable - sizeof(std::uint32_t), sizeof(std::uint32_t)))
    {
        return {};
    }
    const auto locator = ReadWord(vtable - sizeof(std::uint32_t), ok);
    if (!ok || !InImage(locator, kLocatorTypeDescriptor + sizeof(std::uint32_t)))
    {
        return {};
    }
    const auto descriptor = ReadWord(locator + kLocatorTypeDescriptor, ok);
    if (!ok || !InImage(descriptor, kTypeDescriptorName + 1))
    {
        return {};
    }
    const auto *name = reinterpret_cast<const char *>(descriptor + kTypeDescriptorName);
    const std::size_t room = kMaxNameLength;
    if (!InImage(descriptor + kTypeDescriptorName, room))
    {
        return {};
    }
    const std::size_t length = ::strnlen(name, room);
    if (length == room || length < 4 || std::string_view(name, 3) != ".?A")
    {
        return {};
    }
    return {name, length};
}

std::string TypeName(const void *object)
{
    return core::UndecorateClassName(DecoratedTypeName(object));
}

} // namespace dao
