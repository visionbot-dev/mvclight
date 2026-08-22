#include "light/light_watchstore.h"

#include "test_framework.h"

#include <vector>

using mvclight::CLightWatchStore;
using mvclight::TxRecord;
using mvclight::uint256S;

static TxRecord MakeRecord(const char* hex, int64_t height) {
    TxRecord r;
    r.txid = uint256S(hex);
    r.height = height;
    r.block_hash = uint256S("bb");
    r.tx_blob = {1, 2, 3};
    r.script_verified = true;
    return r;
}

int main() {
    CLightWatchStore store;

    // 原子写入 + 双表查询
    TxRecord r1 = MakeRecord("aa", 100);
    CHECK(store.CommitTx("addr1", r1));
    CHECK(store.HasTx(r1.txid));
    std::vector<mvclight::uint256> txids;
    CHECK(store.GetTxidsByAddr("addr1", txids));
    CHECK(txids.size() == 1);
    CHECK(txids[0] == r1.txid);

    // txid 去重：同一 txid 关联第二个地址，仅新增关联
    CHECK(store.CommitTx("addr2", r1));
    CHECK(store.TxCount() == 1);
    CHECK(store.IndexCount() == 2);

    // 磁盘满 -> 写入失败且无半写
    store.SetDiskFullForTest(true);
    TxRecord r2 = MakeRecord("cc", 101);
    CHECK(!store.CommitTx("addr1", r2));
    CHECK(!store.HasTx(r2.txid));
    CHECK(store.IndexCount() == 2);
    store.SetDiskFullForTest(false);

    // 恢复后可写
    CHECK(store.CommitTx("addr1", r2));
    CHECK(store.HasTx(r2.txid));

    TEST_MAIN_RETURN();
}
