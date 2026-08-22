# Agent 提示词：P2P 机制研究与轻节点防拉黑适配

> 使用方法：将下文从“开始”到“结束”整段复制给 agent，或在 Deep Code 中直接粘贴执行。

---

## 开始

请作为资深 P2P 网络协议工程师，为 `D:\Project\Sample\mvclight` 轻节点 SDK 完成两项工作：

1. **彻底研究 MVC 全节点（Bitcoin Cash 系）的 P2P 连接/黑名单/限流机制**
2. **把这些机制适配进 mvclight SDK 的 `src/light/` 自研模块**，让轻节点成为“好邻居”，避免频繁请求被全节点拉黑或断开

## 工作环境

- 仓库根目录：`D:\Project\Sample\mvclight`（轻节点 SDK，白名单构建，已实现 Phase 0~5 + Windows Demo）
- 上游全节点源码（**只读研究**）：`D:\Project\Sample\microvisionchain`
- 工具链：Windows + VS2022 + MSVC + CMake + Ninja
- 当前测试：19 个全绿；Demo 可直连真实主网种子

## 第一步：阅读核验（只读研究，绝不链接/复制黑名单代码）

先通读以下上游文件，**重点提取所有与“被拉黑/被断开”相关的触发条件、阈值、超时、计数、限流常量**：

1. `src/net.cpp` / `src/net.h`
   - 连接超时、接收/发送超时
   - 单 IP 连接数限制、最大连接数
   - 消息队列/缓冲上限
2. `src/banman.cpp` / `src/banman.h`
   - 禁止得分（misbehavior score）、封禁阈值、封禁时长
   - 封禁原因、封禁名单结构
3. `src/net_processing.cpp`
   - `Misbehaving()` 调用点与分数（如 bad-prevblk、bad-txns、过滤规则违规等）
   - INV/GETDATA 请求频率限制、每批数量上限
   - HEADERS 同步节奏、GETHEADERS 响应限制
   - FILTERLOAD/BIP-37 限制（元素数、字节数、更新策略；节点对超限的断开/报错）
   - PING/PONG 心跳与超时断开
   - 未知消息、畸形消息的处理
   - 版本/握手校验（services、relay、protocol version）
4. `src/protocol.h`
   - 协议版本号、消息类型、`NODE_*` 服务位、`MAX_*` 常量
5. `src/netbase.h/cpp`（如存在）
   - 地址解析、代理、端口
6. `src/chainparams.cpp`
   - 主网/测试网种子节点、端口

**硬性约束**：

- 以上黑名单文件（尤其 `net_processing.cpp`、`net.cpp`、`banman.cpp`）**只能读、不能链接、不能直接复制进白名单**
- 提取出的策略/常量必须写成**自研轻量实现**，放在 `src/light/` 下（如 `light_peer_policy.h/.cpp`、`light_sync.cpp` 内调整）
- 不引入 Boost/libevent；只用现有自研 socket 与标准库
- 不实现完整全节点 P2P 功能，只做“轻节点不被拉黑”所必需的合规策略

## 第二步：输出《P2P 策略研究笔记》

新建 `doc/p2p-policy-notes.md`，用表格整理：

| 类别 | 上游机制 | 关键常量/阈值 | 对轻节点的影响 | 适配方案 |
|------|---------|--------------|---------------|---------|
| 握手 | version/verack 校验 | 协议版本、services、relay | 错误握手直接断开 | 发送正确字段 |
| 心跳 | ping/pong | 间隔/超时 | 超时断开 | 维持心跳 |
| 消息格式 | magic/checksum/长度 | 最大消息大小 | 畸形消息断开 | 严格校验 |
| Header 同步 | getheaders 频率与批量 | 单批最大 2000、响应节奏 | 高频请求拉黑 | 批次限速/退避 |
| INV/GETDATA | 请求频率 | 每连接窗口限制 | 重复请求拉黑 | 去重+退避+上限 |
| Bloom 过滤 | BIP-37 限制 | 20000 元素/32768 字节、更新标志 | 超限断开 | 预检+重建策略 |
| 惩罚分 | Misbehaving | 阈值/封禁时长 | 累积拉黑 | 避免触发点 |
| 重连 | 连接频率 | 每 IP 连接限制 | 快速重连拉黑 | 指数退避+上限 |

笔记必须给出**具体数值出处**（上游文件+行号或常量名），并标注哪些已适配、哪些待适配。

## 第三步：SDK 适配实现

### 必须实现/调整项（按优先级）

1. **握手合规**
   - 检查 `light_peer.cpp` 的 version payload：protocol version、services（NODE_NETWORK|NODE_BLOOM）、relay 标志、user agent 是否合理
   - 对端 version 校验失败应优雅断开，不重试轰炸

2. **心跳机制**
   - 已有 `SendPing/CheckPingTimeout`，确认：主动 ping 间隔、pong 超时、连续超时断开
   - **必须响应对端 ping**（发送 pong），否则节点可能断开/拉黑

3. **Header 同步限速**
   - 单批 `getheaders` 后必须等收到响应再发下一批，禁止并发/连发
   - 每批之间增加可配置间隔（如 50~200ms），避免突发
   - 收到 `count == 0` 或 `count < 2000` 后停止，不重复从同一位置请求
   - 断点续传时 locator 用上次 tip，禁止每次从 genesis 重拉

4. **INV/TX 请求去重与退避**
   - 对 `inv` 消息：若已知/已请求则忽略；需要 getdata 时按 `CPendingTxMap` 已有重试逻辑（30s 超时、3 次上限）执行
   - 禁止对同一 txid 连续高频 getdata
   - `send_raw_tx` 广播失败重试也应有退避（沿用 `NextRetryDelayMs` 思路）

5. **Bloom 过滤器合规**
   - 发送 `FILTERLOAD` 前预检：元素数 ≤ 20000、字节数 ≤ 32768、flags 合法
   - 超限拒绝添加并返回 `ERR_FILTER_FULL`（现有 `mvc_light_watch_add` 已有）
   - 过滤器更新频率限制：同一连接不要频繁整体重建（如 5 秒内最多 1 次），否则用 `filteradd` 增量

6. **消息读取与超时**
   - `CLightSocket::RecvSome` 超时必须保持连接（已修复），但读取循环不能忙等
   - 连续超时不应被误判为断线；只有 `IsConnected()==false` 才算断开
   - 未知消息静默忽略；畸形消息记录日志并断开（但不要疯狂重连）

7. **重连策略**
   - 已有指数退避 `min(5s×2^n, 300s)` + 抖动 + 10 次上限
   - 确认失败原因不立即重连；被断开后至少等待退避时间
   - 同一节点连续失败达到上限后停止自动重连，等待用户手动操作

8. **新增策略模块（建议）**
   - `src/light/light_peer_policy.h/.cpp`：集中存放常量（如 `kMaxGetHeadersPerMinute`、`kMinFilterReloadMs`、`kMaxInvRequestsPerWindow` 等）与检查函数
   - 在 `light_sync.cpp` / `light_api.cpp` 调用这些策略检查
   - 保持白名单构建：新文件加入 `src/CMakeLists.txt` 的 `MVCLIGHT_WHITELIST_SOURCES`

### 测试要求

- 新增单元测试（如 `test_light_peer_policy.cpp`）覆盖：
  - getheaders 限速（同一窗口超过阈值拒绝/等待）
  - filterload 预检（超限拒绝、重建间隔）
  - inv 去重与 getdata 上限
  - 重连退避序列
- 更新 mock 集成测试，验证：
  - 同步循环不会在收到 `count==0` 后继续发 getheaders
  - 对端 ping 会收到 pong
  - 长时间无消息不触发断开（保持连接）
- 现有 19 个测试必须保持全绿

## 第四步：验收 DoD（全部满足才算完成）

- [ ] `doc/p2p-policy-notes.md` 完成，包含上游数值出处
- [ ] `src/light/light_peer_policy.h/.cpp`（或等价模块）加入白名单并构建通过
- [ ] `cmake --build build --config Debug` 0 error
- [ ] `ctest -C Debug` 全绿（原有 19 + 新增测试）
- [ ] `check_blacklist.py` PASS；`check_symbols.sh` PASS
- [ ] Demo 稳态运行 ≥ 5 分钟不被节点断开（日志无 `disconnected`）
- [ ] Demo 日志显示：getheaders 批次间有间隔、收到空 headers 后停止、对端 ping 有 pong 响应
- [ ] git commit + push，commit message 如 `feat: p2p ban-avoidance policy`

## 第五步：沟通要求

- 先输出研究笔记，再动代码；每步汇报
- 遇到上游机制不明确时，先阅读源码确认，不要猜测；无法确认就记录为“待验证”
- 禁止把黑名单文件源码复制进 `src/light/`；只允许参考后自研实现
- 不改变现有 C ABI 签名，除非确有必要并说明
- 完成后按交付物清单确认

## 交付物清单

1. `doc/p2p-policy-notes.md`
2. `src/light/light_peer_policy.h/.cpp`（策略常量与检查函数）
3. `src/light/light_sync.cpp`、`light_peer.cpp`、`light_api.cpp` 的适配改动
4. 新增/更新测试
5. README 或笔记中说明“防拉黑策略”
6. git commit + push

---

## 结束
