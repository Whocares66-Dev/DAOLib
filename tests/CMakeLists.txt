find_package(Catch2 3 CONFIG REQUIRED)

add_executable(
    dao_tests
    test_code.cpp
    test_crashlog.cpp
    test_fingerprint.cpp
    test_range.cpp
    test_relocate.cpp
    test_saferead.cpp
)
dao_apply_warnings(dao_tests)
target_link_libraries(dao_tests PRIVATE daolib Catch2::Catch2WithMain)

include(Catch)
catch_discover_tests(dao_tests)
