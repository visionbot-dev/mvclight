#include "light/light_state.h"

namespace mvclight {

bool IsValidTransition(PeerState from, PeerState to) {
    if (from == to) {
        return true; // 幂等迁移允许（便于重入）
    }
    // 任何状态都允许进入 DISCONNECTED（断连/错误）
    if (to == PeerState::DISCONNECTED) {
        return true;
    }
    switch (from) {
        case PeerState::INIT:
            return to == PeerState::HANDSHAKE;
        case PeerState::HANDSHAKE:
            return to == PeerState::FILTER_SENT; // 收到 verack
        case PeerState::FILTER_SENT:
            return to == PeerState::WAIT_FILTER_ACK; // FILTERLOAD 已写入 Socket
        case PeerState::WAIT_FILTER_ACK:
            return to == PeerState::SYNCING_HEADERS; // 收到首个 HEADERS
        case PeerState::SYNCING_HEADERS:
            return to == PeerState::SYNCING_TXS; // headers 追平
        case PeerState::SYNCING_TXS:
            return to == PeerState::STEADY; // PendingTxMap 清空 + 首轮过滤完成
        case PeerState::STEADY:
            return to == PeerState::HANDSHAKE; // 重连
        case PeerState::DISCONNECTED:
            return to == PeerState::HANDSHAKE; // 重连
    }
    return false;
}

const char* ToString(PeerState state) {
    switch (state) {
        case PeerState::INIT: return "INIT";
        case PeerState::HANDSHAKE: return "HANDSHAKE";
        case PeerState::FILTER_SENT: return "FILTER_SENT";
        case PeerState::WAIT_FILTER_ACK: return "WAIT_FILTER_ACK";
        case PeerState::SYNCING_HEADERS: return "SYNCING_HEADERS";
        case PeerState::SYNCING_TXS: return "SYNCING_TXS";
        case PeerState::STEADY: return "STEADY";
        case PeerState::DISCONNECTED: return "DISCONNECTED";
    }
    return "UNKNOWN";
}

} // namespace mvclight
