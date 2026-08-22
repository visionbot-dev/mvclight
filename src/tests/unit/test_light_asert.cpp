#include "light/light_arith_uint256.h"
#include "light/light_asert.h"
#include "light/light_header.h"

#include "test_framework.h"

using mvclight::arith_uint256;
using mvclight::CalculateASERT;
using mvclight::GetNextASERTBits;
using mvclight::LightASERTParams;
using mvclight::LightBlockHeader;

int main() {
    LightASERTParams params;

    // 恰好按理想节奏出块 -> 指数 0 -> 目标不变
    {
        arith_uint256 ref;
        ref.SetCompact(params.nBits);
        arith_uint256 pow_limit;
        pow_limit.SetCompact(params.nPowLimit);
        int64_t spacing = params.nPowTargetSpacing;
        int64_t height_diff = 10;
        int64_t time_diff = spacing * (height_diff + 1);
        arith_uint256 next = CalculateASERT(ref, spacing, time_diff, height_diff,
                                            pow_limit, params.nHalfLife);
        CHECK(next == ref);
    }

    // 出块落后 -> 目标变大（难度降低）
    {
        arith_uint256 ref;
        ref.SetCompact(params.nBits);
        arith_uint256 pow_limit;
        pow_limit.SetCompact(params.nPowLimit);
        int64_t spacing = params.nPowTargetSpacing;
        int64_t height_diff = 10;
        int64_t time_diff = spacing * (height_diff + 1) + params.nHalfLife; // 落后一个半衰期
        arith_uint256 next = CalculateASERT(ref, spacing, time_diff, height_diff,
                                            pow_limit, params.nHalfLife);
        CHECK(next > ref);
    }

    // prev 高度低于锚点 -> 沿用 prev.nBits
    {
        LightBlockHeader prev;
        prev.nBits = 0x207fffff;
        prev.nTime = 1000;
        CHECK(GetNextASERTBits(prev, 100, params) == 0x207fffff);
    }

    // prev 高度高于锚点 -> 返回 ASERT 计算结果（非零且有效）
    {
        LightBlockHeader prev;
        prev.nBits = params.nBits;
        prev.nTime = static_cast<uint32_t>(params.nPrevBlockTime + 600 * 11);
        uint32_t bits = GetNextASERTBits(prev, params.nHeight + 10, params);
        CHECK(bits != 0);
    }

    TEST_MAIN_RETURN();
}
