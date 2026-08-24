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
#include <txmempool.h>
#include <util.h>
#include <validation.h>

#include <boost/filesystem.hpp>
#include <boost/thread.hpp>

#include <chrono>
#include <cstdio>
#include <memory>

#include <mining/factory.h>
#include <mining/journal_builder.h>
#include <net/net_processing.h>
#include <netmessagemaker.h>
#include <protocol.h>
#include <random.h>
#include <rpc/webhook_client.h>
#include <scheduler.h>
#include <time_locked_mempool.h>
#include <ui_interface.h>

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
    std::printf("[nodecore] step selectparams ok\n");
    std::fflush(stdout);

    // Phase B-1：签名/脚本执行缓存（init.cpp:3012-3013）
    InitSignatureCache();
    InitScriptExecutionCache();
    std::printf("[nodecore] step caches ok\n");
    std::fflush(stdout);

    ConfigInit& config = GlobalConfig::GetModifiableGlobalConfig();
    std::printf("[nodecore] init setdefault before\n");
    std::fflush(stdout);
    config.SetDefaultBlockSizeParams(Params().GetDefaultBlockSizeParams());
    std::printf("[nodecore] init setdefault after\n");
    std::fflush(stdout);

    if (!data_dir.empty()) {
        gArgs.SoftSetArg("-datadir", data_dir);
        boost::filesystem::create_directories(boost::filesystem::absolute(data_dir));
    }
    // 轻节点默认不监听、不做服务端
    gArgs.SoftSetBoolArg("-listen", false);
    gArgs.SoftSetBoolArg("-dnsseed", true);
    gArgs.SoftSetArg("-maxconnections", "8");
    std::printf("[nodecore] step gargs ok\n");
    std::fflush(stdout);

    std::printf("[nodecore] init ok network=%s\n",
                network.empty() ? "main" : network.c_str());
    std::fflush(stdout);
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

    // ---- Phase B-3：mempool 初始化（init.cpp:3019-3024）----
    mempool.SuspendSanityCheck();
    mempool.getNonFinalPool().loadConfig();
    mempool.InitMempoolTxDB();
    if (!gArgs.GetArg("-persistmempool", DEFAULT_PERSIST_MEMPOOL)) {
        mempool.ResumeSanityCheck();
    }

    std::printf("[nodecore] chainstate ok tip=%s height=%d\n",
                chainActive.Tip() ? chainActive.Tip()->GetBlockHash().GetHex().c_str() : "(null)",
                chainActive.Height());
    std::printf("[nodecore] mempool ok size=%lu\n", static_cast<unsigned long>(mempool.Size()));

    // 激活创世块/最佳链（对应 init.cpp ThreadImport 尾部 ActivateBestChain）
    {
        CValidationState dummyState;
        mining::CJournalChangeSetPtr changeSet{ mempool.getJournalBuilder().getNewChangeSet(mining::JournalUpdateReason::INIT) };
        auto source = task::CCancellationSource::Make();
        if (!ActivateBestChain(task::CCancellationToken::JoinToken(source->GetToken(), GetShutdownToken()), config, dummyState, changeSet)) {
            std::printf("[nodecore] ActivateBestChain failed\n");
            return false;
        }
    }
    std::printf("[nodecore] chain activated height=%d\n", chainActive.Height());

    // ---- Phase B-4：CConnman / PeerLogicValidation（init.cpp Step 6/11 裁剪版）----
    if (!SetupNetworking()) {
        std::printf("[nodecore] SetupNetworking failed\n");
        return false;
    }
    std::printf("[nodecore] step setupnetworking ok\n");
    std::fflush(stdout);

    boost::thread_group* threadGroup = new boost::thread_group();
    CScheduler* scheduler = new CScheduler();
    scheduler->startServiceThread(*threadGroup);
    InitScriptCheckQueues(config, *threadGroup);
    m_thread_group = threadGroup;
    m_scheduler = scheduler;

    g_connman = std::make_unique<CConnman>(
        config,
        GetRand(std::numeric_limits<uint64_t>::max()),
        GetRand(std::numeric_limits<uint64_t>::max()),
        std::chrono::milliseconds(0));
    m_connman = g_connman.get();
    std::printf("[nodecore] step connman ctor ok\n");
    std::fflush(stdout);

    PeerLogicValidation* plv = new PeerLogicValidation(g_connman.get());
    m_peer_logic = plv;
    std::printf("[nodecore] step peerlogic ctor ok\n");
    std::fflush(stdout);
    RegisterValidationInterface(plv);
    std::printf("[nodecore] step register validation ok\n");
    std::fflush(stdout);
    RegisterNodeSignals(GetNodeSignals());
    std::printf("[nodecore] step register nodesignals ok\n");
    std::fflush(stdout);

    CConnman::Options connOptions;
    connOptions.nLocalServices = ServiceFlags(NODE_NETWORK | NODE_BLOOM);
    connOptions.nRelevantServices = ServiceFlags(NODE_NETWORK);
    connOptions.nMaxConnections = gArgs.GetArg("-maxconnections", 8);
    connOptions.nMaxOutbound = std::min(MAX_OUTBOUND_CONNECTIONS, connOptions.nMaxConnections);
    connOptions.nMaxAddnode = 0;
    connOptions.nMaxFeeler = 1;
    connOptions.nBestHeight = chainActive.Height();
    connOptions.uiInterface = &uiInterface;
    connOptions.nSendBufferMaxSize = gArgs.GetArgAsBytes("-maxsendbuffer", DEFAULT_MAXSENDBUFFER, ONE_KILOBYTE);
    connOptions.nReceiveFloodSize = gArgs.GetArgAsBytes("-maxreceivebuffer", DEFAULT_MAXRECEIVEBUFFER, ONE_KILOBYTE);

    std::string strNodeError;
    if (!g_connman->Start(*scheduler, strNodeError, connOptions)) {
        std::printf("[nodecore] connman.Start failed: %s\n", strNodeError.c_str());
        return false;
    }
    std::printf("[nodecore] connman started\n");

    // init.cpp 3529-3561：msghand 运行时依赖
    mining::g_miningFactory = std::make_unique<mining::CMiningFactory>(config);
    mempool.getNonFinalPool().startPeriodicChecks(*scheduler);
    rpc::client::g_pWebhookClient = std::make_unique<rpc::client::WebhookClient>(config);
    CheckSafeModeParametersForAllForksOnStartup(config);
    std::printf("[nodecore] runtime init ok\n");

    // B-5：发起种子连接（可指定本地 testnet；fOneShot=false 保持长连接）
    {
        const char* seed = m_seed.empty() ? "47.242.24.63:9883" : m_seed.c_str();
        CAddress seedAddr;
        NodeConnectInfo seedInfo{seedAddr, seed};
        bool opened = g_connman->OpenNetworkConnection(seedInfo, nullptr, false);
        std::printf("[nodecore] seed open(%s)=%d\n", seed, opened ? 1 : 0);
    }

    m_running = true;
    return true;
}

size_t CLightNodeCore::GetNodeCount() const {
    if (!g_connman) return 0;
    return g_connman->GetNodeCount(CConnman::CONNECTIONS_OUT);
}

void CLightNodeCore::Stop() {
    if (g_connman) {
        g_connman->Interrupt();
        g_connman->Stop();
        g_connman.reset();
    }
    m_connman = nullptr;

    if (m_peer_logic) {
        UnregisterValidationInterface(static_cast<PeerLogicValidation*>(m_peer_logic));
        delete static_cast<PeerLogicValidation*>(m_peer_logic);
        m_peer_logic = nullptr;
    }

    if (m_scheduler) {
        delete static_cast<CScheduler*>(m_scheduler);
        m_scheduler = nullptr;
    }
    if (m_thread_group) {
        delete static_cast<boost::thread_group*>(m_thread_group);
        m_thread_group = nullptr;
    }

    rpc::client::g_pWebhookClient.reset();
    mining::g_miningFactory.reset();

    UnloadBlockIndex();
    pcoinsTip.reset();
    delete pblocktree;
    pblocktree = nullptr;
    m_running = false;
}

} // namespace mvclight
