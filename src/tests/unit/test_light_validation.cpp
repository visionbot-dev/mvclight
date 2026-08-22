#include "light/light_chainstore.h"
#include "light/light_checkpoints.h"
#include "light/light_header.h"
#include "light/light_validation.h"

#include "test_framework.h"

#include <cstring>
#include <string>

using mvclight::CLightChainStore;
using mvclight::CheckCheckpoint;
using mvclight::LightBlockHeader;
using mvclight::LightCheckpoint;
using mvclight::uint256;
using mvclight::uint256S;
using mvclight::ValidateHeader;

static bool HashBeFirstByteLe(const LightBlockHeader& h, uint8_t limit) {
    uint256 hh = h.GetHash();
    uint8_t be[32];
    for (size_t i = 0; i < 32; ++i) be[i] = hh.begin()[31 - i];
    return be[0] <= limit;
}

static void MakeHeader(LightBlockHeader& h, const uint256& prev_hash, uint32_t time, int32_t version) {
    h.nVersion = version;
    h.hashPrevBlock = prev_hash;
    h.hashMerkleRoot = uint256S(
        "ab" "cd" "ab" "cd" "ab" "cd" "ab" "cd"
        "ab" "cd" "ab" "cd" "ab" "cd" "ab" "cd"
        "ab" "cd" "ab" "cd" "ab" "cd" "ab" "cd"
        "ab" "cd" "ab" "cd" "ab" "cd" "ab" "cd");
    h.nTime = time;
    h.nBits = 0x207fffff; // 最大 target
    h.nNonce = 0;
    // 找合法 PoW（大端首字节 <= 0x7f）
    while (!HashBeFirstByteLe(h, 0x7f)) {
        ++h.nNonce;
    }
}

int main() {
    const int64_t now = 1000000000;

    // 构造 prev + 合法 header
    LightBlockHeader prev;
    MakeHeader(prev, uint256S("00"), now - 100, 4);
    LightBlockHeader good;
    MakeHeader(good, prev.GetHash(), now - 50, 4);

    std::string reason;
    CHECK(ValidateHeader(good, &prev, 100, false, now, reason));

    // high-hash：换更难的 target 且不满足 PoW（用最大 target 的反例）
    LightBlockHeader high = good;
    high.nBits = 0x1d00ffff; // 难度更高
    // 找一个不满足的 nonce
    while (HashBeFirstByteLe(high, 0x00)) {
        ++high.nNonce;
    }
    CHECK(!ValidateHeader(high, &prev, 100, false, now, reason));
    CHECK(reason == "high-hash");

    // bad-prevblk
    LightBlockHeader badprev = good;
    badprev.hashPrevBlock = uint256S("11");
    CHECK(!ValidateHeader(badprev, &prev, 100, false, now, reason));
    CHECK(reason == "bad-prevblk");

    // time-too-old（重新挖矿，保证 PoW 仍合法）
    LightBlockHeader old;
    MakeHeader(old, prev.GetHash(), prev.nTime, 4);
    CHECK(!ValidateHeader(old, &prev, 100, false, now, reason));
    CHECK(reason == "time-too-old");

    // time-too-new（重新挖矿）
    LightBlockHeader future;
    MakeHeader(future, prev.GetHash(), static_cast<uint32_t>(now + 3 * 3600), 4);
    CHECK(!ValidateHeader(future, &prev, 100, false, now, reason));
    CHECK(reason == "time-too-new");

    // bad-version（重新挖矿）
    LightBlockHeader ver;
    MakeHeader(ver, prev.GetHash(), now - 50, 1);
    CHECK(!ValidateHeader(ver, &prev, 100, false, now, reason));
    CHECK(reason == "bad-version");

    // 历史段：只校验 prev 连续
    CHECK(ValidateHeader(good, &prev, 100, true, now, reason));

    // Checkpoint 工作量/哈希锚定
    CLightChainStore store;
    uint256 prev_hash = uint256S("00");
    for (int64_t i = 0; i < 5; ++i) {
        LightBlockHeader h;
        MakeHeader(h, prev_hash, static_cast<uint32_t>(now - 100 + i * 10), 4);
        store.AddHeader(h, i);
        store.AddWork();
        prev_hash = h.GetHash();
    }
    LightCheckpoint cp;
    cp.height = 4;
    cp.nChainWork = uint256S("5"); // 5 块占位工作量
    CHECK(CheckCheckpoint(store, cp));

    cp.nChainWork = uint256S("6"); // 不足
    CHECK(!CheckCheckpoint(store, cp));

    cp.nChainWork = uint256S("5");
    LightBlockHeader tip;
    CHECK(store.GetTip(tip));
    cp.hash = tip.GetHash();
    CHECK(CheckCheckpoint(store, cp));

    cp.hash = uint256S("ff");
    CHECK(!CheckCheckpoint(store, cp));

    TEST_MAIN_RETURN();
}
