#include "light/light_peer.h"

#include "light/light_sha256.h"

#include <chrono>
#include <ctime>
#include <random>

namespace mvclight {

namespace {

int64_t NowUsec() {
    return std::chrono::duration_cast<std::chrono::microseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

void WriteLE64(std::vector<uint8_t>& out, uint64_t v) {
    for (int i = 0; i < 8; ++i) {
        out.push_back(static_cast<uint8_t>(v >> (i * 8)));
    }
}

void WriteLE32(std::vector<uint8_t>& out, uint32_t v) {
    for (int i = 0; i < 4; ++i) {
        out.push_back(static_cast<uint8_t>(v >> (i * 8)));
    }
}

void WriteLE16(std::vector<uint8_t>& out, uint16_t v) {
    out.push_back(static_cast<uint8_t>(v));
    out.push_back(static_cast<uint8_t>(v >> 8));
}

void WriteCompactSize(std::vector<uint8_t>& out, uint64_t v) {
    if (v < 253) {
        out.push_back(static_cast<uint8_t>(v));
    } else if (v <= 0xFFFF) {
        out.push_back(253);
        WriteLE16(out, static_cast<uint16_t>(v));
    } else if (v <= 0xFFFFFFFF) {
        out.push_back(254);
        WriteLE32(out, static_cast<uint32_t>(v));
    } else {
        out.push_back(255);
        WriteLE64(out, v);
    }
}

uint64_t ReadLE64(const uint8_t* p) {
    uint64_t v = 0;
    for (int i = 7; i >= 0; --i) {
        v = (v << 8) | p[i];
    }
    return v;
}

uint64_t RandomNonce() {
    std::random_device rd;
    std::mt19937_64 gen(rd());
    return gen();
}

// 构建最小 VERSION payload（与上游序列化兼容）
std::vector<uint8_t> BuildVersionPayload(uint64_t nonce) {
    std::vector<uint8_t> out;
    WriteLE32(out, 70016);                 // PROTOCOL_VERSION
    WriteLE64(out, 1 | 4);                 // services: NODE_NETWORK | NODE_BLOOM
    WriteLE64(out, static_cast<uint64_t>(::time(nullptr))); // timestamp
    // addr_recv: services(8) + ip(16) + port(2)
    out.insert(out.end(), 26, 0);
    // addr_from
    out.insert(out.end(), 26, 0);
    WriteLE64(out, nonce);
    const std::string ua = "/mvclight:0.1.0/";
    WriteCompactSize(out, ua.size());
    out.insert(out.end(), ua.begin(), ua.end());
    WriteLE32(out, 0);                     // start_height
    out.push_back(0);                      // relay
    return out;
}

bool ParseVersionServices(const std::vector<uint8_t>& payload, uint64_t& services) {
    if (payload.size() < 12) return false;
    services = ReadLE64(payload.data() + 4);
    return true;
}

bool ParsePongNonce(const std::vector<uint8_t>& payload, uint64_t& nonce) {
    if (payload.size() < 8) return false;
    nonce = ReadLE64(payload.data());
    return true;
}

} // namespace

CLightPeer::CLightPeer()
    : m_state(PeerState::INIT),
      m_local_nonce(RandomNonce()),
      m_remote_nonce(0),
      m_ping_sent_usec(0),
      m_last_ping_usec(0),
      m_pending_pong_nonce(0),
      m_pong_timeout_count(0) {}

CLightPeer::~CLightPeer() {
    Disconnect();
}

bool CLightPeer::ConnectAndHandshake(const std::string& host, uint16_t port,
                                     uint32_t connect_timeout_ms, uint32_t recv_timeout_ms) {
    Disconnect();
    SetState(PeerState::HANDSHAKE);

    if (!m_sock.Connect(host, port, connect_timeout_ms)) {
        SetState(PeerState::DISCONNECTED);
        return false;
    }
    m_sock.SetRecvTimeout(recv_timeout_ms);

    if (!DoHandshake(MainnetMagic(), recv_timeout_ms)) {
        m_sock.Close();
        SetState(PeerState::DISCONNECTED);
        return false;
    }
    return true;
}

bool CLightPeer::DoHandshake(const uint8_t magic[4], uint32_t recv_timeout_ms) {
    (void)recv_timeout_ms; // 接收超时已在 socket 上设置
    // 1. 发送 VERSION
    std::vector<uint8_t> version_payload = BuildVersionPayload(m_local_nonce);
    std::vector<uint8_t> version_msg;
    if (!EncodeMessage(magic, "version", version_payload, version_msg)) {
        return false;
    }
    if (!m_sock.SendAll(version_msg.data(), version_msg.size())) {
        return false;
    }

    // 2. 读取 VERSION（对端），校验 NODE_BLOOM
    bool got_version = false;
    uint64_t remote_services = 0;
    while (!got_version) {
        LightMessage msg;
        if (!ReadMessage(m_sock, magic, msg)) {
            return false;
        }
        if (msg.command == "version") {
            if (!ParseVersionServices(msg.payload, remote_services)) {
                return false;
            }
            got_version = true;
        } else if (msg.command == "verack") {
            // 对端先发 verack 也接受（极端时序），继续等 version
            continue;
        }
    }
    if ((remote_services & kServiceNodeBloom) == 0) {
        return false; // ERR_PEER_NO_BLOOM
    }

    // 3. 发送 VERACK
    std::vector<uint8_t> verack_msg;
    if (!EncodeMessage(magic, "verack", {}, verack_msg)) {
        return false;
    }
    if (!m_sock.SendAll(verack_msg.data(), verack_msg.size())) {
        return false;
    }

    // 4. 读取 VERACK
    bool got_verack = false;
    while (!got_verack) {
        LightMessage msg;
        if (!ReadMessage(m_sock, magic, msg)) {
            return false;
        }
        if (msg.command == "verack") {
            got_verack = true;
        } else if (msg.command == "version") {
            // 对端延迟重发 version，忽略
            continue;
        }
    }

    SetState(PeerState::FILTER_SENT);
    return true;
}

void CLightPeer::Disconnect() {
    m_sock.Close();
    m_state = PeerState::DISCONNECTED;
    m_pending_pong_nonce = 0;
}

bool CLightPeer::SendPing(uint64_t nonce) {
    if (m_state != PeerState::FILTER_SENT && m_state != PeerState::STEADY) {
        return false;
    }
    std::vector<uint8_t> payload;
    WriteLE64(payload, nonce);
    std::vector<uint8_t> msg;
    if (!EncodeMessage(MainnetMagic(), "ping", payload, msg)) {
        return false;
    }
    if (!m_sock.SendAll(msg.data(), msg.size())) {
        return false;
    }
    m_pending_pong_nonce = nonce;
    m_ping_sent_usec = NowUsec();
    return true;
}

bool CLightPeer::CheckPingTimeout(int64_t now_usec) {
    if (m_pending_pong_nonce == 0 || m_ping_sent_usec == 0) {
        return false;
    }
    int64_t elapsed_usec = now_usec - m_ping_sent_usec;
    if (elapsed_usec < static_cast<int64_t>(kPongTimeoutMs) * 1000) {
        return false;
    }
    ++m_pong_timeout_count;
    m_pending_pong_nonce = 0;
    m_ping_sent_usec = 0;
    if (m_pong_timeout_count >= 2) {
        Disconnect(); // 连续 2 次超时 → 判定连接失效
    }
    return true;
}

bool CLightPeer::HandleMessage(const LightMessage& msg) {
    if (msg.command == "pong") {
        uint64_t nonce = 0;
        if (!ParsePongNonce(msg.payload, nonce)) {
            return false;
        }
        if (nonce == m_pending_pong_nonce && m_ping_sent_usec != 0) {
            m_last_ping_usec = NowUsec() - m_ping_sent_usec;
            m_pending_pong_nonce = 0;
            m_ping_sent_usec = 0;
            m_pong_timeout_count = 0;
        }
        return true;
    }
    if (msg.command == "ping") {
        // 对端 ping：回 pong（Phase 1 骨架；Phase 4 完善）
        std::vector<uint8_t> pong_payload = msg.payload;
        std::vector<uint8_t> pong_msg;
        if (EncodeMessage(MainnetMagic(), "pong", pong_payload, pong_msg)) {
            m_sock.SendAll(pong_msg.data(), pong_msg.size());
        }
        return true;
    }
    // 其他消息在后续 Phase 处理；Phase 1 忽略
    return true;
}

bool CLightPeer::ReadAndHandle(const uint8_t magic[4]) {
    LightMessage msg;
    if (!ReadMessage(m_sock, magic, msg)) {
        return false;
    }
    return HandleMessage(msg);
}

bool CLightPeer::SetState(PeerState state) {
    if (!IsValidTransition(m_state, state)) {
        return false;
    }
    m_state = state;
    return true;
}

uint32_t CLightPeer::NextRetryDelayMs(int attempt, double jitter_ratio) {
    if (attempt < 0) attempt = 0;
    uint64_t base = kRetryBaseMs;
    for (int i = 0; i < attempt && base < kRetryMaxMs; ++i) {
        base *= 2;
        if (base > kRetryMaxMs) base = kRetryMaxMs;
    }
    if (base > kRetryMaxMs) base = kRetryMaxMs;
    if (jitter_ratio < -0.2) jitter_ratio = -0.2;
    if (jitter_ratio > 0.2) jitter_ratio = 0.2;
    double v = static_cast<double>(base) * (1.0 + jitter_ratio);
    if (v < 1.0) v = 1.0;
    return static_cast<uint32_t>(v);
}

} // namespace mvclight
