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

# Phase 3 集成测试（配对 + 原子存储 + 篡改拒绝）
add_executable(test_phase3 integration/test_phase3.cpp)
target_include_directories(test_phase3 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/unit)
target_link_libraries(test_phase3 PRIVATE mvclight_objects)
add_test(NAME test_phase3 COMMAND test_phase3)
