#ifndef MVC_LIGHT_LIGHT_SHA256_H
#define MVC_LIGHT_LIGHT_SHA256_H

/*
 * 轻量 SHA-256 实现（自包含，无外部依赖）。
 * 用于 P2P 消息头 checksum = SHA256(SHA256(payload)) 前 4 字节。
 * Phase 2 若导入上游 crypto/sha256 可替换。
 */

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace mvclight {

class CSHA256 {
public:
    CSHA256();

    CSHA256& Write(const uint8_t* data, size_t len);
    void Finalize(uint8_t hash[32]);
    std::string FinalizeHex();

    static void Hash(const uint8_t* data, size_t len, uint8_t hash[32]);

private:
    void Transform(const uint8_t block[64]);

    uint32_t m_state[8];
    uint64_t m_bytes;
    uint8_t m_buffer[64];
    size_t m_buffer_len;
};

// SHA256d：SHA256(SHA256(data))
void SHA256D(const uint8_t* data, size_t len, uint8_t hash[32]);

} // namespace mvclight

#endif // MVC_LIGHT_LIGHT_SHA256_H
