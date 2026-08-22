#include "light/light_validation.h"

#include <cstring>

namespace mvclight {

namespace {

// 由 nBits 计算 32 字节大端 target
bool TargetFromBits(uint32_t nBits, uint8_t target_be[32]) {
    int32_t exponent = static_cast<int32_t>(nBits >> 24);
    uint32_t mantissa = nBits & 0x007FFFFF;
    if (mantissa == 0) return false;

    memset(target_be, 0, 32);
    if (exponent <= 3) {
        // target = mantissa >> (8 * (3 - exponent))
        uint64_t v = mantissa >> (8 * (3 - exponent));
        if (v == 0) return false;
        target_be[31] = static_cast<uint8_t>(v);
        target_be[30] = static_cast<uint8_t>(v >> 8);
        target_be[29] = static_cast<uint8_t>(v >> 16);
    } else {
        int shift = exponent - 3;
        if (shift >= 32) {
            // 超出 256 位范围视为无效
            return false;
        }
        // mantissa 占 3 字节，放到大端数组末尾（低地址为大端高位）
        int pos = 31 - shift;
        target_be[pos - 2] = static_cast<uint8_t>(mantissa >> 16);
        target_be[pos - 1] = static_cast<uint8_t>(mantissa >> 8);
        target_be[pos] = static_cast<uint8_t>(mantissa);
    }
    return true;
}

void Uint256ToBigEndian(const uint256& h, uint8_t out[32]) {
    for (size_t i = 0; i < 32; ++i) {
        out[i] = h.begin()[31 - i];
    }
}

bool HashLeTarget(const uint256& hash, const uint8_t target_be[32]) {
    uint8_t hash_be[32];
    Uint256ToBigEndian(hash, hash_be);
    for (int i = 0; i < 32; ++i) {
        if (hash_be[i] < target_be[i]) return true;
        if (hash_be[i] > target_be[i]) return false;
    }
    return true; // 相等
}

} // namespace

bool ValidateHeader(const LightBlockHeader& h, const LightBlockHeader* prev,
                    int64_t height, bool historical_segment, int64_t adjusted_time_now,
                    std::string& reason) {
    if (height < 0) {
        reason = "bad-height";
        return false;
    }

    // prev 连续性（两阶段都必须满足）
    if (prev != nullptr && h.hashPrevBlock != prev->GetHash()) {
        reason = "bad-prevblk";
        return false;
    }
    if (prev == nullptr && height > 0) {
        reason = "bad-prevblk";
        return false;
    }

    // 历史段：只校验哈希链（+ Checkpoint 工作量在 CheckCheckpoint 中校验）
    if (historical_segment) {
        return true;
    }

    // 1. PoW
    uint8_t target_be[32];
    if (!TargetFromBits(h.nBits, target_be)) {
        reason = "bad-diffbits";
        return false;
    }
    if (!HashLeTarget(h.GetHash(), target_be)) {
        reason = "high-hash";
        return false;
    }

    // 3. 时间戳 > 前块时间（简化 MTP）
    if (prev != nullptr && h.nTime <= prev->nTime) {
        reason = "time-too-old";
        return false;
    }

    // 4. 时间戳 <= 调整后当前时间 + 2h
    if (h.nTime > static_cast<uint32_t>(adjusted_time_now + 2 * 3600)) {
        reason = "time-too-new";
        return false;
    }

    // 5. nVersion >= 4（简化：高度 >= 0 即要求）
    if (h.nVersion < 4) {
        reason = "bad-version";
        return false;
    }

    return true;
}

bool CheckCheckpoint(const CLightChainStore& store, const LightCheckpoint& cp) {
    if (!cp.nChainWork.IsNull()) {
        if (!ChainWorkGe(store.ChainWork(), cp.nChainWork)) {
            return false; // ERR_CP_CHECKPOINT_FAILED
        }
    }
    if (!cp.hash.IsNull()) {
        LightBlockHeader hdr;
        if (!store.GetHeaderAtHeight(cp.height, hdr)) {
            return false;
        }
        if (hdr.GetHash() != cp.hash) {
            return false;
        }
    }
    return true;
}

} // namespace mvclight
