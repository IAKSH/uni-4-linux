add_library(unicore STATIC
    src/ec.cpp
    src/hid.cpp
    src/machine.cpp
    src/touchpad.cpp
)
target_include_directories(unicore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(unicore PUBLIC cxx_std_23)
uni_enable_warnings(unicore)

find_path(HIDAPI_INCLUDE_DIR NAMES hidapi/hidapi.h REQUIRED)
find_library(HIDAPI_LIBRARY NAMES hidapi-hidraw hidapi REQUIRED)
target_include_directories(unicore PRIVATE ${HIDAPI_INCLUDE_DIR})
target_link_libraries(unicore PUBLIC ${HIDAPI_LIBRARY})
