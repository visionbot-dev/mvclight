#ifndef MVC_LIGHT_LIGHT_SENDMONITOR_H
#define MVC_LIGHT_LIGHT_SENDMONITOR_H

/*
 * 发送结果监控（设计文档 §4.1.3）。
 * - SEND_OK：发送至 Socket（由调用方在 PushMessage 成功后触发）
 * - SEND_REJECTED：对端 REJECT（30s 窗口）
 * - SEND_TIMEOUT_UNCONFIRMED：120 分钟未上链兜底
 */

#include "light/light_uint256.h"
#include "light/light_watchstore.h"

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>

namespace mvclight {

class CLightSendMonitor {
public:
    static constexpr int64_t kConfirmTimeoutMs = 120 * 60 * 1000; // 120 分钟

    using Callback = std::function<void(const uint256& txid, int code, const std::string& reason)>;

    void RecordSent(const uint256& txid, int64_t now_ms);
    void RecordRejected(const uint256& txid);

    // 到期检查：已上链则静默注销；未上链回调 TIMEOUT_UNCONFIRMED 并注销
    void CheckTimeouts(int64_t now_ms, const CLightWatchStore& store, const Callback& cb);

    size_t Size() const { return m_entries.size(); }

private:
    struct Entry {
        int64_t deadline_ms = 0;
        bool rejected = false;
    };
    std::unordered_map<uint256, Entry> m_entries;
};

} // namespace mvclight

#endif // MVC_LIGHT_LIGHT_SENDMONITOR_H
