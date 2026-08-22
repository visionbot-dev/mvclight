#include "light/light_merkle.h"

#include "light/light_sha256.h"

#include <vector>

namespace mvclight {

uint256 ComputeMerkleRoot(const std::vector<uint256>& txids) {
    if (txids.empty()) return uint256();
    std::vector<uint256> level = txids;
    while (level.size() > 1) {
        if (level.size() % 2 == 1) {
            level.push_back(level.back());
        }
        std::vector<uint256> next;
        next.reserve(level.size() / 2);
        for (size_t i = 0; i < level.size(); i += 2) {
            std::vector<uint8_t> buf;
            buf.insert(buf.end(), level[i].begin(), level[i].end());
            buf.insert(buf.end(), level[i + 1].begin(), level[i + 1].end());
            uint8_t hash[32];
            SHA256D(buf.data(), buf.size(), hash);
            next.emplace_back(std::vector<uint8_t>(hash, hash + 32));
        }
        level.swap(next);
    }
    return level[0];
}

bool LightMerkleBlock::Verify() const {
    if (txids.empty()) return false;
    uint256 root = ComputeMerkleRoot(txids);
    return root == header.hashMerkleRoot;
}

} // namespace mvclight
