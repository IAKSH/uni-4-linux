cmake_minimum_required(VERSION 3.20)
# "uni-4-linux" = "uni (uniwill) for Linux"; the installed product is "uni".
project(uni_4_linux VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(GNUInstallDirs)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

add_library(unicore STATIC
    src/ec.cpp
    src/hid.cpp
    src/input.cpp
    src/touchpad.cpp
    src/util.cpp
)
target_include_directories(unicore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_options(unicore PRIVATE -Wall -Wextra -Wpedantic)

find_path(HIDAPI_INCLUDE_DIR NAMES hidapi/hidapi.h REQUIRED)
find_library(HIDAPI_LIBRARY NAMES hidapi-hidraw hidapi REQUIRED)
target_include_directories(unicore PRIVATE ${HIDAPI_INCLUDE_DIR})
target_link_libraries(unicore PUBLIC ${HIDAPI_LIBRARY})

find_package(CLI11 QUIET)
if(NOT TARGET CLI11::CLI11)
    find_path(CLI11_INCLUDE_DIR NAMES CLI/CLI.hpp REQUIRED)
    add_library(CLI11::CLI11 INTERFACE IMPORTED)
    set_target_properties(CLI11::CLI11 PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${CLI11_INCLUDE_DIR}")
endif()

add_executable(uni src/main.cpp)
target_link_libraries(uni PRIVATE unicore CLI11::CLI11)
target_compile_options(uni PRIVATE -Wall -Wextra -Wpedantic)
target_compile_definitions(uni PRIVATE UNI_VERSION="${PROJECT_VERSION}")

include(CTest)
if(BUILD_TESTING)
    find_package(doctest QUIET)
    if(NOT TARGET doctest::doctest)
        find_path(DOCTEST_INCLUDE_DIR NAMES doctest/doctest.h)
        if(DOCTEST_INCLUDE_DIR)
            add_library(doctest::doctest INTERFACE IMPORTED)
            set_target_properties(doctest::doctest PROPERTIES
                INTERFACE_INCLUDE_DIRECTORIES "${DOCTEST_INCLUDE_DIR}")
        endif()
    endif()

    if(TARGET doctest::doctest)
        add_executable(uni_tests tests/tests.cpp)
        target_link_libraries(uni_tests PRIVATE unicore doctest::doctest)
        target_compile_options(uni_tests PRIVATE -Wall -Wextra -Wpedantic)
        add_test(NAME uni_tests COMMAND uni_tests)
    else()
        message(STATUS "doctest not found; unit tests disabled")
    endif()
endif()

install(TARGETS uni RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
