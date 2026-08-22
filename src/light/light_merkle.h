#ifndef MVC_LIGHT_LIGHT_MERKLE_H
#define MVC_LIGHT_LIGHT_MERKLE_H

/*
 * 简化默克尔证明（Phase 3 自包含实现）。
 * 真实协议需使用上游 CPartialMerkleTree/ExtractMatches；当前实现覆盖根校验语义。
 */

#include "light/light_header.h"
#include "light/light_uint256.h"

#include <vector>

namespace mvclight {

uint256 ComputeMerkleRoot(const std::vector<uint256>& txids);

struct LightMerkleBlock {
    LightBlockHeader header;
    std::vector<uint256> txids;

    // 校验 txids 的默克尔根 == header.hashMerkleRoot
    bool Verify() const;
};

} // namespace mvclight

#endif // MVC_LIGHT_LIGHT_MERKLE_H
