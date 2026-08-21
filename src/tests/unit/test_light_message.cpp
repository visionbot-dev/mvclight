#include "light/light_message.h"
#include "light/light_sha256.h"

#include "test_framework.h"

#include <cstring>
#include <string>
#include <vector>

using mvclight::EncodeMessage;
using mvclight::MainnetMagic;
using mvclight::VerifyChecksum;

static uint32_t ReadLE32(const uint8_t* p) {
    return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}

int main() {
    std::vector<uint8_t> payload = {'h', 'e', 'l', 'l', 'o'};
    std::vector<uint8_t> msg;
    CHECK(EncodeMessage(MainnetMagic(), "ping", payload, msg));
    CHECK_EQ(msg.size(), mvclight::kMessageHeaderSize + payload.size());

    // magic
    CHECK(memcmp(msg.data(), MainnetMagic(), 4) == 0);
    // command 零填充
    CHECK(memcmp(msg.data() + 4, "ping", 4) == 0);
    CHECK(msg[4 + 4] == 0);
    // payload len
    CHECK_EQ(ReadLE32(msg.data() + 16), payload.size());
    // checksum = SHA256d(payload) 前 4 字节
    uint8_t hash[32];
    mvclight::SHA256D(payload.data(), payload.size(), hash);
    CHECK_EQ(ReadLE32(msg.data() + 20), ReadLE32(hash));
    // payload 原样
    CHECK(memcmp(msg.data() + mvclight::kMessageHeaderSize, payload.data(), payload.size()) == 0);

    // VerifyChecksum
    CHECK(VerifyChecksum(payload, ReadLE32(hash)));
    std::vector<uint8_t> tampered = payload;
    tampered[0] ^= 0xFF;
    CHECK(!VerifyChecksum(tampered, ReadLE32(hash)));

    // 非法参数
    std::vector<uint8_t> out;
    CHECK(!EncodeMessage(MainnetMagic(), std::string(13, 'x'), payload, out));
    CHECK(!EncodeMessage(MainnetMagic(), "ping", std::vector<uint8_t>(mvclight::kMessageMaxPayloadSize + 1), out));

    TEST_MAIN_RETURN();
}
