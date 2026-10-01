find_package(Catch2 3 CONFIG REQUIRED)

add_executable(
    dao_tests
    test_angle.cpp
    test_code.cpp
    test_crashlog.cpp
    test_fingerprint.cpp
    test_gff4.cpp
    test_msvc8.cpp
    test_objects.cpp
    test_range.cpp
    test_relocate.cpp
    test_saferead.cpp
    test_script_table.cpp
)
dao_apply_warnings(dao_tests)
target_link_libraries(dao_tests PRIVATE daolib Catch2::Catch2WithMain)

include(Catch)
catch_discover_tests(dao_tests)

# The Python tools' own tests (tools/test_*.py), where there is a Python.
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
    add_test(
        NAME python_tools
        COMMAND
            "${Python3_EXECUTABLE}" -m unittest discover -s
            "${PROJECT_SOURCE_DIR}/tools" -p "test_*.py"
    )
    # The generated field descriptors are what the tables say.
    add_test(
        NAME generated_fields
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/tools/gen_script_fields.py" --check
    )
    # The scripts' constants are what script.ldf and the scripts say (passes
    # with no game or no extracted toolset).
    add_test(
        NAME script_constants
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/tools/gen_script_constants.py" --check
    )
    # Every command's C++ wrapper is what script.ldf's signatures say.
    add_test(
        NAME script_api
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/tools/gen_script_api.py" --check
    )
    # The message handlers' hook points are what the table and the exe say.
    add_test(
        NAME message_handlers
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/tools/gen_message_handlers.py" --check
    )
    # CGameObject's cast slots are what the exe says (passes with no copy).
    add_test(
        NAME object_casts
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/tools/object_casts.py" --check
    )
endif()
