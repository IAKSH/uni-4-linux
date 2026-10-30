find_package(ftxui CONFIG QUIET)
if(NOT TARGET ftxui::component)
    message(WARNING "FTXUI not found; skipping uni-tui (install ftxui-devel)")
    return()
endif()

add_library(unitui STATIC
    src/model.cpp
    src/ui.cpp
)
target_include_directories(unitui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(unitui PUBLIC unicore ftxui::component)
target_compile_features(unitui PUBLIC cxx_std_23)
uni_enable_warnings(unitui)

add_executable(uni_tui src/main.cpp)
target_link_libraries(uni_tui PRIVATE unitui)
target_compile_features(uni_tui PRIVATE cxx_std_23)
uni_enable_warnings(uni_tui)
set_target_properties(uni_tui PROPERTIES
    OUTPUT_NAME "uni-tui"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

install(TARGETS uni_tui RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
