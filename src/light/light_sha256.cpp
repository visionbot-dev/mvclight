#include "light/light_sha256.h"

#include <cstring>

namespace mvclight {

namespace {

constexpr uint32_t K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1,
    0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786,
    0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
    0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
    0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a,
    0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
};

inline uint32_t rotr(uint32_t x, uint32_t n) { return (x >> n) | (x << (32 - n)); }

} // namespace

CSHA256::CSHA256() : m_bytes(0), m_buffer_len(0) {
    m_state[0] = 0x6a09e667;
    m_state[1] = 0xbb67ae85;
    m_state[2] = 0x3c6ef372;
    m_state[3] = 0xa54ff53a;
    m_state[4] = 0x510e527f;
    m_state[5] = 0x9b05688c;
    m_state[6] = 0x1f83d9ab;
    m_state[7] = 0x5be0cd19;
}

void CSHA256::Transform(const uint8_t block[64]) {
    uint32_t w[64];
    for (int i = 0; i < 16; ++i) {
        w[i] = (uint32_t(block[i * 4]) << 24) | (uint32_t(block[i * 4 + 1]) << 16) |
               (uint32_t(block[i * 4 + 2]) << 8) | uint32_t(block[i * 4 + 3]);
    }
    for (int i = 16; i < 64; ++i) {
        uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
        uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }

    uint32_t a = m_state[0], b = m_state[1], c = m_state[2], d = m_state[3];
    uint32_t e = m_state[4], f = m_state[5], g = m_state[6], h = m_state[7];

    for (int i = 0; i < 64; ++i) {
        uint32_t S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
        uint32_t ch = (e & f) ^ (~e & g);
        uint32_t temp1 = h + S1 + ch + K[i] + w[i];
        uint32_t S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t temp2 = S0 + maj;
        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
    }

    m_state[0] += a;
    m_state[1] += b;
    m_state[2] += c;
    m_state[3] += d;
    m_state[4] += e;
    m_state[5] += f;
    m_state[6] += g;
    m_state[7] += h;
}

CSHA256& CSHA256::Write(const uint8_t* data, size_t len) {
    m_bytes += len;
    while (len > 0) {
        size_t take = 64 - m_buffer_len;
        if (take > len) take = len;
        memcpy(m_buffer + m_buffer_len, data, take);
        m_buffer_len += take;
        data += take;
        len -= take;
        if (m_buffer_len == 64) {
            Transform(m_buffer);
            m_buffer_len = 0;
        }
    }
    return *this;
}

void CSHA256::Finalize(uint8_t hash[32]) {
    uint64_t bits = m_bytes * 8;
    uint8_t pad = 0x80;
    Write(&pad, 1);
    uint8_t zero = 0;
    while (m_buffer_len != 56) {
        Write(&zero, 1);
    }
    uint8_t len_bytes[8];
    for (int i = 0; i < 8; ++i) {
        len_bytes[i] = static_cast<uint8_t>(bits >> (56 - i * 8));
    }
    Write(len_bytes, 8);
    // m_buffer_len 此时应为 0；若不为 0 说明输入长度异常，仍输出当前状态
    for (int i = 0; i < 8; ++i) {
        hash[i * 4] = static_cast<uint8_t>(m_state[i] >> 24);
        hash[i * 4 + 1] = static_cast<uint8_t>(m_state[i] >> 16);
        hash[i * 4 + 2] = static_cast<uint8_t>(m_state[i] >> 8);
        hash[i * 4 + 3] = static_cast<uint8_t>(m_state[i]);
    }
}

std::string CSHA256::FinalizeHex() {
    uint8_t hash[32];
    Finalize(hash);
    static const char* hex = "0123456789abcdef";
    std::string out(64, '0');
    for (int i = 0; i < 32; ++i) {
        out[i * 2] = hex[hash[i] >> 4];
        out[i * 2 + 1] = hex[hash[i] & 15];
    }
    return out;
}

void CSHA256::Hash(const uint8_t* data, size_t len, uint8_t hash[32]) {
    CSHA256 ctx;
    ctx.Write(data, len);
    ctx.Finalize(hash);
}

void SHA256D(const uint8_t* data, size_t len, uint8_t hash[32]) {
    uint8_t inner[32];
    CSHA256::Hash(data, len, inner);
    CSHA256::Hash(inner, 32, hash);
}

} // namespace mvclight
