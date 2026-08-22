#ifndef MVC_LIGHT_LIGHT_FILTER_H
#define MVC_LIGHT_LIGHT_FILTER_H

/*
 * BIP-37 布隆过滤器封装（Phase 2 自包含实现）。
 *
 * 设计文档 §4.2：
 *   - MAX_FILTER_ELEMENTS = 20000
 *   - MAX_FILTER_BYTES    = 32768
 *   - nFlags = BLOOM_UPDATE_ALL
 *   - 超限拒绝新增 -> ERR_FILTER_FULL
 *
 * 序列化格式与 FILTERLOAD payload 兼容：
 *   vData(varstr) + nHashFuncs(4 LE) + nTweak(4 LE) + nFlags(1)
 */

#include <cstddef>
#include <cstdint>
#include <vector>

namespace mvclight {

constexpr size_t kMaxFilterElements = 20000;
constexpr size_t kMaxFilterBytes = 32768;
constexpr uint8_t kBloomUpdateAll = 2; // BLOOM_UPDATE_ALL

class CBloomFilter {
public:
    CBloomFilter() = default;

    // 按期望元素数与误报率创建过滤器
    static CBloomFilter Create(size_t nElements, double fp_rate, uint32_t nTweak,
                               uint8_t nFlags = kBloomUpdateAll);

    bool Insert(const uint8_t* data, size_t len);
    bool Contains(const uint8_t* data, size_t len) const;

    size_t ElementCount() const { return m_element_count; }
    bool IsFull() const { return m_element_count >= kMaxFilterElements; }

    // 编码为 FILTERLOAD payload
    bool Serialize(std::vector<uint8_t>& out) const;

private:
    bool Hash(uint32_t nHashNum, const uint8_t* data, size_t len, uint32_t& out) const;

    std::vector<uint8_t> m_vData;
    uint32_t m_nHashFuncs = 0;
    uint32_t m_nTweak = 0;
    uint8_t m_nFlags = kBloomUpdateAll;
    size_t m_element_count = 0;
};

} // namespace mvclight

#endif // MVC_LIGHT_LIGHT_FILTER_H
