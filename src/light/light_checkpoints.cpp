#include "light/light_checkpoints.h"

namespace mvclight {

bool ChainWorkGe(const uint256& a, const uint256& b) {
    // uint256 内部为小端存储，大端数值序等价于从尾部向前 memcmp
    for (size_t i = 0; i < a.size(); ++i) {
        uint8_t ca = a.begin()[a.size() - 1 - i];
        uint8_t cb = b.begin()[b.size() - 1 - i];
        if (ca != cb) return ca > cb;
    }
    return true; // 相等
}

const LightCheckpoint& GetBuiltinCheckpoint() {
    // 2026-08-22 从主网种子 47.242.24.63:9883 实拉 22000 个头部校验后生成：
    //   height=21256（MVC 主网 ASERT 锚点高度）
    //   hash=000000000000000006f6631897b7095706a61148245e5dbf94166fe1d3c67623
    //   nBits=0x18366875
    //   nChainWork=000000000000000000000000000000000000000000023dfb816b391d12d40116
    //     （逐块 2^256/(target+1) 累加，与上游 GetBlockProof 一致）
    static const LightCheckpoint cp = {
        21256,
        uint256S("000000000000000006f6631897b7095706a61148245e5dbf94166fe1d3c67623"),
        uint256S("000000000000000000000000000000000000000000023dfb816b391d12d40116"),
    };
    return cp;
}

} // namespace mvclight
