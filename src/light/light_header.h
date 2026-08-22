#ifndef MVC_LIGHT_LIGHT_HEADER_H
#define MVC_LIGHT_LIGHT_HEADER_H

/*
 * 最小区块头（Phase 2 自包含实现，不依赖上游 primitives/block）。
 * 80 字节，与 P2P HEADERS 消息兼容。
 */

#include "light/light_uint256.h"

#include <cstdint>
#include <vector>

namespace mvclight {

struct LightBlockHeader {
    static constexpr size_t kHeaderSize = 80;

    int32_t nVersion = 0;
    uint256 hashPrevBlock;
    uint256 hashMerkleRoot;
    uint32_t nTime = 0;
    uint32_t nBits = 0;
    uint32_t nNonce = 0;

    std::vector<uint8_t> Serialize() const;
    bool Deserialize(const uint8_t* data, size_t len);

    // 区块哈希 = SHA256d(Serialize())
    uint256 GetHash() const;

    bool IsNull() const { return nVersion == 0 && hashPrevBlock.IsNull() && nTime == 0; }
};

} // namespace mvclight

#endif // MVC_LIGHT_LIGHT_HEADER_H
