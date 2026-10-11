#include "dao/core/Instance.h"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <string>

using dao::core::InstanceDocuments;
using dao::core::InstanceMutexName;
using dao::core::InstanceOf;
using dao::core::ValidInstanceName;

TEST_CASE("an instance's name is letters, digits, '-' and '_', 1 to 32 of them", "[instance]")
{
    CHECK(ValidInstanceName("two"));
    CHECK(ValidInstanceName("turn-based_2"));
    CHECK(ValidInstanceName(std::string(32, 'a')));
    CHECK_FALSE(ValidInstanceName(""));
    CHECK_FALSE(ValidInstanceName(std::string(33, 'a')));
    // It becomes part of a folder's name and a kernel object's.
    CHECK_FALSE(ValidInstanceName("a b"));
    CHECK_FALSE(ValidInstanceName("..\\up"));
    CHECK_FALSE(ValidInstanceName("a/b"));
    CHECK_FALSE(ValidInstanceName("Global\\x"));
    CHECK_FALSE(ValidInstanceName("caf\xC3\xA9"));
}

TEST_CASE("two spellings of a name are one instance", "[instance]")
{
    // A folder's name does not tell case apart and a mutex's does: "Two" and "two" as two
    // mutexes would be two games in one profile.
    CHECK(InstanceOf("two") == "two");
    CHECK(InstanceOf("Two") == "two");
    CHECK(InstanceOf("TURN-Based_2") == "turn-based_2");
    CHECK(InstanceMutexName(*InstanceOf("Two")) == InstanceMutexName(*InstanceOf("tWO")));
    CHECK_FALSE(InstanceOf("").has_value());
    CHECK_FALSE(InstanceOf("a b").has_value());
}

TEST_CASE("an instance's mutex is not the game's, nor another instance's", "[instance]")
{
    CHECK(InstanceMutexName("two") == L"dragonage-two");
    CHECK(InstanceMutexName("two") != dao::core::kGameMutex);
    CHECK(InstanceMutexName("two") != InstanceMutexName("three"));
}

TEST_CASE("an instance's Documents folder is its own, under the local application data", "[instance]")
{
    const std::filesystem::path local = L"C:\\Users\\someone\\AppData\\Local";
    const std::filesystem::path ours = InstanceDocuments(local, "two");
    CHECK(ours == local / L"DAOLib" / L"instances" / L"two");
    CHECK(InstanceDocuments(local, "two") != InstanceDocuments(local, "three"));
    // One instance's folder is never inside another's: the game makes BioWare\Dragon Age
    // under what it is given, and a name has no separator in it.
    CHECK(InstanceDocuments(local, "two").parent_path() == InstanceDocuments(local, "three").parent_path());
}
