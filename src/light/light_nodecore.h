#ifndef MVC_LIGHT_LIGHT_NODECORE_H
#define MVC_LIGHT_LIGHT_NODECORE_H

/*
 * CLightNodeCore —— 全节点网络内核封装（Phase B 骨架）。
 *
 * 目标：封装上游 CConnman / PeerLogicValidation / CTxMemPool 的启动，
 * 使 mvclight 网络行为与全节点一致。当前已实现 chainparams/Config 初始化；
 * CConnman 启动需逐步从 init.cpp 移植（UTXO/chainstate/mempool 初始化）。
 */

#include <memory>
#include <string>

namespace mvclight {

class CLightNodeCore {
public:
    CLightNodeCore();
    ~CLightNodeCore();

    CLightNodeCore(const CLightNodeCore&) = delete;
    CLightNodeCore& operator=(const CLightNodeCore&) = delete;

    // 初始化链参数与全局配置（main/testnet）
    bool Init(const std::string& network, const std::string& data_dir);

    // 启动网络内核（TODO Phase B：CConnman/PeerLogicValidation 全量启动）
    bool Start();

    // 停止并清理
    void Stop();

    // 设置要连接的种子（host:port）；为空则用内置主网种子
    void SetSeed(const std::string& hostport) { m_seed = hostport; }

    bool IsRunning() const { return m_running; }

    // 当前出站连接数（B-5 验证用）
    size_t GetNodeCount() const;

private:
    bool m_running = false;
    std::string m_seed;
    void* m_connman = nullptr;      // std::unique_ptr<CConnman> 所有权在全局 g_connman
    void* m_peer_logic = nullptr;   // PeerLogicValidation*
    void* m_scheduler = nullptr;    // CScheduler*
    void* m_thread_group = nullptr; // boost::thread_group*
};

} // namespace mvclight

#endif // MVC_LIGHT_LIGHT_NODECORE_H
