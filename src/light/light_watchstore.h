#ifndef MVC_LIGHT_LIGHT_WATCHSTORE_H
#define MVC_LIGHT_LIGHT_WATCHSTORE_H

/*
 * 关注交易双表存储（设计文档 §4.8）。
 * 桌面端：LevelDB 持久化 + 内存索引；原子写入使用 WriteBatch。
 * 编码约定见设计文档附录 B.2：
 *   tx_store        : 0x01 | txid(32)        -> TxRecord
 *   addr_tx_index   : 0x02 | addr | txid(32) -> empty
 *   watch_addr_store: 0x03 | addr             -> empty
 */

#include "light/light_uint256.h"

#include <cstdint>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

#include "leveldb/db.h"

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
    CLightWatchStore() = default;
    ~CLightWatchStore();

    // 打开 LevelDB 并加载现有数据到内存索引
    bool Open(const std::string& path);
    void Close();
    bool IsOpen() const { return m_db != nullptr; }

    // 清空内存索引（force_reset_chain 使用；LevelDB 数据保留）
    void ClearMemory();

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
    void LoadFromDB();

    leveldb::DB* m_db = nullptr;
    std::unordered_map<uint256, TxRecord> m_tx_store;
    std::map<std::pair<std::string, uint256>, bool> m_addr_tx_index;
    bool m_disk_full = false;
};

} // namespace mvclight

#endif // MVC_LIGHT_LIGHT_WATCHSTORE_H
