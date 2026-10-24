find_package(Catch2 3 CONFIG REQUIRED)

# dao_core_tests: dao/core alone, as daolib_core is built, x86 or x64. The
# data tables generated from the game's own (dao/generated) are core's kind:
# headers of constants and signatures that read nothing of the process.
add_executable(
    dao_core_tests
    crt_reports.cpp
    test_anb.cpp
    test_angle.cpp
    test_archive_cache.cpp
    test_area.cpp
    test_campaign.cpp
    test_character.cpp
    test_code.cpp
    test_collision.cpp
    test_overlap.cpp
    test_sweep.cpp
    test_crashlog.cpp
    test_cutscene.cpp
    test_facefx.cpp
    test_stage.cpp
    test_wind.cpp
    test_ctab.cpp
    test_dazip.cpp
    test_dds_write.cpp
    test_effect.cpp
    test_erf.cpp
    test_erf_roundtrip.cpp
    test_fev.cpp
    test_fsb.cpp
    test_gdafile.cpp
    test_gdafile_roundtrip.cpp
    test_gda_schema.cpp
    test_gff3_roundtrip.cpp
    test_gff3_schema.cpp
    test_gff3.cpp
    test_gff4.cpp
    test_gff4_roundtrip.cpp
    test_gff4_walk.cpp
    test_savegame.cpp
    test_gfxmovie.cpp
    test_heldevents.cpp
    test_inflate.cpp
    test_grid.cpp
    test_walkpatch.cpp
    test_windowwatch.cpp
    test_headpreset.cpp
    test_clip.cpp
    test_ldf.cpp
    test_level.cpp
    test_model.cpp
    test_model_write.cpp
    test_materialdefinition.cpp
    test_materialobject.cpp
    test_morph.cpp
    test_ncs.cpp
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
    test_plot.cpp
    test_range.cpp
    test_readat.cpp
    test_readat.cpp
    test_relocate.cpp
    test_savemeta.cpp
    test_sky.cpp
    test_script_table.cpp
    test_talktable.cpp
    test_talktables.cpp
    test_talktable_roundtrip.cpp
)
dao_apply_warnings(dao_core_tests)
target_link_libraries(
    dao_core_tests
    PRIVATE daolib_core Catch2::Catch2WithMain shell32 ole32
)

include(Catch)
# Each test's tags are its labels too: the debug-asan preset leaves out
# "folder", the tests over every file of a folder of the game's, which took
# over an hour of an ASan gate while the debug run reads the same files
# (DAOLib #194).
catch_discover_tests(dao_core_tests ADD_TAGS_AS_LABELS)

# The rest is the library over the running game: x86 only.
if(DAO_CORE_ONLY)
    return()
endif()

# dao_tests: what of daolib can be checked in a test process with no game,
# the address library's files and reads that fault among it.
add_executable(
    dao_tests
    crt_reports.cpp
    test_fingerprint.cpp
    test_patch.cpp
    test_saferead.cpp
)
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
    # The worksheets' catalogue and typed rows are what the scanned schema says.
    add_test(
        NAME gda_schema
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/tools/gen_gda_schema.py" --check
    )
    # The templates' catalogue and views are what the scanned schema says.
    add_test(
        NAME gff3_schema
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/tools/gen_gff3_schema.py" --check
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
