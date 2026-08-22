#include "light/light_tx.h"

#include "light/light_sha256.h"

namespace mvclight {

namespace {

// 简化序列化：仅用于 txid 占位与大小估算
void WriteCompactSize(std::vector<uint8_t>& out, uint64_t v) {
    if (v < 253) {
        out.push_back(static_cast<uint8_t>(v));
    } else if (v <= 0xFFFF) {
        out.push_back(253);
        out.push_back(static_cast<uint8_t>(v));
        out.push_back(static_cast<uint8_t>(v >> 8));
    } else {
        out.push_back(254);
        for (int i = 0; i < 4; ++i) out.push_back(static_cast<uint8_t>(v >> (i * 8)));
    }
}

} // namespace

uint256 LightTx::GetTxid() const {
    std::vector<uint8_t> raw;
    // 简化：version(4) + vin count + vin 摘要 + vout count + vout 摘要 + locktime(4)
    raw.push_back(static_cast<uint8_t>(nVersion));
    raw.push_back(static_cast<uint8_t>(nVersion >> 8));
    raw.push_back(static_cast<uint8_t>(nVersion >> 16));
    raw.push_back(static_cast<uint8_t>(nVersion >> 24));
    WriteCompactSize(raw, vin.size());
    for (const auto& in : vin) {
        raw.insert(raw.end(), in.prevout_hash.begin(), in.prevout_hash.end());
        raw.push_back(static_cast<uint8_t>(in.prevout_n));
        raw.push_back(static_cast<uint8_t>(in.prevout_n >> 8));
        raw.push_back(static_cast<uint8_t>(in.prevout_n >> 16));
        raw.push_back(static_cast<uint8_t>(in.prevout_n >> 24));
    }
    WriteCompactSize(raw, vout.size());
    for (const auto& out : vout) {
        for (int i = 0; i < 8; ++i) raw.push_back(static_cast<uint8_t>(out.nValue >> (i * 8)));
    }
    for (int i = 0; i < 4; ++i) raw.push_back(static_cast<uint8_t>(nLockTime >> (i * 8)));
    uint8_t hash[32];
    SHA256D(raw.data(), raw.size(), hash);
    return uint256(std::vector<uint8_t>(hash, hash + 32));
}

bool CheckTransactionCommon(const LightTx& tx, uint32_t nSerializedSize,
                            std::string& reject) {
    if (tx.vin.empty()) {
        reject = "bad-txns-vin-empty";
        return false;
    }
    if (tx.vout.empty()) {
        reject = "bad-txns-vout-empty";
        return false;
    }
    if (nSerializedSize > kMaxTxSize) {
        reject = "bad-txns-oversize";
        return false;
    }
    int64_t nValueOut = 0;
    for (const auto& txout : tx.vout) {
        if (txout.nValue < 0) {
            reject = "bad-txns-vout-negative";
            return false;
        }
        if (txout.nValue > kMaxMoney) {
            reject = "bad-txns-vout-toolarge";
            return false;
        }
        nValueOut += txout.nValue;
        if (!(nValueOut >= 0 && nValueOut <= kMaxMoney)) {
            reject = "bad-txns-txouttotal-toolarge";
            return false;
        }
    }
    // sigops 上限：Phase 3 简化为不校验（Genesis 前规则；后续补）
    return true;
}

} // namespace mvclight
