#ifndef MVC_LIGHT_LIGHT_WATCHSTORE_H
#define MVC_LIGHT_LIGHT_WATCHSTORE_H

/*
 * 关注交易双表存储（设计文档 §4.8，Phase 3 内存实现）。
 * 提供 tx_store + addr_tx_index 原子写入语义、去重与磁盘满模拟。
 * Phase 5 将替换为 LevelDB/SQLite 后端。
 */

#include "light/light_uint256.h"

#include <cstdint>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

namespace mvclight {

struct TxRecord {
    uint256 txid;
    int64_t height = -1;
    uint256 block_hash;
    std::vector<uint8_t> tx_blob;
    std::vector<uint8_t> proof_blob;
    bool script_verified = true;
};

class CLightWatchStore {
public:
    // 原子写入：BEGIN -> INSERT OR IGNORE tx_store -> INSERT addr_tx_index -> COMMIT
    bool CommitTx(const std::string& addr, const TxRecord& rec);

    bool HasTx(const uint256& txid) const;
    bool GetTx(const uint256& txid, TxRecord& out) const;
    bool GetTxidsByAddr(const std::string& addr, std::vector<uint256>& out) const;

    size_t TxCount() const { return m_tx_store.size(); }
    size_t IndexCount() const { return m_addr_tx_index.size(); }

    void SetDiskFullForTest(bool full) { m_disk_full = full; }

private:
    bool CheckDiskSpace() const;

    std::unordered_map<uint256, TxRecord> m_tx_store;
    std::map<std::pair<std::string, uint256>, bool> m_addr_tx_index;
    bool m_disk_full = false;
};

} // namespace mvclight

#endif // MVC_LIGHT_LIGHT_WATCHSTORE_H
