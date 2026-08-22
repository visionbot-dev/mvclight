#include "light/light_peer_policy.h"

#include "test_framework.h"

using mvclight::CLightPeerPolicy;

int main() {
    CLightPeerPolicy p;

    // getheaders 限速：第一次允许，立即第二次拒绝，间隔后允许
    const int64_t t0 = 1000000;
    CHECK(p.AllowGetHeaders(t0));
    CHECK(!p.AllowGetHeaders(t0 + 10));
    CHECK(p.AllowGetHeaders(t0 + CLightPeerPolicy::kGetHeadersMinIntervalMs));

    // filterload 重建限速
    CHECK(p.AllowFilterReload(t0));
    CHECK(!p.AllowFilterReload(t0 + 1000));
    CHECK(p.AllowFilterReload(t0 + CLightPeerPolicy::kFilterReloadMinIntervalMs));

    // Bloom 预检
    CHECK(p.IsValidBloom(100, 1024, 10, 2));
    CHECK(!p.IsValidBloom(CLightPeerPolicy::kMaxBloomElements + 1, 1024, 10, 2));
    CHECK(!p.IsValidBloom(100, CLightPeerPolicy::kMaxBloomBytes + 1, 10, 2));
    CHECK(!p.IsValidBloom(100, 1024, CLightPeerPolicy::kMaxBloomHashFuncs + 1, 2));
    CHECK(!p.IsValidBloom(100, 1024, 10, 3));

    // 常量与上游一致
    CHECK(CLightPeerPolicy::kBanScoreThreshold == 100);
    CHECK(CLightPeerPolicy::kMaxGetHeadersBatch == 2000);
    CHECK(CLightPeerPolicy::kPingIntervalMs == 120000);

    TEST_MAIN_RETURN();
}
