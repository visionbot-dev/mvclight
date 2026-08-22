// mvc_core 冒烟测试：初始化主网 chainparams 并打印创世块哈希
// 不加入 ctest；手动运行：test_mvc_core_smoke

#include <chainparams.h>
#include <primitives/block.h>

#include <cstdio>
#include <string>

int main() {
    SelectParams("main");
    const CChainParams& params = Params();
    std::string genesis = params.GenesisBlock().GetHash().GetHex();
    std::printf("CHAINPARAMS_OK genesis=%s\n", genesis.c_str());
    return genesis.empty() ? 1 : 0;
}
