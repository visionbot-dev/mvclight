#ifndef MVC_LIGHT_LIGHT_CHAINSTORE_H
#define MVC_LIGHT_LIGHT_CHAINSTORE_H

/*
 * Header 链缓存（Phase 2 内存实现）。
 * 提供按高度/哈希索引；Phase 3 接入 LevelDB/SQLite 持久化 meta。
 */

#include "light/light_header.h"
#include "light/light_uint256.h"

#include <cstdint>
#include <map>
#include <unordered_map>

namespace mvclight {

class CLightChainStore {
public:
    bool AddHeader(const LightBlockHeader& header, int64_t height);
    bool GetHeaderAtHeight(int64_t height, LightBlockHeader& out) const;
    bool GetHeightByHash(const uint256& hash, int64_t& height) const;
    bool HasHash(const uint256& hash) const;

    int64_t TipHeight() const { return m_tip_height; }
    bool GetTip(LightBlockHeader& out) const;

    // 累计工作量（Phase 2 占位：每块 +1；发布前替换为真实 2^256/(target+1) 累加）
    void AddWork();
    const uint256& ChainWork() const { return m_chainwork; }

    void Reset();

private:
    std::map<int64_t, LightBlockHeader> m_by_height;
    std::unordered_map<uint256, int64_t> m_by_hash;
    int64_t m_tip_height = -1;
    uint256 m_chainwork;
};

} // namespace mvclight

#endif // MVC_LIGHT_LIGHT_CHAINSTORE_H
