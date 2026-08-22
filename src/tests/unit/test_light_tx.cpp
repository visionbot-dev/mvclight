#include "light/light_tx.h"

#include "test_framework.h"

#include <string>

using mvclight::CheckTransactionCommon;
using mvclight::LightTx;
using mvclight::LightTxIn;
using mvclight::LightTxOut;

static LightTx MakeTx(bool with_vin, bool with_vout) {
    LightTx tx;
    if (with_vin) {
        LightTxIn in;
        tx.vin.push_back(in);
    }
    if (with_vout) {
        LightTxOut out;
        out.nValue = 1000;
        tx.vout.push_back(out);
    }
    return tx;
}

int main() {
    std::string reject;

    // 合法
    LightTx good = MakeTx(true, true);
    CHECK(CheckTransactionCommon(good, 100, reject));

    // vin 空
    LightTx no_vin = MakeTx(false, true);
    CHECK(!CheckTransactionCommon(no_vin, 100, reject));
    CHECK(reject == "bad-txns-vin-empty");

    // vout 空
    LightTx no_vout = MakeTx(true, false);
    CHECK(!CheckTransactionCommon(no_vout, 100, reject));
    CHECK(reject == "bad-txns-vout-empty");

    // 超大
    CHECK(!CheckTransactionCommon(good, mvclight::kMaxTxSize + 1, reject));
    CHECK(reject == "bad-txns-oversize");

    // 输出为负
    LightTx neg = MakeTx(true, true);
    neg.vout[0].nValue = -1;
    CHECK(!CheckTransactionCommon(neg, 100, reject));
    CHECK(reject == "bad-txns-vout-negative");

    // 输出超 MAX_MONEY
    LightTx big = MakeTx(true, true);
    big.vout[0].nValue = mvclight::kMaxMoney + 1;
    CHECK(!CheckTransactionCommon(big, 100, reject));
    CHECK(reject == "bad-txns-vout-toolarge");

    // 输出总和溢出
    LightTx total = MakeTx(true, true);
    total.vout[0].nValue = mvclight::kMaxMoney;
    LightTxOut extra;
    extra.nValue = 1;
    total.vout.push_back(extra);
    CHECK(!CheckTransactionCommon(total, 100, reject));
    CHECK(reject == "bad-txns-txouttotal-toolarge");

    // txid 可计算
    CHECK(!good.GetTxid().IsNull());

    TEST_MAIN_RETURN();
}
