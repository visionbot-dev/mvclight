#include "light/light_message.h"

#include "light/light_sha256.h"

#include <cstring>

namespace mvclight {

namespace {

bool ReadExact(CLightSocket& sock, uint8_t* buf, size_t len) {
    size_t got = 0;
    while (got < len) {
        size_t n = sock.RecvSome(buf + got, len - got);
        if (n == 0) {
            return false;
        }
        got += n;
    }
    return true;
}

uint32_t ReadLE32(const uint8_t* p) {
    return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}

void WriteLE32(uint8_t* p, uint32_t v) {
    p[0] = static_cast<uint8_t>(v);
    p[1] = static_cast<uint8_t>(v >> 8);
    p[2] = static_cast<uint8_t>(v >> 16);
    p[3] = static_cast<uint8_t>(v >> 24);
}

} // namespace

bool EncodeMessage(const uint8_t magic[4], const std::string& command,
                   const std::vector<uint8_t>& payload, std::vector<uint8_t>& out) {
    if (command.size() > kMessageCommandSize || payload.size() > kMessageMaxPayloadSize) {
        return false;
    }
    out.resize(kMessageHeaderSize + payload.size());
    memcpy(out.data(), magic, 4);
    memset(out.data() + 4, 0, kMessageCommandSize);
    memcpy(out.data() + 4, command.data(), command.size());
    WriteLE32(out.data() + 16, static_cast<uint32_t>(payload.size()));

    uint8_t hash[32];
    SHA256D(payload.data(), payload.size(), hash);
    WriteLE32(out.data() + 20, ReadLE32(hash));

    if (!payload.empty()) {
        memcpy(out.data() + kMessageHeaderSize, payload.data(), payload.size());
    }
    return true;
}

bool VerifyChecksum(const std::vector<uint8_t>& payload, uint32_t checksum) {
    uint8_t hash[32];
    SHA256D(payload.data(), payload.size(), hash);
    return ReadLE32(hash) == checksum;
}

bool ReadMessage(CLightSocket& sock, const uint8_t magic[4], LightMessage& msg) {
    uint8_t header[kMessageHeaderSize];
    if (!ReadExact(sock, header, sizeof(header))) {
        sock.Close();
        return false;
    }
    if (memcmp(header, magic, 4) != 0) {
        sock.Close();
        return false;
    }

    char cmd[kMessageCommandSize + 1];
    memcpy(cmd, header + 4, kMessageCommandSize);
    cmd[kMessageCommandSize] = '\0';
    // 去掉尾部 '\0'
    size_t cmd_len = strnlen(cmd, kMessageCommandSize);
    msg.command.assign(cmd, cmd_len);

    uint32_t payload_len = ReadLE32(header + 16);
    uint32_t checksum = ReadLE32(header + 20);
    if (payload_len > kMessageMaxPayloadSize) {
        sock.Close();
        return false;
    }

    msg.payload.resize(payload_len);
    if (payload_len > 0 && !ReadExact(sock, msg.payload.data(), payload_len)) {
        sock.Close();
        return false;
    }

    if (!VerifyChecksum(msg.payload, checksum)) {
        sock.Close();
        return false;
    }
    return true;
}

} // namespace mvclight
