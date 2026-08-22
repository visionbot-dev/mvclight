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

    bool IsRunning() const { return m_running; }

private:
    bool m_running = false;
    void* m_connman = nullptr; // TODO: std::unique_ptr<CConnman>
    void* m_peer_logic = nullptr; // TODO: std::unique_ptr<PeerLogicValidation>
};

} // namespace mvclight

#endif // MVC_LIGHT_LIGHT_NODECORE_H
