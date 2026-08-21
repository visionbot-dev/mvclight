#include "light/light_txpool.h"

namespace mvclight {

bool CLightTxPool::Enqueue(const uint256& txid) {
    if (m_relayed.count(txid) != 0) {
        return false; // 已广播，防重复
    }
    if (m_pending.size() >= MAX_PENDING) {
        // 溢出 FIFO：丢弃最旧
        m_pending.pop_front();
    }
    if (IsPending(txid)) {
        return false; // 已在待广播队列
    }
    m_pending.push_back(txid);
    return true;
}

bool CLightTxPool::PopNext(uint256& txid_out) {
    if (m_pending.empty()) {
        return false;
    }
    txid_out = m_pending.front();
    m_pending.pop_front();
    return true;
}

void CLightTxPool::MarkRelayed(const uint256& txid) {
    m_relayed.insert(txid);
}

bool CLightTxPool::IsRelayed(const uint256& txid) const {
    return m_relayed.count(txid) != 0;
}

bool CLightTxPool::IsPending(const uint256& txid) const {
    // 线性查找；Phase 0 骨架足够，后续可换 unordered_set 索引
    for (const auto& p : m_pending) {
        if (p == txid) return true;
    }
    return false;
}

} // namespace mvclight
