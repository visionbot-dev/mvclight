#include "light/light_header.h"

#include "light/light_sha256.h"

#include <cstring>

namespace mvclight {

namespace {

void WriteLE32(std::vector<uint8_t>& out, uint32_t v) {
    for (int i = 0; i < 4; ++i) out.push_back(static_cast<uint8_t>(v >> (i * 8)));
}

uint32_t ReadLE32(const uint8_t* p) {
    return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}

} // namespace

std::vector<uint8_t> LightBlockHeader::Serialize() const {
    std::vector<uint8_t> out;
    out.reserve(kHeaderSize);
    WriteLE32(out, static_cast<uint32_t>(nVersion));
    out.insert(out.end(), hashPrevBlock.begin(), hashPrevBlock.end());
    out.insert(out.end(), hashMerkleRoot.begin(), hashMerkleRoot.end());
    WriteLE32(out, nTime);
    WriteLE32(out, nBits);
    WriteLE32(out, nNonce);
    return out;
}

bool LightBlockHeader::Deserialize(const uint8_t* data, size_t len) {
    if (len < kHeaderSize) return false;
    nVersion = static_cast<int32_t>(ReadLE32(data));
    memcpy(hashPrevBlock.begin(), data + 4, 32);
    memcpy(hashMerkleRoot.begin(), data + 36, 32);
    nTime = ReadLE32(data + 68);
    nBits = ReadLE32(data + 72);
    nNonce = ReadLE32(data + 76);
    return true;
}

uint256 LightBlockHeader::GetHash() const {
    std::vector<uint8_t> raw = Serialize();
    uint8_t hash[32];
    SHA256D(raw.data(), raw.size(), hash);
    return uint256(std::vector<uint8_t>(hash, hash + 32));
}

} // namespace mvclight
