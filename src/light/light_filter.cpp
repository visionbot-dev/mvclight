#include "light/light_filter.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace mvclight {

namespace {

// BIP-37 使用的 MurmurHash3 x86_32
uint32_t MurmurHash3(const uint8_t* data, size_t len, uint32_t seed) {
    uint32_t h = seed;
    const uint32_t c1 = 0xcc9e2d51;
    const uint32_t c2 = 0x1b873593;
    const size_t nblocks = len / 4;
    const uint32_t* blocks = reinterpret_cast<const uint32_t*>(data);
    for (size_t i = 0; i < nblocks; ++i) {
        uint32_t k = blocks[i];
        k *= c1;
        k = (k << 15) | (k >> 17);
        k *= c2;
        h ^= k;
        h = (h << 13) | (h >> 19);
        h = h * 5 + 0xe6546b64;
    }
    const uint8_t* tail = data + nblocks * 4;
    uint32_t k = 0;
    switch (len & 3) {
        case 3: k ^= tail[2] << 16; [[fallthrough]];
        case 2: k ^= tail[1] << 8; [[fallthrough]];
        case 1:
            k ^= tail[0];
            k *= c1;
            k = (k << 15) | (k >> 17);
            k *= c2;
            h ^= k;
    }
    h ^= static_cast<uint32_t>(len);
    h ^= h >> 16;
    h *= 0x85ebca6b;
    h ^= h >> 13;
    h *= 0xc2b2ae35;
    h ^= h >> 16;
    return h;
}

void WriteLE32(std::vector<uint8_t>& out, uint32_t v) {
    for (int i = 0; i < 4; ++i) out.push_back(static_cast<uint8_t>(v >> (i * 8)));
}

void WriteCompactSize(std::vector<uint8_t>& out, uint64_t v) {
    if (v < 253) {
        out.push_back(static_cast<uint8_t>(v));
    } else if (v <= 0xFFFF) {
        out.push_back(253);
        out.push_back(static_cast<uint8_t>(v));
        out.push_back(static_cast<uint8_t>(v >> 8));
    } else if (v <= 0xFFFFFFFF) {
        out.push_back(254);
        for (int i = 0; i < 4; ++i) out.push_back(static_cast<uint8_t>(v >> (i * 8)));
    } else {
        out.push_back(255);
        for (int i = 0; i < 8; ++i) out.push_back(static_cast<uint8_t>(v >> (i * 8)));
    }
}

} // namespace

CBloomFilter CBloomFilter::Create(size_t nElements, double fp_rate, uint32_t nTweak,
                                  uint8_t nFlags) {
    CBloomFilter f;
    // 与上游 CBloomFilter 相同的最小尺寸公式
    size_t nBytes = static_cast<size_t>(
        std::min(static_cast<double>(kMaxFilterBytes),
                 (-1.0 / std::log(2.0) * static_cast<double>(nElements) *
                  std::log(fp_rate) / 8.0) + 1.0));
    if (nBytes < 1) nBytes = 1;
    if (nBytes > kMaxFilterBytes) nBytes = kMaxFilterBytes;
    size_t nHashFuncs = static_cast<size_t>(
        std::min(50.0, static_cast<double>(nBytes) * 8.0 / static_cast<double>(nElements) *
                           std::log(2.0)));
    if (nHashFuncs < 1) nHashFuncs = 1;

    f.m_vData.assign(nBytes, 0);
    f.m_nHashFuncs = static_cast<uint32_t>(nHashFuncs);
    f.m_nTweak = nTweak;
    f.m_nFlags = nFlags;
    f.m_element_count = 0;
    return f;
}

bool CBloomFilter::Hash(uint32_t nHashNum, const uint8_t* data, size_t len,
                        uint32_t& out) const {
    if (m_nHashFuncs == 0 || m_vData.empty()) return false;
    uint32_t seed = nHashNum * 0xFBA4C795 + m_nTweak;
    out = MurmurHash3(data, len, seed);
    return true;
}

bool CBloomFilter::Insert(const uint8_t* data, size_t len) {
    if (IsFull()) return false;
    for (uint32_t i = 0; i < m_nHashFuncs; ++i) {
        uint32_t h = 0;
        if (!Hash(i, data, len, h)) return false;
        size_t bit = h % (m_vData.size() * 8);
        m_vData[bit / 8] |= static_cast<uint8_t>(1 << (bit & 7));
    }
    ++m_element_count;
    return true;
}

bool CBloomFilter::Contains(const uint8_t* data, size_t len) const {
    for (uint32_t i = 0; i < m_nHashFuncs; ++i) {
        uint32_t h = 0;
        if (!Hash(i, data, len, h)) return false;
        size_t bit = h % (m_vData.size() * 8);
        if ((m_vData[bit / 8] & static_cast<uint8_t>(1 << (bit & 7))) == 0) {
            return false;
        }
    }
    return true;
}

bool CBloomFilter::Serialize(std::vector<uint8_t>& out) const {
    if (m_vData.size() > kMaxFilterBytes) return false;
    WriteCompactSize(out, m_vData.size());
    out.insert(out.end(), m_vData.begin(), m_vData.end());
    WriteLE32(out, m_nHashFuncs);
    WriteLE32(out, m_nTweak);
    out.push_back(m_nFlags);
    return true;
}

} // namespace mvclight
