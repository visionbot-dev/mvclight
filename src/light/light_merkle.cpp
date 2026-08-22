#include "light/light_merkle.h"

#include "light/light_sha256.h"

#include <cstring>
#include <functional>

namespace mvclight {

namespace {

uint32_t ReadLE32(const uint8_t* p) {
    return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}

void WriteLE32(std::vector<uint8_t>& out, uint32_t v) {
    for (int i = 0; i < 4; ++i) out.push_back(static_cast<uint8_t>(v >> (i * 8)));
}

uint64_t ReadCompactSize(const uint8_t*& p, const uint8_t* end, bool& ok) {
    if (p >= end) { ok = false; return 0; }
    uint8_t first = *p++;
    if (first < 253) return first;
    if (first == 253) {
        if (p + 2 > end) { ok = false; return 0; }
        uint64_t v = p[0] | (uint64_t(p[1]) << 8);
        p += 2;
        return v;
    }
    if (first == 254) {
        if (p + 4 > end) { ok = false; return 0; }
        uint64_t v = 0;
        for (int i = 0; i < 4; ++i) v |= uint64_t(p[i]) << (i * 8);
        p += 4;
        return v;
    }
    if (p + 8 > end) { ok = false; return 0; }
    uint64_t v = 0;
    for (int i = 0; i < 8; ++i) v |= uint64_t(p[i]) << (i * 8);
    p += 8;
    return v;
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

uint256 HashPair(const uint256& a, const uint256& b) {
    std::vector<uint8_t> buf;
    buf.insert(buf.end(), a.begin(), a.end());
    buf.insert(buf.end(), b.begin(), b.end());
    uint8_t hash[32];
    SHA256D(buf.data(), buf.size(), hash);
    return uint256(std::vector<uint8_t>(hash, hash + 32));
}

} // namespace

uint256 ComputeMerkleRoot(const std::vector<uint256>& txids) {
    if (txids.empty()) return uint256();
    std::vector<uint256> level = txids;
    while (level.size() > 1) {
        if (level.size() % 2 == 1) level.push_back(level.back());
        std::vector<uint256> next;
        next.reserve(level.size() / 2);
        for (size_t i = 0; i < level.size(); i += 2) {
            next.push_back(HashPair(level[i], level[i + 1]));
        }
        level.swap(next);
    }
    return level[0];
}

bool CLightPartialMerkleTree::Deserialize(const uint8_t*& p, const uint8_t* end) {
    if (p + 4 > end) return false;
    nTransactions = ReadLE32(p);
    p += 4;
    bool ok = true;

    // 上游顺序：nTransactions -> vHash(CompactSize+hashes) -> vBits(CompactSize+bytes)
    uint64_t nHashes = ReadCompactSize(p, end, ok);
    if (!ok || nHashes > 1024 * 1024 || p + nHashes * 32 > end) return false;
    vHash.clear();
    vHash.reserve(nHashes);
    for (uint64_t i = 0; i < nHashes; ++i) {
        vHash.emplace_back(std::vector<uint8_t>(p, p + 32));
        p += 32;
    }

    uint64_t nBytes = ReadCompactSize(p, end, ok);
    if (!ok || nBytes > 1024 * 1024 || p + nBytes > end) return false;
    vBits.assign(p, p + nBytes);
    p += nBytes;
    return true;
}

std::vector<uint8_t> CLightPartialMerkleTree::Serialize() const {
    std::vector<uint8_t> out;
    WriteLE32(out, nTransactions);
    WriteCompactSize(out, vHash.size());
    for (const auto& h : vHash) {
        out.insert(out.end(), h.begin(), h.end());
    }
    WriteCompactSize(out, vBits.size());
    out.insert(out.end(), vBits.begin(), vBits.end());
    return out;
}

bool CLightPartialMerkleTree::ExtractMatches(std::vector<uint256>& vMatch,
                                             uint256& root) const {
    vMatch.clear();
    if (nTransactions == 0 || vHash.empty() || vBits.empty()) return false;
    if (vBits.size() * 8 < vHash.size()) return false;

    // 树高度：CalcTreeWidth(height) > 1 的最大 height
    auto TreeWidth = [&](int height) -> unsigned int {
        return (nTransactions + (1u << height) - 1) >> height;
    };
    int nHeight = 0;
    while (TreeWidth(nHeight) > 1) ++nHeight;

    size_t nBitsUsed = 0;
    size_t nHashUsed = 0;
    bool fBad = false;
    const size_t nBitsTotal = vBits.size() * 8;

    auto GetBit = [&](size_t idx) -> bool {
        if (idx >= nBitsTotal) return false;
        return (vBits[idx / 8] >> (idx % 8)) & 1;
    };

    // 与上游 TraverseAndExtract 一致：每节点 1 bit
    std::function<uint256(int, unsigned int)> traverse =
        [&](int height, unsigned int pos) -> uint256 {
            if (nBitsUsed >= nBitsTotal) {
                fBad = true;
                return uint256();
            }
            bool fParentOfMatch = GetBit(nBitsUsed++);
            if (height == 0 || !fParentOfMatch) {
                if (nHashUsed >= vHash.size()) {
                    fBad = true;
                    return uint256();
                }
                const uint256& hash = vHash[nHashUsed++];
                if (height == 0 && fParentOfMatch) {
                    vMatch.push_back(hash);
                }
                return hash;
            }
            uint256 left = traverse(height - 1, pos * 2);
            uint256 right;
            if (pos * 2 + 1 < TreeWidth(height - 1)) {
                right = traverse(height - 1, pos * 2 + 1);
                if (right == left) {
                    fBad = true;
                }
            } else {
                right = left;
            }
            return HashPair(left, right);
        };

    root = traverse(nHeight, 0);

    if (fBad) return false;
    // 允许序列化字节填充位：仅要求消耗的字节数与 vBits 字节数一致
    if ((nBitsUsed + 7) / 8 != (vBits.size() + 7) / 8) return false;
    if (nHashUsed != vHash.size()) return false;
    return true;
}

bool LightMerkleBlock::Deserialize(const uint8_t* data, size_t len) {
    if (len < LightBlockHeader::kHeaderSize) return false;
    if (!header.Deserialize(data, LightBlockHeader::kHeaderSize)) return false;
    const uint8_t* p = data + LightBlockHeader::kHeaderSize;
    const uint8_t* end = data + len;
    return txn.Deserialize(p, end);
}

bool LightMerkleBlock::ExtractMatches(std::vector<uint256>& vMatch, uint256& root) const {
    if (!txn.ExtractMatches(vMatch, root)) return false;
    return root == header.hashMerkleRoot;
}

} // namespace mvclight
