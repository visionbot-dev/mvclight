#include "light/light_message.h"
#include "light/mvc_light.h"

#include "test_framework.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <thread>

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

void MockPeer(std::atomic<int>& out_port, std::atomic<bool>& ready, std::atomic<bool>& got_tx) {
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

    mvclight::LightMessage msg;
    if (!mvclight::ReadMessage(s, mvclight::MainnetMagic(), msg) || msg.command != "version") return;
    std::vector<uint8_t> vp(12, 0);
    vp[4] = 5;
    std::vector<uint8_t> vmsg;
    mvclight::EncodeMessage(mvclight::MainnetMagic(), "version", vp, vmsg);
    s.SendAll(vmsg.data(), vmsg.size());
    std::vector<uint8_t> vac;
    mvclight::EncodeMessage(mvclight::MainnetMagic(), "verack", {}, vac);
    s.SendAll(vac.data(), vac.size());
    if (!mvclight::ReadMessage(s, mvclight::MainnetMagic(), msg) || msg.command != "verack") return;

    // 等待 tx 消息（send_raw_tx）
    if (mvclight::ReadMessage(s, mvclight::MainnetMagic(), msg) && msg.command == "tx") {
        got_tx = true;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
}

int StartMock(std::atomic<int>& port, std::atomic<bool>& ready, std::atomic<bool>& got_tx) {
    std::thread th(MockPeer, std::ref(port), std::ref(ready), std::ref(got_tx));
    th.detach();
    for (int i = 0; i < 100 && !ready.load(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return ready.load() ? port.load() : 0;
}

int g_peer_events = 0;

void OnPeerState(void*, const char*, int, const char*) { ++g_peer_events; }

} // namespace

int main() {
    std::atomic<int> port{0};
    std::atomic<bool> ready{false};
    std::atomic<bool> got_tx{false};
    int p = StartMock(port, ready, got_tx);
    CHECK(p != 0);

    mvc_light_config cfg{};
    cfg.network = "main";
    cfg.peer = "127.0.0.1";
    char peer_buf[64];
    snprintf(peer_buf, sizeof(peer_buf), "127.0.0.1:%d", p);
    cfg.peer = peer_buf;
    cfg.store_path = "store";
    cfg.on_peer_state = OnPeerState;

    mvc_light_ctx* ctx = mvc_light_init(&cfg);
    CHECK(ctx != nullptr);
    CHECK(mvc_light_start(ctx) == MVC_LIGHT_OK);
    CHECK(mvc_light_is_running(ctx) == 1);
    CHECK(g_peer_events > 0);

    // watch 操作
    CHECK(mvc_light_watch_add(ctx, "addr1") == MVC_LIGHT_OK);
    CHECK(mvc_light_watch_add(ctx, "addr2") == MVC_LIGHT_OK);
    char* list = mvc_light_watch_list(ctx);
    CHECK(list != nullptr);
    CHECK(strstr(list, "addr1") != nullptr);
    mvc_light_free_string(list);

    // send_raw_tx：16 字节 hex（8 字节 payload）
    char* txid = mvc_light_send_raw_tx(ctx, "01000000020000000000000000");
    CHECK(txid != nullptr);
    CHECK(strlen(txid) == 64);
    mvc_light_free_string(txid);
    for (int i = 0; i < 50 && !got_tx.load(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    CHECK(got_tx.load());

    // 查询
    char* status = mvc_light_sync_status(ctx);
    CHECK(status != nullptr);
    CHECK(strstr(status, "\"running\":true") != nullptr);
    mvc_light_free_string(status);

    char* wget = mvc_light_watch_get(ctx, "addr1", 0, -1);
    CHECK(wget != nullptr);
    mvc_light_free_string(wget);

    // 切换（mock 已关闭 -> 失败路径）
    CHECK(mvc_light_switch_peer(ctx, "127.0.0.1:1") == MVC_LIGHT_ERR_PEER_DISCONNECTED);

    CHECK(mvc_light_force_reset_chain(ctx) == MVC_LIGHT_OK);
    mvc_light_stop(ctx);
    CHECK(mvc_light_is_running(ctx) == 0);
    mvc_light_destroy(ctx);

    std::printf("API_ALL_GREEN\n");
    TEST_MAIN_RETURN();
}
