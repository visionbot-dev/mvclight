#include "light/light_pendingtx.h"

#include "test_framework.h"

#include <cstdint>
#include <vector>

using mvclight::CPendingTxMap;
using mvclight::PendingEntry;
using mvclight::uint256;
using mvclight::uint256S;

int main() {
    const int64_t now = 1000000;

    // TX 先到 -> MERKLEBLOCK 后到
    {
        CPendingTxMap m;
        uint256 txid = uint256S("aa");
        CHECK(m.AddTx(txid, now));
        PendingEntry out;
        CHECK(!m.TryPair(txid, out)); // 只有 TX
        CHECK(!m.AddMerkle(txid, 100, uint256S("bb"), now + 10)); // 已存在
        CHECK(m.TryPair(txid, out));
        CHECK(out.has_tx && out.has_merkle);
        CHECK(out.height == 100);
        CHECK(m.Size() == 0);
    }

    // MERKLEBLOCK 先到 -> TX 后到
    {
        CPendingTxMap m;
        uint256 txid = uint256S("cc");
        CHECK(m.AddMerkle(txid, 200, uint256S("dd"), now));
        PendingEntry out;
        CHECK(!m.TryPair(txid, out));
        CHECK(!m.AddTx(txid, now + 5)); // 已存在
        CHECK(m.TryPair(txid, out));
        CHECK(out.height == 200);
    }

    // 30s 超时 -> 重传；3 次后丢弃
    {
        CPendingTxMap m;
        uint256 txid = uint256S("ee");
        m.AddTx(txid, now);
        std::vector<uint256> retry;
        m.Expire(now + CPendingTxMap::kPairTimeoutMs, retry);
        CHECK(retry.size() == 1); // 第 1 次
        retry.clear();
        m.Expire(now + CPendingTxMap::kPairTimeoutMs * 2, retry);
        CHECK(retry.size() == 1); // 第 2 次
        retry.clear();
        m.Expire(now + CPendingTxMap::kPairTimeoutMs * 3, retry);
        CHECK(retry.empty());     // 第 3 次达上限，丢弃
        CHECK(m.Size() == 0);
    }

    // FIFO 4096 淘汰最旧
    {
        CPendingTxMap m;
        uint256 base = uint256S("ab");
        for (int i = 0; i < 5000; ++i) {
            uint256 t = base;
            t.begin()[0] = static_cast<uint8_t>(i & 0xFF);
            t.begin()[1] = static_cast<uint8_t>((i >> 8) & 0xFF);
            m.AddTx(t, now + i);
        }
        CHECK(m.Size() == CPendingTxMap::MAX_PENDING);
    }

    TEST_MAIN_RETURN();
}
