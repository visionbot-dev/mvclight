#include "light/light_connman.h"

namespace mvclight {

CLightConnMan::CLightConnMan(uint32_t connect_timeout_ms,
                             uint32_t recv_timeout_ms)
    : m_connect_timeout_ms(connect_timeout_ms),
      m_recv_timeout_ms(recv_timeout_ms) {}

bool CLightConnMan::AddPeer(const std::string& host, uint16_t port) {
    if (host.empty() || port == 0) return false;
    Slot s;
    s.host = host;
    s.port = port;
    m_slots.push_back(std::move(s));
    return true;
}

bool CLightConnMan::ConnectPeer(size_t idx) {
    if (idx >= m_slots.size()) return false;
    Slot& s = m_slots[idx];
    if (s.connected) return true;
    if (!s.peer) {
        s.peer = std::make_unique<CLightPeer>();
    }
    if (s.peer->ConnectAndHandshake(s.host, s.port, m_connect_timeout_ms,
                                    m_recv_timeout_ms)) {
        s.connected = true;
        return true;
    }
    return false;
}

void CLightConnMan::DisconnectPeer(size_t idx) {
    if (idx >= m_slots.size()) return;
    Slot& s = m_slots[idx];
    if (s.peer) s.peer->Disconnect();
    s.connected = false;
}

bool CLightConnMan::IsConnected(size_t idx) const {
    if (idx >= m_slots.size()) return false;
    return m_slots[idx].connected;
}

CLightPeer* CLightConnMan::Peer(size_t idx) {
    if (idx >= m_slots.size()) return nullptr;
    return m_slots[idx].peer.get();
}

size_t CLightConnMan::ConnectUpTo(size_t max_conns) {
    size_t connected = 0;
    for (size_t i = 0; i < m_slots.size() && connected < max_conns; ++i) {
        if (ConnectPeer(i)) ++connected;
    }
    return connected;
}

bool CLightConnMan::PromoteToPrimary(size_t idx) {
    if (idx == 0) return IsConnected(0);
    if (idx >= m_slots.size() || !IsConnected(idx)) return false;
    std::swap(m_slots[0], m_slots[idx]);
    return true;
}

std::string CLightConnMan::PeerName(size_t idx) const {
    if (idx >= m_slots.size()) return "";
    return m_slots[idx].host + ":" + std::to_string(m_slots[idx].port);
}

} // namespace mvclight
