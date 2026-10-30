# Generate uni/version.hpp.
#
# Precedence:
#   1. UNI_VERSION_OVERRIDE
#   2. git describe (tag / tag-distance / commit hash / dirty)
#   3. <project version>+unknown
#
# We intentionally do not claim a bare release version when no VCS metadata is
# available: a source tarball may not correspond exactly to the tagged commit.

set(UNI_VERSION_OVERRIDE "" CACHE STRING
    "Override the generated uni version string")

set(UNI_VERSION_STRING "${PROJECT_VERSION}+unknown")
set(UNI_GIT_HASH "")
set(UNI_GIT_TAG "")

if(UNI_VERSION_OVERRIDE)
    set(UNI_VERSION_STRING "${UNI_VERSION_OVERRIDE}")
else()
    find_package(Git QUIET)
    # Only inspect the repository rooted at this source tree.  Without this
    # check, a source archive extracted inside another checkout could inherit
    # that parent repository's version metadata.
    if(GIT_FOUND AND EXISTS "${CMAKE_SOURCE_DIR}/.git")
        execute_process(
            COMMAND "${GIT_EXECUTABLE}" describe --tags --always --dirty
                    --abbrev=7 --match "v[0-9]*"
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
            OUTPUT_VARIABLE _uni_git_describe
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
            RESULT_VARIABLE _uni_git_result
        )

        if(_uni_git_result EQUAL 0 AND _uni_git_describe)
            execute_process(
                COMMAND "${GIT_EXECUTABLE}" rev-parse --short=7 HEAD
                WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
                OUTPUT_VARIABLE UNI_GIT_HASH
                OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_QUIET
            )
            execute_process(
                COMMAND "${GIT_EXECUTABLE}" describe --tags --exact-match
                        --match "v[0-9]*"
                WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
                OUTPUT_VARIABLE UNI_GIT_TAG
                OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_QUIET
            )

            if(_uni_git_describe MATCHES "^v(.+)$")
                set(UNI_VERSION_STRING "${CMAKE_MATCH_1}")
            elseif(_uni_git_describe MATCHES "^(.*)-dirty$")
                set(UNI_VERSION_STRING
                    "${PROJECT_VERSION}+g${CMAKE_MATCH_1}-dirty")
            else()
                set(UNI_VERSION_STRING
                    "${PROJECT_VERSION}+g${_uni_git_describe}")
            endif()
        endif()
    endif()
endif()

file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/generated/include/uni")
configure_file(
    "${CMAKE_SOURCE_DIR}/cmake/version.hpp.in"
    "${CMAKE_BINARY_DIR}/generated/include/uni/version.hpp"
    @ONLY
)
