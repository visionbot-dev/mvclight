// light_nodecore 网络连接测试（B-5）
// 启动 CConnman 接入种子，等待 20 秒后打印出站连接数。
// 注意：不调用 Stop()（CConnman 停止清理仍在完善），进程退出时 OS 回收线程。
// 手动运行：test_light_nodecore_net
// 预期输出：NODECORE_CONNECTIONS <n>

#include "light/light_nodecore.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>

int main() {
    std::printf("MAIN_START\n");
    std::fflush(stdout);
    try {
    mvclight::CLightNodeCore core;
    core.SetSeed("127.0.0.1:19883");
    if (!core.Init("test", "demo_nodecore_net_data")) {
        std::printf("NODECORE_INIT_FAILED\n");
        return 1;
    }
    if (!core.Start()) {
        std::printf("NODECORE_START_FAILED\n");
        return 2;
    }
    for (int i = 1; i <= 10; ++i) {
        std::this_thread::sleep_for(std::chrono::seconds(30));
        std::printf("NODECORE_CONNECTIONS %zu (t=%ds)\n", core.GetNodeCount(), i * 30);
        std::fflush(stdout);
    }
    } catch (const std::exception& e) {
        std::printf("EXCEPTION: %s\n", e.what());
        std::fflush(stdout);
        return 3;
    }
    std::fflush(stdout);
    std::exit(0);
}
