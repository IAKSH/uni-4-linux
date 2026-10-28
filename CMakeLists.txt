cmake_minimum_required(VERSION 3.20)
# "uni-4-linux" = "uni (uniwill) for Linux"; the installed product is "uni".
project(uni_4_linux VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(GNUInstallDirs)
include(CTest)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

function(uni_enable_warnings target)
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
endfunction()

add_subdirectory(lib)
add_subdirectory(cli)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
