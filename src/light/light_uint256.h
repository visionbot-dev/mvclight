#ifndef MVC_LIGHT_LIGHT_UINT256_H
#define MVC_LIGHT_LIGHT_UINT256_H

/*
 * Phase 0 临时最小 uint256 实现（32 字节原始二进制，内存布局与上游一致）。
 *
 * 设计文档要求 txid/block_hash 统一为 32B BLOB（附录 B.1/B.2）。
 * 当前自研自包含，避免 Phase 0 引入 Boost 与 rpc/text_writer 依赖；
 * Phase 2/3 导入上游 D:\Project\Sample\microvisionchain\src\uint256.h
 * （rpc-free patch + boost-functional 就绪）后，本类型将被替换/别名到上游 uint256。
 */

#include <cassert>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace mvclight {

class uint256 {
public:
    static constexpr size_t WIDTH = 32;

    uint256() { memset(m_data, 0, sizeof(m_data)); }

    explicit uint256(const std::vector<uint8_t>& vch) {
        assert(vch.size() == WIDTH);
        memcpy(m_data, vch.data(), WIDTH);
    }

    bool IsNull() const {
        for (size_t i = 0; i < WIDTH; ++i) {
            if (m_data[i] != 0) return false;
        }
        return true;
    }

    void SetNull() { memset(m_data, 0, sizeof(m_data)); }

    uint8_t* begin() { return m_data; }
    uint8_t* end() { return m_data + WIDTH; }
    const uint8_t* begin() const { return m_data; }
    const uint8_t* end() const { return m_data + WIDTH; }
    size_t size() const { return WIDTH; }

    uint64_t GetCheapHash() const {
        uint64_t v = 0;
        memcpy(&v, m_data, sizeof(v));
        return v;
    }

    std::string GetHex() const {
        static const char* hexmap = "0123456789abcdef";
        std::string hex(WIDTH * 2, '0');
        for (size_t i = 0; i < WIDTH; ++i) {
            hex[i * 2] = hexmap[m_data[WIDTH - i - 1] >> 4];
            hex[i * 2 + 1] = hexmap[m_data[WIDTH - i - 1] & 15];
        }
        return hex;
    }

    void SetHex(const std::string& str) {
        SetNull();
        size_t len = str.size();
        size_t pos = 0;
        if (len >= 2 && str[0] == '0' && (str[1] == 'x' || str[1] == 'X')) pos = 2;
        if (len <= pos) return;
        // 从字符串末尾向 data 起始填充（与上游 SetHex 行为一致：大端 hex → 小端内存）
        size_t data_idx = 0;
        size_t str_idx = len;
        while (str_idx > pos && data_idx < WIDTH) {
            uint8_t c = HexDigit(str[str_idx - 1]);
            if (c == 0xFF) { SetNull(); return; }
            m_data[data_idx] = c;
            --str_idx;
            if (str_idx > pos) {
                uint8_t hi = HexDigit(str[str_idx - 1]);
                if (hi == 0xFF) { SetNull(); return; }
                m_data[data_idx] |= static_cast<uint8_t>(hi << 4);
                --str_idx;
            }
            ++data_idx;
        }
    }

    std::string ToString() const { return GetHex(); }

    int Compare(const uint256& other) const { return memcmp(m_data, other.m_data, WIDTH); }

    friend bool operator==(const uint256& a, const uint256& b) { return a.Compare(b) == 0; }
    friend bool operator!=(const uint256& a, const uint256& b) { return a.Compare(b) != 0; }
    friend bool operator<(const uint256& a, const uint256& b) { return a.Compare(b) < 0; }

private:
    static uint8_t HexDigit(char c) {
        if (c >= '0' && c <= '9') return static_cast<uint8_t>(c - '0');
        if (c >= 'a' && c <= 'f') return static_cast<uint8_t>(c - 'a' + 10);
        if (c >= 'A' && c <= 'F') return static_cast<uint8_t>(c - 'A' + 10);
        return 0xFF;
    }

    uint8_t m_data[WIDTH];
};

inline uint256 uint256S(const std::string& str) {
    uint256 rv;
    rv.SetHex(str);
    return rv;
}

} // namespace mvclight

namespace std {
template <>
struct hash<mvclight::uint256> {
    size_t operator()(const mvclight::uint256& v) const {
        return static_cast<size_t>(v.GetCheapHash());
    }
};
} // namespace std

#endif // MVC_LIGHT_LIGHT_UINT256_H
