#include "light/light_asert.h"

#include <cassert>
#include <cstdlib>

namespace mvclight {

arith_uint256 CalculateASERT(const arith_uint256& refTarget, int64_t nPowTargetSpacing,
                             int64_t nTimeDiff, int64_t nHeightDiff,
                             const arith_uint256& powLimit, int64_t nHalfLife) {
    // 与上游 pow.cpp CalculateASERT 一致
    const int64_t exponent =
        ((nTimeDiff - nPowTargetSpacing * (nHeightDiff + 1)) * 65536) / nHalfLife;

    int64_t shifts = exponent >> 16;
    const auto frac = uint16_t(exponent);

    const uint32_t factor = 65536 + ((195766423245049ull * frac +
                                      971821376ull * frac * frac +
                                      5127ull * frac * frac * frac +
                                      (1ull << 47)) >> 48);

    arith_uint256 nextTarget = refTarget * factor;

    shifts -= 16;
    if (shifts <= 0) {
        nextTarget >>= static_cast<int>(-shifts);
    } else {
        arith_uint256 shifted = nextTarget << static_cast<int>(shifts);
        if ((shifted >> static_cast<int>(shifts)) != nextTarget) {
            nextTarget = powLimit;
        } else {
            nextTarget = shifted;
        }
    }

    if (nextTarget.IsZero()) {
        nextTarget = arith_uint256(1);
    } else if (nextTarget > powLimit) {
        nextTarget = powLimit;
    }
    return nextTarget;
}

uint32_t GetNextASERTBits(const LightBlockHeader& prev, int64_t prev_height,
                          const LightASERTParams& params) {
    if (prev_height < params.nHeight) {
        return prev.nBits;
    }
    arith_uint256 pow_limit;
    pow_limit.SetCompact(params.nPowLimit);
    arith_uint256 ref_target;
    ref_target.SetCompact(params.nBits);

    const int64_t nTimeDiff = static_cast<int64_t>(prev.nTime) - params.nPrevBlockTime;
    const int64_t nHeightDiff = prev_height - params.nHeight;

    arith_uint256 next = CalculateASERT(ref_target, params.nPowTargetSpacing, nTimeDiff,
                                        nHeightDiff, pow_limit, params.nHalfLife);
    return next.GetCompact();
}

} // namespace mvclight
