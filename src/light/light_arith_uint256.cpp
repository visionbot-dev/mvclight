#include "light/light_arith_uint256.h"

#include <cstring>
#include <vector>

namespace mvclight {

namespace {

constexpr uint32_t kLimbCount = 8;

} // namespace

arith_uint256 arith_uint256::FromUint256(const uint256& v) {
    arith_uint256 r;
    for (int i = 0; i < 32; ++i) {
        r.m_words[i / 4] |= uint32_t(v.begin()[i]) << ((i % 4) * 8);
    }
    return r;
}

uint256 arith_uint256::ToUint256() const {
    std::vector<uint8_t> bytes(32, 0);
    for (int i = 0; i < 32; ++i) {
        bytes[i] = static_cast<uint8_t>(m_words[i / 4] >> ((i % 4) * 8));
    }
    return uint256(bytes);
}

void arith_uint256::SetCompact(uint32_t nCompact) {
    int nSize = nCompact >> 24;
    uint32_t nWord = nCompact & 0x007fffff;
    if (nSize <= 3) {
        nWord >>= 8 * (3 - nSize);
        m_words[0] = nWord;
        for (int i = 1; i < kLimbCount; ++i) m_words[i] = 0;
    } else {
        for (int i = 0; i < kLimbCount; ++i) m_words[i] = 0;
        int shift = 8 * (nSize - 3);
        if (shift < 256) {
            int word = shift / 32;
            int bit = shift % 32;
            if (word < kLimbCount) m_words[word] |= nWord << bit;
            if (bit != 0 && word + 1 < kLimbCount) m_words[word + 1] |= nWord >> (32 - bit);
        }
    }
    // 负/溢出标志位（Bitcoin 中 nCompact 符号位）本实现忽略，仅处理正数
    if ((nCompact & 0x00800000) != 0) {
        // 负号位：MVC 主网难度均为正数，置零处理
    }
}

uint32_t arith_uint256::GetCompact() const {
    int nSize = 0;
    for (int i = kLimbCount - 1; i >= 0; --i) {
        if (m_words[i] != 0) {
            nSize = i + 1;
            break;
        }
    }
    if (nSize == 0) return 0;
    uint32_t nWord = 0;
    for (int i = nSize - 1; i >= 0 && nWord == 0; --i) {
        nWord = m_words[i];
    }
    // 找到最高非零字，确定其最高有效字节
    int high_bit = 31;
    while (high_bit > 0 && ((nWord >> high_bit) & 1) == 0) --high_bit;
    int nCompactSize = nSize * 4 - (31 - high_bit) / 8;
    uint32_t nCompactWord = nWord >> ((31 - high_bit) / 8 * 8);
    if (nCompactWord & 0x00800000) {
        nCompactWord >>= 8;
        ++nCompactSize;
    }
    return (static_cast<uint32_t>(nCompactSize) << 24) | (nCompactWord & 0x007fffff);
}

arith_uint256& arith_uint256::operator<<=(int shift) {
    if (shift <= 0) return *this;
    int word_shift = shift / 32;
    int bit_shift = shift % 32;
    if (word_shift >= kLimbCount) {
        for (auto& w : m_words) w = 0;
        return *this;
    }
    if (bit_shift == 0) {
        for (int i = kLimbCount - 1; i >= word_shift; --i) m_words[i] = m_words[i - word_shift];
        for (int i = 0; i < word_shift; ++i) m_words[i] = 0;
        return *this;
    }
    for (int i = kLimbCount - 1; i >= 0; --i) {
        uint32_t val = 0;
        int src = i - word_shift;
        if (src >= 0) {
            val = m_words[src] << bit_shift;
            if (src - 1 >= 0) val |= m_words[src - 1] >> (32 - bit_shift);
        }
        m_words[i] = val;
    }
    return *this;
}

arith_uint256& arith_uint256::operator>>=(int shift) {
    if (shift <= 0) return *this;
    int word_shift = shift / 32;
    int bit_shift = shift % 32;
    if (word_shift >= kLimbCount) {
        for (auto& w : m_words) w = 0;
        return *this;
    }
    if (bit_shift == 0) {
        for (int i = 0; i < kLimbCount - word_shift; ++i) m_words[i] = m_words[i + word_shift];
        for (int i = kLimbCount - word_shift; i < kLimbCount; ++i) m_words[i] = 0;
        return *this;
    }
    for (int i = 0; i < kLimbCount; ++i) {
        uint32_t val = 0;
        int src = i + word_shift;
        if (src < kLimbCount) {
            val = m_words[src] >> bit_shift;
            if (src + 1 < kLimbCount) val |= m_words[src + 1] << (32 - bit_shift);
        }
        m_words[i] = val;
    }
    return *this;
}

arith_uint256& arith_uint256::operator*=(uint64_t factor) {
    uint64_t carry = 0;
    for (int i = 0; i < kLimbCount; ++i) {
        uint64_t cur = uint64_t(m_words[i]) * factor + carry;
        m_words[i] = static_cast<uint32_t>(cur);
        carry = cur >> 32;
    }
    return *this;
}

arith_uint256& arith_uint256::operator+=(const arith_uint256& o) {
    uint64_t carry = 0;
    for (int i = 0; i < kLimbCount; ++i) {
        uint64_t cur = uint64_t(m_words[i]) + o.m_words[i] + carry;
        m_words[i] = static_cast<uint32_t>(cur);
        carry = cur >> 32;
    }
    return *this;
}

arith_uint256& arith_uint256::operator-=(const arith_uint256& o) {
    uint64_t borrow = 0;
    for (int i = 0; i < kLimbCount; ++i) {
        uint64_t cur = uint64_t(m_words[i]) - uint64_t(o.m_words[i]) - borrow;
        m_words[i] = static_cast<uint32_t>(cur);
        borrow = (cur >> 32) & 1;
    }
    return *this;
}

arith_uint256 arith_uint256::operator~() const {
    arith_uint256 r;
    for (int i = 0; i < kLimbCount; ++i) r.m_words[i] = ~m_words[i];
    return r;
}

arith_uint256& arith_uint256::operator/=(const arith_uint256& divisor) {
    if (divisor.IsZero()) return *this; // 除零保持原值（调用方保证非零）
    arith_uint256 quotient;
    arith_uint256 rem;
    for (int bit = 255; bit >= 0; --bit) {
        rem <<= 1;
        int word = bit / 32;
        int off = bit % 32;
        rem.m_words[0] |= (m_words[word] >> off) & 1;
        if (rem >= divisor) {
            rem -= divisor;
            quotient.m_words[word] |= (1u << off);
        }
    }
    *this = quotient;
    return *this;
}

bool arith_uint256::IsZero() const {
    for (auto w : m_words) if (w != 0) return false;
    return true;
}

bool arith_uint256::operator==(const arith_uint256& o) const {
    for (int i = 0; i < kLimbCount; ++i) if (m_words[i] != o.m_words[i]) return false;
    return true;
}

bool arith_uint256::operator<(const arith_uint256& o) const {
    for (int i = kLimbCount - 1; i >= 0; --i) {
        if (m_words[i] < o.m_words[i]) return true;
        if (m_words[i] > o.m_words[i]) return false;
    }
    return false;
}

} // namespace mvclight
