#ifndef MVC_LIGHT_LIGHT_MESSAGE_H
#define MVC_LIGHT_LIGHT_MESSAGE_H

/*
 * P2P 消息帧读写（Phase 1 轻量实现，设计文档 §4.1.1）。
 *
 * 帧格式（与 BTC/BCH/MVC 一致）：
 *   magic(4) + command(12, '\0' 填充) + payload_len(4 LE) + checksum(4 LE) + payload
 * checksum = SHA256(SHA256(payload)) 前 4 字节。
 */

#include "light/light_socket.h"

#include <cstdint>
#include <string>
#include <vector>

namespace mvclight {

constexpr uint32_t kMessageHeaderSize = 24;
constexpr uint32_t kMessageCommandSize = 12;
constexpr uint32_t kMessageMaxPayloadSize = 32 * 1024 * 1024; // Phase 1 上限，后续按 maxTxSize 收紧

// MVC 主网 / 测试网 P2P magic（来源：上游 chainparams.cpp）
inline const uint8_t* MainnetMagic() {
    static const uint8_t magic[4] = {0x52, 0x9B, 0x68, 0x96};
    return magic;
}
inline const uint8_t* TestnetMagic() {
    static const uint8_t magic[4] = {0xD4, 0x48, 0x28, 0x12};
    return magic;
}

struct LightMessage {
    std::string command;
    std::vector<uint8_t> payload;
};

// 编码完整消息帧；command 长度不得超过 kMessageCommandSize
bool EncodeMessage(const uint8_t magic[4], const std::string& command,
                   const std::vector<uint8_t>& payload, std::vector<uint8_t>& out);

// 从 socket 读取一条完整消息；失败（超时/校验失败/长度异常）返回 false 并关闭连接
bool ReadMessage(CLightSocket& sock, const uint8_t magic[4], LightMessage& msg);

// 校验 payload 的 SHA256d 前 4 字节是否等于 checksum（小端）
bool VerifyChecksum(const std::vector<uint8_t>& payload, uint32_t checksum);

} // namespace mvclight

#endif // MVC_LIGHT_LIGHT_MESSAGE_H
