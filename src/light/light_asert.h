#ifndef MVC_LIGHT_LIGHT_ASERT_H
#define MVC_LIGHT_LIGHT_ASERT_H

/*
 * ASERT 难度算法（设计文档 §4.3，MVC 主网当前 DAA）。
 * 参数来源：上游 chainparams.cpp 主网 asertAnchorParams。
 */

#include "light/light_arith_uint256.h"
#include "light/light_header.h"

#include <cstdint>

namespace mvclight {

struct LightASERTParams {
    int64_t nHeight = 21256;          // 锚点高度
    uint32_t nBits = 0x1836845b;      // 锚点 nBits
    int64_t nPrevBlockTime = 1686641614; // 锚点前块时间
    int64_t nPowTargetSpacing = 600;  // 10 分钟
    int64_t nHalfLife = 2 * 24 * 60 * 60; // 2 天
    uint32_t nPowLimit = 0x1d00ffff;
};

// ASERT 目标计算（与上游 CalculateASERT 一致）
arith_uint256 CalculateASERT(const arith_uint256& refTarget, int64_t nPowTargetSpacing,
                             int64_t nTimeDiff, int64_t nHeightDiff,
                             const arith_uint256& powLimit, int64_t nHalfLife);

// 计算 prev 之后下一个区块应使用的 nBits
uint32_t GetNextASERTBits(const LightBlockHeader& prev, int64_t prev_height,
                          const LightASERTParams& params);

} // namespace mvclight

#endif // MVC_LIGHT_LIGHT_ASERT_H
