#ifndef MVC_LIGHT_LIGHT_CONNMAN_H
#define MVC_LIGHT_LIGHT_CONNMAN_H

/*
 * 轻量多连接管理器（Phase 5 加固；参考全节点 connman 思路，自研实现）。
 *
 * 全节点通过同时保持多条出站连接实现断线零切换；本模块提供最小等价物：
 *   - 维护多个 peer 槽位（主连接 + 备用连接）
 *   - 逐个建立连接/握手
 *   - 支持把备用连接提升为主连接（无需重新握手）
 * 不链接上游 net/connman/addrman，仅自研白名单代码。
 */

#include "light/light_peer.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace mvclight {

class CLightConnMan {
public:
    CLightConnMan(uint32_t connect_timeout_ms = 10000,
                  uint32_t recv_timeout_ms = 3000);

    // 增加一个候选 peer（不立即连接）
    bool AddPeer(const std::string& host, uint16_t port);

    // 连接并握手第 idx 个 peer；成功返回 true
    bool ConnectPeer(size_t idx);

    void DisconnectPeer(size_t idx);
    bool IsConnected(size_t idx) const;
    CLightPeer* Peer(size_t idx);
    size_t Count() const { return m_slots.size(); }

    // 依次连接，直到成功 max_conns 个或候选耗尽；返回成功连接数
    size_t ConnectUpTo(size_t max_conns);

    // 把已连接的 idx 提升到主连接（槽位 0）；返回是否成功
    bool PromoteToPrimary(size_t idx);

    std::string PeerName(size_t idx) const;

private:
    struct Slot {
        std::string host;
        uint16_t port = 0;
        std::unique_ptr<CLightPeer> peer;
        bool connected = false;
    };

    uint32_t m_connect_timeout_ms;
    uint32_t m_recv_timeout_ms;
    std::vector<Slot> m_slots;
};

} // namespace mvclight

#endif // MVC_LIGHT_LIGHT_CONNMAN_H
