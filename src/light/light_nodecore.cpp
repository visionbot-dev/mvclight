#include "light/light_nodecore.h"

#include <chainparams.h>
#include <config.h>
#include <util.h>

#include <cstdio>

namespace mvclight {

CLightNodeCore::CLightNodeCore() = default;
CLightNodeCore::~CLightNodeCore() { Stop(); }

bool CLightNodeCore::Init(const std::string& network, const std::string& data_dir) {
    // Phase A：chainparams 与全局配置可初始化（已验证）
    SelectParams(network.empty() ? "main" : network);

    Config& config = GlobalConfig::GetModifiableGlobalConfig();
    (void)config;

    if (!data_dir.empty()) {
        gArgs.SoftSetArg("-datadir", data_dir);
    }
    // 轻节点默认不监听、不做服务端
    gArgs.SoftSetBoolArg("-listen", false);
    gArgs.SoftSetBoolArg("-dnsseed", true);
    gArgs.SoftSetArg("-maxconnections", "8");

    std::printf("[nodecore] init ok network=%s\n",
                network.empty() ? "main" : network.c_str());
    m_running = false; // Start() 成功后置 true
    return true;
}

bool CLightNodeCore::Start() {
    // TODO Phase B：按 init.cpp 移植
    // 1. InitSignatureCache/InitScriptExecutionCache
    // 2. pblocktree / pcoinsTip / CChainState 初始化
    // 3. mempool 初始化
    // 4. CConnman 创建 + PeerLogicValidation + RegisterValidationInterface
    // 5. connman.Start(scheduler, options) 连接种子
    std::printf("[nodecore] start not implemented yet (Phase B)\n");
    return false;
}

void CLightNodeCore::Stop() {
    // TODO Phase B：停止 connman、注销 validation interface、清理 chainstate
    m_running = false;
}

} // namespace mvclight
