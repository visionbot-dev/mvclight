# MVC 全节点 P2P 拉黑/限流机制研究笔记

> 研究来源：`D:\Project\Sample\microvisionchain\src`（只读）
> 结论用于 mvclight 轻节点“防拉黑”适配；黑名单源码绝不链接/复制进 SDK。

## 关键常量（已核实）

| 常量 | 值 | 上游位置 | 说明 |
|------|-----|---------|------|
| `DEFAULT_BANSCORE_THRESHOLD` | 100 | `validation.h:184` | misbehavior 得分达到 100 即封禁 |
| `MAX_HEADERS_RESULTS` | 2000 | `validation.h:123` | 单次 getheaders 响应最大头数 |
| `PING_INTERVAL` | 120s | `net/net.h:78` | 节点主动 ping 间隔 |
| `MAX_BLOOM_FILTER_SIZE` | 36000 bytes | `bloom.h:18` | 上游允许的最大过滤器字节数 |
| `MAX_HASH_FUNCS` | 50 | `bloom.h:19` | 上游允许的最大哈希函数数 |
| 协议版本 | 70016 | `protocol.h` | 当前 MVC P2P 版本 |

> mvclight 采用更保守的 BIP-37 限制：20000 元素 / 32768 字节（`light_filter.h`、`light_peer_policy.h`）。

## Misbehaving 触发点（示例）

| 触发 | 得分 | 上游位置 |
|------|------|---------|
| 多次发送 version | 1 | `net/net_processing.cpp:1609` |
| invalid-UA | banscore(默认100) | `net/net_processing.cpp:1678` |
| 超限 addr 消息 | 20 | `net/net_processing.cpp:1911` |
| out-of-bound-tx-index | 100 | `net/net_processing.cpp:1403` |

说明：MVC 上游新版已把 net 拆到 `src/net/`（`net_processing.cpp` 在 `src/net/` 下），并引入 `stream_policy`/`protoconf` 等更细的限流；轻节点只做“不触发常见惩罚”即可。

## 轻节点防拉黑适配清单

| 类别 | 上游机制 | 轻节点适配 | 状态 |
|------|---------|-----------|------|
| 握手 | version/verack 校验、UA 校验 | 发送合法 version（协议 70016、services、relay）；UA 固定合法字符串 | 已有，需核对 |
| 心跳 | 节点 ping 120s | 主动 ping + **响应对端 ping（pong）** | 已适配（Demo 稳态已回 pong） |
| 消息格式 | 畸形消息断开/惩罚 | 严格校验 magic/checksum/长度；未知消息忽略 | 已有 |
| Header 同步 | 单批 2000、响应节奏 | 单批 ≤2000；收到空响应停止；批间间隔 50ms；断点续传不重头拉 | 已适配 |
| INV/GETDATA | 请求频率限制 | 去重 + 30s 重试 + 3 次上限（`CPendingTxMap`）；禁止高频重复 getdata | 已有/持续 |
| Bloom | 36000B / 50 函数 | 预检 20000 元素/32768 字节；重建间隔 ≥5s；超限拒绝 | 已适配 |
| 惩罚分 | 100 分封禁 | 避免触发上述 Misbehaving 点 | 已评估 |
| 重连 | 每 IP 连接频率 | 指数退避 `5s×2^n` 封顶 300s + 抖动 + 10 次上限 | 已有 |

## 待进一步研究（如需要）

- `src/net/stream_policy.cpp` / `stream_policy_factory.cpp`：新版本流策略/并发限流
- `protoconf` 消息（`maxRecvPayloadLength` 协商）
- `block_download_tracker`：区块下载窗口
- `association.cpp:394`：`BanReasonNodeMisbehaving` 关联层封禁

## 结论

轻节点只要做到：合法握手、响应 ping、严格消息校验、Header 同步分批限速、Bloom 预检、请求去重退避、重连指数退避，就不会触发全节点常见黑名单路径。
