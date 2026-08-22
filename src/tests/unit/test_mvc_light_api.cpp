#include "light/mvc_light.h"

#include "test_framework.h"

#include <cstring>

int main() {
    // init(NULL) → ERR_PARAM_INVALID（init 返回 ctx*，NULL 表示失败）
    mvc_light_ctx* null_ctx = mvc_light_init(nullptr);
    CHECK(null_ctx == nullptr);

    // 非法配置：network/peer/store_path 缺一不可
    mvc_light_config bad{};
    bad.network = "main";
    bad.peer = "127.0.0.1:9883";
    bad.store_path = nullptr;
    CHECK(mvc_light_init(&bad) == nullptr);

    // 合法配置 init/destroy 无崩溃
    mvc_light_config cfg{};
    cfg.network = "main";
    cfg.peer = "127.0.0.1:9883";
    cfg.store_path = "store";
    cfg.start_height = -1;
    cfg.dbcache_mb = 8;
    cfg.store_proof = 0;
    mvc_light_ctx* ctx = mvc_light_init(&cfg);
    CHECK(ctx != nullptr);

    // 同步查询骨架
    CHECK(mvc_light_is_running(ctx) == 0);
    char state[64];
    CHECK(mvc_light_get_peer_state(ctx, state, sizeof(state)) == 0);
    CHECK(std::strcmp(state, "INIT") == 0);

    // 参数校验
    CHECK(mvc_light_get_peer_state(nullptr, state, sizeof(state)) == MVC_LIGHT_ERR_PARAM_INVALID);
    CHECK(mvc_light_get_peer_state(ctx, nullptr, sizeof(state)) == MVC_LIGHT_ERR_PARAM_INVALID);
    CHECK(mvc_light_watch_add(nullptr, "addr") == MVC_LIGHT_ERR_PARAM_INVALID);
    CHECK(mvc_light_watch_add(ctx, "addr") == MVC_LIGHT_ERR_PEER_DISCONNECTED);

    mvc_light_destroy(ctx);
    TEST_MAIN_RETURN();
}
