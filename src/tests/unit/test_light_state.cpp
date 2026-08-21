#include "light/light_state.h"

#include "test_framework.h"

#include <string>

using mvclight::IsValidTransition;
using mvclight::PeerState;

int main() {
    // 合法主链路
    CHECK(IsValidTransition(PeerState::INIT, PeerState::HANDSHAKE));
    CHECK(IsValidTransition(PeerState::HANDSHAKE, PeerState::FILTER_SENT));
    CHECK(IsValidTransition(PeerState::FILTER_SENT, PeerState::WAIT_FILTER_ACK));
    CHECK(IsValidTransition(PeerState::WAIT_FILTER_ACK, PeerState::SYNCING_HEADERS));
    CHECK(IsValidTransition(PeerState::SYNCING_HEADERS, PeerState::SYNCING_TXS));
    CHECK(IsValidTransition(PeerState::SYNCING_TXS, PeerState::STEADY));
    CHECK(IsValidTransition(PeerState::DISCONNECTED, PeerState::HANDSHAKE)); // 重连

    // 任何状态可进入 DISCONNECTED
    CHECK(IsValidTransition(PeerState::STEADY, PeerState::DISCONNECTED));
    CHECK(IsValidTransition(PeerState::SYNCING_HEADERS, PeerState::DISCONNECTED));

    // 非法迁移必须被拒绝
    CHECK(!IsValidTransition(PeerState::INIT, PeerState::STEADY));
    CHECK(!IsValidTransition(PeerState::INIT, PeerState::FILTER_SENT));
    CHECK(!IsValidTransition(PeerState::HANDSHAKE, PeerState::SYNCING_TXS));
    CHECK(!IsValidTransition(PeerState::FILTER_SENT, PeerState::STEADY));
    CHECK(!IsValidTransition(PeerState::STEADY, PeerState::INIT));

    // 幂等迁移
    CHECK(IsValidTransition(PeerState::STEADY, PeerState::STEADY));

    // ToString
    CHECK(mvclight::ToString(PeerState::INIT) == std::string("INIT"));
    CHECK(mvclight::ToString(PeerState::STEADY) == std::string("STEADY"));
    CHECK(mvclight::ToString(PeerState::DISCONNECTED) == std::string("DISCONNECTED"));

    TEST_MAIN_RETURN();
}
