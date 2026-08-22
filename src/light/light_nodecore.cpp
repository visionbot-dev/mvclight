#include "light/light_nodecore.h"

#include <chainparams.h>
#include <config.h>
#include <init.h>
#include <net/net.h>
#include <script/scriptcache.h>
#include <script/sigcache.h>
#include <sync.h>
#include <taskcancellation.h>
#include <txdb.h>
#include <util.h>
#include <validation.h>

#include <boost/filesystem.hpp>

#include <cstdio>
#include <memory>

// init.cpp 中被裁剪的全局 shutdown/connman（网络内核依赖）
static std::shared_ptr<task::CCancellationSource> shutdownSource(task::CCancellationSource::Make());
void StartShutdown() { shutdownSource->Cancel(); }
task::CCancellationToken GetShutdownToken() { return shutdownSource->GetToken(); }
std::unique_ptr<CConnman> g_connman;

namespace mvclight {

CLightNodeCore::CLightNodeCore() = default;
CLightNodeCore::~CLightNodeCore() { Stop(); }

bool CLightNodeCore::Init(const std::string& network, const std::string& data_dir) {
    // Phase A：chainparams 与全局配置可初始化（已验证）
    SelectParams(network.empty() ? "main" : network);

    // Phase B-1：签名/脚本执行缓存（init.cpp:3012-3013）
    InitSignatureCache();
    InitScriptExecutionCache();

    Config& config = GlobalConfig::GetModifiableGlobalConfig();
    (void)config;

    if (!data_dir.empty()) {
        gArgs.SoftSetArg("-datadir", data_dir);
        boost::filesystem::create_directories(boost::filesystem::absolute(data_dir));
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
    const CChainParams& chainparams = Params();
    Config& config = GlobalConfig::GetModifiableGlobalConfig();

    // ---- Phase B-2：chainstate / UTXO 初始化（init.cpp Step 7 裁剪版）----
    const bool fReindex = false;
    const bool fReindexChainState = false;

    // 简化缓存配置：默认 450MiB，与 init.cpp 同款公式
    int64_t nTotalCache = gArgs.GetArgAsBytes("-dbcache", 450, ONE_MEBIBYTE);
    nTotalCache = std::max(nTotalCache, nMinDbCache << 20);
    nTotalCache = std::min(nTotalCache, nMaxDbCache << 20);
    int64_t nBlockTreeDBCache = nTotalCache / 8;
    nTotalCache -= nBlockTreeDBCache;
    int64_t nCoinDBCache = std::min(nTotalCache / 2, (nTotalCache / 4) + (1 << 23));
    nTotalCache -= nCoinDBCache;
    nCoinCacheUsage = nTotalCache;

    UnloadBlockIndex();
    pcoinsTip.reset();
    delete pblocktree;
    pblocktree = new CBlockTreeDB(static_cast<size_t>(nBlockTreeDBCache), false, fReindex);
    pcoinsTip = std::make_unique<CoinsDB>(
        config.GetMaxCoinsProviderCacheSize(),
        static_cast<size_t>(nCoinDBCache),
        CoinsDB::MaxFiles{config.GetMaxCoinsDbOpenFiles()},
        false,
        fReindex || fReindexChainState);

    if (!LoadBlockIndex(chainparams)) {
        std::printf("[nodecore] LoadBlockIndex failed\n");
        return false;
    }
    if (!InitBlockIndex(config)) {
        std::printf("[nodecore] InitBlockIndex failed\n");
        return false;
    }
    if (!ReplayBlocks(config, *pcoinsTip)) {
        std::printf("[nodecore] ReplayBlocks failed\n");
        return false;
    }
    {
        LOCK(cs_main);
        LoadChainTip(chainparams);
    }

    std::printf("[nodecore] chainstate ok tip=%s height=%d\n",
                chainActive.Tip() ? chainActive.Tip()->GetBlockHash().GetHex().c_str() : "(null)",
                chainActive.Height());
    m_running = true;
    return true;
}

void CLightNodeCore::Stop() {
    // TODO Phase B：停止 connman、注销 validation interface、清理 chainstate
    UnloadBlockIndex();
    pcoinsTip.reset();
    delete pblocktree;
    pblocktree = nullptr;
    m_running = false;
}

} // namespace mvclight
