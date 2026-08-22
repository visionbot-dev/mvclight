#include "light/light_chainstore.h"

#include <algorithm>
#include <vector>

namespace mvclight {

bool CLightChainStore::AddHeader(const LightBlockHeader& header, int64_t height) {
    uint256 h = header.GetHash();
    m_by_height[height] = header;
    m_by_hash[h] = height;
    if (height > m_tip_height) {
        m_tip_height = height;
    }
    return true;
}

bool CLightChainStore::GetHeaderAtHeight(int64_t height, LightBlockHeader& out) const {
    auto it = m_by_height.find(height);
    if (it == m_by_height.end()) return false;
    out = it->second;
    return true;
}

bool CLightChainStore::GetHeightByHash(const uint256& hash, int64_t& height) const {
    auto it = m_by_hash.find(hash);
    if (it == m_by_hash.end()) return false;
    height = it->second;
    return true;
}

bool CLightChainStore::HasHash(const uint256& hash) const {
    return m_by_hash.count(hash) != 0;
}

bool CLightChainStore::GetTip(LightBlockHeader& out) const {
    return GetHeaderAtHeight(m_tip_height, out);
}

int64_t CLightChainStore::GetMedianTimePast(int64_t height) const {
    std::vector<uint32_t> times;
    for (int64_t h = height; h >= 0 && times.size() < 11; --h) {
        LightBlockHeader hdr;
        if (!GetHeaderAtHeight(h, hdr)) break;
        times.push_back(hdr.nTime);
    }
    if (times.empty()) return 0;
    std::sort(times.begin(), times.end());
    return times[times.size() / 2];
}

void CLightChainStore::AddWork(uint32_t nBits) {
    // 与上游 GetBlockProof 一致：work = (~target / (target + 1)) + 1
    arith_uint256 target;
    target.SetCompact(nBits);
    arith_uint256 work = (~target / (target + arith_uint256(1))) + arith_uint256(1);
    m_chainwork_arith += work;
    m_chainwork = m_chainwork_arith.ToUint256();
}

void CLightChainStore::Reset() {
    m_by_height.clear();
    m_by_hash.clear();
    m_tip_height = -1;
    m_chainwork.SetNull();
    m_chainwork_arith = arith_uint256();
}

} // namespace mvclight
