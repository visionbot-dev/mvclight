#include "light/light_chainstore.h"
#include "light/light_header.h"

#include "test_framework.h"

using mvclight::CLightChainStore;
using mvclight::LightBlockHeader;
using mvclight::uint256S;

static LightBlockHeader MakeHeader(uint32_t time) {
    LightBlockHeader h;
    h.nVersion = 4;
    h.nTime = time;
    h.nBits = 0x207fffff;
    return h;
}

int main() {
    CLightChainStore store;

    // 空链 MTP = 0
    CHECK(store.GetMedianTimePast(0) == 0);

    // 11 个时间戳 1..11 -> 中位数 6
    for (int i = 1; i <= 11; ++i) {
        store.AddHeader(MakeHeader(static_cast<uint32_t>(i)), i - 1);
    }
    CHECK(store.GetMedianTimePast(10) == 6);
    CHECK(store.GetMedianTimePast(5) == 4); // 1..6 的中位数（上中位）

    // 超过 11 块时只取最近 11 块
    for (int i = 12; i <= 20; ++i) {
        store.AddHeader(MakeHeader(static_cast<uint32_t>(i)), i - 1);
    }
    // height 19：最近 11 块 time = 10..20 -> 中位数 15
    CHECK(store.GetMedianTimePast(19) == 15);

    TEST_MAIN_RETURN();
}
