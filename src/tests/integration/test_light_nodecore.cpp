// light_nodecore 集成测试：初始化 chainparams + chainstate/UTXO
// 不加入 ctest；手动运行：test_light_nodecore
// 预期输出：NODECORE_STARTED

#include "light/light_nodecore.h"

#include <cstdio>

int main() {
    mvclight::CLightNodeCore core;
    if (!core.Init("main", "demo_nodecore_data")) {
        std::printf("NODECORE_INIT_FAILED\n");
        return 1;
    }
    if (!core.Start()) {
        std::printf("NODECORE_START_FAILED\n");
        return 2;
    }
    std::printf("NODECORE_STARTED\n");
    core.Stop();
    return 0;
}
