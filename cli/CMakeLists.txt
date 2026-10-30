find_package(CLI11 QUIET)
if(NOT TARGET CLI11::CLI11)
    find_path(CLI11_INCLUDE_DIR NAMES CLI/CLI.hpp REQUIRED)
    add_library(CLI11::CLI11 INTERFACE IMPORTED)
    set_target_properties(CLI11::CLI11 PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${CLI11_INCLUDE_DIR}")
endif()

add_executable(uni
    src/cli.cpp
    src/commands_debug.cpp
    src/commands_device.cpp
    src/commands_kbd.cpp
    src/input.cpp
    src/main.cpp
    src/util.cpp
)
target_include_directories(uni PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(uni PRIVATE unicore CLI11::CLI11)
target_compile_features(uni PRIVATE cxx_std_23)
uni_enable_warnings(uni)

# Keep the historical build/uni path for scripts and README examples.
set_target_properties(uni PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

install(TARGETS uni RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
