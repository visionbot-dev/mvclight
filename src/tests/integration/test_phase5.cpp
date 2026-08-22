#include "light/light_peer.h"
#include "light/mvc_light.h"

#include "test_framework.h"

#include <cstdio>
#include <cstring>
#include <string>

int main() {
    // 深重组阈值
    CHECK(!mvclight::CLightPeer::IsDeepReorg(144));
    CHECK(mvclight::CLightPeer::IsDeepReorg(145));

    // C ABI 桌面端加固路径
    mvc_light_config cfg{};
    cfg.network = "main";
    cfg.peer = "127.0.0.1:9883";
    cfg.store_path = "store";
    mvc_light_ctx* ctx = mvc_light_init(&cfg);
    CHECK(ctx != nullptr);

    // 未运行状态：send_raw_tx 仍可入队并返回 txid（广播待连接后执行）
    char* txid = mvc_light_send_raw_tx(ctx, "01000000020000000000000000");
    CHECK(txid != nullptr);
    CHECK(strlen(txid) == 64);
    mvc_light_free_string(txid);

    // 畸形 hex -> nullptr
    CHECK(mvc_light_send_raw_tx(ctx, "zz") == nullptr);

    // 查询 JSON 字段
    char* status = mvc_light_sync_status(ctx);
    CHECK(status != nullptr);
    CHECK(strstr(status, "\"running\":false") != nullptr);
    CHECK(strstr(status, "\"watch_count\":0") != nullptr);
    mvc_light_free_string(status);

    // force_reset_chain 可调用
    CHECK(mvc_light_force_reset_chain(ctx) == MVC_LIGHT_OK);

    // 参数校验
    CHECK(mvc_light_switch_peer(nullptr, "a:b") == MVC_LIGHT_ERR_PARAM_INVALID);
    CHECK(mvc_light_send_raw_tx(nullptr, "aa") == nullptr);

    mvc_light_destroy(ctx);

    std::printf("DESKTOP_HARDENING_OK\n");
    TEST_MAIN_RETURN();
}
