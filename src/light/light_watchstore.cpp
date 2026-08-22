#include "light/light_watchstore.h"

#include <algorithm>
#include <cstring>
#include <memory>

#include "leveldb/write_batch.h"

namespace mvclight {

namespace {

constexpr uint8_t kPrefixTxStore = 0x01;
constexpr uint8_t kPrefixAddrIndex = 0x02;

void AppendU64(std::vector<uint8_t>& out, uint64_t v) {
    for (int i = 0; i < 8; ++i) out.push_back(static_cast<uint8_t>(v >> (i * 8)));
}

uint64_t ReadU64(const uint8_t* p) {
    uint64_t v = 0;
    for (int i = 7; i >= 0; --i) v = (v << 8) | p[i];
    return v;
}

void AppendU32(std::vector<uint8_t>& out, uint32_t v) {
    for (int i = 0; i < 4; ++i) out.push_back(static_cast<uint8_t>(v >> (i * 8)));
}

uint32_t ReadU32(const uint8_t* p) {
    return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}

std::vector<uint8_t> MakeTxKey(const uint256& txid) {
    std::vector<uint8_t> key;
    key.push_back(kPrefixTxStore);
    key.insert(key.end(), txid.begin(), txid.end());
    return key;
}

std::vector<uint8_t> MakeAddrKey(const std::string& addr, const uint256& txid) {
    std::vector<uint8_t> key;
    key.push_back(kPrefixAddrIndex);
    key.insert(key.end(), addr.begin(), addr.end());
    key.push_back(0);
    key.insert(key.end(), txid.begin(), txid.end());
    return key;
}

std::vector<uint8_t> SerializeRecord(const TxRecord& rec) {
    std::vector<uint8_t> out;
    AppendU64(out, static_cast<uint64_t>(rec.height));
    out.insert(out.end(), rec.block_hash.begin(), rec.block_hash.end());
    out.push_back(rec.script_verified ? 1 : 0);
    AppendU32(out, static_cast<uint32_t>(rec.tx_blob.size()));
    out.insert(out.end(), rec.tx_blob.begin(), rec.tx_blob.end());
    AppendU32(out, static_cast<uint32_t>(rec.proof_blob.size()));
    out.insert(out.end(), rec.proof_blob.begin(), rec.proof_blob.end());
    return out;
}

bool DeserializeRecord(const uint8_t* p, size_t len, TxRecord& rec) {
    if (len < 8 + 32 + 1) return false;
    rec.height = static_cast<int64_t>(ReadU64(p));
    p += 8;
    memcpy(rec.block_hash.begin(), p, 32);
    p += 32;
    rec.script_verified = (*p != 0);
    ++p;
    if (len < 8 + 32 + 1 + 4) return false;
    uint32_t blob_len = ReadU32(p);
    p += 4;
    if (len < 8 + 32 + 1 + 4 + blob_len + 4) return false;
    rec.tx_blob.assign(p, p + blob_len);
    p += blob_len;
    uint32_t proof_len = ReadU32(p);
    p += 4;
    if (len < 8 + 32 + 1 + 4 + blob_len + 4 + proof_len) return false;
    rec.proof_blob.assign(p, p + proof_len);
    return true;
}

} // namespace

CLightWatchStore::~CLightWatchStore() {
    Close();
}

bool CLightWatchStore::Open(const std::string& path) {
    Close();
    leveldb::Options opts;
    opts.create_if_missing = true;
    leveldb::DB* db = nullptr;
    leveldb::Status st = leveldb::DB::Open(opts, path, &db);
    if (!st.ok()) return false;
    m_db = db;
    LoadFromDB();
    return true;
}

void CLightWatchStore::Close() {
    if (m_db != nullptr) {
        delete m_db;
        m_db = nullptr;
    }
    m_tx_store.clear();
    m_addr_tx_index.clear();
}

void CLightWatchStore::ClearMemory() {
    m_tx_store.clear();
    m_addr_tx_index.clear();
}

void CLightWatchStore::LoadFromDB() {
    if (m_db == nullptr) return;
    std::unique_ptr<leveldb::Iterator> it(m_db->NewIterator(leveldb::ReadOptions()));
    for (it->SeekToFirst(); it->Valid(); it->Next()) {
        leveldb::Slice key = it->key();
        leveldb::Slice val = it->value();
        const uint8_t* kp = reinterpret_cast<const uint8_t*>(key.data());
        if (key.size() >= 33 && kp[0] == kPrefixTxStore) {
            TxRecord rec;
            std::vector<uint8_t> txid_bytes(kp + 1, kp + 33);
            rec.txid = uint256(txid_bytes);
            if (DeserializeRecord(reinterpret_cast<const uint8_t*>(val.data()), val.size(), rec)) {
                m_tx_store.emplace(rec.txid, rec);
            }
        } else if (key.size() > 33 && kp[0] == kPrefixAddrIndex) {
            // 0x02 | addr | 0x00 | txid(32)
            const uint8_t* p = kp + 1;
            const uint8_t* end = kp + key.size();
            const uint8_t* sep = std::find(p, end, 0);
            if (sep != end && end - sep == 33) {
                std::string addr(p, sep);
                std::vector<uint8_t> txid_bytes(sep + 1, end);
                m_addr_tx_index.emplace(std::make_pair(addr, uint256(txid_bytes)), true);
            }
        }
    }
}

bool CLightWatchStore::CheckDiskSpace() const {
    return !m_disk_full;
}

bool CLightWatchStore::CommitTx(const std::string& addr, const TxRecord& rec) {
    if (!CheckDiskSpace()) {
        return false; // ERR_DISK_FULL
    }

    // 1. LevelDB WriteBatch 原子提交
    if (m_db != nullptr) {
        leveldb::WriteBatch batch;
        if (!HasTx(rec.txid)) {
            std::vector<uint8_t> tx_key = MakeTxKey(rec.txid);
            std::vector<uint8_t> tx_val = SerializeRecord(rec);
            batch.Put(leveldb::Slice(reinterpret_cast<const char*>(tx_key.data()), tx_key.size()),
                      leveldb::Slice(reinterpret_cast<const char*>(tx_val.data()), tx_val.size()));
        }
        std::vector<uint8_t> addr_key = MakeAddrKey(addr, rec.txid);
        batch.Put(leveldb::Slice(reinterpret_cast<const char*>(addr_key.data()), addr_key.size()),
                  leveldb::Slice());
        leveldb::Status st = m_db->Write(leveldb::WriteOptions(), &batch);
        if (!st.ok()) {
            return false; // 原子失败，不更新内存
        }
    }

    // 2. 更新内存索引
    m_tx_store.emplace(rec.txid, rec); // txid 去重：已存在则保持原记录
    m_addr_tx_index.emplace(std::make_pair(addr, rec.txid), true);
    return true;
}

bool CLightWatchStore::HasTx(const uint256& txid) const {
    return m_tx_store.count(txid) != 0;
}

bool CLightWatchStore::GetTx(const uint256& txid, TxRecord& out) const {
    auto it = m_tx_store.find(txid);
    if (it == m_tx_store.end()) return false;
    out = it->second;
    return true;
}

bool CLightWatchStore::GetTxidsByAddr(const std::string& addr, std::vector<uint256>& out) const {
    out.clear();
    for (const auto& kv : m_addr_tx_index) {
        if (kv.first.first == addr) {
            out.push_back(kv.first.second);
        }
    }
    return !out.empty();
}

} // namespace mvclight
