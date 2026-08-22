#include "light/light_socket.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include <cerrno>
#include <cstring>

namespace mvclight {

namespace {

#ifdef _WIN32
using socket_t = SOCKET;
constexpr socket_t kInvalidSocket = INVALID_SOCKET;
#else
using socket_t = int;
constexpr socket_t kInvalidSocket = -1;
#endif

bool IsSocketError(int rc) {
#ifdef _WIN32
    return rc == SOCKET_ERROR;
#else
    return rc < 0;
#endif
}

int LastSocketError() {
#ifdef _WIN32
    return WSAGetLastError();
#else
    return errno;
#endif
}

void CloseSocket(socket_t s) {
#ifdef _WIN32
    closesocket(s);
#else
    ::close(s);
#endif
}

} // namespace

CLightSocket::CLightSocket() : m_connected(false), m_sock(static_cast<int>(kInvalidSocket)) {}

CLightSocket::~CLightSocket() {
    Close();
}

bool CLightSocket::Connect(const std::string& host, uint16_t port, uint32_t timeout_ms) {
    Close();

#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        return false;
    }
#endif

    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%u", port);

    struct addrinfo* res = nullptr;
    if (getaddrinfo(host.c_str(), port_str, &hints, &res) != 0) {
        return false;
    }

    socket_t sock = kInvalidSocket;
    bool ok = false;
    for (struct addrinfo* ai = res; ai != nullptr; ai = ai->ai_next) {
        sock = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (sock == kInvalidSocket) {
            continue;
        }

#ifdef _WIN32
        u_long mode = 1;
        ioctlsocket(sock, FIONBIO, &mode);
#else
        int flags = fcntl(sock, F_GETFL, 0);
        fcntl(sock, F_SETFL, flags | O_NONBLOCK);
#endif

        int rc = ::connect(sock, ai->ai_addr, static_cast<int>(ai->ai_addrlen));
        if (rc == 0) {
            ok = true;
            break;
        }

        bool pending = false;
#ifdef _WIN32
        pending = (LastSocketError() == WSAEWOULDBLOCK);
#else
        pending = (LastSocketError() == EINPROGRESS);
#endif
        if (!pending) {
            CloseSocket(sock);
            sock = kInvalidSocket;
            continue;
        }

        fd_set wset;
        FD_ZERO(&wset);
        FD_SET(sock, &wset);
        timeval tv{};
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = static_cast<long>((timeout_ms % 1000) * 1000);
        int sel = select(static_cast<int>(sock) + 1, nullptr, &wset, nullptr, &tv);
        if (sel > 0) {
            int err = 0;
            socklen_t err_len = sizeof(err);
            getsockopt(sock, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&err), &err_len);
            if (err == 0) {
                ok = true;
                break;
            }
        }
        CloseSocket(sock);
        sock = kInvalidSocket;
    }
    freeaddrinfo(res);

    if (!ok) {
        return false;
    }

#ifdef _WIN32
    u_long mode = 0;
    ioctlsocket(sock, FIONBIO, &mode);
#else
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags & ~O_NONBLOCK);
#endif

    m_sock = static_cast<int>(sock);
    m_connected = true;
    return true;
}

void CLightSocket::SetRecvTimeout(uint32_t timeout_ms) {
    if (!m_connected) return;
    socket_t sock = static_cast<socket_t>(m_sock);
#ifdef _WIN32
    DWORD tv = static_cast<DWORD>(timeout_ms);
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&tv), sizeof(tv));
#else
    timeval tv{};
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = static_cast<long>((timeout_ms % 1000) * 1000);
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif
}

bool CLightSocket::Attach(int sock) {
    if (sock < 0) return false;
    Close();
    m_sock = sock;
    m_connected = true;
    return true;
}

bool CLightSocket::SendAll(const uint8_t* data, size_t len) {
    if (!m_connected) return false;
    socket_t sock = static_cast<socket_t>(m_sock);
    size_t sent = 0;
    while (sent < len) {
        int rc = ::send(sock, reinterpret_cast<const char*>(data + sent),
                        static_cast<int>(len - sent), 0);
        if (IsSocketError(rc)) {
            Close();
            return false;
        }
        sent += static_cast<size_t>(rc);
    }
    return true;
}

size_t CLightSocket::RecvSome(uint8_t* buf, size_t max_len) {
    if (!m_connected) return 0;
    socket_t sock = static_cast<socket_t>(m_sock);
    int rc = ::recv(sock, reinterpret_cast<char*>(buf), static_cast<int>(max_len), 0);
    if (rc > 0) {
        return static_cast<size_t>(rc);
    }
    if (rc == 0) {
        // 对端关闭
        Close();
        return 0;
    }
    // rc < 0：区分接收超时与真实错误
#ifdef _WIN32
    int err = WSAGetLastError();
    if (err == WSAETIMEDOUT) {
        return 0; // 超时，连接保持
    }
#else
    int err = errno;
    if (err == EAGAIN || err == EWOULDBLOCK) {
        return 0; // 超时，连接保持
    }
#endif
    Close();
    return 0;
}

void CLightSocket::Close() {
    if (m_sock != static_cast<int>(kInvalidSocket)) {
        CloseSocket(static_cast<socket_t>(m_sock));
        m_sock = static_cast<int>(kInvalidSocket);
    }
    m_connected = false;
}

} // namespace mvclight
