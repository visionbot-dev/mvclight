#include "light/light_sync.h"

#include "light/light_asert.h"
#include "light/light_header.h"
#include "light/light_validation.h"

#include <cstring>
#include <vector>

namespace mvclight {

namespace {

void WriteLE32(std::vector<uint8_t>& out, uint32_t v) {
    for (int i = 0; i < 4; ++i) out.push_back(static_cast<uint8_t>(v >> (i * 8)));
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

void WriteCompactSize(std::vector<uint8_t>& out, uint64_t v) {
    if (v < 253) {
        out.push_back(static_cast<uint8_t>(v));
    } else if (v <= 0xFFFF) {
        out.push_back(253);
        out.push_back(static_cast<uint8_t>(v));
        out.push_back(static_cast<uint8_t>(v >> 8));
    } else {
        out.push_back(254);
        for (int i = 0; i < 4; ++i) out.push_back(static_cast<uint8_t>(v >> (i * 8)));
    }
}

} // namespace

bool CLightSync::SendGetHeaders(CLightPeer& peer, const LightCheckpoint& cp,
                                const CLightChainStore& store) {
    std::vector<uint8_t> payload;
    WriteLE32(payload, 70016); // PROTOCOL_VERSION

    std::vector<uint8_t> locator;
    if (store.TipHeight() >= 0) {
        LightBlockHeader tip;
        if (!store.GetTip(tip)) return false;
        uint256 tip_hash = tip.GetHash();
        locator.insert(locator.end(), tip_hash.begin(), tip_hash.end());
    } else {
        locator.insert(locator.end(), cp.hash.begin(), cp.hash.end());
    }
    WriteCompactSize(payload, 1);
    payload.insert(payload.end(), locator.begin(), locator.end());
    payload.insert(payload.end(), 32, 0); // hashstop

    return peer.SendMessage("getheaders", payload);
}

int CLightSync::ProcessHeaders(CLightPeer& peer, CLightChainStore& store,
                               const LightCheckpoint& cp, int64_t start_height,
                               int64_t adjusted_time_now, std::string& last_reason) {
    // 丢弃首个 HEADERS 之前的 INV/MERKLEBLOCK（防空窗污染）
    for (;;) {
        LightMessage msg;
        if (!peer.ReadMessage(msg)) {
            last_reason = "read-failed";
            return -1;
        }
        if (msg.command == "headers") {
            // 解析
            const uint8_t* p = msg.payload.data();
            const uint8_t* end = p + msg.payload.size();
            bool ok = true;
            uint64_t count = ReadCompactSize(p, end, ok);
            // 真实节点 headers 消息：每个头后跟 1 字节 tx-count（varint 0）
            if (!ok || count > 2000 || p + count * (LightBlockHeader::kHeaderSize + 1) > end) {
                last_reason = "bad-headers-format";
                return -1;
            }
            int processed = 0;
            for (uint64_t i = 0; i < count; ++i) {
                const uint8_t* hp = p + i * (LightBlockHeader::kHeaderSize + 1);
                LightBlockHeader h;
                if (!h.Deserialize(hp, LightBlockHeader::kHeaderSize)) {
                    last_reason = "bad-header-deserialize";
                    return -1;
                }
                // tx-count 必须为 0（headers 消息不含交易）
                if (hp[LightBlockHeader::kHeaderSize] != 0) {
                    last_reason = "bad-headers-txcount";
                    return -1;
                }
                int64_t height = start_height + static_cast<int64_t>(i);
                bool historical = height <= cp.height;
                LightBlockHeader prev;
                const LightBlockHeader* prev_ptr = nullptr;
                if (height > 0 && store.GetHeaderAtHeight(height - 1, prev)) {
                    prev_ptr = &prev;
                }
                std::string reason;
                int64_t mtp = -1;
                if (prev_ptr != nullptr) {
                    mtp = store.GetMedianTimePast(height - 1);
                }
                if (!ValidateHeader(h, prev_ptr, height, historical, adjusted_time_now, reason, mtp)) {
                    last_reason = reason;
                    return -1;
                }
                // ASERT 难度连续性（新区段，prev 存在且高于锚点时生效）
                if (!historical && prev_ptr != nullptr && height > 0) {
                    LightASERTParams asert;
                    uint32_t expected = GetNextASERTBits(*prev_ptr, height - 1, asert);
                    if (h.nBits != expected) {
                        last_reason = "bad-diffbits";
                        return -1;
                    }
                }
                store.AddHeader(h, height);
                store.AddWork(h.nBits);
                ++processed;
            }
            return processed;
        }
        if (msg.command == "inv" || msg.command == "merkleblock") {
            continue; // 丢弃
        }
        // 其他消息暂忽略
    }
}

} // namespace mvclight
