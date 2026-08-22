# mvclight 测试（Phase 0/1：轻量断言；后续阶段按计划引入 Boost.Test）

find_package(Threads REQUIRED)

add_executable(test_light_txpool unit/test_light_txpool.cpp)
target_link_libraries(test_light_txpool PRIVATE mvclight_objects)
add_test(NAME test_light_txpool COMMAND test_light_txpool)

add_executable(test_mvc_light_api unit/test_mvc_light_api.cpp)
target_link_libraries(test_mvc_light_api PRIVATE mvclight_objects)
add_test(NAME test_mvc_light_api COMMAND test_mvc_light_api)

# Phase 1 单元测试
add_executable(test_light_state unit/test_light_state.cpp)
target_link_libraries(test_light_state PRIVATE mvclight_objects)
add_test(NAME test_light_state COMMAND test_light_state)

add_executable(test_light_message unit/test_light_message.cpp)
target_link_libraries(test_light_message PRIVATE mvclight_objects)
add_test(NAME test_light_message COMMAND test_light_message)

add_executable(test_light_peer unit/test_light_peer.cpp)
target_link_libraries(test_light_peer PRIVATE mvclight_objects)
add_test(NAME test_light_peer COMMAND test_light_peer)

# Phase 1 集成测试（本地 mock peer）
add_executable(test_phase1 integration/test_phase1.cpp)
target_include_directories(test_phase1 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/unit)
target_link_libraries(test_phase1 PRIVATE mvclight_objects Threads::Threads)
add_test(NAME test_phase1 COMMAND test_phase1)

# Phase 2 单元测试
add_executable(test_light_filter unit/test_light_filter.cpp)
target_link_libraries(test_light_filter PRIVATE mvclight_objects)
add_test(NAME test_light_filter COMMAND test_light_filter)

add_executable(test_light_validation unit/test_light_validation.cpp)
target_link_libraries(test_light_validation PRIVATE mvclight_objects)
add_test(NAME test_light_validation COMMAND test_light_validation)

# Phase 2 集成测试（本地 mock peer 返回 HEADERS）
add_executable(test_phase2 integration/test_phase2.cpp)
target_include_directories(test_phase2 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/unit)
target_link_libraries(test_phase2 PRIVATE mvclight_objects Threads::Threads)
add_test(NAME test_phase2 COMMAND test_phase2)

# Phase 3 单元测试
add_executable(test_light_tx unit/test_light_tx.cpp)
target_link_libraries(test_light_tx PRIVATE mvclight_objects)
add_test(NAME test_light_tx COMMAND test_light_tx)

add_executable(test_light_pendingtx unit/test_light_pendingtx.cpp)
target_link_libraries(test_light_pendingtx PRIVATE mvclight_objects)
add_test(NAME test_light_pendingtx COMMAND test_light_pendingtx)

add_executable(test_light_watchstore unit/test_light_watchstore.cpp)
target_link_libraries(test_light_watchstore PRIVATE mvclight_objects)
add_test(NAME test_light_watchstore COMMAND test_light_watchstore)

add_executable(test_light_watchstore_leveldb unit/test_light_watchstore_leveldb.cpp)
target_link_libraries(test_light_watchstore_leveldb PRIVATE mvclight_objects)
add_test(NAME test_light_watchstore_leveldb COMMAND test_light_watchstore_leveldb)

# Phase 3 集成测试（配对 + 原子存储 + 篡改拒绝）
add_executable(test_phase3 integration/test_phase3.cpp)
target_include_directories(test_phase3 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/unit)
target_link_libraries(test_phase3 PRIVATE mvclight_objects)
add_test(NAME test_phase3 COMMAND test_phase3)

# Phase 4 单元测试
add_executable(test_light_sendmonitor unit/test_light_sendmonitor.cpp)
target_link_libraries(test_light_sendmonitor PRIVATE mvclight_objects)
add_test(NAME test_light_sendmonitor COMMAND test_light_sendmonitor)

# Phase 4 集成测试（C ABI + mock peer）
add_executable(test_phase4 integration/test_phase4.cpp)
target_include_directories(test_phase4 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/unit)
target_link_libraries(test_phase4 PRIVATE mvclight_objects Threads::Threads)
add_test(NAME test_phase4 COMMAND test_phase4)

# Phase 5 桌面端加固验收（移动端交叉编译暂缓）
add_executable(test_phase5 integration/test_phase5.cpp)
target_include_directories(test_phase5 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/unit)
target_link_libraries(test_phase5 PRIVATE mvclight_objects)
add_test(NAME test_phase5 COMMAND test_phase5)

# Phase 5 共识：ASERT
add_executable(test_light_asert unit/test_light_asert.cpp)
target_link_libraries(test_light_asert PRIVATE mvclight_objects)
add_test(NAME test_light_asert COMMAND test_light_asert)

# Phase 5 共识：真实 11 块 MTP
add_executable(test_light_chainstore unit/test_light_chainstore.cpp)
target_link_libraries(test_light_chainstore PRIVATE mvclight_objects)
add_test(NAME test_light_chainstore COMMAND test_light_chainstore)

# P2P 防拉黑策略
add_executable(test_light_peer_policy unit/test_light_peer_policy.cpp)
target_link_libraries(test_light_peer_policy PRIVATE mvclight_objects)
add_test(NAME test_light_peer_policy COMMAND test_light_peer_policy)

# 轻量多连接管理器
add_executable(test_light_connman unit/test_light_connman.cpp)
target_link_libraries(test_light_connman PRIVATE mvclight_objects)
add_test(NAME test_light_connman COMMAND test_light_connman)

# 主网端到端同步（依赖外网，不加入 ctest；手动运行：test_mainnet_sync）
add_executable(test_mainnet_sync integration/test_mainnet_sync.cpp)
target_include_directories(test_mainnet_sync PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/unit)
target_link_libraries(test_mainnet_sync PRIVATE mvclight_objects Threads::Threads)

# 主网 MERKLEBLOCK 抓取调试（依赖外网，不加入 ctest；手动运行：test_merkleblock_fetch）
add_executable(test_merkleblock_fetch integration/test_merkleblock_fetch.cpp)
target_include_directories(test_merkleblock_fetch PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/unit)
target_link_libraries(test_merkleblock_fetch PRIVATE mvclight_objects Threads::Threads)

# mvc_core 冒烟测试（依赖外网内核，不加入 ctest；手动运行：test_mvc_core_smoke）
if(BUILD_MVC_LIGHT_UPSTREAM_CORE)
    add_executable(test_mvc_core_smoke integration/test_mvc_core_smoke.cpp)
    target_include_directories(test_mvc_core_smoke PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/unit
        "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/microvisionchain/src"
        "${BOOST_ROOT_CACHE}"
        "${OPENSSL_ROOT_CACHE}/include"
    )
    target_link_directories(test_mvc_core_smoke PRIVATE
        "${BOOST_ROOT_CACHE}/lib64-msvc-14.3"
        "${OPENSSL_ROOT_CACHE}/lib/VC/x64/MD"
    )
    target_link_libraries(test_mvc_core_smoke PRIVATE mvc_core
        "${BOOST_ROOT_CACHE}/lib64-msvc-14.3/libboost_filesystem-vc143-mt-x64-1_85.lib"
        "${BOOST_ROOT_CACHE}/lib64-msvc-14.3/libboost_chrono-vc143-mt-x64-1_85.lib"
        "${BOOST_ROOT_CACHE}/lib64-msvc-14.3/libboost_program_options-vc143-mt-x64-1_85.lib"
        "${BOOST_ROOT_CACHE}/lib64-msvc-14.3/libboost_thread-vc143-mt-x64-1_85.lib"
        "${BOOST_ROOT_CACHE}/lib64-msvc-14.3/libboost_system-vc143-mt-x64-1_85.lib"
    )
endif()
