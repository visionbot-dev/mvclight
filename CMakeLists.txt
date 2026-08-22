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
option(BUILD_MVC_LIGHT_DEMO "Build Windows desktop visual demo" ON)
option(BUILD_MVC_LIGHT_UPSTREAM_CORE "Link upstream mvc_core (full node network core)" OFF)

# ---- 上游 mvc_core 导入库（独立构建于 third_party/microvisionchain）----
if(BUILD_MVC_LIGHT_UPSTREAM_CORE)
    set(MVC_CORE_ROOT "C:/deps/mvc_core_build" CACHE PATH "mvc_core build root")
    set(BOOST_ROOT_CACHE "C:/local/boost_1_85_0" CACHE PATH "Boost root")
    set(LIBEVENT_ROOT_CACHE "C:/deps/libevent-build" CACHE PATH "libevent build root")
    set(OPENSSL_ROOT_CACHE "C:/Program Files/OpenSSL-Win64" CACHE PATH "OpenSSL root")

    add_library(mvc_core STATIC IMPORTED GLOBAL)
    set_target_properties(mvc_core PROPERTIES
        IMPORTED_LOCATION "${MVC_CORE_ROOT}/Release/mvc_core.lib"
        INTERFACE_INCLUDE_DIRECTORIES
            "${CMAKE_CURRENT_SOURCE_DIR}/third_party/microvisionchain/src"
            "${CMAKE_CURRENT_SOURCE_DIR}/third_party/microvisionchain/src/leveldb/include"
            "${CMAKE_CURRENT_SOURCE_DIR}/third_party/microvisionchain/src/leveldb/helpers/memenv"
            "${CMAKE_CURRENT_SOURCE_DIR}/third_party/microvisionchain/src/secp256k1/include"
            "${CMAKE_CURRENT_SOURCE_DIR}/third_party/microvisionchain/src/univalue/include"
            "${OPENSSL_ROOT_CACHE}/include"
            "${BOOST_ROOT_CACHE}"
            "${LIBEVENT_ROOT_CACHE}/include"
            "${LIBEVENT_ROOT_CACHE}/../libevent-release-2.1.12-stable/include"
    )
    set_property(TARGET mvc_core PROPERTY INTERFACE_LINK_LIBRARIES
        "${MVC_CORE_ROOT}/Release/mvc_core.lib"
        "${MVC_CORE_ROOT}/src/leveldb/Release/leveldb.lib"
        "${MVC_CORE_ROOT}/src/crypto/Release/crypto.lib"
        "${MVC_CORE_ROOT}/src/secp256k1/Release/secp256k1.lib"
        "${MVC_CORE_ROOT}/src/univalue/Release/univalue.lib"
        "${LIBEVENT_ROOT_CACHE}/lib/Release/event.lib"
        "${LIBEVENT_ROOT_CACHE}/lib/Release/event_core.lib"
        "${LIBEVENT_ROOT_CACHE}/lib/Release/event_extra.lib"
        "${OPENSSL_ROOT_CACHE}/lib/VC/x64/MD/libssl.lib"
        "${OPENSSL_ROOT_CACHE}/lib/VC/x64/MD/libcrypto.lib"
        ws2_32 shlwapi bcrypt userenv
    )
endif()

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

if(BUILD_MVC_LIGHT_DEMO AND BUILD_MVC_LIGHT AND WIN32)
    add_subdirectory(demo)
endif()
