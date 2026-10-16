find_package(Catch2 3 CONFIG REQUIRED)

# dao_core_tests: dao/core alone, as daolib_core is built, x86 or x64. The
# data tables generated from the game's own (dao/generated) are core's kind:
# headers of constants and signatures that read nothing of the process.
add_executable(
    dao_core_tests
    crt_reports.cpp
    test_angle.cpp
    test_archive_cache.cpp
    test_code.cpp
    test_crashlog.cpp
    test_dazip.cpp
    test_dds_write.cpp
    test_erf.cpp
    test_gdafile.cpp
    test_gff3.cpp
    test_gff4.cpp
    test_gfxmovie.cpp
    test_heldevents.cpp
    test_inflate.cpp
    test_level.cpp
    test_model.cpp
    test_model_write.cpp
    test_materialobject.cpp
    test_morph.cpp
    test_lightprobes.cpp
    test_input.cpp
    test_install.cpp
    test_instance.cpp
    test_msvc8.cpp
    test_optionsmovie.cpp
    test_owedtime.cpp
    test_names.cpp
    test_objects.cpp
    test_path.cpp
    test_picture.cpp
    test_range.cpp
    test_relocate.cpp
    test_savemeta.cpp
    test_sky.cpp
    test_script_table.cpp
    test_talktable.cpp
)
dao_apply_warnings(dao_core_tests)
target_link_libraries(dao_core_tests PRIVATE daolib_core Catch2::Catch2WithMain)

include(Catch)
catch_discover_tests(dao_core_tests)

# The rest is the library over the running game: x86 only.
if(DAO_CORE_ONLY)
    return()
endif()

# dao_tests: what of daolib can be checked in a test process with no game,
# the address library's files and reads that fault among it.
add_executable(dao_tests crt_reports.cpp test_fingerprint.cpp test_saferead.cpp)
dao_apply_warnings(dao_tests)
target_link_libraries(dao_tests PRIVATE daolib Catch2::Catch2WithMain)
# The address library's files, as its build packs them: the tests read them
# as dao::Module does at start-up.
target_compile_definitions(
    dao_tests
    PRIVATE DAO_ADDRESS_DATA_DIR="${DAOADDR_DATA_DIR}"
)
add_dependencies(dao_tests daoaddr_data)
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
    # Every ID in the source is the address library's for the address
    # written beside it.
    add_test(
        NAME address_ids
        COMMAND
            "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/address_ids.py"
            --check
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
