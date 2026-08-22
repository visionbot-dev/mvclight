#include "light/light_merkle.h"
#include "light/light_pendingtx.h"
#include "light/light_tx.h"
#include "light/light_watchstore.h"

#include "test_framework.h"

#include <cstdio>
#include <vector>

using mvclight::CLightWatchStore;
using mvclight::CPendingTxMap;
using mvclight::ComputeMerkleRoot;
using mvclight::LightBlockHeader;
using mvclight::LightMerkleBlock;
using mvclight::PendingEntry;
using mvclight::TxRecord;
using mvclight::uint256;
using mvclight::uint256S;

static uint256 MakeTxid(char c) {
    std::string hex(64, c);
    return uint256S(hex);
}

static LightMerkleBlock MakeMerkleBlock(const std::vector<uint256>& txids, bool tamper) {
    LightMerkleBlock mb;
    mb.header.hashPrevBlock = uint256S("00");
    mb.header.hashMerkleRoot = ComputeMerkleRoot(txids);
    mb.header.nTime = 1000;
    mb.header.nBits = 0x207fffff;
    mb.txids = txids;
    if (tamper) {
        mb.header.hashMerkleRoot = uint256S("ff");
    }
    return mb;
}

int main() {
    std::vector<uint256> txids = {MakeTxid('a'), MakeTxid('b'), MakeTxid('c')};
    const int64_t now = 1000000;

    // 1. TX 先到 + MERKLEBLOCK 后到 -> 配对 -> 原子入库
    {
        CPendingTxMap pending;
        CLightWatchStore store;
        LightMerkleBlock mb = MakeMerkleBlock(txids, false);

        pending.AddTx(txids[0], now);
        pending.AddMerkle(txids[0], 100, mb.header.GetHash(), now + 1);

        PendingEntry out;
        CHECK(pending.TryPair(txids[0], out));
        CHECK(out.height == 100);

        TxRecord rec;
        rec.txid = txids[0];
        rec.height = out.height;
        rec.block_hash = out.block_hash;
        rec.tx_blob = {1, 2, 3};
        CHECK(store.CommitTx("addr1", rec));
        CHECK(store.HasTx(txids[0]));
        std::printf("TX_PAIRED\n");
        std::printf("STORE_COMMITTED\n");
    }

    // 2. MERKLEBLOCK 先到 + TX 后到
    {
        CPendingTxMap pending;
        CLightWatchStore store;
        LightMerkleBlock mb = MakeMerkleBlock(txids, false);
        pending.AddMerkle(txids[1], 101, mb.header.GetHash(), now);
        PendingEntry out;
        CHECK(!pending.TryPair(txids[1], out));
        pending.AddTx(txids[1], now + 1);
        CHECK(pending.TryPair(txids[1], out));
        TxRecord rec;
        rec.txid = txids[1];
        rec.height = out.height;
        rec.block_hash = out.block_hash;
        CHECK(store.CommitTx("addr2", rec));
        CHECK(store.HasTx(txids[1]));
    }

    // 3. 篡改 MERKLEBLOCK 根 -> 拒绝（断开语义）
    {
        LightMerkleBlock bad = MakeMerkleBlock(txids, true);
        CHECK(!bad.Verify());
        std::printf("MERKLE_TAMPERED_REJECTED\n");
    }

    // 4. 正常 MERKLEBLOCK 根校验
    {
        LightMerkleBlock good = MakeMerkleBlock(txids, false);
        CHECK(good.Verify());
    }

    TEST_MAIN_RETURN();
}
