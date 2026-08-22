#include "light/light_checkpoints.h"
#include "light/light_chainstore.h"
#include "light/light_header.h"
#include "light/light_message.h"
#include "light/light_peer.h"
#include "light/light_sync.h"
#include "light/light_validation.h"

#include "test_framework.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <thread>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

using mvclight::CLightChainStore;
using mvclight::CLightPeer;
using mvclight::CLightSync;
using mvclight::EncodeMessage;
using mvclight::LightBlockHeader;
using mvclight::LightCheckpoint;
using mvclight::LightMessage;
using mvclight::MainnetMagic;
using mvclight::ReadMessage;
using mvclight::uint256;
using mvclight::uint256S;

namespace {

#ifdef _WIN32
using sock_t = SOCKET;
constexpr sock_t kBadSock = INVALID_SOCKET;
inline void CloseSock(sock_t s) { closesocket(s); }
inline void InitNet() {
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
}
#else
using sock_t = int;
constexpr sock_t kBadSock = -1;
inline void CloseSock(sock_t s) { ::close(s); }
inline void InitNet() {}
#endif

void WriteLE32(std::vector<uint8_t>& out, uint32_t v) {
    for (int i = 0; i < 4; ++i) out.push_back(static_cast<uint8_t>(v >> (i * 8)));
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

static bool HashBeFirstByteLe(const LightBlockHeader& h, uint8_t limit) {
    uint256 hh = h.GetHash();
    uint8_t be[32];
    for (size_t i = 0; i < 32; ++i) be[i] = hh.begin()[31 - i];
    return be[0] <= limit;
}

static void MakeHeader(LightBlockHeader& h, const uint256& prev_hash, uint32_t time) {
    h.nVersion = 4;
    h.hashPrevBlock = prev_hash;
    h.hashMerkleRoot = uint256S(
        "ab" "cd" "ab" "cd" "ab" "cd" "ab" "cd"
        "ab" "cd" "ab" "cd" "ab" "cd" "ab" "cd"
        "ab" "cd" "ab" "cd" "ab" "cd" "ab" "cd"
        "ab" "cd" "ab" "cd" "ab" "cd" "ab" "cd");
    h.nTime = time;
    h.nBits = 0x207fffff;
    h.nNonce = 0;
    while (!HashBeFirstByteLe(h, 0x7f)) {
        ++h.nNonce;
    }
}

void MockHeaderServer(bool send_bad, int count, std::atomic<int>& out_port,
                      std::atomic<bool>& ready) {
    InitNet();
    sock_t listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd == kBadSock) return;
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;
    bind(listen_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    socklen_t len = sizeof(addr);
    getsockname(listen_fd, reinterpret_cast<sockaddr*>(&addr), &len);
    out_port = ntohs(addr.sin_port);
    ready = true;
    listen(listen_fd, 1);
    sock_t client_fd = accept(listen_fd, nullptr, nullptr);
    if (client_fd == kBadSock) {
        CloseSock(listen_fd);
        return;
    }
    mvclight::CLightSocket s;
    s.Attach(static_cast<int>(client_fd));

    // 握手
    LightMessage msg;
    if (!ReadMessage(s, MainnetMagic(), msg) || msg.command != "version") return;
    std::vector<uint8_t> vp(12, 0);
    vp[4] = 5;
    std::vector<uint8_t> vmsg;
    EncodeMessage(MainnetMagic(), "version", vp, vmsg);
    s.SendAll(vmsg.data(), vmsg.size());
    std::vector<uint8_t> vac;
    EncodeMessage(MainnetMagic(), "verack", {}, vac);
    s.SendAll(vac.data(), vac.size());
    if (!ReadMessage(s, MainnetMagic(), msg) || msg.command != "verack") return;

    // 读 getheaders
    if (!ReadMessage(s, MainnetMagic(), msg) || msg.command != "getheaders") return;

    // 构造 headers
    std::vector<uint8_t> payload;
    WriteCompactSize(payload, count);
    uint256 prev = uint256S("00");
    uint32_t t = 1000000000;
    for (int i = 0; i < count; ++i) {
        LightBlockHeader h;
        MakeHeader(h, prev, t);
        if (send_bad && i == 1) {
            h.hashPrevBlock = uint256S("ff"); // 破坏 prev 连续性
        }
        std::vector<uint8_t> raw = h.Serialize();
        payload.insert(payload.end(), raw.begin(), raw.end());
        payload.push_back(0); // tx-count（headers 消息每头后必须）
        prev = h.GetHash();
        t += 10;
    }
    std::vector<uint8_t> hmsg;
    EncodeMessage(MainnetMagic(), "headers", payload, hmsg);
    s.SendAll(hmsg.data(), hmsg.size());

    std::this_thread::sleep_for(std::chrono::milliseconds(300));
}

int StartHeaderMock(bool send_bad, int count, std::atomic<int>& port, std::atomic<bool>& ready) {
    std::thread th(MockHeaderServer, send_bad, count, std::ref(port), std::ref(ready));
    th.detach();
    for (int i = 0; i < 100 && !ready.load(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return ready.load() ? port.load() : 0;
}

} // namespace

int main() {
    const int64_t now = 1000000100;

    // 用例 1：合法 header 链同步
    {
        std::atomic<int> port{0};
        std::atomic<bool> ready{false};
        int p = StartHeaderMock(false, 3, port, ready);
        CHECK(p != 0);
        CLightPeer peer;
        CHECK(peer.ConnectAndHandshake("127.0.0.1", static_cast<uint16_t>(p), 2000, 2000));
        CHECK(peer.SetState(mvclight::PeerState::WAIT_FILTER_ACK));
        CHECK(peer.SetState(mvclight::PeerState::SYNCING_HEADERS));

        CLightChainStore store;
        LightCheckpoint cp;
        cp.height = 0;
        cp.hash = uint256S("00"); // 首个 header 的 prev 作为锚点（占位）
        cp.nChainWork = uint256S("0");

        CHECK(CLightSync::SendGetHeaders(peer, cp, store));
        std::string reason;
        int n = CLightSync::ProcessHeaders(peer, store, cp, 0, now, reason);
        CHECK(n == 3);
        CHECK(store.TipHeight() == 2);
        // Checkpoint 锚定：cp.hash 应等于 height 0 的 header 哈希
        LightBlockHeader h0;
        CHECK(store.GetHeaderAtHeight(0, h0));
        cp.hash = h0.GetHash();
        CHECK(mvclight::CheckCheckpoint(store, cp));
        std::printf("CHECKPOINT_ANCHORED\n");
        std::printf("HEADERS_SYNCED\n");
    }

    // 用例 2：非法 header 被拒绝
    {
        std::atomic<int> port{0};
        std::atomic<bool> ready{false};
        int p = StartHeaderMock(true, 3, port, ready);
        CHECK(p != 0);
        CLightPeer peer;
        CHECK(peer.ConnectAndHandshake("127.0.0.1", static_cast<uint16_t>(p), 2000, 2000));
        CHECK(peer.SetState(mvclight::PeerState::WAIT_FILTER_ACK));
        CHECK(peer.SetState(mvclight::PeerState::SYNCING_HEADERS));

        CLightChainStore store;
        LightCheckpoint cp;
        cp.height = 0;
        cp.hash = uint256S("00");
        cp.nChainWork = uint256S("0");

        CHECK(CLightSync::SendGetHeaders(peer, cp, store));
        std::string reason;
        int n = CLightSync::ProcessHeaders(peer, store, cp, 0, now, reason);
        CHECK(n == -1);
        CHECK(reason == "bad-prevblk");
        std::printf("BAD_HEADER_REJECTED\n");
    }

    TEST_MAIN_RETURN();
}
