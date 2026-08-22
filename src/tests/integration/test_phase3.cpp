#include "light/light_merkle.h"
#include "light/light_pendingtx.h"
#include "light/light_tx.h"
#include "light/light_watchstore.h"

#include "test_framework.h"

#include <cstdio>
#include <functional>
#include <string>
#include <vector>

using mvclight::CLightPartialMerkleTree;
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

// 构造“全部匹配”的部分默克尔树（与上游 TraverseAndBuild 位序一致）
static CLightPartialMerkleTree MakeFullTree(const std::vector<uint256>& txids) {
    CLightPartialMerkleTree t;
    t.nTransactions = static_cast<uint32_t>(txids.size());

    auto TreeWidth = [&](int height) -> unsigned int {
        return (t.nTransactions + (1u << height) - 1) >> height;
    };
    int nHeight = 0;
    while (TreeWidth(nHeight) > 1) ++nHeight;

    std::vector<uint8_t> bits;
    std::function<void(int, unsigned int)> collect = [&](int height, unsigned int pos) {
        bits.push_back(1); // 本节点是匹配父节点
        if (height == 0) return;
        collect(height - 1, pos * 2);
        if (pos * 2 + 1 < TreeWidth(height - 1)) {
            collect(height - 1, pos * 2 + 1);
        }
    };
    collect(nHeight, 0);

    t.vBits.assign((bits.size() + 7) / 8, 0);
    for (size_t i = 0; i < bits.size(); ++i) {
        if (bits[i]) t.vBits[i / 8] |= static_cast<uint8_t>(1 << (i % 8));
    }
    t.vHash = txids;
    return t;
}

static LightMerkleBlock MakeMerkleBlock(const std::vector<uint256>& txids, bool tamper) {
    LightMerkleBlock mb;
    mb.header.hashPrevBlock = uint256S("00");
    mb.header.hashMerkleRoot = ComputeMerkleRoot(txids);
    mb.header.nTime = 1000;
    mb.header.nBits = 0x207fffff;
    mb.txn = MakeFullTree(txids);
    if (tamper) {
        mb.header.hashMerkleRoot = uint256S("ff");
    }
    return mb;
}

int main() {
    std::vector<uint256> txids = {MakeTxid('a'), MakeTxid('b'), MakeTxid('c'), MakeTxid('d')};
    const int64_t now = 1000000;

    // 0. 序列化 -> 反序列化 -> ExtractMatches 根校验
    {
        LightMerkleBlock mb = MakeMerkleBlock(txids, false);
        std::vector<uint8_t> raw = mb.header.Serialize();
        std::vector<uint8_t> tree = mb.txn.Serialize();
        raw.insert(raw.end(), tree.begin(), tree.end());

        LightMerkleBlock parsed;
        CHECK(parsed.Deserialize(raw.data(), raw.size()));
        std::vector<uint256> matched;
        uint256 root;
        CHECK(parsed.ExtractMatches(matched, root));
        CHECK(matched.size() == txids.size());
        CHECK(root == mb.header.hashMerkleRoot);
        std::printf("MERKLE_SERIALIZE_OK\n");
    }

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
        std::vector<uint256> matched;
        uint256 root;
        CHECK(!bad.ExtractMatches(matched, root));
        std::printf("MERKLE_TAMPERED_REJECTED\n");
    }

    // 4. 正常 MERKLEBLOCK 根校验
    {
        LightMerkleBlock good = MakeMerkleBlock(txids, false);
        std::vector<uint256> matched;
        uint256 root;
        CHECK(good.ExtractMatches(matched, root));
    }

    TEST_MAIN_RETURN();
}
