# The format and lint targets, over every language of ours in the repository:
#
#   cmake --build --preset debug --target format         rewrite files in place
#   cmake --build --preset debug --target format-check    fail if anything is unformatted
#   cmake --build --preset debug --target tidy            static analysis
#
#   C++         clang-format, clang-tidy
#   Python      ruff format, ruff check
#   PowerShell  PSScriptAnalyzer's Invoke-Formatter and Invoke-ScriptAnalyzer
#   CMake       gersemi, a formatter only
#
# What clang-tidy checks is what the build compiles, under the including
# project's src/, tests/ and examples/: a plugin's repository that includes
# this file has its tests linted as DAOLib's are, with nothing to set
# (DAOLib #34).
#
# Each formatter runs its stock style, and each linter the checks that find
# bugs, costs and code that reads worse than it need, but no matter of taste
# (DAOLib #33): .clang-format, .clang-tidy, ruff.toml and .gersemirc say
# which; PSScriptAnalyzer runs its defaults.
#
# clang-format and clang-tidy are LLVM's own release, 22 or later
# (`winget install LLVM.LLVM`, into Program Files\LLVM, or where LLVM_ROOT
# says), not the older one Visual Studio bundles, which is searched nowhere:
# clang-tidy 22 walks none of a system header's declarations, which is most
# of a file's cost. ruff and gersemi are `pip install --user`, and
# PSScriptAnalyzer `Install-Module PSScriptAnalyzer -Scope CurrentUser`. A
# tool that is missing is said at configure time, and its language skipped.

set(_llvm_hints "$ENV{LLVM_ROOT}/bin" "$ENV{ProgramFiles}/LLVM/bin")

find_program(
    DAO_CLANG_FORMAT
    NAMES clang-format
    HINTS ${_llvm_hints}
    NO_DEFAULT_PATH
)
find_program(
    DAO_CLANG_TIDY
    NAMES clang-tidy
    HINTS ${_llvm_hints}
    NO_DEFAULT_PATH
)

# ruff by `python -m`: pip's --user install puts ruff.exe in a Scripts folder
# that is not on PATH, so find_program would miss it.
set(DAO_RUFF "")
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
    execute_process(
        COMMAND "${Python3_EXECUTABLE}" -m ruff --version
        RESULT_VARIABLE _ruff_result
        OUTPUT_VARIABLE _ruff_version
        ERROR_QUIET
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(_ruff_result EQUAL 0)
        set(DAO_RUFF "${Python3_EXECUTABLE}" -m ruff)
        message(STATUS "ruff: ${_ruff_version} (${Python3_EXECUTABLE})")
    endif()
endif()
if(NOT DAO_RUFF)
    message(
        STATUS
        "ruff: NOT FOUND -- Python is not formatted or linted; pip install --user ruff"
    )
endif()

# gersemi the same way. It walks into build/ if given a folder, so it is
# given git's list instead.
set(DAO_GERSEMI "")
if(Python3_Interpreter_FOUND)
    execute_process(
        COMMAND "${Python3_EXECUTABLE}" -m gersemi --version
        RESULT_VARIABLE _gersemi_result
        OUTPUT_VARIABLE _gersemi_version
        ERROR_QUIET
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(_gersemi_result EQUAL 0)
        set(DAO_GERSEMI "${Python3_EXECUTABLE}" -m gersemi)
        message(STATUS "gersemi: ${_gersemi_version}")
    endif()
endif()
if(DAO_GERSEMI)
    execute_process(
        COMMAND
            git ls-files --cached --others --exclude-standard -- "*.cmake"
            "*CMakeLists.txt"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        RESULT_VARIABLE _git_result
        OUTPUT_VARIABLE DAO_CMAKE_FILES
        ERROR_QUIET
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(NOT _git_result EQUAL 0 OR NOT DAO_CMAKE_FILES)
        message(
            STATUS
            "gersemi: no CMake files from git -- CMake is not formatted"
        )
        set(DAO_GERSEMI "")
    else()
        string(REPLACE "\n" ";" DAO_CMAKE_FILES "${DAO_CMAKE_FILES}")
    endif()
else()
    message(
        STATUS
        "gersemi: NOT FOUND -- CMake is not formatted; pip install --user gersemi"
    )
endif()
unset(_gersemi_result)
unset(_gersemi_version)
unset(_git_result)

set(DAO_PSCHECK "")
find_program(DAO_PWSH NAMES pwsh)
if(DAO_PWSH)
    execute_process(
        COMMAND
            "${DAO_PWSH}" -NoProfile -NonInteractive -Command
            "if (-not (Get-Module -ListAvailable PSScriptAnalyzer)) { exit 1 }"
        RESULT_VARIABLE _pssa_result
        OUTPUT_QUIET
        ERROR_QUIET
    )
    if(_pssa_result EQUAL 0)
        set(DAO_PSCHECK
            "${DAO_PWSH}"
            -NoProfile
            -NonInteractive
            -File
            "${CMAKE_SOURCE_DIR}/tools/check-powershell.ps1"
        )
        message(STATUS "PSScriptAnalyzer: found (${DAO_PWSH})")
    endif()
endif()
if(NOT DAO_PSCHECK)
    message(
        STATUS
        "PSScriptAnalyzer: NOT FOUND -- PowerShell is not formatted or linted; "
        "Install-Module PSScriptAnalyzer -Scope CurrentUser, under pwsh 7"
    )
endif()
unset(_ruff_result)
unset(_ruff_version)
unset(_pssa_result)

# Our own sources only. Never glob the build tree: it holds vcpkg headers,
# and reformatting those would be both wrong and enormous.
file(
    GLOB_RECURSE DAO_SOURCES
    CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_SOURCE_DIR}/src/*.h"
    "${CMAKE_SOURCE_DIR}/tests/*.cpp"
    "${CMAKE_SOURCE_DIR}/tests/*.h"
    "${CMAKE_SOURCE_DIR}/examples/*.cpp"
    "${CMAKE_SOURCE_DIR}/examples/*.h"
)

set(DAO_FORMAT_COMMANDS "")
set(DAO_FORMAT_CHECK_COMMANDS "")
if(DAO_CLANG_FORMAT)
    message(STATUS "clang-format: ${DAO_CLANG_FORMAT}")
    list(
        APPEND DAO_FORMAT_COMMANDS
        COMMAND
        "${DAO_CLANG_FORMAT}"
        -i
        --style=file
        ${DAO_SOURCES}
    )
    # -Werror turns "would reformat" into a non-zero exit, which is what makes
    # this usable as a gate rather than a report.
    list(
        APPEND DAO_FORMAT_CHECK_COMMANDS
        COMMAND
        "${DAO_CLANG_FORMAT}"
        --dry-run
        -Werror
        --style=file
        ${DAO_SOURCES}
    )
else()
    message(STATUS "clang-format: NOT FOUND -- C++ is not formatted")
endif()
if(DAO_RUFF)
    list(APPEND DAO_FORMAT_COMMANDS COMMAND ${DAO_RUFF} format --quiet)
    list(APPEND DAO_FORMAT_CHECK_COMMANDS COMMAND ${DAO_RUFF} format --check)
endif()
if(DAO_GERSEMI)
    list(
        APPEND DAO_FORMAT_COMMANDS
        COMMAND
        ${DAO_GERSEMI}
        --in-place
        --quiet
        ${DAO_CMAKE_FILES}
    )
    list(
        APPEND DAO_FORMAT_CHECK_COMMANDS
        COMMAND
        ${DAO_GERSEMI}
        --check
        ${DAO_CMAKE_FILES}
    )
endif()
if(DAO_PSCHECK)
    list(APPEND DAO_FORMAT_COMMANDS COMMAND ${DAO_PSCHECK} -Mode Format)
    list(
        APPEND DAO_FORMAT_CHECK_COMMANDS
        COMMAND
        ${DAO_PSCHECK}
        -Mode
        FormatCheck
    )
endif()
if(DAO_FORMAT_COMMANDS)
    add_custom_target(
        format
        ${DAO_FORMAT_COMMANDS}
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        COMMENT "Formatting C++, Python, PowerShell and CMake"
        VERBATIM
    )
    add_custom_target(
        format-check
        ${DAO_FORMAT_CHECK_COMMANDS}
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        COMMENT "Checking the formatting of C++, Python, PowerShell and CMake"
        VERBATIM
    )
endif()

# The C++ sources the including project's targets compile, under its own
# src/, tests/ and examples/: every directory's targets, the subdirectories'
# too (a plugin's DAOLib among them, whose sources lie outside those three
# and are left out).
function(_dao_built_sources out)
    set(result "")
    set(dirs "${CMAKE_SOURCE_DIR}")
    while(dirs)
        list(POP_FRONT dirs dir)
        get_property(subdirs DIRECTORY "${dir}" PROPERTY SUBDIRECTORIES)
        list(APPEND dirs ${subdirs})
        get_property(targets DIRECTORY "${dir}" PROPERTY BUILDSYSTEM_TARGETS)
        foreach(target IN LISTS targets)
            get_target_property(type ${target} TYPE)
            if(type STREQUAL "INTERFACE_LIBRARY" OR type STREQUAL "UTILITY")
                continue()
            endif()
            get_target_property(sources ${target} SOURCES)
            get_target_property(base ${target} SOURCE_DIR)
            foreach(source IN LISTS sources)
                if(NOT source MATCHES "\\.cpp$" OR source MATCHES "\\$<")
                    continue()
                endif()
                cmake_path(
                    ABSOLUTE_PATH source
                    BASE_DIRECTORY "${base}"
                    NORMALIZE
                )
                foreach(root IN ITEMS src tests examples)
                    string(FIND "${source}" "${CMAKE_SOURCE_DIR}/${root}/" at)
                    if(at EQUAL 0)
                        list(APPEND result "${source}")
                    endif()
                endforeach()
            endforeach()
        endforeach()
    endwhile()
    list(REMOVE_DUPLICATES result)
    list(SORT result)
    set(${out} "${result}" PARENT_SCOPE)
endfunction()

# The lint target, made once the including project has defined all its
# targets, so that their sources are known (cmake_language DEFER).
function(_dao_quality_tidy)
    if(DAO_CLANG_TIDY)
        message(STATUS "clang-tidy: ${DAO_CLANG_TIDY}")

        # One custom command per file, not one command over all of them: the
        # files run in parallel across cores, and a stamp per file means an
        # unchanged file is not checked again. The stamps deliberately do NOT
        # depend on the compile database, which is rewritten on every configure.
        # Delete build/<preset>/tidy to force a full pass.
        # What is linted is what the build compiles: every C++ source of a
        # target the including project defines, under its own src/, tests/ and
        # examples/ (DAOLib #34). Not a glob, since clang-tidy needs a file's
        # entry in the compile database and a file no target builds has none;
        # and not DAOLib's own build options, which a plugin's repository that
        # includes this file never sets, so its tests went unlinted.
        _dao_built_sources(DAO_TIDY_SOURCES)
        file(
            GLOB_RECURSE DAO_TIDY_HEADERS
            CONFIGURE_DEPENDS
            "${CMAKE_SOURCE_DIR}/src/*.h"
            "${CMAKE_SOURCE_DIR}/tests/*.h"
            "${CMAKE_SOURCE_DIR}/examples/*.h"
        )

        file(
            GLOB DAO_TIDY_CONFIGS
            "${CMAKE_SOURCE_DIR}/.clang-tidy"
            "${CMAKE_SOURCE_DIR}/src/.clang-tidy"
            "${CMAKE_SOURCE_DIR}/tests/.clang-tidy"
            "${CMAKE_SOURCE_DIR}/examples/.clang-tidy"
        )

        # Reports from any header under this tree's src/, tests/ or examples/, by absolute
        # path, so a vendored library's src/ never matches. clang-tidy prints
        # mixed separators (src\dao/Log.h), hence either slash.
        set(DAO_TIDY_HEADER_FILTER "${CMAKE_SOURCE_DIR}")
        foreach(
            _c
            .
            +
            *
            ?
            ^
            $
            "("
            ")"
            "|"
            "{"
            "}"
            "["
        )
            string(
                REPLACE "${_c}"
                "\\${_c}"
                DAO_TIDY_HEADER_FILTER
                "${DAO_TIDY_HEADER_FILTER}"
            )
        endforeach()
        string(APPEND DAO_TIDY_HEADER_FILTER "/(src|tests|examples)/")
        string(
            REPLACE "/"
            "[/\\]"
            DAO_TIDY_HEADER_FILTER
            "${DAO_TIDY_HEADER_FILTER}"
        )
        unset(_c)

        set(DAO_TIDY_STAMPS "")
        file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/tidy")
        foreach(_src IN LISTS DAO_TIDY_SOURCES)
            file(RELATIVE_PATH _rel "${CMAKE_SOURCE_DIR}" "${_src}")
            string(REPLACE "/" "_" _stampname "${_rel}")
            set(_stamp "${CMAKE_BINARY_DIR}/tidy/${_stampname}.stamp")
            # The compile database is MSVC's; clang takes /Zc:preprocessor and
            # /external:anglebrackets without using them, and under
            # WarningsAsErrors that note would fail every file. And it is a
            # 32-bit build: cl.exe's command line does not say so (the x86
            # cl.exe is the whole of it), so clang would assume the host's
            # x86_64 and reject the proxy's naked functions; the target is
            # named outright.
            add_custom_command(
                OUTPUT "${_stamp}"
                COMMAND
                    "${DAO_CLANG_TIDY}" -p "${CMAKE_BINARY_DIR}"
                    "--header-filter=${DAO_TIDY_HEADER_FILTER}"
                    --extra-arg=--target=i386-pc-windows-msvc
                    --extra-arg=-Wno-unused-command-line-argument "${_src}"
                COMMAND "${CMAKE_COMMAND}" -E touch "${_stamp}"
                DEPENDS "${_src}" ${DAO_TIDY_HEADERS} ${DAO_TIDY_CONFIGS}
                COMMENT "clang-tidy ${_rel}"
                VERBATIM
            )
            list(APPEND DAO_TIDY_STAMPS "${_stamp}")
        endforeach()
        unset(_src)
        unset(_rel)
        unset(_stampname)
        unset(_stamp)
    else()
        set(DAO_TIDY_STAMPS "")
        message(STATUS "clang-tidy: NOT FOUND -- C++ is not linted")
    endif()

    # ruff check runs every time: it takes well under a second. PSScriptAnalyzer
    # takes seconds to load, so its script keeps a stamp and returns at once while
    # no script has changed.
    set(DAO_LINT_COMMANDS "")
    if(DAO_RUFF)
        list(APPEND DAO_LINT_COMMANDS COMMAND ${DAO_RUFF} check --quiet)
    endif()
    if(DAO_PSCHECK)
        list(
            APPEND DAO_LINT_COMMANDS
            COMMAND
            ${DAO_PSCHECK}
            -Mode
            Lint
            -Stamp
            "${CMAKE_BINARY_DIR}/tidy/powershell.stamp"
        )
    endif()
    if(DAO_TIDY_STAMPS OR DAO_LINT_COMMANDS)
        file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/tidy")
        add_custom_target(
            tidy
            ${DAO_LINT_COMMANDS}
            DEPENDS ${DAO_TIDY_STAMPS}
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
            VERBATIM
        )
    endif()
endfunction()
cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL _dao_quality_tidy)

unset(_llvm_hints)
