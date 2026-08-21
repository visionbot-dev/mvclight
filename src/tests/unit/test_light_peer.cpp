#include "light/light_peer.h"
#include "light/light_state.h"

#include "test_framework.h"

using mvclight::CLightPeer;
using mvclight::PeerState;

int main() {
    // 重连退避（无抖动）
    CHECK_EQ(CLightPeer::NextRetryDelayMs(0), 5000u);
    CHECK_EQ(CLightPeer::NextRetryDelayMs(1), 10000u);
    CHECK_EQ(CLightPeer::NextRetryDelayMs(2), 20000u);
    CHECK_EQ(CLightPeer::NextRetryDelayMs(3), 40000u);
    CHECK_EQ(CLightPeer::NextRetryDelayMs(4), 80000u);
    CHECK_EQ(CLightPeer::NextRetryDelayMs(5), 160000u);
    CHECK_EQ(CLightPeer::NextRetryDelayMs(6), 300000u); // 封顶
    CHECK_EQ(CLightPeer::NextRetryDelayMs(10), 300000u);

    // 抖动范围 ±20%
    for (int attempt = 0; attempt <= 6; ++attempt) {
        uint32_t base = CLightPeer::NextRetryDelayMs(attempt, 0.0);
        uint32_t lo = CLightPeer::NextRetryDelayMs(attempt, -0.2);
        uint32_t hi = CLightPeer::NextRetryDelayMs(attempt, 0.2);
        CHECK(lo >= static_cast<uint32_t>(base * 0.8 - 1));
        CHECK(hi <= static_cast<uint32_t>(base * 1.2 + 1));
        CHECK(lo <= hi);
    }

    // 状态机：SetState 仅允许合法迁移
    CLightPeer peer;
    CHECK(peer.GetState() == PeerState::INIT);
    CHECK(peer.SetState(PeerState::HANDSHAKE));
    CHECK(!peer.SetState(PeerState::STEADY)); // 非法
    CHECK(peer.GetState() == PeerState::HANDSHAKE);
    CHECK(peer.SetState(PeerState::FILTER_SENT));
    CHECK(peer.SetState(PeerState::DISCONNECTED));

    TEST_MAIN_RETURN();
}
