cmake_minimum_required(VERSION 3.20)
project(uni_4_linux VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(GNUInstallDirs)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

add_library(uni4core STATIC
    src/ec.cpp
    src/input.cpp
    src/touchpad.cpp
    src/util.cpp
)
target_include_directories(uni4core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_options(uni4core PRIVATE -Wall -Wextra -Wpedantic)

find_path(HIDAPI_INCLUDE_DIR NAMES hidapi/hidapi.h REQUIRED)
find_library(HIDAPI_LIBRARY NAMES hidapi-hidraw hidapi REQUIRED)
target_include_directories(uni4core PRIVATE ${HIDAPI_INCLUDE_DIR})
target_link_libraries(uni4core PUBLIC ${HIDAPI_LIBRARY})

find_package(CLI11 QUIET)
if(NOT TARGET CLI11::CLI11)
    find_path(CLI11_INCLUDE_DIR NAMES CLI/CLI.hpp REQUIRED)
    add_library(CLI11::CLI11 INTERFACE IMPORTED)
    set_target_properties(CLI11::CLI11 PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${CLI11_INCLUDE_DIR}")
endif()

add_executable(uni4 src/main.cpp)
target_link_libraries(uni4 PRIVATE uni4core CLI11::CLI11)
target_compile_options(uni4 PRIVATE -Wall -Wextra -Wpedantic)
target_compile_definitions(uni4 PRIVATE UNI4_VERSION="${PROJECT_VERSION}")

install(TARGETS uni4 RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
