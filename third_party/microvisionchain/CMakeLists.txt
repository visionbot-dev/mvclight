# mvc_core：直接编译上游网络/验证/交易池内核（自写 CMake，不依赖上游残缺 CMake）
cmake_minimum_required(VERSION 3.16)
project(microvisionchain-vendored)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

if(MSVC)
    add_compile_definitions(NOMINMAX _HAS_STD_BYTE=0)
    add_compile_options(/MP /wd4146 /wd4244 /wd4267 /wd4996 /wd4018 /wd4305 /UNDEBUG)
endif()

# ---- 依赖 ----
find_package(OpenSSL REQUIRED)
find_package(Boost 1.59 REQUIRED chrono filesystem program_options thread)

# libevent（无官方 FindEvent，使用 LIBEVENT_ROOT 导入）
set(LIBEVENT_ROOT "" CACHE PATH "libevent build/install root (containing lib/Release/*.lib and include/)")
if(NOT LIBEVENT_ROOT)
    set(LIBEVENT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../deps/libevent-build")
endif()
set(LIBEVENT_SRC "${LIBEVENT_ROOT}/../libevent-release-2.1.12-stable")
if(NOT EXISTS "${LIBEVENT_ROOT}/lib/Release/event_core.lib")
    message(FATAL_ERROR "libevent not found at ${LIBEVENT_ROOT}; set -DLIBEVENT_ROOT=...")
endif()
add_library(event STATIC IMPORTED)
set_target_properties(event PROPERTIES
    IMPORTED_LOCATION "${LIBEVENT_ROOT}/lib/Release/event.lib"
    INTERFACE_INCLUDE_DIRECTORIES "${LIBEVENT_SRC}/include;${LIBEVENT_ROOT}/include")
add_library(event_core STATIC IMPORTED)
set_target_properties(event_core PROPERTIES
    IMPORTED_LOCATION "${LIBEVENT_ROOT}/lib/Release/event_core.lib"
    INTERFACE_INCLUDE_DIRECTORIES "${LIBEVENT_SRC}/include;${LIBEVENT_ROOT}/include")
add_library(event_extra STATIC IMPORTED)
set_target_properties(event_extra PROPERTIES
    IMPORTED_LOCATION "${LIBEVENT_ROOT}/lib/Release/event_extra.lib"
    INTERFACE_INCLUDE_DIRECTORIES "${LIBEVENT_SRC}/include;${LIBEVENT_ROOT}/include")

# ---- 上游子库 ----
set(BUILD_MVC_WALLET OFF CACHE BOOL "" FORCE)
set(BUILD_MVC_ZMQ OFF CACHE BOOL "" FORCE)
set(UNIVALUE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(LEVELDB_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(SECP256K1_ECMULT_STATIC_PRECOMPUTATION OFF CACHE BOOL "" FORCE)
set(SECP256K1_ENABLE_MODULE_MULTISET OFF CACHE BOOL "" FORCE)
set(SECP256K1_ENABLE_ENDOMORPHISM OFF CACHE BOOL "" FORCE)
set(SECP256K1_BUILD_TEST OFF CACHE BOOL "" FORCE)

add_subdirectory(src/config)
add_subdirectory(src/crypto)
add_subdirectory(src/leveldb)
add_subdirectory(src/secp256k1)
add_subdirectory(src/univalue)

# ---- mvc_core 源文件收集 ----
file(GLOB_RECURSE MVC_CORE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
list(FILTER MVC_CORE_SOURCES EXCLUDE REGEX
    "/(rpc|wallet|zmq|qt|http|test|bench|config|crypto|leveldb|secp256k1|univalue)/")
list(FILTER MVC_CORE_SOURCES EXCLUDE REGEX
    "/(init|noui|rest|httprpc|httpserver|vmtouch|mvc-cli|mvc-tx|mvcd)\\.cpp$")
list(FILTER MVC_CORE_SOURCES EXCLUDE REGEX
    "/(compat/glibc[a-z_]*|compat/strnlen|compat/glibcxx_sanity|compat/glibc_sanity)\\.cpp$")
# rpc 目录整体排除后，显式补回网络内核依赖的 jsonwriter/webhook_client/client
list(APPEND MVC_CORE_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/rpc/jsonwriter.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/rpc/webhook_client.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/rpc/client.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/rpc/client_config.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/rpc/http_request.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/rpc/http_response.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/rpc/protocol.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/helpers/memenv/memenv.cc"
)

add_library(mvc_core STATIC ${MVC_CORE_SOURCES})
target_include_directories(mvc_core PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/helpers/memenv"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/secp256k1/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/univalue/include"
    "${CMAKE_CURRENT_BINARY_DIR}/src/config"
    "${BOOST_ROOT}"
)
target_compile_definitions(mvc_core PUBLIC HAVE_CONFIG_H BOOST_SP_USE_STD_ATOMIC BOOST_AC_USE_STD_ATOMIC)
if(MSVC)
    target_compile_definitions(mvc_core PUBLIC BOOST_THREAD_USE_LIB _WIN32_WINNT=0x0601)
endif()
target_link_libraries(mvc_core PUBLIC
    crypto leveldb secp256k1 univalue event event_core event_extra
    OpenSSL::SSL OpenSSL::Crypto
    ${Boost_LIBRARIES}
)
if(WIN32)
    target_link_libraries(mvc_core PUBLIC ws2_32 shlwapi bcrypt userenv)
endif()
