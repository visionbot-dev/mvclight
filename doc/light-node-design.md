# MVC SPV 轻节点动态库(SDK)工程设计文档

> 版本:v1.1(可编码实施版,含工程修正)
> 基线:microvisionchain v0.2.1.0(Bitcoin Cash 系分叉)
> 形态:动态库(SDK),嵌入业务层 App,直连指定主网全节点(单 peer),业务层可切换节点
> 安全基线:内置 Checkpoint 锚定 + 过滤器容量上限 + 深重组拒绝 + 乱序交易兜底 + 完整错误码/日志体系

---

## 修订记录(Change Log)

| 版本 | 修订章节 | 修订原因 | 核心修改点 |
|------|---------|---------|-----------|
| v0.1 | 全篇 | 初始设计 | SPV 方案初稿 |
| v0.2 | §1-§10 | 需求变更 | 多 peer→单 peer+切换;无 RPC→C ABI;动态库形态 |
| v0.3 | §1-§10 | 需求变更 | 单 peer 直连 + 业务层切换;应用一致性修正(B1-C4) |
| v0.4 | §1.2/1.4/3/4/5/7/8/9/10 | 9 条强制优化 | 内置 Checkpoint、过滤器上限、PendingTxMap 乱序、深重组拒绝、双表存储、store_proof、回调防死锁、Facade 构建、ForkId 脚本 |
| **v1.0** | **§4.1/4.5/4.6/4.8/5/8/10 + 新增附录 A-E** | 补全工程细节至可编码 | P2P 状态机/心跳/reject/重连;存储 DDL+LevelDB 编码+原子性+磁盘容量;完整错误码/日志/生命周期查询;共识验证边界(header/交易/脚本/难度);CMake 目标+依赖锁定+符号可见性+NDK/iOS 交叉编译;三层测试+异常场景+升级策略 |
| **v1.1** | §3.1/3.2/3.3/3.4、§4.1.1、§4.1.3、§4.3、§4.5.2、§4.6、§5.1、§7、附录 A、附录 B.1、附录 D.2、附录源码索引 | v1.0 工程审核(6 条强制修复) | ①废弃 txmempool,新增 light_txpool(§4.6/附录 D);②状态机新增 WAIT_FILTER_ACK 防空窗丢消息(§4.1.1);③on_send_result 语义修正 + 120 分钟链上确认超时兜底(§4.1.3);④SQLite txid/block_hash 统一 BLOB 32B(附录 B.1);⑤内置 Checkpoint 增加 nChainWork,历史段跳过难度重算,CPU 降 95%(§4.3/§7);⑥交易大小上限明确 maxTxSize(主网 32MB)/区块 maxBlockSize(4GB),禁用 BTC 100KB(§4.5.2) |
| **v1.2** | §1.3/1.4/3.1/3.2/3.3/3.4、§9、附录 D.2、新增 §11 | 用户决策:直接集成全节点网络层 | ①网络行为与全节点完全一致(上游 CConnman/PeerLogicValidation/addrman/banman);②过滤改为本地 scriptPubKey 过滤(替代 BIP-37 bloom);③交易同步直接复用上游 mempool/区块下载;④交易入库 LevelDB 后由上游 `-prune` 删除区块文件;⑤黑名单边界改为仅禁 rpc/wallet/mining/zmq/qt/http/init;⑥自研 P2P(light_peer/light_sync/light_connman)降级为 legacy |

---

## 1. 背景与目标

### 1.1 背景

业务需要在用户终端(手机 App / 桌面客户端)上直接通过 P2P 网络从 MVC **主网全节点**同步与"关注地址"相关的交易,而非依赖中心化索引服务。基于全节点源码 `microvisionchain` 裁剪为**动态库 SDK**,嵌入业务层应用。

### 1.2 目标(功能边界)

| 功能 | 要求 |
|------|------|
| P2P 交易同步 | 直连**指定的主网全节点**,同步与关注地址相关的交易(含区块头与默克尔证明) |
| 交易广播 | 将新交易广播给当前连接的全节点,并**接收 reject 反馈** |
| 关注交易存储 | 仅存储并索引与预设关注地址相关的交易(txid 去重双表) |
| 交易完整性 | 默克尔证明 + 区块头链验证,保证交易未被篡改、所在区块已确认 |
| 指定高度同步 | 支持从用户指定区块高度开始;**强制以内置安全 Checkpoint 为前置锚点** |
| 节点切换 | 业务层可随时切换全节点(断旧连新、增量续同步、深重组拒绝) |
| 嵌入形态 | **动态库**(so/dll/dylib),稳定 C ABI,无 RPC 服务 |
| 轻量 | 流量/存储/内存最小化,可在手机端运行 |

### 1.3 非目标

- ~~完整区块同步与全链验证~~ → **v1.2 起下载完整区块做本地过滤，验证后由 `-prune` 删除区块文件；不保留全量历史区块**
- 挖矿、出块;内置钱包托管;对外 P2P 区块服务
- **RPC 服务**(HTTP/JSON-RPC 全部移除,改为库内 API + 回调)
- ~~多 peer 并发连接 / 节点自动发现(addrman、DNS 种子)~~ → **v1.2 起作为全节点网络层能力引入(见 §11)**

### 1.4 设计决策(结论先行)

| 决策点 | 结论（v1.2 起） |
|--------|------|
| 网络层 | **直接集成上游全节点网络层**(`CConnman` + `PeerLogicValidation` + `Addrman` + `Banman`),网络行为与全节点完全一致(见 §11) |
| 过滤 | **本地 scriptPubKey 过滤**(替代 BIP-37 bloom);复用上游 `base58.h`/`CBitcoinAddress` 解析地址 |
| 节点切换 | 上游多连接自动管理;`switch_peer` 语义保留(业务层可指定优先节点);深重组拒绝保留 |
| 信任锚定 | SDK 内置硬编码安全 Checkpoint(约 6 个月前),强制前置验证 |
| 同步范围 | 区块头链 + 区块体下载(**本地过滤后由 `-prune` 删除区块文件**) |
| 验证模型 | 上游全节点验证(header/交易/共识)+ 本地 scriptPubKey 过滤 |
| 存储 | 双表(tx_store + addr_tx_index)+ watch_addr_store(LevelDB/SQLite);区块文件由上游 prune 管理 |
| 交互形态 | 动态库 + C ABI + 事件回调;回调内严禁调用 SDK API |
| 配置 | 初始化参数(struct)+ 上游 ArgsManager 内部配置;不使用 conf 文件 |
| 黑名单边界 | 仅禁 `rpc/ wallet/ mining/ zmq/ qt/ http/ init.cpp`;net/validation/txmempool 允许导入 |

---

## 2. 总体架构

```
┌────────────────────────── 业务层 App(Android / iOS / Desktop) ──────────────────────────┐
│                                                                                          │
│  业务代码 ◄── C ABI 回调(on_watch_tx / on_sync_progress / on_peer_state / on_send_result / on_log)
│     │     ▲ 回调内禁止调用 SDK API(死锁风险,见 §5.2)
│     │     └─ Post 到业务线程后再调用 SDK
│     ▼ 调用 C ABI API(init / start / watch_add / switch_peer / send_raw_tx / ...)
│  ┌────────────────────────── libmvclight.{so,dll,dylib} ──────────────────────────────┐  │
│  │  SDK 入口层:API + 回调分发 + 线程管理(网络/同步/存储线程)                           │  │
│  │  ┌──────────────────────────────────────────────────────────────────────────────┐  │
│  │  │  连接管理:P2P 状态机(INIT→HANDSHAKE→FILTER_SENT→WAIT_FILTER_ACK→SYNCING→STEADY)│  │
│  │  │            心跳 ping/pong + reject 监听 + 重连退避 + 深重组拒绝               │  │
│  │  └──────────────────────────────────────────────────────────────────────────────┘  │
│  │  ┌──────────┐ ┌──────────┐ ┌────────────────┐ ┌──────────────┐ ┌───────────────┐  │  │
│  │  │ bloom    │ │ merkle   │ │ header 链验证   │ │ light_txpool │ │ CWatchTxStore │  │  │
│  │  │ 过滤器   │ │ 证明校验 │ │ 内置Checkpoint │ │ 广播队列      │ │ 双表+原子事务  │  │  │
│  │  │(容量上限)│ │(乱序兜底)│ │ +难度验证      │ │ +reject兜底  │ │ +磁盘容量检查  │  │  │
│  │  └──────────┘ └──────────┘ └────────────────┘ └──────────────┘ └───────┬───────┘  │  │
│  └───────────────────────────────────────────────────────────────────────────┼──────────┘  │
└──────────────────────────────────────────────────────────────────────────────┼─────────────┘
                                                                               ▼
                                                        SQLite(移动) / LevelDB(桌面)
```

---

## 3. 核心模块裁剪清单

### 3.1 保留模块

| 模块 | 文件 | 说明（v1.2 起） |
|------|------|------|
| P2P 通信/连接管理 | `net/net.cpp`、`net/net.h`、`net/netbase.*`、`net/netaddress.*`、`net/net_message.*`、`net/stream.*`、`net/association.*`、`net/stream_policy*`、`net/block_download_tracker.*`、`net/node_state.*`、`net/validation_scheduler.*` | **完整上游多连接网络层**，与全节点一致 |
| 消息处理 | `net/net_processing.cpp` | 完整消息处理（HEADERS/INV/GETDATA/BLOCK/TX/PING/PONG 等） |
| 地址管理 | `addrman.cpp/h`、`chainparamsseeds.h` | 节点发现/持久化（原黑名单，v1.2 起导入） |
| 封禁管理 | `banman.*`（如存在）/ `net` 内惩罚逻辑 | 与全节点一致 |
| 区块验证 | `validation.cpp/h`、`validationinterface.*` | 完整区块/交易验证（原黑名单，v1.2 起导入） |
| 交易池 | `txmempool.cpp/h`、`txn_validation_*` | mempool 交易同步（原黑名单，v1.2 起导入） |
| 共识参数 | `consensus/params.h`、`consensus/consensus.h`、`chainparams.cpp/h` | mainnet 参数 |
| 本地过滤(新增) | `light/light_local_filter.h/.cpp` | watch 地址→scriptPubKey；订阅 `BlockConnected`/`TransactionAddedToMempool` 本地过滤 |
| 存储 | `dbwrapper.cpp/h`、`leveldb/`(桌面) | 双表 tx_store + addr_tx_index；区块文件由上游 `-prune` 管理 |
| 哈希/加密/基础 | `crypto/`、`hash.cpp/h`、`util/`、`support/`、`logging`、`random`、`fs`、`sync`、`streams.h`、`serialize.h`、`version.h` | 全局依赖 |
| 轻量广播池(legacy) | `light/light_txpool.h/.cpp` | 保留为广播兜底/旧接口兼容，v1.2 后主路径走上游 mempool |

### 3.2 移除/简化模块

v1.2 起只移除：`rpc/` 全部、`wallet/`、`mining/`、`zmq/`、`qt/`、`http/`、`init.cpp`（不启动完整节点进程）。`validation.cpp`、`txmempool.cpp`、`addrman`、多连接管理由“移除”改为“导入”。旧自研 `light_peer/light_sync/light_connman` 标记 legacy，逐步下线。

### 3.3 依赖关系

```
mvc_core(上游网络内核) ── CConnman/PeerLogicValidation/Addrman/Banman
                     ├─ validation ── txmempool ── chainparams/consensus
                     ├─ net/stream_policy(限流)
                     └─ Boost / libevent / LevelDB / secp256k1 / univalue

src/light ── light_local_filter(订阅 CValidationInterface)
           ── CWatchTxStore(LevelDB/SQLite 双表)
           ── light_api(C ABI facade)
```

### 3.4 构建策略:上游内核静态库 + Facade

- 新增 `third_party/microvisionchain/`(vendored 只读副本)与 `mvc_core` 静态库；
- `mvc_core` 编译：`src/net/*`、`src/validation.*`、`src/txmempool.*`、`src/addrman.*`、`src/chainparams.*`、`src/protocol.*`、`src/consensus/*`、`src/crypto/*`、`src/util/*`、`src/dbwrapper.*` 及依赖；
- `src/light/` 只编译 facade、本地过滤、存储、C ABI；
- **排除**：`rpc/ wallet/ mining/ zmq/ qt/ http/ init.cpp`；
- 符号可见性：`mvc_core` 内部符号隐藏，`mvclight` SHARED 仅导出 `mvc_light_*`(附录 D)。

---

## 4. 关键技术路径

### 4.1 连接管理:P2P 状态机、心跳、reject、重连、乱序

#### 4.1.1 P2P 消息交互状态机

```
        INIT ──(配置校验 OK)──► HANDSHAKE ──(收到 verack)──► FILTER_SENT
         │                          │                              │
         │(失败/超时→重连/报错)      │(version 协商失败→断开)        │ FILTERLOAD 已写入 Socket
         ▼                          ▼                              ▼
      (重连退避)                (断开)                        WAIT_FILTER_ACK
                                                                    │
                                                    ① 已发 getheaders
                                                    ② 收到第一个 HEADERS 回复
                                                                    ▼
                                                              SYNCING_HEADERS
                                                                    │
                                                        (headers 追平 tip)
                                                                    ▼
                                                              SYNCING_TXS
                                                                    │
                                                    (PendingTxMap 清空 + 首次过滤完成)
                                                                    ▼
                                                              STEADY
```

| 状态 | 允许接收 | 允许发送 | 进入条件 | 离开条件 |
|------|---------|---------|---------|---------|
| **INIT** | 无 | — | SDK start() | 配置校验通过 |
| **HANDSHAKE** | VERSION, VERACK | VERSION(已发), VERACK | 连接建立 | 收到 verack |
| **FILTER_SENT** | PING, PONG | FILTERLOAD, FILTERADD | 收到 verack | **FILTERLOAD 已写入 Socket 发送缓冲区** |
| **WAIT_FILTER_ACK(防丢消息过渡态)** | HEADERS, PING, PONG | GETHEADERS | FILTERLOAD 已写 Socket + 已发 getheaders | **收到第一个 HEADERS 回复** |
| **SYNCING_HEADERS** | HEADERS, PING/PONG | GETHEADERS | 收到首个 HEADERS | headers 追平本地 tip(含内置 Checkpoint 前置段) |
| **SYNCING_TXS** | MERKLEBLOCK, TX, INV, HEADERS, REJECT | GETDATA, GETHEADERS | header 追平 | PendingTxMap 清空 + 首次过滤完成 |
| **STEADY** | MERKLEBLOCK, TX, INV, HEADERS, PING/PONG, REJECT | GETDATA, PING, (定时 GETHEADERS) | 初始过滤完成 | 连接断开/切换/深重组拒绝 |

**丢弃规则(防过滤器空窗期丢消息)**:

- 在 `WAIT_FILTER_ACK` 及之前的状态(即收到首个 HEADERS 之前),**收到的所有 `INV` 与 `MERKLEBLOCK` 一律忽略并丢弃**(不缓存、不请求、不处理)——此时过滤器尚未在对端生效,这些消息是未过滤的,处理会造成污染;
- "FILTERLOAD 已写入 Socket 发送缓冲区"的判定:以 CConnman 发送队列 `PushMessage(FILTERLOAD)` 返回、且该消息已进入 OS socket 发送缓冲区的确认(发送队列 flush 后的回调/标志位)为准;
- **兜底**:若 30s 内未收到任何 HEADERS 回复(`WAIT_FILTER_ACK` 超时),按连接超时处理,触发断开重连(§4.1.4)。

#### 4.1.2 心跳(ping/pong)

- 发送间隔:`PING_INTERVAL = 120s`(源码 `net.h:78` 同值);
- pong 超时阈值:**60s**(从发送 ping 起算,未收到对应 nonce 的 pong 视为超时);
- 处理策略:连续 **2 次** ping 超时 → 判定连接失效,触发断开与重连(§4.1.4);每次收到 pong 更新 `nPingUsecTime`(RTT 统计,可暴露于 sync_status)。

#### 4.1.3 reject 消息监听与链上确认兜底

- 广播后对端可能回 `REJECT`(reject.cpp / net_processing 已有解析基础),SDK 需在 ProcessMessage 保留 REJECT 分支;
- **重要语义修正**:BCH 系(含 MVC)新版节点已逐步**弃用 REJECT**(转向静默丢弃),因此不能仅依赖 REJECT 判断广播成败;
- 设计扩展回调 `on_send_result`(见 §5.1):

```c
void (*on_send_result)(void* user, const char* txid, int code, const char* reason);
// code: MVC_LIGHT_SEND_OK=0(仅表示已发送至 Socket,不代表对端接受)
//       MVC_LIGHT_SEND_REJECTED=1(对端回 REJECT,reason 为对端字符串)
//       MVC_LIGHT_SEND_TIMEOUT_UNCONFIRMED=2(120 分钟内未上链,人工核对)
```

| 事件 | 触发回调 | 说明 |
|------|---------|------|
| `PushMessage(TX)` 成功 | `on_send_result(txid, SEND_OK, "")` | 立即(发送层面) |
| 对端回 REJECT(txid) | `on_send_result(txid, SEND_REJECTED, reason)` | 网络消息层面 |
| **120 分钟定时器到期,且该 txid 未通过 MERKLEBLOCK 同步回来(即未触发 on_watch_tx)** | `on_send_result(txid, SEND_TIMEOUT_UNCONFIRMED, "not seen on-chain within 2h")` | **兜底**:告知业务层"已发送但未上链,请人工核对" |

- **链上确认超时兜底机制**:每个广播 txid 登记一个 120 分钟到期事件(SDK 工作线程定时器);到期时查询 `CWatchTxStore::HasTx(txid)`,为 false 则回调 `SEND_TIMEOUT_UNCONFIRMED` 并注销,已上链则静默注销;
- 若同一 txid 曾回 REJECT,则不启动 120 分钟定时器(避免重复回调);
- 30s reject 监听窗口仍保留(快速反馈路径),120 分钟兜底为最终仲裁。

#### 4.1.4 重连策略

- 触发:连接断开(pong 超时/对端关闭/畸形消息/切换失败);
- 退避算法:指数退避,`retry = min(5s × 2^attempt, 300s)`,随机抖动 ±20%;
- 最大重试:连续 **10 次**失败后停止,回调 `on_peer_state(peer, 0, "MAX_RETRY_EXCEEDED")`,等待业务层 `switch_peer` 或 `start` 重新触发;
- 重连期间 API 行为:
  - 读操作(`watch_get`、`sync_status`、`watch_list`):正常返回本地数据(不依赖连接);
  - 写操作(`watch_add`、`send_raw_tx`、`switch_peer`):立即返回 `ERR_PEER_DISCONNECTED`(不阻塞、不排队),由业务层决定重试时机。

#### 4.1.5 消息乱序与超时

- MERKLEBLOCK/TX 配对超时:**30s**(§4.5.1);
- 超时清理:丢弃 Pending 记录,发送 `getdata(MSG_TX)` 请求重传该 txid;重试上限 **3 次**,超过则丢弃并回调 `on_peer_state(..., "TX_RECONCILE_TIMEOUT")` 告警;
- PendingTxMap 内存上限 **4096 条**,超限采用 FIFO 淘汰最旧条目(先清理未配对记录)。

#### 4.1.6 节点切换与深重组拒绝(保留 v0.4 设计)

`switch_peer` → 断旧连新 → 握手 → FilterLoad → 增量 getheaders → 校验首个 Header:分叉深度 > `MAX_REORG_DEPTH=144` → 拒绝切换(返回 `ERR_DEEP_REORG`,保持旧连接);≤144 接受切换并重组。

### 4.2 关注地址 → 布隆过滤器(容量上限)

- 硬编码 `MAX_FILTER_ELEMENTS=20000`、`MAX_FILTER_BYTES=32768`;超限拒绝新增,回调 `on_peer_state(...,"WATCH_FILTER_FULL")`,`watch_add` 返回 `ERR_FILTER_FULL`;
- 匹配方向:输出地址(收款)+ 输入引用关注地址输出(付款);`nFlags=BLOOM_UPDATE_ALL`;定期重建防误报膨胀;
- 对端需 NODE_BLOOM(`net_processing.cpp:3173`),不支持则 `ERR_PEER_NO_BLOOM` 并提示切换节点。

### 4.3 区块头链同步与验证检查项

复用 headers 消息流(`net_processing.cpp:2472`),Facade 模式在 `light_validation` 独立实现。**必须保留的检查项**(逐项对应源码):

| # | 检查项 | 源码位置 | 失败处理 |
|---|--------|---------|---------|
| 1 | PoW 难度(哈希 ≤ 目标,`nBits` 合法且 ≤ powLimit) | `validation.cpp:5026`(CheckBlockHeader) | 拒绝该 header(high-hash) |
| 2 | `nBits == GetNextWorkRequired(prev, block)`(难度连续性) | `validation.cpp:5243` | 拒绝(bad-diffbits) |
| 3 | 时间戳 > 前块中位数时间(`> pindexPrev->GetMedianTimePast()`) | `validation.cpp:5248` | 拒绝(time-too-old) |
| 4 | 时间戳 ≤ 调整后当前时间 + MAX_FUTURE_BLOCK_TIME(+2h) | `validation.cpp:5252` | 拒绝(time-too-new) |
| 5 | 高度 ≥ BitcoinSoftForksHeight 时 `nVersion ≥ 4`(BIP34/65/66/113 系) | `validation.cpp:5260` | 拒绝(bad-version) |
| 6 | 与已采纳 header 链的 prev 哈希连续 | `ContextualCheckBlockHeader` 前置 | 拒绝 |
| 7 | **分两阶段**:① 创世→内置 Checkpoint(历史段):仅校验哈希链指向 + 累计工作量 ≥ 内置 `nChainWork`,**不逐块重算难度**(§7);② Checkpoint→tip(新区段):逐块完整验证全部检查项 | `chainparams.cpp` + light_checkpoints | 拒绝(ERR_CP_CHECKPOINT_FAILED) |

**难度算法(MVC 特殊性)**:mainnet 当前使用 **ASERT DAA**(`pow.cpp:132 GetNextASERTWorkRequired`,激活时间 `asertActivationTime=1686636000`);EDA(`GetNextEDAWorkRequired`)/Cash DAA(`GetNextCashWorkRequired`)为历史分支,仅当 header 高度处于激活前才需要(轻节点从内置 Checkpoint 起步时通常已过激活点,可仅实现 ASERT 路径 + 激活判断 `IsASERTEnabled`)。**依赖数据范围**:prev 块 nBits、时间戳、ASERT 锚点参数(`consensus.asertAnchorParams`)与激活时间。**由于 §7 的 nChainWork 优化,新区段的难度验证仅需从 Checkpoint 起逐块计算,历史段无需回溯 2016 块**,首同步 CPU 开销降低约 95%。

### 4.4 MERKLEBLOCK 接收与完整性验证

(保留 v0.4 设计)先 header 后 merkleblock 时序 + `ExtractMatches` 根校验,失败断开 `fDisconnect`;成功 → 与 PendingTxMap 配对。

### 4.5 交易接收与验证(乱序 + 校验项 + ForkId 脚本)

#### 4.5.1 PendingTxMap 配对(保留 v0.4,补充超时细节)

TX 先到→结构校验→存 Pending;MERKLEBLOCK 先到→根校验→标记预期;到齐匹配→原子写库+回调;30s 超时→丢弃+重传(§4.1.5)。

#### 4.5.2 交易结构校验项(CheckTransactionCommon 等价,`validation.cpp:512`)

| # | 检查项 | 源码 reject 码 |
|---|--------|---------------|
| 1 | `vin` 非空 | bad-txns-vin-empty |
| 2 | `vout` 非空 | bad-txns-vout-empty |
| 3 | 序列化大小 ≤ 单笔交易上限 `maxTxSize`(MVC 主网当前 **32MB**) | bad-txns-oversize |
| 4 | 每个输出 `nValue ≥ 0` | bad-txns-vout-negative |
| 5 | 每个输出 `nValue ≤ MAX_MONEY` | bad-txns-vout-toolarge |
| 6 | 输出总和在 `MoneyRange` 内(无溢出) | bad-txns-txouttotal-toolarge |
| 7 | (Genesis 前) sigops 计数 ≤ 上限 | bad-txn-sigops |

#### 4.5.3 脚本验证"最小必须集合"

- **必须支持**:P2PKH、P2SH 的标准 `VerifyScript`(flags: `SCRIPT_ENABLE_SIGHASH_FORKID | SCRIPT_VERIFY_P2SH | SCRIPT_VERIFY_DERSIG | SCRIPT_VERIFY_NULLFAIL` 等 `standard.h:44` 基线);
- **签名哈希**:必须使用 **ForkId 共识**(`SCRIPT_ENABLE_SIGHASH_FORKID`, `script_flags.h:92`;`SignatureHash` ForkId 分支 `interpreter.cpp:258/285/1341/2061`),**禁止**退化为 BTC 原生 `SIGHASH_ALL`(否则 `ALL|FORKID` 签名校验必然失败,`core_write.cpp:64`);
- 通用库接口:`mvcconsensus_verify_script_with_amount`(`mvcconsensus.h:84`);
- **非标准脚本降级策略**:非 P2PKH/P2SH(如 P2SH 内多签、OP_RETURN 附加、其他锁定脚本)执行失败或无法识别时 → **跳过脚本验证,标记 `script_verified=false`**,仍按"已接收"写入存储,并在 `watch_get` 返回记录中暴露该标记;由业务层决定是否展示;
- 操作码表以 MVC 实际 `script/script.h` 为准。

### 4.6 交易广播(light_txpool + reject 兜底)

**广播不经过全节点 txmempool**(依赖 UTXO,已废弃),改由轻量内存池 `CLightTxPool` 驱动:

```cpp
// light_txpool.h(完整设计)
class CLightTxPool {
    std::unordered_set<uint256>   m_relayed;    // 已广播 txid(防重复)
    std::vector<CTransactionRef>  m_pending;    // 待广播队列
    static constexpr size_t MAX_PENDING = 1024; // 溢出 FIFO 丢弃最旧
public:
    bool Enqueue(const CTransactionRef& tx);    // true=新交易入队;false=重复
    CTransactionRef PopNext();                  // 网络线程取队首
    bool IsRelayed(const uint256& txid) const;
    size_t PendingCount() const;
};
// 依赖边界:仅 include primitives/transaction.h、uint256.h、streams.h;
// 禁止 include validation.h、coins.h、txmempool.h
```

广播时序:

```
send_raw_tx(hex)
  └─ CheckTransactionCommon 结构校验(失败→ERR_INVALID_TX)
  └─ hex 长度校验 ≤ MAX_RAW_TX_HEX=16MB(约 8MB 二进制,低于 32MB 共识上限;
     超限→ERR_PARAM_INVALID)
  └─ CLightTxPool::Enqueue(tx)
  └─ 网络线程:PopNext() → pnode->PushMessage(NetMsgType::TX, tx) → MarkRelayed
  └─ 开启 30s reject 监听窗口 + 120 分钟链上确认兜底(§4.1.3)
```

无 UTXO 集:不做 `AcceptToMemoryPool` 输入校验,验证完全交给对端全节点。

**接收路径长度上限**:对端 `TX` 消息解码前按协议层 `GetMaxMessageLength`(`protocol.h`)校验,上限取 `maxTxSize`(主网 32MB),防超大消息撑爆内存(畸形数据处理,§8.2)。

### 4.7 重组与确认数

- 确认数 = header tip 高度 - 交易高度 + 1(实时计算);
- 浅重组(≤144):按新链更新,回滚块交易标记"待重确认"并回调;
- 深重组(>144):`ERR_DEEP_REORG` + 暂停同步,业务层 `force_reset_chain` 决策。

### 4.8 存储设计(双表 + 原子性 + 容量管理)

#### 4.8.1 数据模型(保留 v0.4 双表 + watch_addr_store)

| 表 | 主键 | 内容 |
|----|------|------|
| `tx_store` | txid | 交易体、height、block_hash、[proof] |
| `addr_tx_index` | (addr, txid) | 仅关联 |
| `watch_addr_store` | addr | 当前关注地址(重启恢复过滤器) |
| `pending_tx`(可选落盘) | txid | 未配对的中间态 |

- 写入原子性:配齐后单事务 `BEGIN → INSERT tx_store(若不存在) → INSERT addr_tx_index → DELETE pending_tx → COMMIT`;LevelDB 用 `WriteBatch` 等价实现;
- txid 去重:已存在则仅插入关联;
- 磁盘容量:存储写入前检查剩余空间,< `MIN_FREE_DISK_BYTES=50MB` → `ERR_DISK_FULL`;
- 完整 DDL 与 LevelDB Key 编码见 **附录 B**。

---

## 5. 业务层集成接口(C ABI)

### 5.1 头文件(mvc_light.h 摘要)

```c
/* 错误码:完整枚举见附录 C */
typedef enum mvc_light_error {
    MVC_LIGHT_OK = 0,
    MVC_LIGHT_ERR_PARAM_INVALID = -1,
    MVC_LIGHT_ERR_NET_TIMEOUT = -2,
    MVC_LIGHT_ERR_PEER_NO_BLOOM = -3,
    MVC_LIGHT_ERR_PEER_DISCONNECTED = -4,
    MVC_LIGHT_ERR_STORE_FAILED = -5,
    MVC_LIGHT_ERR_DEEP_REORG = -6,
    MVC_LIGHT_ERR_DISK_FULL = -7,
    MVC_LIGHT_ERR_FILTER_FULL = -8,
    MVC_LIGHT_ERR_INVALID_TX = -9,
    MVC_LIGHT_ERR_NOT_RUNNING = -10,
    MVC_LIGHT_ERR_ALREADY_RUNNING = -11,
    MVC_LIGHT_ERR_STORE_OPEN_FAILED = -12,
    MVC_LIGHT_ERR_CP_CHECKPOINT_FAILED = -13,
    MVC_LIGHT_ERR_INTERNAL = -99,
} mvc_light_error;

typedef enum mvc_light_log_level { MVC_LIGHT_LOG_DEBUG=0, INFO, WARN, ERROR } mvc_light_log_level;

typedef struct mvc_light_config {
    const char* network;              // "main"/"test"
    const char* peer;                 // 初始直连节点(ip:port)
    int64_t     start_height;         // 起始同步高度(-1=从创世;受内置 Checkpoint 前置约束)
    int         dbcache_mb;           // 默认 8
    int         store_proof;          // 默认 0
    const char* store_path;           // 存储目录
    /* 回调 */
    void (*on_watch_tx)(void* user, const char* addr, const char* txid,
                        int64_t height, int64_t confirmations, int script_verified);
    void (*on_sync_progress)(void* user, int64_t synced_height, int64_t target_height);
    void (*on_peer_state)(void* user, const char* peer, int connected, const char* event);
    void (*on_send_result)(void* user, const char* txid, int code, const char* reason);
    // code: 0=SEND_OK(仅已发送至Socket) / 1=SEND_REJECTED(对端REJECT) / 2=SEND_TIMEOUT_UNCONFIRMED(120min未上链,见§4.1.3)
    void (*on_log)(void* user, int level, const char* msg);
    void* user_data;
} mvc_light_config;

mvc_light_ctx*   mvc_light_init(const mvc_light_config* cfg);   // ERR_STORE_OPEN_FAILED / ERR_PARAM_INVALID
int              mvc_light_start(mvc_light_ctx* ctx);           // ERR_ALREADY_RUNNING / ERR_PEER_DISCONNECTED(首连失败)
void             mvc_light_stop(mvc_light_ctx* ctx);
void             mvc_light_destroy(mvc_light_ctx* ctx);

int              mvc_light_switch_peer(mvc_light_ctx*, const char* address); // ERR_DEEP_REORG / ERR_PEER_DISCONNECTED
int              mvc_light_watch_add(mvc_light_ctx*, const char* address);   // ERR_FILTER_FULL / ERR_PEER_DISCONNECTED
int              mvc_light_watch_remove(mvc_light_ctx*, const char* address);
char*            mvc_light_watch_list(mvc_light_ctx*);                       // JSON
char*            mvc_light_watch_get(mvc_light_ctx*, const char* address,
                                     int64_t from_height, int64_t to_height); // JSON
char*            mvc_light_send_raw_tx(mvc_light_ctx*, const char* hex);     // ERR_INVALID_TX / ERR_PEER_DISCONNECTED / ERR_PARAM_INVALID(hex>16MB)
char*            mvc_light_sync_status(mvc_light_ctx*);                      // JSON
int              mvc_light_force_reset_chain(mvc_light_ctx*);
/* 同步查询(不依赖回调,线程安全) */
int              mvc_light_is_running(mvc_light_ctx*);                       // 0/1
int              mvc_light_get_peer_state(mvc_light_ctx*, char* buf, size_t len); // "CONNECTED"/"SYNCING"/"STEADY"/"DISCONNECTED"/...
void             mvc_light_free_string(char* s);
```

### 5.2 线程安全契约(防死锁,强制)

1. 所有回调在 SDK 工作线程触发,不保证与业务线程同步;
2. **回调内严禁**同步调用任何获取 SDK 内部互斥锁的 API:`watch_add`、`watch_remove`、`switch_peer`、`force_reset_chain`、`send_raw_tx`、`start/stop/destroy` — 否则回调线程自锁死锁;
3. 正确姿势:回调内轻量拷贝数据(字符串)后 `Post` 到业务线程,由业务线程调用 SDK;
4. SDK 保证同一实例所有回调**串行触发**;
5. 同步查询 `is_running` / `get_peer_state` 为无锁原子读,可在任意线程调用。

---

## 6. 资源预算(手机端)

| 资源 | 预算 | 说明 |
|------|------|------|
| 首次同步流量 | ~13 MB header + 关注交易 | 内置 Checkpoint 前为强制 header 同步 |
| 常驻流量 | 每新块几十~几百 KB + 心跳 2B/120s | 单 peer |
| 磁盘 | 10~50 MB + watchstore(Proof 默认不存) | 保留 50MB 安全余量(ERR_DISK_FULL) |
| 内存 | 20~50 MB + dbcache 8MB + PendingTxMap(≤4096 条) | — |
| 动态库体积 | ~5~15 MB | Facade 链接 |
| CPU/电池 | 低 | 事件驱动;空闲可断连 |

---

## 7. 指定高度同步与信任模型(内置 Checkpoint 强制锚定 + nChainWork)

```cpp
// src/light/light_checkpoints.h
struct LightCheckpoint {
    int64_t  height;          // 高度
    uint256  hash;            // 区块哈希
    arith_uint256 nChainWork; // 该高度处累计工作量(新增)
};
static const LightCheckpoint kBuiltinCheckpoints[] = {
    { 150000, uint256S("..."), UintToArith256(uint256S("0000...")) }, // 示例,发布时填真实值
};
```

强制同步顺序(无论 start_height 为何值):

```
① 同步 创世 → 内置 Checkpoint 的 header 链:
   【历史段验证】仅校验 ①prev 哈希连续 ②tip(at Checkpoint).nChainWork ≥ 内置 nChainWork
   —— 不逐块重算难度(历史段 CPU 降低约 95%,§4.3)
② start_height ≤ Checkpoint → 以 Checkpoint 为锚点,过滤同步从 Checkpoint 高度开始
②' start_height > Checkpoint → 从 Checkpoint 继续同步至 start_height 的 header(仅 header,
   逐块完整验证 §4.3 检查项)
③ 以 start_height 为过滤同步起点(assumevalid 锚定)
```

- 内置 Checkpoint 表:见 `light_checkpoints.h`,SDK 发布方维护,季度滚动;Checkpoint 同时提供 `(height, hash, nChainWork)` 三元组;
- **安全性不变**:伪造者无法在"总工作量 ≥ 内置 nChainWork 且哈希链从 Checkpoint 分叉"条件下伪造(仍需超过该工作量,不可行);
- 信任边界:Checkpoint 前由 SDK 保证;Checkpoint→start_height 为用户指定区间(明示);start_height 后为标准 SPV;
- 主网保持 `minimumchainwork` 默认;`sync_status` 暴露 `checkpoint_height`、`checkpoint_chainwork` 与 `start_height`。

---

## 8. 风险与异常场景

### 8.1 风险表(含 v0.4 全部 + 新增)

| 风险 | 等级 | 应对 |
|------|------|------|
| BIP-37 隐私泄露 | 高 | 受信节点;地址分片;二期 BIP-158 |
| 过滤器膨胀 DoS | 高 | 容量上限 + WATCH_FILTER_FULL(§4.2) |
| 单节点投毒(假链) | 高 | 内置 Checkpoint 强制锚定(§7) |
| 伪造 MERKLEBLOCK | 高 | 时序 + 根校验(§4.4) |
| 乱序丢交易 | 中 | PendingTxMap + 30s 重传(§4.5.1) |
| 深重组 | 中 | MAX_REORG_DEPTH + ERROR_DEEP_REORG(§4.1.6) |
| 回调死锁 | 高 | 线程安全契约(§5.2) |
| 无 UTXO 验证 | 中 | 语义声明:完整性≠未被双花 |
| 主网升级合并冲突 | 中→低 | Facade 隔离(§3.4) |

### 8.2 异常场景清单及处理预案

| 场景 | 检测 | 处理 |
|------|------|------|
| **磁盘满/不足 50MB** | 每次存储写前检查剩余空间 | 写操作返回 `ERR_DISK_FULL`;`on_peer_state(...,"DISK_FULL")`;SDK 继续运行(读不受影响),业务层清理后自动恢复 |
| **存储目录权限不足** | init 时 open store 失败 | `init` 返回 `ERR_STORE_OPEN_FAILED`;on_log(ERROR) 输出路径;业务层修正权限后重试 |
| **系统时间严重偏差** | 与对端 median 时间差 > `MAX_TIME_ADJUSTMENT=70min` | on_log(WARN) + 时间偏移暴露于 sync_status;**不自动修正**;header 验证的 time-too-new/old 仍按 nAdjustedTime 执行 |
| **对端返回畸形数据** | 消息反序列化异常 / 超长 / 非法字段 | 断开该连接(防污染),按重连策略重连;畸形 MERKLEBLOCK 另触发 `ERR_*` 记录 |
| **切换节点时旧连接残留消息** | `m_switching` 互斥 + 状态机 | 切换开始即标记,丢弃旧连接未处理消息(§4.1.6) |
| 关注地址超限 | watch_add | `ERR_FILTER_FULL` + WATCH_FILTER_FULL 回调(§4.2) |

---

## 9. 分阶段实施计划

> v1.2 起以“直接集成全节点网络层”为主线；旧自研 P2P 阶段（light_peer/light_sync/light_connman）降级为 legacy。

| 阶段 | 内容 | 验收标准 |
|------|------|---------|
| A 依赖与底座 | vcpkg 安装 Boost/libevent；vendor `third_party/microvisionchain`；`mvc_core` 静态库编译（net/validation/txmempool/addrman/chainparams/consensus） | Windows+MSVC 构建 0 error；空壳可初始化 chainparams |
| B 网络内核启动 | 封装 `light_nodecore`：启动 `CConnman`/`PeerLogicValidation`/`CTxMemPool`，接入种子与 addrman | 主网 **≥2 条连接稳定保持 5 分钟**，网络行为与全节点一致 |
| C 本地过滤+存储+Prune | `light_local_filter` 订阅 `BlockConnected`/`TransactionAddedToMempool`；交易写 LevelDB 双表；启用 `-prune` | watch 地址新交易自动入库；区块文件删除后交易仍可查；历史高度 185136 用例通过 |
| D C ABI 适配 | `mvc_light_*` 基于 `CLightNodeCore`；watch_add/remove、get_tx_list、send_raw_tx 走上游 | 现有 C ABI 测试全绿；回调语义不变 |
| E Demo+测试 | Demo 改用新内核；集成测试（长连接/本地过滤/prune） | 全部测试通过；check_blacklist/check_symbols 更新并通过 |
| F 移动端化(可选) | SQLite 后端、Android/iOS 交叉编译 | 双端 SDK 端到端通过 |

---

## 10. 初始化参数(替代 conf 文件)

```c
mvc_light_config cfg = {
    .network = "main", .peer = "198.51.100.10:9883",
    .start_height = 100000, .dbcache_mb = 8, .store_proof = 0,
    .store_path = "/data/user/0/com.app/files/mvclight",
    .on_watch_tx = ..., .on_sync_progress = ..., .on_peer_state = ...,
    .on_send_result = ..., .on_log = ...,
    .user_data = &my_ctx,
};
```

- 主网 P2P 端口 9883(testnet 19883);v1.2 起由上游 CConnman 多连接管理,不再单节点直连;
- 内置 Checkpoint 硬编码,无需配置;store_proof 仅举证场景开启;
- 日志:未注册 on_log 时默认静默(内部仍记录到内存环形缓冲,可用 `sync_status` 导出最近 N 条)。

---

## 11. 全节点网络层集成方案(v1.2)

### 11.1 动机

自研单连接 P2P 在公共种子上频繁被断开、历史回扫受节点 `OutboundTargetReached` 限制。用户决策:直接使用 MVC 全节点网络层,使网络行为与全节点完全一致。

### 11.2 导入范围

| 类别 | 文件 |
|------|------|
| 连接/消息 | `src/net/*`(net/net_processing/netbase/netaddress/net_message/stream/stream_policy/association/block_download_tracker/node_state/validation_scheduler) |
| 地址/封禁 | `src/addrman.*`、banman/惩罚逻辑 |
| 验证/交易池 | `src/validation.*`、`src/validationinterface.*`、`src/txmempool.*`、`src/txn_validation_*` |
| 参数/协议 | `src/chainparams.*`、`src/chainparamsbase.*`、`src/chainparamsseeds.h`、`src/protocol.*` |
| 共识/基础 | `src/consensus/*`、`src/uint256.*`、`src/arith_uint256.*`、`src/crypto/*`、`src/util/*`、`src/primitives/*`、`src/dbwrapper.*` |
| 依赖 | Boost(signals2/filesystem/thread)、libevent、LevelDB、secp256k1、univalue(vcpkg/vendored) |

### 11.3 本地过滤与存储

- `CLightLocalFilter`(src/light/light_local_filter.h/.cpp):
  - watch 地址 → scriptPubKey(复用上游 `CBitcoinAddress`)
  - 实现 `CValidationInterface`:
    - `BlockConnected` → 遍历区块交易输出,匹配 scriptPubKey
    - `TransactionAddedToMempool` → 匹配 mempool 新交易
  - 命中 → 写 `CLightWatchStore`(LevelDB 双表 tx_store + addr_tx_index)
- 区块文件:启用 `-prune=550`(或更大),上游验证后自动删除已处理区块文件,满足“交易入库后再删区块文件”。

#### 11.3.1 指定高度起步(start_height)

- `mvc_light_config.start_height` 复用为“区块体开始下载高度”：
  - `start_height <= 0`：从头开始
  - `0 < start_height <= checkpoint.height`：以内置 Checkpoint 为信任锚，只下载 `start_height` 之后的区块体；之前历史不下载、不扫描
  - `start_height > checkpoint.height`：调用方必须提供该高度的**可信 block hash**（防投毒），否则拒绝启动；或先快速拉取 header 链（header 很小）到该高度，再以其为锚点
- Header 链仍从头/checkpoint 快速同步（仅 80B/块，约 15MB/18.6 万块，分钟级），但**区块体下载只从 start_height 开始**
- 本地过滤只对 start_height 之后的区块生效；之前历史交易不自动扫描（可配合 Backfill 单独补扫）

### 11.4 构建形态

```
third_party/microvisionchain/  (vendored 只读)
        └─ mvc_core 静态库 (net/validation/txmempool/addrman/...)
src/light/                     (facade + light_local_filter + CWatchTxStore + C ABI)
        └─ mvclight SHARED (仅导出 mvc_light_*)
```

### 11.5 黑名单边界(更新)

- 允许:net/validation/txmempool/addrman/chainparams/consensus
- 仍禁:`rpc/`、`wallet/`、`mining/`、`zmq/`、`qt/`、`http/`、`init.cpp`

### 11.6 验收(DoD)

- 主网 ≥2 条连接稳定 5 分钟,网络行为与全节点一致
- watch 地址交易(新区块 + mempool)自动入库 LevelDB
- 历史用例:`1J3NjfS7eYTddzina6s4bddvEwu4W8UUwc` 高度 185136 交易自动出现
- **start_height=185000 起步：不下载 185000 之前的区块体，仍能同步到链尖并过滤新交易**
- prune 后区块文件删除,交易仍可查询
- `check_blacklist.py` 更新并通过;符号仍只导出 `mvc_light_*`

### 11.7 风险

| 风险 | 对策 |
|------|------|
| Boost/libevent 依赖 | vcpkg 固定版本 |
| 符号冲突 | mvc_core 内部符号隐藏,独立命名空间 |
| 与 init.cpp 耦合 | light_nodecore 只提取网络+验证子流程 |
| 全量验证资源 | `-dbcache` 控制 + prune 限制磁盘 |

---

## 附录 A:P2P 消息状态转换图

(见 §4.1.1 状态机;ASCII 图补充消息收发)

```
INIT ──start()──► HANDSHAKE
                     │ 发 VERSION
                     │ 收 VERSION → 校验服务位(NODE_BLOOM) → 发 VERACK
                     ▼
              (收 VERACK) ──► FILTER_SENT
                                  │ 发 FILTERLOAD(关注地址全量) → 写入 Socket
                                  ▼
                             WAIT_FILTER_ACK
                                  │ 发 GETHEADERS(自 Checkpoint/本地 tip)
                                  │ 收第一个 HEADERS 回复(此前 INV/MERKLEBLOCK 一律丢弃)
                                  ▼
                             SYNCING_HEADERS
                                  │ 收 HEADERS → 逐块验证(§4.3)
                                  │ 追平 → 发 GETDATA(MSG_FILTERED_BLOCK, 新区块)
                                  ▼
                              SYNCING_TXS
                                  │ 收 MERKLEBLOCK + TX → PendingTxMap 配对(§4.5)
                                  │ 配对清空 → 完成首轮
                                  ▼
                                STEADY
                                  │ 定时: PING(120s) / GETHEADERS(新区块探测)
                                  │ 事件: MERKLEBLOCK/TX/INV → 增量同步
                                  ▼
                              (断连/超时) ──► 重连退避 ──► HANDSHAKE
```

## 附录 B:存储层详细设计

### B.1 SQLite DDL

```sql
PRAGMA journal_mode = WAL;
PRAGMA synchronous = NORMAL;

-- 交易本体(txid 主键,32 字节二进制;与 LevelDB 编码一致,跨端数据兼容)
CREATE TABLE IF NOT EXISTS tx_store (
    txid        BLOB PRIMARY KEY,          -- 32B,与 uint256 内存布局一致
    height      INTEGER NOT NULL,
    block_hash  BLOB NOT NULL,             -- 32B 二进制
    tx_blob     BLOB NOT NULL,             -- 序列化 CTransaction
    proof_blob  BLOB,                      -- store_proof=1 时写入,否则 NULL
    script_verified INTEGER NOT NULL DEFAULT 1,
    created_at  INTEGER NOT NULL DEFAULT (unixepoch())
);
CREATE INDEX IF NOT EXISTS idx_tx_height ON tx_store(height);

-- 地址↔交易关联(txid 同样为 BLOB)
CREATE TABLE IF NOT EXISTS addr_tx_index (
    addr  TEXT NOT NULL,                   -- 地址保持 TEXT(ASCII 跨端一致)
    txid  BLOB NOT NULL,
    PRIMARY KEY (addr, txid),
    FOREIGN KEY (txid) REFERENCES tx_store(txid) ON DELETE CASCADE
);
CREATE INDEX IF NOT EXISTS idx_addr ON addr_tx_index(addr);

-- 关注地址(重启恢复过滤器)
CREATE TABLE IF NOT EXISTS watch_addr_store (
    addr       TEXT PRIMARY KEY,
    created_at INTEGER NOT NULL DEFAULT (unixepoch())
);

-- 未配对中间态(可选落盘,异常退出恢复)
CREATE TABLE IF NOT EXISTS pending_tx (
    txid        BLOB PRIMARY KEY,
    tx_blob     BLOB,                      -- TX 已到时写入
    height      INTEGER,                   -- MERKLEBLOCK 已到时写入
    block_hash  BLOB,
    first_seen  INTEGER NOT NULL
);
CREATE INDEX IF NOT EXISTS idx_pending_seen ON pending_tx(first_seen);
```

**编码约定**:`txid`/`block_hash` 一律存 32 字节原始二进制(memcpy `uint256::begin()`,网络序同内存序);读取时按需转 hex 供展示;`addr` 保持 TEXT(base58 字符串)。LevelDB 编码本就为 32B 二进制(见 B.2),两端数据格式完全一致,查询逻辑单一化。

原子写入(配齐):

```sql
BEGIN IMMEDIATE;
INSERT OR IGNORE INTO tx_store (txid, height, block_hash, tx_blob, proof_blob, script_verified) VALUES (?,?,?,?,?,?);
INSERT OR IGNORE INTO addr_tx_index (addr, txid) VALUES (?,?);
DELETE FROM pending_tx WHERE txid = ?;
COMMIT;
```

### B.2 LevelDB Key/Value 编码(桌面端)

| 空间 | Key(前缀 | 数据) | Value |
|------|---------|-------|
| tx_store | `0x01` \| txid(32B) | 序列化 TxRecord{height, blockHash, tx, [proof], scriptVerified} |
| addr_tx_index | `0x02` \| addr(\| 网络前缀编码) \| txid(32B) | 空 |
| watch_addr_store | `0x03` \| addr | 空(存在即关注) |
| pending_tx | `0x04` \| txid | 序列化 PendingEntry |
| meta | `0xFF` \| "checkpoint_height" / "tip_height" | 8B int64 |

原子性:`WriteBatch`(batch 内多 put + delete)。

### B.3 存储路径与容量

```
store_path/
├── mvclight.db        (SQLite, 移动端)
├── mvclight/          (LevelDB 目录, 桌面端)
└── header_cache/      (可选:header 链缓存)
```

- 写入前 `statvfs` 检查剩余 < 50MB → `ERR_DISK_FULL`;
- 目录/文件权限:移动端由平台沙箱保证;桌面端 init 时校验可写。

## 附录 C:完整错误码枚举定义

```c
typedef enum mvc_light_error {
    MVC_LIGHT_OK = 0,
    /* 参数/状态 */
    MVC_LIGHT_ERR_PARAM_INVALID      = -1,   // 参数非法(地址格式、高度范围、配置)
    MVC_LIGHT_ERR_NOT_RUNNING        = -2,   // 未 start 或已 stop
    MVC_LIGHT_ERR_ALREADY_RUNNING    = -3,   // 重复 start
    /* 网络 */
    MVC_LIGHT_ERR_NET_TIMEOUT        = -4,   // 握手/心跳/请求超时
    MVC_LIGHT_ERR_PEER_NO_BLOOM      = -5,   // 对端不支持 NODE_BLOOM
    MVC_LIGHT_ERR_PEER_DISCONNECTED  = -6,   // 当前无有效连接(重连期间写操作)
    MVC_LIGHT_ERR_DEEP_REORG         = -7,   // 深重组拒绝切换(§4.1.6)
    /* 存储 */
    MVC_LIGHT_ERR_STORE_OPEN_FAILED  = -8,   // 存储打开/初始化失败
    MVC_LIGHT_ERR_STORE_FAILED       = -9,   // 写入失败(非磁盘满)
    MVC_LIGHT_ERR_DISK_FULL          = -10,  // 磁盘剩余 < 50MB
    /* 过滤/交易 */
    MVC_LIGHT_ERR_FILTER_FULL        = -11,  // 关注地址超限(20000/32KB)
    MVC_LIGHT_ERR_INVALID_TX         = -12,  // 交易结构校验失败
    /* 共识 */
    MVC_LIGHT_ERR_CP_CHECKPOINT_FAILED = -13, // header 链未通过内置 Checkpoint
    MVC_LIGHT_ERR_INTERNAL           = -99,  // 内部错误
} mvc_light_error;
```

各 API 可能返回码(概要):init → {ERR_PARAM_INVALID, ERR_STORE_OPEN_FAILED, ERR_DISK_FULL};start → {ERR_ALREADY_RUNNING, ERR_PEER_DISCONNECTED, ERR_NET_TIMEOUT, ERR_CP_CHECKPOINT_FAILED};switch_peer → {ERR_DEEP_REORG, ERR_PEER_DISCONNECTED, ERR_NET_TIMEOUT, ERR_PEER_NO_BLOOM};watch_add → {ERR_FILTER_FULL, ERR_PEER_DISCONNECTED, ERR_PARAM_INVALID};watch_get/watch_list → {ERR_NOT_RUNNING, ERR_STORE_FAILED};send_raw_tx → {ERR_INVALID_TX, ERR_PEER_DISCONNECTED, ERR_DISK_FULL}。

## 附录 D:构建系统配置清单

### D.1 依赖版本锁定(以 MVC v0.2.1.0 为准)

| 依赖 | 版本要求 | 来源 |
|------|---------|------|
| Boost | **≥ 1.49**(C++11 需 ≥1.57;构建验证 1.74) | 系统/vcpkg |
| OpenSSL | libssl + libcrypto(1.1.x / 3.x) | 系统/vcpkg |
| libevent | ≥ 2.0 + libevent_pthreads | 系统/vcpkg |
| LevelDB | **vendored**(`src/leveldb/`) | 仓库自带 |
| secp256k1 | **vendored**(`src/secp256k1/`) | 仓库自带 |
| univalue | **vendored**(`src/univalue/`) | 仓库自带 |
| C++ 标准 | C++17 | — |

### D.2 CMake 目标(上游内核 + Facade, v1.2)

```cmake
# third_party/microvisionchain/CMakeLists.txt(新增)
add_library(mvc_core STATIC
    src/net/net.cpp src/net/net_processing.cpp
    src/net/netbase.cpp src/net/netaddress.cpp src/net/net_message.cpp
    src/net/stream.cpp src/net/stream_policy.cpp src/net/stream_policy_factory.cpp
    src/net/association.cpp src/net/association_id.cpp
    src/net/block_download_tracker.cpp src/net/node_state.cpp
    src/net/validation_scheduler.cpp
    src/addrman.cpp src/validation.cpp src/validationinterface.cpp
    src/txmempool.cpp src/txn_validation_data.cpp src/txmempoolevictioncandidates.cpp
    src/chainparams.cpp src/chainparamsbase.cpp src/protocol.cpp
    src/consensus/*.cpp src/crypto/*.cpp src/hash.cpp src/dbwrapper.cpp
    src/util/*.cpp src/primitives/*.cpp
)
target_link_libraries(mvc_core PUBLIC secp256k1 leveldb univalue ${Boost_LIBRARIES} ${OPENSSL_LIBS} ${EVENT_LIBS})

# src/CMakeLists.txt(改)
add_library(mvclight SHARED
    light/light_api.cpp
    light/light_local_filter.cpp       # 本地 scriptPubKey 过滤(替代 bloom)
    light/light_watchstore.cpp
    light/light_nodecore.cpp           # 封装 CConnman/PeerLogicValidation 启动
)
target_link_libraries(mvclight PRIVATE mvc_core)
# 明确排除:不 add_subdirectory(rpc) / (wallet) / (mining) / (zmq) / (qt) / (http)
# 不链接 init.cpp(不启动完整节点进程)
# 旧自研 light_peer/light_sync/light_connman 保留为 legacy,不进新内核链接
```

### D.3 符号可见性控制

```cmake
target_compile_options(mvclight PRIVATE -fvisibility=hidden)
# 头文件中:
#if defined(_WIN32) && defined(MVCLIGHT_SHARED)
#  define MVCLIGHT_API __declspec(dllexport)
#elif defined(__GNUC__) && defined(MVCLIGHT_SHARED)
#  define MVCLIGHT_API __attribute__((visibility("default")))
#else
#  define MVCLIGHT_API
#endif
// 仅 mvc_light_* 函数标注 MVCLIGHT_API;内部 C++ 符号全部隐藏
```

### D.4 移动端交叉编译命令

```bash
# Android(NDK r25+)
export ANDROID_NDK=/opt/android-ndk-r25c
cmake -S . -B build-android \
  -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=arm64-v8a          # 或 armeabi-v7a / x86_64
  -DANDROID_PLATFORM=android-26 \
  -DANDROID_STL=c++_shared \
  -DBUILD_MVC_LIGHT=ON
cmake --build build-android --target mvclight -j$(nproc)
# 产物: build-android/src/libmvclight.so

# iOS(Xcode 15+, 用 ios.toolchain.cmake: https://github.com/leetal/ios-cmake)
export IOS_SDK=/Applications/Xcode.app/Contents/Developer/Platforms
cmake -S . -B build-ios \
  -DCMAKE_TOOLCHAIN_FILE=path/to/ios.toolchain.cmake \
  -DPLATFORM=OS64                 # 真机 arm64;模拟器用 SIMULATOR64
  -DENABLE_BITCODE=OFF \
  -DBUILD_MVC_LIGHT=ON
cmake --build build-ios --target mvclight
# 产物: build-ios/libmvclight.dylib(嵌入 .framework)
```

## 附录 E:测试与部署检查清单

### E.1 三层测试方案

| 层 | 框架 | 范围 | 关键用例 |
|----|------|------|---------|
| 单元 | Boost.Test(仓库已依赖) | merkle 解析、header 验证、过滤器、双表存储、错误码 | ExtractMatches 根匹配/篡改;CheckTransactionCommon 各 reject;过滤器容量上限;LevelDB/SQLite 原子事务 |
| 集成 | 本地私有 **testnet** 节点(*MVC 无 regtest*,用降难度 testnet 私有链,即本仓库 local-dev 方案) | 同步、广播、切换 | 连接→FilterLoad→同步关注交易;send_raw_tx→reject 回调;switch_peer 深重组拒绝;断连重连 |
| 端到端 | 直连主网公开节点 | 真实数据 | 主网 header 同步至内置 Checkpoint;关注地址(测试地址)交易出现;错误码路径 |

### E.2 异常场景测试清单

- 磁盘满:构造 <50MB 剩余环境 → 写操作返回 ERR_DISK_FULL;
- 目录权限:只读 store_path → init 返回 ERR_STORE_OPEN_FAILED;
- 时间偏差:伪造对端时间戳(>70min)→ WARN 日志 + time-too-new/old 生效;
- 畸形数据:注入截断/超长消息 → 断开重连,无崩溃;
- 切换残留:切换中注入旧连接消息 → 被丢弃,数据一致;
- 回调死锁:在回调内调用 watch_add → 测试套件检测超时(预期触发断言/死锁检测,文档明示禁止)。

### E.3 发布检查清单

- [ ] `nm -D libmvclight.so | grep mvc_light_` 仅导出 mvc_light_* 符号;
- [ ] 内置 Checkpoint 高度/哈希与发布说明一致(季度更新);
- [ ] Android(arm64-v8a/armeabi-v7a)、iOS(arm64/simulator)构建通过;
- [ ] 三层测试全绿;异常场景清单逐项过;
- [ ] 版本号 `MAJOR.MINOR.PATCH`(见 E.4)更新并记录 Change Log。

### E.4 升级与兼容性策略

- 版本号:`MAJOR.MINOR.PATCH`(语义化;MAJOR=ABI 破坏,MINOR=新增 API, PATCH=修复);
- **主网硬分叉应对**:
  - 共识变更(难度算法/脚本规则/软分叉高度)不影响 P2P 协议时:SDK 更新内置 Checkpoint + 共识参数即可,**PATCH/MINOR 升级**,旧版本继续可用(仅停止同步到新规则区段时提示升级);
  - P2P 协议或序列化变更(BIP 级):需强制升级,SDK 在 `on_log(WARN)`/`sync_status` 暴露"协议版本不匹配",业务层引导用户升级;
  - 内置 Checkpoint 过期(超过 SDK 维护节奏 2 个周期):`start` 返回 `ERR_CP_CHECKPOINT_FAILED` 并提示升级,防止陈旧锚点被利用。

---

## 附录:源码关键位置索引(已验证)

| 能力 | 位置 |
|------|------|
| CBloomFilter(BIP-37) | `src/bloom.h:45` |
| CPartialMerkleTree / CMerkleBlock | `src/merkleblock.h:57/170` |
| FilterLoad/Add/Clear 处理 | `src/net/net_processing.cpp:3295-3314` |
| MERKLEBLOCK 推送 | `src/net/net_processing.cpp:1243-1270` |
| NODE_BLOOM | `src/init.cpp:2701`、`net_processing.cpp:3173` |
| headers 收发 | `net_processing.cpp:2472/3702` |
| RelayTransaction | `net_processing.cpp:942` |
| PING_INTERVAL | `src/net/net.h:78`(=120s) |
| NetMsgType 命名空间 | `src/protocol.h:198` |
| CheckBlockHeader(PoW) | `validation.cpp:5026` |
| ContextualCheckBlockHeader(bits/时间戳/版本/checkpoint) | `validation.cpp:5231-5265` |
| CheckTransactionCommon | `validation.cpp:512` |
| 单笔交易上限接口 | `config.h:48`(`Config::GetMaxTxSize(isGenesisEnabled,isConsensus)`;策略值 `maxTxSizePolicy` `config.h:626`;主网当前 **32MB**) |
| 区块总 size 上限接口 | `config.h:40`(`Config::GetMaxBlockSize()`;默认 `MAIN_DEFAULT_MAX_BLOCK_SIZE` `policy/policy.h:24`;主网当前 **4GB**) |
| ASERT 难度 | `pow.cpp:132`(GetNextASERTWorkRequired;EDA/DAA 为激活前分支) |
| ForkId 脚本规则 | `script_flags.h:92`、`interpreter.cpp:258/285/1341/2061`、`core_write.cpp:64` |
| 通用脚本库接口 | `script/mvcconsensus.h:78/84` |
| 依赖版本(configure.ac) | Boost≥1.49、OpenSSL、libevent(`configure.ac:821-945`) |
| CNode/CConnman | `net/net.h:268/841` |
| LevelDB 封装 | `src/dbwrapper.cpp/h` |
