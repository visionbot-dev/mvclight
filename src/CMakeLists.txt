# =============================================================
# mvclight SDK 核心构建 —— 白名单模式
# 所有编译单元必须显式列于 MVCLIGHT_WHITELIST_SOURCES。
# 黑名单路径一旦出现即 FATAL（防回归）。
# =============================================================

set(MVCLIGHT_WHITELIST_SOURCES
    # ---- 自研 Facade（业务逻辑唯一写入区）----
    light/light_api.cpp
    light/light_txpool.cpp
)

# ---- 黑名单防回归：白名单中出现以下路径直接 FATAL ----
set(MVCLIGHT_BLACKLIST_REGEX
    "(^|/)(validation|txmempool|mempooltxdb|net_processing|init)\\.cpp$"
    "(^|/)(rpc|wallet|mining|zmq|qt|http|addrman)/")

foreach(_src IN LISTS MVCLIGHT_WHITELIST_SOURCES)
    foreach(_pat IN LISTS MVCLIGHT_BLACKLIST_REGEX)
        if(_src MATCHES "${_pat}")
            message(FATAL_ERROR "BLACKLISTED source in whitelist: ${_src}")
        endif()
    endforeach()
endforeach()

# OBJECT 库：内部实现单元（测试直接链接，共享库由同一组对象构建）
add_library(mvclight_objects OBJECT ${MVCLIGHT_WHITELIST_SOURCES})

target_compile_definitions(mvclight_objects PRIVATE MVCLIGHT_BUILD_SHARED)

target_include_directories(mvclight_objects
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

if(MSVC)
    target_compile_options(mvclight_objects PRIVATE /W4 /permissive-)
else()
    target_compile_options(mvclight_objects PRIVATE -Wall -Wextra -fvisibility=hidden)
endif()

# 动态库：仅导出 mvc_light_*（符号可见性控制）
add_library(mvclight SHARED $<TARGET_OBJECTS:mvclight_objects>)

target_compile_definitions(mvclight PRIVATE MVCLIGHT_BUILD_SHARED)

target_include_directories(mvclight
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

set_target_properties(mvclight PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    OUTPUT_NAME mvclight
)
