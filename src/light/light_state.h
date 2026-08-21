#ifndef MVC_LIGHT_LIGHT_STATE_H
#define MVC_LIGHT_LIGHT_STATE_H

/*
 * P2P 状态机（设计文档 §4.1.1 / 附录 A）。
 * 所有状态迁移必须经过 IsValidTransition 校验，禁止业务线程直接改写状态。
 */

namespace mvclight {

enum class PeerState {
    INIT,
    HANDSHAKE,
    FILTER_SENT,
    WAIT_FILTER_ACK,
    SYNCING_HEADERS,
    SYNCING_TXS,
    STEADY,
    DISCONNECTED,
};

// 是否允许 from -> to 迁移
bool IsValidTransition(PeerState from, PeerState to);

const char* ToString(PeerState state);

} // namespace mvclight

#endif // MVC_LIGHT_LIGHT_STATE_H
