#include "light/light_header.h"
#include "light/light_message.h"
#include "light/light_peer.h"
#include "light/light_validation.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using mvclight::CLightPeer;
using mvclight::LightBlockHeader;
using mvclight::LightMessage;
using mvclight::MainnetMagic;
using mvclight::PeerState;
using mvclight::ValidateHeader;
using mvclight::uint256;
using mvclight::uint256S;

namespace {

constexpr int kTargetHeight = 21256; // MVC 主网 ASERT 锚点高度
constexpr int kMaxBatches = 32;

void AppendLE32(std::vector<uint8_t>& out, uint32_t v) {
    for (int i = 0; i < 4; ++i) out.push_back(static_cast<uint8_t>(v >> (i * 8)));
}

void AppendCompactSize(std::vector<uint8_t>& out, uint64_t v) {
    if (v < 253) {
        out.push_back(static_cast<uint8_t>(v));
    } else {
        out.push_back(253);
        out.push_back(static_cast<uint8_t>(v));
        out.push_back(static_cast<uint8_t>(v >> 8));
    }
}

uint64_t ReadCompactSize(const uint8_t*& p, const uint8_t* end, bool& ok) {
    if (p >= end) { ok = false; return 0; }
    uint8_t first = *p++;
    if (first < 253) return first;
    if (first == 253) {
        if (p + 2 > end) { ok = false; return 0; }
        uint64_t v = p[0] | (uint64_t(p[1]) << 8);
        p += 2;
        return v;
    }
    if (first == 254) {
        if (p + 4 > end) { ok = false; return 0; }
        uint64_t v = 0;
        for (int i = 0; i < 4; ++i) v |= uint64_t(p[i]) << (i * 8);
        p += 4;
        return v;
    }
    if (p + 8 > end) { ok = false; return 0; }
    uint64_t v = 0;
    for (int i = 0; i < 8; ++i) v |= uint64_t(p[i]) << (i * 8);
    p += 8;
    return v;
}

std::vector<uint8_t> MakeGetHeaders(const uint256& locator) {
    std::vector<uint8_t> p;
    AppendLE32(p, 70016);
    AppendCompactSize(p, 1);
    p.insert(p.end(), locator.begin(), locator.end());
    p.insert(p.end(), 32, 0); // hashstop = 0
    return p;
}

} // namespace

int main() {
    const char* host = "47.242.24.63";
    const int port = 9883;

    CLightPeer peer;
    if (!peer.ConnectAndHandshake(host, port, 10000, 10000)) {
        std::printf("MAINNET_CONNECT_FAILED\n");
        return 1;
    }
    std::printf("MAINNET_HANDSHAKE_OK state=%s\n", mvclight::ToString(peer.GetState()));

    std::vector<LightBlockHeader> headers;
    uint256 locator; // 全零 = genesis
    bool done = false;

    for (int batch = 0; batch < kMaxBatches && !done; ++batch) {
        std::vector<uint8_t> getheaders = MakeGetHeaders(locator);
        if (!peer.SendMessage("getheaders", getheaders)) {
            std::printf("SEND_GETHEADERS_FAILED\n");
            return 1;
        }
        bool got = false;
        for (int attempt = 0; attempt < 40; ++attempt) {
            LightMessage msg;
            if (!peer.ReadMessage(msg)) {
                std::printf("READ_FAILED\n");
                return 1;
            }
            if (msg.command != "headers") continue;
            const uint8_t* p = msg.payload.data();
            const uint8_t* end = p + msg.payload.size();
            bool ok = true;
            uint64_t count = ReadCompactSize(p, end, ok);
            // 真实 headers 消息：每头 80B + 1B tx-count(0)
            if (!ok || count == 0 || count > 2000 ||
                p + count * (LightBlockHeader::kHeaderSize + 1) > end) {
                std::printf("BAD_HEADERS_FORMAT\n");
                return 1;
            }
            for (uint64_t i = 0; i < count; ++i) {
                const uint8_t* hp = p + i * (LightBlockHeader::kHeaderSize + 1);
                LightBlockHeader h;
                if (!h.Deserialize(hp, LightBlockHeader::kHeaderSize)) {
                    std::printf("BAD_HEADER_DESERIALIZE\n");
                    return 1;
                }
                if (hp[LightBlockHeader::kHeaderSize] != 0) {
                    std::printf("BAD_HEADERS_TXCOUNT\n");
                    return 1;
                }
                headers.push_back(h);
            }
            got = true;
            if (!headers.empty()) {
                locator = headers.back().GetHash();
            }
            break;
        }
        if (!got) {
            std::printf("NO_HEADERS_RESPONSE\n");
            return 1;
        }
        std::printf("BATCH %d: total headers=%zu tip=%s\n", batch, headers.size(),
                    headers.empty() ? "-" : headers.back().GetHash().GetHex().c_str());
        if (headers.size() >= static_cast<size_t>(kTargetHeight + 1)) {
            done = true;
        }
    }

    if (headers.size() < static_cast<size_t>(kTargetHeight + 1)) {
        std::printf("INSUFFICIENT_HEADERS (%zu)\n", headers.size());
        return 1;
    }

    // 校验哈希链连续性 + PoW（历史段模式）
    for (size_t i = 1; i < headers.size(); ++i) {
        LightBlockHeader prev = headers[i - 1];
        std::string reason;
        if (prev.GetHash() != headers[i].hashPrevBlock) {
            std::printf("BAD_PREV at %zu\n", i);
            return 1;
        }
        if (!ValidateHeader(headers[i], &prev, static_cast<int64_t>(i), true, 0, reason)) {
            std::printf("BAD_HEADER at %zu: %s\n", i, reason.c_str());
            return 1;
        }
    }

    const LightBlockHeader& anchor = headers[kTargetHeight];
    std::printf("MAINNET_ANCHOR height=%d hash=%s nBits=0x%08x time=%u\n",
                kTargetHeight, anchor.GetHash().GetHex().c_str(), anchor.nBits, anchor.nTime);
    std::printf("MAINNET_TIP height=%zu hash=%s\n", headers.size() - 1,
                headers.back().GetHash().GetHex().c_str());
    std::printf("MAINNET_E2E_OK\n");
    return 0;
}
