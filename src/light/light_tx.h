#ifndef MVC_LIGHT_LIGHT_TX_H
#define MVC_LIGHT_LIGHT_TX_H

/*
 * 最简交易结构 + CheckTransactionCommon（设计文档 §4.5.2）。
 * Phase 3 自包含实现；后续接入 primitives/transaction.h 时替换。
 */

#include "light/light_uint256.h"

#include <cstdint>
#include <string>
#include <vector>

namespace mvclight {

constexpr int64_t kMaxMoney = 21000000LL * 100000000LL; // MAX_MONEY
constexpr uint32_t kMaxTxSize = 32 * 1024 * 1024;       // MVC 主网单笔交易上限

struct LightTxIn {
    uint256 prevout_hash;
    uint32_t prevout_n = 0;
    std::vector<uint8_t> script_sig;
    uint32_t sequence = 0xFFFFFFFF;
};

struct LightTxOut {
    int64_t nValue = 0;
    std::vector<uint8_t> script_pubkey;
};

struct LightTx {
    int32_t nVersion = 1;
    std::vector<LightTxIn> vin;
    std::vector<LightTxOut> vout;
    uint32_t nLockTime = 0;

    uint256 GetTxid() const; // 占位：Phase 3 使用序列化字节 SHA256d（简化）
};

// 等价 CheckTransactionCommon；返回 true 或设置 reject 码（bad-txns-*）
bool CheckTransactionCommon(const LightTx& tx, uint32_t nSerializedSize,
                            std::string& reject);

} // namespace mvclight

#endif // MVC_LIGHT_LIGHT_TX_H
