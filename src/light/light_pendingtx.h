#ifndef MVC_LIGHT_LIGHT_PENDINGTX_H
#define MVC_LIGHT_LIGHT_PENDINGTX_H

/*
 * PendingTxMap（设计文档 §4.5.1）。
 * 处理 TX / MERKLEBLOCK 乱序配对、30s 超时重传、3 次上限、FIFO 4096 淘汰。
 */

#include "light/light_uint256.h"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <unordered_map>
#include <vector>

namespace mvclight {

struct PendingEntry {
    uint256 txid;
    bool has_tx = false;
    bool has_merkle = false;
    int64_t height = -1;
    uint256 block_hash;
    int64_t first_seen_ms = 0;
    int retry_count = 0;
};

class CPendingTxMap {
public:
    static constexpr size_t MAX_PENDING = 4096;
    static constexpr int64_t kPairTimeoutMs = 30000;
    static constexpr int kMaxRetry = 3;

    bool AddTx(const uint256& txid, int64_t now_ms);
    bool AddMerkle(const uint256& txid, int64_t height, const uint256& block_hash,
                   int64_t now_ms);

    // 两者到齐则返回 true 并移除
    bool TryPair(const uint256& txid, PendingEntry& out);

    bool Get(const uint256& txid, PendingEntry& out) const;
    size_t Size() const { return m_map.size(); }
    void Remove(const uint256& txid);

    // 超时清理：返回需要 getdata 重传的 txid；超过 kMaxRetry 的丢弃
    void Expire(int64_t now_ms, std::vector<uint256>& to_retry);

private:
    void EnforceFifo();
    void Erase(const uint256& txid);

    std::unordered_map<uint256, PendingEntry> m_map;
    std::deque<uint256> m_fifo;
};

} // namespace mvclight

#endif // MVC_LIGHT_LIGHT_PENDINGTX_H
