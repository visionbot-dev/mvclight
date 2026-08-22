#ifndef MVC_LIGHT_LIGHT_MERKLE_H
#define MVC_LIGHT_LIGHT_MERKLE_H

/*
 * BIP-37 部分默克尔树（Phase 5 完整实现，兼容真实节点 MERKLEBLOCK）。
 *
 * 序列化格式：
 *   header(80) + nTransactions(4 LE) + vBits(CompactSize+bitpack) + vHash(CompactSize+hashes)
 */

#include "light/light_header.h"
#include "light/light_uint256.h"

#include <cstdint>
#include <vector>

namespace mvclight {

uint256 ComputeMerkleRoot(const std::vector<uint256>& txids);

class CLightPartialMerkleTree {
public:
    bool Deserialize(const uint8_t*& p, const uint8_t* end);
    std::vector<uint8_t> Serialize() const;

    // 提取匹配 txid 并计算根；失败返回 false
    bool ExtractMatches(std::vector<uint256>& vMatch, uint256& root) const;

    uint32_t nTransactions = 0;
    std::vector<uint8_t> vBits;   // 位打包（LSB-first）
    std::vector<uint256> vHash;
};

struct LightMerkleBlock {
    LightBlockHeader header;
    CLightPartialMerkleTree txn;

    bool Deserialize(const uint8_t* data, size_t len);

    // 提取匹配 txid 并校验根 == header.hashMerkleRoot
    bool ExtractMatches(std::vector<uint256>& vMatch, uint256& root) const;
};

} // namespace mvclight

#endif // MVC_LIGHT_LIGHT_MERKLE_H
