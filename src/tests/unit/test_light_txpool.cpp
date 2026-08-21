#include "light/light_txpool.h"

#include "test_framework.h"

using mvclight::CLightTxPool;
using mvclight::uint256;
using mvclight::uint256S;

int main() {
    CLightTxPool pool;

    // 基础入队/重复检测
    uint256 tx1 = uint256S("11" "11" "11" "11" "11" "11" "11" "11"
                           "11" "11" "11" "11" "11" "11" "11" "11"
                           "11" "11" "11" "11" "11" "11" "11" "11"
                           "11" "11" "11" "11" "11" "11" "11" "11");
    uint256 tx2 = uint256S("22" "22" "22" "22" "22" "22" "22" "22"
                           "22" "22" "22" "22" "22" "22" "22" "22"
                           "22" "22" "22" "22" "22" "22" "22" "22"
                           "22" "22" "22" "22" "22" "22" "22" "22");

    CHECK(pool.Enqueue(tx1));
    CHECK(!pool.Enqueue(tx1));       // 已在 pending，重复
    CHECK_EQ(pool.PendingCount(), 1u);
    CHECK(pool.IsPending(tx1));

    uint256 popped;
    CHECK(pool.PopNext(popped));
    CHECK(popped == tx1);
    CHECK(!pool.IsPending(tx1));

    // 广播后登记 relayed，再次入队应拒绝
    pool.MarkRelayed(tx1);
    CHECK(pool.IsRelayed(tx1));
    CHECK(!pool.Enqueue(tx1));       // 已广播，防重复

    // FIFO 顺序（使用新 txid；tx1 已 relayed 不应再入队）
    uint256 tx3 = uint256S("33" "33" "33" "33" "33" "33" "33" "33"
                           "33" "33" "33" "33" "33" "33" "33" "33"
                           "33" "33" "33" "33" "33" "33" "33" "33"
                           "33" "33" "33" "33" "33" "33" "33" "33");
    CHECK(pool.Enqueue(tx2));
    CHECK(pool.Enqueue(tx3));
    CHECK(pool.PopNext(popped));
    CHECK(popped == tx2);
    CHECK(pool.PopNext(popped));
    CHECK(popped == tx3);

    // 溢出 FIFO：MAX_PENDING=1024，插入 1025 个不同 txid，最旧被淘汰
    CLightTxPool big;
    uint256 base = uint256S("ab" "ab" "ab" "ab" "ab" "ab" "ab" "ab"
                            "ab" "ab" "ab" "ab" "ab" "ab" "ab" "ab"
                            "ab" "ab" "ab" "ab" "ab" "ab" "ab" "ab"
                            "ab" "ab" "ab" "ab" "ab" "ab" "ab" "ab");
    for (int i = 0; i < 1025; ++i) {
        uint256 t = base;
        t.begin()[0] = static_cast<uint8_t>(i & 0xFF);
        t.begin()[1] = static_cast<uint8_t>((i >> 8) & 0xFF);
        big.Enqueue(t);
    }
    CHECK_EQ(big.PendingCount(), CLightTxPool::MAX_PENDING);
    // 最旧的 base 应被淘汰，新的最后一条应在队尾
    uint256 first;
    CHECK(big.PopNext(first));
    CHECK(first != base);

    TEST_MAIN_RETURN();
}
