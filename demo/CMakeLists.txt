# mvclight Windows 桌面 Demo（仅 Windows，不参与 ctest）
add_executable(mvclight_demo WIN32 demo_win32.cpp)

target_include_directories(mvclight_demo PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)

target_link_libraries(mvclight_demo PRIVATE
    mvclight_objects
    ws2_32
    user32
    gdi32
    comctl32
)

set_target_properties(mvclight_demo PROPERTIES
    OUTPUT_NAME demo_mvclight
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)
