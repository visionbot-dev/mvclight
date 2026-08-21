#ifndef MVC_LIGHT_LIGHT_PEER_H
#define MVC_LIGHT_LIGHT_PEER_H

/*
 * CLightPeer —— 单连接 P2P 对端管理（Phase 1 骨架）。
 *
 * 覆盖：连接、VERSION/VERACK 握手、NODE_BLOOM 服务位校验、
 *       PING/PONG 心跳原语、重连退避计算。
 * 后续 Phase 将在此接入 FILTERLOAD、GETHEADERS、MERKLEBLOCK/TX 处理。
 */

#include "light/light_message.h"
#include "light/light_socket.h"
#include "light/light_state.h"

#include <cstdint>
#include <string>

namespace mvclight {

class CLightPeer {
public:
    static constexpr uint32_t kPingIntervalMs = 120000; // 设计文档 §4.1.2
    static constexpr uint32_t kPongTimeoutMs = 60000;
    static constexpr int kMaxRetry = 10;
    static constexpr uint32_t kRetryBaseMs = 5000;
    static constexpr uint32_t kRetryMaxMs = 300000;
    static constexpr int kServiceNodeNetwork = 1 << 0;
    static constexpr int kServiceNodeBloom = 1 << 2;

    CLightPeer();
    ~CLightPeer();

    CLightPeer(const CLightPeer&) = delete;
    CLightPeer& operator=(const CLightPeer&) = delete;

    // 连接 + 握手（VERSION/VERACK）。成功返回 true，状态进入 FILTER_SENT。
    bool ConnectAndHandshake(const std::string& host, uint16_t port,
                             uint32_t connect_timeout_ms, uint32_t recv_timeout_ms);

    void Disconnect();

    // 发送 ping（payload = nonce LE64）
    bool SendPing(uint64_t nonce);

    // 检查 ping/pong 超时：now_usec 由调用方提供（测试可注入）。
    // 单次超时返回 true 并累计；连续 2 次超时断开连接。
    bool CheckPingTimeout(int64_t now_usec);

    // 处理一条已读消息；PONG 更新 RTT，REJECT 等后续 Phase 扩展
    bool HandleMessage(const LightMessage& msg);

    // 读取并处理一条消息（阻塞，受 socket recv timeout 约束）
    bool ReadAndHandle(const uint8_t magic[4]);

    // 状态机：仅允许合法迁移
    bool SetState(PeerState state);
    PeerState GetState() const { return m_state; }

    // 重连退避：retry = min(5s * 2^attempt, 300s)，jitter_ratio ∈ [-0.2, 0.2]
    static uint32_t NextRetryDelayMs(int attempt, double jitter_ratio = 0.0);

    int64_t GetLastPingUsec() const { return m_last_ping_usec; }

private:
    bool DoHandshake(const uint8_t magic[4], uint32_t recv_timeout_ms);

    CLightSocket m_sock;
    PeerState m_state;
    uint64_t m_local_nonce;
    uint64_t m_remote_nonce;
    int64_t m_ping_sent_usec;
    int64_t m_last_ping_usec;
    uint64_t m_pending_pong_nonce;
    int m_pong_timeout_count;
};

} // namespace mvclight

#endif // MVC_LIGHT_LIGHT_PEER_H
