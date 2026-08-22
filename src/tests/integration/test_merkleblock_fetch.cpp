// 调试工具：连接主网，发送 FILTERLOAD，getdata 一个过滤区块，解析 MERKLEBLOCK
// 不加入 ctest；手动运行：test_merkleblock_fetch

#include "light/light_filter.h"
#include "light/light_header.h"
#include "light/light_merkle.h"
#include "light/light_message.h"
#include "light/light_peer.h"

#include <cstdio>
#include <string>
#include <vector>

using mvclight::CBloomFilter;
using mvclight::CLightPeer;
using mvclight::LightBlockHeader;
using mvclight::LightMerkleBlock;
using mvclight::LightMessage;
using mvclight::MainnetMagic;
using mvclight::kBloomUpdateAll;
using mvclight::uint256;

namespace {

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
    p.insert(p.end(), 32, 0);
    return p;
}

} // namespace

int main() {
    CLightPeer peer;
    if (!peer.ConnectAndHandshake("47.242.24.63", 9883, 10000, 3000)) {
        std::printf("CONNECT_FAILED\n");
        return 1;
    }
    std::printf("HANDSHAKE_OK\n");

    // FILTERLOAD
    CBloomFilter filter = CBloomFilter::Create(1, 0.0001, 0, kBloomUpdateAll);
    std::string addr = "1By2LtxHQRwzhL2vYMNXuV2WQzkrXM4oS";
    filter.Insert(reinterpret_cast<const uint8_t*>(addr.data()), addr.size());
    std::vector<uint8_t> fl;
    filter.Serialize(fl);
    if (!(peer.SendMessage)("filterload", fl)) {
        std::printf("FILTERLOAD_SEND_FAILED\n");
        return 1;
    }
    std::printf("FILTERLOAD_OK bytes=%zu\n", fl.size());

    // 拉一批 headers 取 tip
    std::vector<LightBlockHeader> headers;
    uint256 locator;
    for (int batch = 0; batch < 1; ++batch) {
        std::vector<uint8_t> gh = MakeGetHeaders(locator);
        if (!(peer.SendMessage)("getheaders", gh)) return 1;
        for (int attempt = 0; attempt < 20; ++attempt) {
            LightMessage msg;
            if (!peer.ReadMessage(msg)) break;
            if (msg.command != "headers") continue;
            const uint8_t* p = msg.payload.data();
            const uint8_t* end = p + msg.payload.size();
            bool ok = true;
            uint64_t count = ReadCompactSize(p, end, ok);
            if (!ok || count == 0 || count > 2000) {
                std::printf("BAD_HEADERS\n");
                return 1;
            }
            for (uint64_t i = 0; i < count; ++i) {
                const uint8_t* hp = p + i * 81;
                LightBlockHeader h;
                h.Deserialize(hp, 80);
                headers.push_back(h);
            }
            locator = headers.back().GetHash();
            break;
        }
    }
    if (headers.empty()) {
        std::printf("NO_HEADERS\n");
        return 1;
    }
    std::printf("TIP height=%zu hash=%s\n", headers.size() - 1,
                headers.back().GetHash().GetHex().c_str());

    // getdata filtered block
    std::vector<uint8_t> gd;
    AppendCompactSize(gd, 1);
    AppendLE32(gd, 3); // MSG_FILTERED_BLOCK
    const uint256& blk = headers.back().GetHash();
    gd.insert(gd.end(), blk.begin(), blk.end());
    if (!(peer.SendMessage)("getdata", gd)) {
        std::printf("GETDATA_FAILED\n");
        return 1;
    }

    for (int attempt = 0; attempt < 20; ++attempt) {
        LightMessage msg;
        if (!peer.ReadMessage(msg)) {
            std::printf("READ_FALSE connected=%d\n", peer.IsConnected() ? 1 : 0);
            break;
        }
        std::printf("MSG command=%s size=%zu\n", msg.command.c_str(), msg.payload.size());
        if (msg.command == "merkleblock") {
            LightMerkleBlock mb;
            bool ok = mb.Deserialize(msg.payload.data(), msg.payload.size());
            std::printf("MERKLE_DESERIALIZE %s\n", ok ? "OK" : "FAILED");
            if (ok) {
                std::vector<uint256> matched;
                uint256 root;
                bool ex = mb.ExtractMatches(matched, root);
                std::printf("MERKLE_EXTRACT %s matched=%zu\n", ex ? "OK" : "FAILED",
                            matched.size());
                return ex ? 0 : 1;
            }
            // dump first bytes
            for (size_t i = 0; i < 16 && i < msg.payload.size(); ++i) {
                std::printf("%02x ", msg.payload[i]);
            }
            std::printf("\n");
            return 1;
        }
        if (msg.command == "tx") {
            std::printf("TX size=%zu\n", msg.payload.size());
        }
    }
    std::printf("NO_MERKLEBLOCK\n");
    return 1;
}
