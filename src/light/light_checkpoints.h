#ifndef MVC_LIGHT_LIGHT_CHECKPOINTS_H
#define MVC_LIGHT_LIGHT_CHECKPOINTS_H

/*
 * 内置 Checkpoint（设计文档 §7）。
 * 发布时需替换为真实主网 (height, hash, nChainWork) 三元组。
 */

#include "light/light_uint256.h"

#include <cstdint>

namespace mvclight {

struct LightCheckpoint {
    int64_t height = 0;
    uint256 hash;
    uint256 nChainWork;
};

// 比较累计工作量（按大端数值序；uint256 内部为小端存储）
bool ChainWorkGe(const uint256& a, const uint256& b);

// 内置 Checkpoint（Phase 2 占位；发布前替换真实值）
const LightCheckpoint& GetBuiltinCheckpoint();

} // namespace mvclight

#endif // MVC_LIGHT_LIGHT_CHECKPOINTS_H
