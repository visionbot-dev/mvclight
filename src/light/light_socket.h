#ifndef MVC_LIGHT_LIGHT_SOCKET_H
#define MVC_LIGHT_LIGHT_SOCKET_H

/*
 * 轻量 TCP Socket 封装（Phase 1 回退方案，不链接上游 net.cpp）。
 * 支持 connect 超时、阻塞式 send/recv 与断线检测。
 * 后续阶段可在其上层增加非阻塞事件循环。
 */

#include <cstddef>
#include <cstdint>
#include <string>

namespace mvclight {

class CLightSocket {
public:
    CLightSocket();
    ~CLightSocket();

    CLightSocket(const CLightSocket&) = delete;
    CLightSocket& operator=(const CLightSocket&) = delete;

    // 解析 host:port 并连接；timeout_ms 为连接超时
    bool Connect(const std::string& host, uint16_t port, uint32_t timeout_ms);

    // 包装已接受的 socket 句柄（服务端测试用）
    bool Attach(int sock);

    // 设置接收超时（毫秒）；0 表示阻塞
    void SetRecvTimeout(uint32_t timeout_ms);

    // 发送全部数据；返回 false 表示连接已断开
    bool SendAll(const uint8_t* data, size_t len);

    // 接收最多 max_len 字节；返回实际接收字节数；0 表示对端关闭/错误
    size_t RecvSome(uint8_t* buf, size_t max_len);

    void Close();
    bool IsConnected() const { return m_connected; }

private:
    bool m_connected;
    int m_sock; // SOCKET 在 Windows 上为 uintptr_t，这里用 int 存底层句柄（见 cpp 转换）
};

} // namespace mvclight

#endif // MVC_LIGHT_LIGHT_SOCKET_H
