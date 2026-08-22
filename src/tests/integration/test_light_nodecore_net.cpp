// light_nodecore 网络连接测试（B-5）
// 启动 CConnman 接入种子，等待 20 秒后打印出站连接数。
// 注意：不调用 Stop()（CConnman 停止清理仍在完善），进程退出时 OS 回收线程。
// 手动运行：test_light_nodecore_net
// 预期输出：NODECORE_CONNECTIONS <n>

#include "light/light_nodecore.h"

#include <chrono>
#include <cstdio>
#include <thread>

int main() {
    mvclight::CLightNodeCore core;
    if (!core.Init("main", "demo_nodecore_net_data")) {
        std::printf("NODECORE_INIT_FAILED\n");
        return 1;
    }
    if (!core.Start()) {
        std::printf("NODECORE_START_FAILED\n");
        return 2;
    }
    std::this_thread::sleep_for(std::chrono::seconds(20));
    std::printf("NODECORE_CONNECTIONS %zu\n", core.GetNodeCount());
    return 0;
}
