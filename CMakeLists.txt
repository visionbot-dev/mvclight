cmake_minimum_required(VERSION 3.16)
project(mvclight VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 源码含 UTF-8 中文注释；MSVC 必须按 UTF-8 解析，避免 CP932 误读导致语法错乱
if(MSVC)
    add_compile_options(/utf-8)
endif()

option(BUILD_MVC_LIGHT "Build mvclight SDK" ON)
option(BUILD_MVC_LIGHT_TESTS "Build mvclight tests" ON)
option(MVCLIGHT_VENDOR_LEVELDB "Build vendored LevelDB for desktop persistence" ON)

if(MVCLIGHT_VENDOR_LEVELDB)
    set(LEVELDB_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    add_subdirectory(third_party/leveldb)
endif()

if(BUILD_MVC_LIGHT)
    add_subdirectory(src)
endif()

if(BUILD_MVC_LIGHT_TESTS AND BUILD_MVC_LIGHT)
    enable_testing()
    add_subdirectory(src/tests)
endif()
