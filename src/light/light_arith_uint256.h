#ifndef MVC_LIGHT_LIGHT_ARITH_UINT256_H
#define MVC_LIGHT_LIGHT_ARITH_UINT256_H

/*
 * 最小 256 位无符号整数（Phase 5，用于 ASERT 难度计算）。
 * 内部为 8x uint32 小端字序，与上游 arith_uint256 行为对齐（仅实现所需运算）。
 */

#include "light/light_uint256.h"

#include <cstdint>

namespace mvclight {

class arith_uint256 {
public:
    arith_uint256() { for (auto& w : m_words) w = 0; }
    explicit arith_uint256(uint64_t v) {
        for (auto& w : m_words) w = 0;
        m_words[0] = static_cast<uint32_t>(v);
        m_words[1] = static_cast<uint32_t>(v >> 32);
    }

    static arith_uint256 FromUint256(const uint256& v);
    uint256 ToUint256() const;

    uint32_t GetCompact() const;
    void SetCompact(uint32_t nCompact);

    arith_uint256& operator<<=(int shift);
    arith_uint256& operator>>=(int shift);
    arith_uint256& operator*=(uint64_t factor);
    arith_uint256& operator+=(const arith_uint256& o);
    arith_uint256& operator-=(const arith_uint256& o);
    arith_uint256& operator/=(const arith_uint256& o);
    arith_uint256 operator~() const;

    bool IsZero() const;
    bool operator==(const arith_uint256& o) const;
    bool operator!=(const arith_uint256& o) const { return !(*this == o); }
    bool operator<(const arith_uint256& o) const;
    bool operator>(const arith_uint256& o) const { return o < *this; }
    bool operator<=(const arith_uint256& o) const { return !(o < *this); }
    bool operator>=(const arith_uint256& o) const { return !(*this < o); }

    friend arith_uint256 operator*(const arith_uint256& a, uint64_t f) {
        arith_uint256 r = a;
        r *= f;
        return r;
    }
    friend arith_uint256 operator<<(const arith_uint256& a, int s) {
        arith_uint256 r = a;
        r <<= s;
        return r;
    }
    friend arith_uint256 operator>>(const arith_uint256& a, int s) {
        arith_uint256 r = a;
        r >>= s;
        return r;
    }
    friend arith_uint256 operator+(const arith_uint256& a, const arith_uint256& b) {
        arith_uint256 r = a;
        r += b;
        return r;
    }
    friend arith_uint256 operator-(const arith_uint256& a, const arith_uint256& b) {
        arith_uint256 r = a;
        r -= b;
        return r;
    }
    friend arith_uint256 operator/(const arith_uint256& a, const arith_uint256& b) {
        arith_uint256 r = a;
        r /= b;
        return r;
    }

private:
    uint32_t m_words[8];
};

} // namespace mvclight

#endif // MVC_LIGHT_LIGHT_ARITH_UINT256_H
