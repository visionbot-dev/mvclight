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
