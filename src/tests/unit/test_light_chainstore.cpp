#include "light/light_chainstore.h"
#include "light/light_arith_uint256.h"
#include "light/light_checkpoints.h"
#include "light/light_header.h"

#include "test_framework.h"

using mvclight::arith_uint256;
using mvclight::CLightChainStore;
using mvclight::ChainWorkGe;
using mvclight::LightBlockHeader;
using mvclight::uint256;
using mvclight::uint256S;

static LightBlockHeader MakeHeader(uint32_t time, uint32_t bits = 0x207fffff) {
    LightBlockHeader h;
    h.nVersion = 4;
    h.nTime = time;
    h.nBits = bits;
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

    // arith_uint256 除法（供 nChainWork 使用）
    CHECK(arith_uint256(10) / arith_uint256(3) == arith_uint256(3));
    CHECK(arith_uint256(1) / arith_uint256(1) == arith_uint256(1));

    // 真实 nChainWork：难度越高（target 越小）单块工作量越大
    {
        CLightChainStore easy, hard;
        easy.AddHeader(MakeHeader(1, 0x1d00ffff), 0);
        easy.AddWork(0x1d00ffff);
        hard.AddHeader(MakeHeader(1, 0x1bffff7f), 0);
        hard.AddWork(0x1bffff7f);
        CHECK(ChainWorkGe(hard.ChainWork(), easy.ChainWork()));
        CHECK(!hard.ChainWork().IsNull());
    }

    // 多块累计：nChainWork 单调递增
    {
        CLightChainStore acc;
        uint256 prev_work;
        for (int i = 0; i < 3; ++i) {
            acc.AddHeader(MakeHeader(static_cast<uint32_t>(1 + i), 0x1d00ffff), i);
            acc.AddWork(0x1d00ffff);
            CHECK(ChainWorkGe(acc.ChainWork(), prev_work));
            prev_work = acc.ChainWork();
        }
    }

    TEST_MAIN_RETURN();
}
