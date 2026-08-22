#include "light/light_sendmonitor.h"
#include "light/light_watchstore.h"

#include "test_framework.h"

#include <string>
#include <vector>

using mvclight::CLightSendMonitor;
using mvclight::CLightWatchStore;
using mvclight::TxRecord;
using mvclight::uint256;
using mvclight::uint256S;

int main() {
    CLightSendMonitor mon;
    CLightWatchStore store;
    int calls = 0;
    std::string reason;

    auto cb = [&](const mvclight::uint256&, int code, const std::string& r) {
        ++calls;
        reason = r;
    };

    uint256 txid = uint256S("aa");
    const int64_t now = 1000000;

    // 未到期不回调
    mon.RecordSent(txid, now);
    mon.CheckTimeouts(now + 1000, store, cb);
    CHECK(calls == 0);

    // 到期且未上链 -> TIMEOUT_UNCONFIRMED
    mon.CheckTimeouts(now + CLightSendMonitor::kConfirmTimeoutMs, store, cb);
    CHECK(calls == 1);
    CHECK(reason == "not seen on-chain within 2h");

    // 已上链 -> 静默注销
    TxRecord rec;
    rec.txid = txid;
    store.CommitTx("addr", rec);
    mon.RecordSent(txid, now);
    mon.CheckTimeouts(now + CLightSendMonitor::kConfirmTimeoutMs, store, cb);
    CHECK(calls == 1); // 无新增回调

    // REJECT 后不启动定时器
    mon.RecordSent(txid, now);
    mon.RecordRejected(txid);
    mon.CheckTimeouts(now + CLightSendMonitor::kConfirmTimeoutMs, store, cb);
    CHECK(calls == 1);

    TEST_MAIN_RETURN();
}
