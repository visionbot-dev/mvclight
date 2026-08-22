#include "light/light_pendingtx.h"

namespace mvclight {

void CPendingTxMap::Erase(const uint256& txid) {
    m_map.erase(txid);
    for (auto it = m_fifo.begin(); it != m_fifo.end(); ++it) {
        if (*it == txid) {
            m_fifo.erase(it);
            break;
        }
    }
}

void CPendingTxMap::EnforceFifo() {
    while (m_map.size() > MAX_PENDING) {
        Erase(m_fifo.front());
        m_fifo.pop_front();
    }
}

bool CPendingTxMap::AddTx(const uint256& txid, int64_t now_ms) {
    auto it = m_map.find(txid);
    if (it != m_map.end()) {
        it->second.has_tx = true;
        return false; // 已存在
    }
    PendingEntry e;
    e.txid = txid;
    e.has_tx = true;
    e.first_seen_ms = now_ms;
    m_map.emplace(txid, e);
    m_fifo.push_back(txid);
    EnforceFifo();
    return true;
}

bool CPendingTxMap::AddMerkle(const uint256& txid, int64_t height,
                              const uint256& block_hash, int64_t now_ms) {
    auto it = m_map.find(txid);
    if (it != m_map.end()) {
        it->second.has_merkle = true;
        it->second.height = height;
        it->second.block_hash = block_hash;
        return false;
    }
    PendingEntry e;
    e.txid = txid;
    e.has_merkle = true;
    e.height = height;
    e.block_hash = block_hash;
    e.first_seen_ms = now_ms;
    m_map.emplace(txid, e);
    m_fifo.push_back(txid);
    EnforceFifo();
    return true;
}

bool CPendingTxMap::TryPair(const uint256& txid, PendingEntry& out) {
    auto it = m_map.find(txid);
    if (it == m_map.end()) return false;
    if (!it->second.has_tx || !it->second.has_merkle) return false;
    out = it->second;
    Erase(txid);
    return true;
}

bool CPendingTxMap::Get(const uint256& txid, PendingEntry& out) const {
    auto it = m_map.find(txid);
    if (it == m_map.end()) return false;
    out = it->second;
    return true;
}

void CPendingTxMap::Remove(const uint256& txid) {
    Erase(txid);
}

void CPendingTxMap::Expire(int64_t now_ms, std::vector<uint256>& to_retry) {
    std::vector<uint256> expired;
    for (const auto& kv : m_map) {
        const PendingEntry& e = kv.second;
        if (e.has_tx && e.has_merkle) continue; // 已配对不参与超时
        if (now_ms - e.first_seen_ms >= kPairTimeoutMs) {
            expired.push_back(e.txid);
        }
    }
    for (const auto& txid : expired) {
        auto it = m_map.find(txid);
        if (it == m_map.end()) continue;
        ++it->second.retry_count;
        if (it->second.retry_count >= kMaxRetry) {
            Erase(txid); // 超过 3 次，丢弃并告警
        } else {
            to_retry.push_back(txid);
        }
    }
}

} // namespace mvclight
