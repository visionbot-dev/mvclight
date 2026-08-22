#include "light/light_watchstore.h"

namespace mvclight {

bool CLightWatchStore::CheckDiskSpace() const {
    // Phase 3 模拟：m_disk_full 表示剩余空间 < 50MB
    return !m_disk_full;
}

bool CLightWatchStore::CommitTx(const std::string& addr, const TxRecord& rec) {
    if (!CheckDiskSpace()) {
        return false; // ERR_DISK_FULL
    }

    // 原子语义：先在副本上构造，全部成功后才应用
    auto tx_store = m_tx_store;
    auto addr_index = m_addr_tx_index;

    tx_store.emplace(rec.txid, rec); // txid 去重：已存在则保持原记录
    addr_index.emplace(std::make_pair(addr, rec.txid), true);

    // 模拟 COMMIT 成功后才替换
    m_tx_store.swap(tx_store);
    m_addr_tx_index.swap(addr_index);
    return true;
}

bool CLightWatchStore::HasTx(const uint256& txid) const {
    return m_tx_store.count(txid) != 0;
}

bool CLightWatchStore::GetTx(const uint256& txid, TxRecord& out) const {
    auto it = m_tx_store.find(txid);
    if (it == m_tx_store.end()) return false;
    out = it->second;
    return true;
}

bool CLightWatchStore::GetTxidsByAddr(const std::string& addr, std::vector<uint256>& out) const {
    out.clear();
    for (const auto& kv : m_addr_tx_index) {
        if (kv.first.first == addr) {
            out.push_back(kv.first.second);
        }
    }
    return !out.empty();
}

} // namespace mvclight
