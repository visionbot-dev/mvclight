# mvclight 测试（Phase 0：轻量断言；后续阶段按计划引入 Boost.Test）

add_executable(test_light_txpool unit/test_light_txpool.cpp)
target_link_libraries(test_light_txpool PRIVATE mvclight_objects)
add_test(NAME test_light_txpool COMMAND test_light_txpool)

add_executable(test_mvc_light_api unit/test_mvc_light_api.cpp)
target_link_libraries(test_mvc_light_api PRIVATE mvclight_objects)
add_test(NAME test_mvc_light_api COMMAND test_mvc_light_api)
