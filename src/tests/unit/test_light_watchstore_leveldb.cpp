#include "light/light_watchstore.h"

#include "test_framework.h"

#include <string>
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
    return r;
}

int main() {
    const std::string path = "store_test_leveldb";

    // 第一次打开：写入
    {
        CLightWatchStore store;
        CHECK(store.Open(path));
        CHECK(store.CommitTx("addr1", MakeRecord("aa", 100)));
        CHECK(store.HasTx(uint256S("aa")));
        store.Close();
    }

    // 第二次打开：验证持久化
    {
        CLightWatchStore store;
        CHECK(store.Open(path));
        CHECK(store.HasTx(uint256S("aa")));
        std::vector<mvclight::uint256> txids;
        CHECK(store.GetTxidsByAddr("addr1", txids));
        CHECK(txids.size() == 1);
        store.Close();
    }

    // 磁盘满：写入失败且不污染
    {
        CLightWatchStore store;
        CHECK(store.Open(path));
        store.SetDiskFullForTest(true);
        CHECK(!store.CommitTx("addr2", MakeRecord("cc", 101)));
        CHECK(!store.HasTx(uint256S("cc")));
        store.SetDiskFullForTest(false);
        store.Close();
    }

    TEST_MAIN_RETURN();
}
