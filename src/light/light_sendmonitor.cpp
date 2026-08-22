#include "light/light_sendmonitor.h"

namespace mvclight {

void CLightSendMonitor::RecordSent(const uint256& txid, int64_t now_ms) {
    Entry e;
    e.deadline_ms = now_ms + kConfirmTimeoutMs;
    e.rejected = false;
    m_entries[txid] = e;
}

void CLightSendMonitor::RecordRejected(const uint256& txid) {
    auto it = m_entries.find(txid);
    if (it != m_entries.end()) {
        it->second.rejected = true;
    }
}

void CLightSendMonitor::CheckTimeouts(int64_t now_ms, const CLightWatchStore& store,
                                      const Callback& cb) {
    std::vector<uint256> expired;
    for (const auto& kv : m_entries) {
        if (kv.second.rejected) continue; // 已 REJECT 不启动 120min 定时器
        if (now_ms >= kv.second.deadline_ms) {
            expired.push_back(kv.first);
        }
    }
    for (const auto& txid : expired) {
        m_entries.erase(txid);
        if (!store.HasTx(txid)) {
            cb(txid, 2, "not seen on-chain within 2h"); // SEND_TIMEOUT_UNCONFIRMED
        }
        // 已上链：静默注销
    }
}

} // namespace mvclight
