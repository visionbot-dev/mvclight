#ifndef MVC_LIGHT_LIGHT_TXPOOL_H
#define MVC_LIGHT_LIGHT_TXPOOL_H

/*
 * CLightTxPool —— 轻量内存广播池（设计文档 §4.6）。
 *
 * 替代全节点 txmempool，绝不依赖 validation.h / coins.h / txmempool.h。
 * 仅缓存已广播 txid（防重复）与待广播队列（FIFO）。
 *
 * Phase 0 说明：当前 pending 队列以 txid 表示；
 * Phase 3 导入 primitives/transaction.h 后，按设计改为
 *   std::vector<CTransactionRef> m_pending;
 *   bool Enqueue(const CTransactionRef& tx);
 *   CTransactionRef PopNext();
 */

#include "light_uint256.h"

#include <cstddef>
#include <deque>
#include <unordered_set>

namespace mvclight {

class CLightTxPool {
public:
    static constexpr size_t MAX_PENDING = 1024; // 溢出 FIFO 丢弃最旧

    // true = 新交易入队；false = 重复（已 relayed 或已在 pending）
    bool Enqueue(const uint256& txid);

    // 取队首待广播 txid；无待广播返回 false
    bool PopNext(uint256& txid_out);

    // 广播成功后调用：登记已广播，防重复入队
    void MarkRelayed(const uint256& txid);

    bool IsRelayed(const uint256& txid) const;
    bool IsPending(const uint256& txid) const;

    size_t PendingCount() const { return m_pending.size(); }
    size_t RelayedCount() const { return m_relayed.size(); }

private:
    std::unordered_set<uint256> m_relayed; // 已广播 txid（防重复）
    std::deque<uint256> m_pending;         // 待广播队列（FIFO）
};

} // namespace mvclight

#endif // MVC_LIGHT_LIGHT_TXPOOL_H
