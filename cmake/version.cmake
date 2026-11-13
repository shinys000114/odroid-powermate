# SPDX-FileCopyrightText: 2026 Hardkernel Co., Ltd.
# SPDX-License-Identifier: GPL-3.0-only

# powermate_git_version(<var>)
#
# Sets <var> to "<latest tag>-<short hash>" (e.g. v1.3.5-47e5ecd) and makes CMake
# reconfigure on its own when that can change: a commit, a checkout, or a tag being
# added, moved or deleted. Without this the version is fixed at the first configure
# and only a manual reconfigure picks up a new commit or tag.
function(powermate_git_version var)
    find_package(Git QUIET)
    set(tag "v0.0.0")
    set(hash "unknown")

    if (GIT_FOUND)
        execute_process(
                COMMAND "${GIT_EXECUTABLE}" rev-parse --absolute-git-dir --git-common-dir
                WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
                OUTPUT_VARIABLE git_dirs
                OUTPUT_STRIP_TRAILING_WHITESPACE
                RESULT_VARIABLE git_result
                ERROR_QUIET
        )
    endif ()
    if (NOT GIT_FOUND OR NOT git_result EQUAL 0)
        message(WARNING "Not a git checkout; firmware version is ${tag}-${hash}")
        set(${var} "${tag}-${hash}" PARENT_SCOPE)
        return()
    endif ()

    # In a linked worktree HEAD lives in the worktree's own git dir, while branches,
    # tags and packed-refs live in the common dir.
    string(REPLACE "\n" ";" git_dirs "${git_dirs}")
    list(GET git_dirs 0 git_dir)
    list(GET git_dirs 1 common_dir)
    get_filename_component(common_dir "${common_dir}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

    execute_process(
            COMMAND "${GIT_EXECUTABLE}" rev-parse --short HEAD
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
            OUTPUT_VARIABLE git_hash
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
    )
    execute_process(
            COMMAND "${GIT_EXECUTABLE}" describe --tags --abbrev=0
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
            OUTPUT_VARIABLE git_tag
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
    )
    if (git_hash)
        set(hash "${git_hash}")
    endif ()
    if (git_tag)
        set(tag "${git_tag}")
    endif ()

    # HEAD changes on checkout; the branch ref it points to changes on commit. A
    # loose tag being added or removed changes the mtime of refs/tags, and pack-refs
    # or gc rewrite packed-refs.
    set(watch "${git_dir}/HEAD" "${common_dir}/packed-refs" "${common_dir}/refs/tags")
    file(READ "${git_dir}/HEAD" head)
    if (head MATCHES "^ref: ([^\n]+)")
        list(APPEND watch "${common_dir}/${CMAKE_MATCH_1}")
    endif ()
    foreach (path IN LISTS watch)
        if (EXISTS "${path}")
            set_property(DIRECTORY "${CMAKE_SOURCE_DIR}" APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${path}")
        endif ()
    endforeach ()

    set(${var} "${tag}-${hash}" PARENT_SCOPE)
endfunction()
