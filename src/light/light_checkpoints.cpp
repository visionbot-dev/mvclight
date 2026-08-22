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
    // TODO(发布前)：从可信主网全节点导出真实 (height, hash, nChainWork)
    static const LightCheckpoint cp = {
        150000,
        uint256S("0000000000000000000000000000000000000000000000000000000000000000"),
        uint256S("0000000000000000000000000000000000000000000000000000000000000000"),
    };
    return cp;
}

} // namespace mvclight
