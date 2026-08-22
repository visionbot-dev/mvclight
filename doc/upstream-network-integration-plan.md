# 直接集成全节点网络层方案（v1.0）

> 目标：mvclight 不再自研 P2P 连接/交易同步，而是**直接使用 MVC 全节点网络层**，网络行为与全节点完全一致；过滤改为本地过滤；交易入库 LevelDB 后删除区块文件（prune）。

## 0. 核心决策（推翻原黑名单约束）

- 原约束：`validation.cpp / txmempool.cpp / net_processing.cpp / net.cpp / addrman / rpc / wallet / mining / zmq / qt / http / init.cpp` 绝不链接
- 新决策：**移除网络层相关黑名单**，导入上游网络栈：
  - `src/net/`（net.cpp、net_processing.cpp、net_message、netbase、netaddress、association、stream_policy、block_download_tracker、node_state、validation_scheduler）
  - `src/addrman.cpp/h`
  - `src/validation.cpp/h`、`src/txmempool.cpp/h`、`src/validationinterface.*`
  - `src/chainparams.cpp/h`、`src/protocol.cpp/h`、`src/consensus/*`
  - 以及它们依赖的 `uint256`、`arith_uint256`、`crypto/*`、`util/*`、Boost、libevent、LevelDB
- 仍**不引入**：`rpc/`、`wallet/`、`mining/`、`zmq/`、`qt/`、`http/`、`init.cpp`（除非后续需要）

## 1. 目标架构

```
┌─────────────────────────────────────────────────────────┐
│ mvclight C ABI（mvc_light_*，唯一导出符号）              │
├─────────────────────────────────────────────────────────┤
│ src/light/（facade + 本地过滤 + LevelDB 存储）           │
│   - light_api / light_watchstore / light_local_filter     │
├─────────────────────────────────────────────────────────┤
│ 全节点网络内核（third_party/microvisionchain/）          │
│   CConnman + PeerLogicValidation + Addrman + Banman       │
│   Validation + TxMempool + ChainParams                    │
│   ├── P2P 连接管理（多连接/ADDR/心跳/限流/惩罚）          │
│   ├── Header/Block 下载与验证                             │
│   ├── Mempool 交易同步（inv/tx）                          │
│   └── Prune（-prune，处理完删除区块文件）                 │
└─────────────────────────────────────────────────────────┘
```

- 全节点内核作为**独立静态库**（如 `mvc_core`）编译
- mvclight 只通过少量 C++ 接口调用：启动/停止、地址过滤回调、交易入库回调
- 网络行为（多连接、重连、心跳、限流、封禁）由上游代码保证，与全节点一致

## 2. 导入范围（文件清单）

### 必须导入（网络/共识核心）
| 目录/文件 | 说明 |
|----------|------|
| `src/net/*` | 连接管理、消息处理、流策略、区块下载跟踪 |
| `src/addrman.*` | 地址管理器（持久化 peer 地址） |
| `src/validation.*` `src/validationinterface.*` | 区块验证与事件通知 |
| `src/txmempool.*` `txn_validation_*` | 交易池与交易同步 |
| `src/chainparams.*` `chainparamsbase.*` `chainparamsseeds.h` | 网络参数/种子 |
| `src/protocol.*` | P2P 消息类型/版本 |
| `src/consensus/*` | 共识规则（含 ASERT/MTP/CheckTx） |
| `src/uint256.*` `src/arith_uint256.*` `src/crypto/*` | 基础类型与哈希 |
| `src/util/*` `src/compat/*` `src/primitives/*` | 工具与交易/区块结构 |
| `src/stream_policy*`（net 内） | MVC 新版流限流 |
| `src/node_state.*` `src/validation_scheduler.*` | 节点状态与验证调度 |

### 依赖（需安装/构建）
| 依赖 | 用途 | 环境 |
|------|------|------|
| Boost（signals2、filesystem、thread 等） | 上游基础库 | vcpkg 安装 |
| libevent | netbase/网络事件 | vcpkg 安装 |
| LevelDB | 链状态/区块索引 | 已有 vendored |
| （可选）libzmq | 交易推送（暂不启用） | 不装 |

### 明确不导入
- `rpc/`、`wallet/`、`mining/`、`zmq/`、`qt/`、`http/`、`init.cpp`
- 这些模块与“轻节点 SDK”目标无关

## 3. 构建与集成步骤

### Phase A：依赖与工程底座（1~2 天）
1. 安装 vcpkg + Boost + libevent（`vcpkg install boost-signals2 boost-filesystem boost-thread libevent`）
2. 新建 `third_party/microvisionchain/`：从 `D:\Project\Sample\microvisionchain` **vendor 只读副本**（或 git submodule）
3. 新建 `third_party/microvisionchain/CMakeLists.txt`，以**静态库** `mvc_core` 编译上述导入清单
4. 根 CMake：`option(BUILD_MVC_LIGHT_UPSTREAM_CORE "Use upstream network core" ON)`
5. 更新 `src/tests/ci/check_blacklist.py`：黑名单正则改为“禁止直接链接 rpc/wallet/mining/zmq/qt/http/init”，**允许 net/validation/txmempool**
6. 验证：空壳程序能链接 `mvc_core` 并初始化 chainparams

### Phase B：网络内核启动（2~3 天）
1. 封装 `CLightNodeCore`（`src/light/light_nodecore.h/.cpp`）：
   - 初始化 `ArgsManager`/`Config`/`ChainParams`
   - 创建 `CChainState`、`CTxMemPool`、`CConnman`、`PeerLogicValidation`
   - 调用上游启动流程（参考 `init.cpp` 的 Init/Start，但只保留网络+验证部分）
2. 接入种子节点（chainparams seeds），由 addrman 管理
3. 验证：能与主网建立**多条长期连接**，行为与全节点一致（不再被频繁断开）
4. 临时输出：连接数、peer 地址、同步高度

### Phase C：本地过滤 + 交易入库（2~3 天）
1. 实现 `CLightLocalFilter`（`src/light/light_local_filter.h/.cpp`）：
   - watch 地址 → scriptPubKey（复用上游 `base58.h`/`CBitcoinAddress`）
   - 订阅 `CValidationInterface`：
     - `BlockConnected`：遍历区块交易输出，匹配 scriptPubKey
     - `TransactionAddedToMempool`：匹配 mempool 新交易
   - 命中 → 组装 `TxRecord{txid, height, block_hash, script}` 写入 `CLightWatchStore`（LevelDB）
2. 启用 `-prune=550`（约 550MB）或 `-prune=1000`
   - 上游在验证后自动删除已处理的区块文件
   - 满足“交易存入 LevelDB 后再删除区块文件”
3. 验证：监视 `1J3NjfS7eYTddzina6s4bddvEwu4W8UUwc`，能自动收到 185136 高度那笔历史交易（从链上同步+本地过滤）

### Phase D：C ABI 适配（1~2 天）
1. `light_api.cpp` 改为基于 `CLightNodeCore`：
   - `mvc_light_init`：启动网络内核 + LevelDB + 过滤器
   - `mvc_light_watch_add/remove`：更新本地过滤 scriptPubKey 集合
   - `mvc_light_get_tx_list`：从 LevelDB 读
   - `mvc_light_send_raw_tx`：通过 `CConnman` 广播（或暂时保留旧实现）
2. 回调：同步进度、连接状态、交易命中事件
3. 符号可见性：`mvc_core` 内部符号隐藏，仅导出 `mvc_light_*`

### Phase E：Demo 与测试（1~2 天）
1. Demo 改用新 SDK（去掉自研 P2P/回扫逻辑，UI 不变）
2. 新增集成测试：
   - `test_upstream_connman`：启动内核，验证 ≥2 条连接保持 5 分钟
   - `test_local_filter`：构造区块/交易，验证本地过滤命中并入库
   - `test_prune`：验证区块文件被删除而交易仍可查询
3. 保留 C ABI 单测；更新 README

## 4. 关键风险与对策

| 风险 | 对策 |
|------|------|
| 依赖爆炸（Boost/libevent/LevelDB） | 固定 vcpkg 版本；只导入所需模块；CI 用缓存 |
| 上游与自研 light_* 符号冲突 | `mvc_core` 独立库 + 隐藏内部符号；必要时用命名空间隔离 |
| 上游启动逻辑与 init.cpp 耦合 | 参考 init 但只提取网络+验证子流程，写成 `light_nodecore` |
| 验证/UTXO 全量状态体积大 | 可先只启用 `-prune` + 不保留 UTXO 之外的索引；或接入 `-dbcache` 控制 |
| 主网同步耗时 | 断点续传由全节点自身保证；测试用小范围/测试网 |
| 黑名单 CI 变更 | 更新 check_blacklist.py 与文档，明确新边界 |

## 5. 验收 DoD

- [ ] `mvc_core` 静态库构建 0 error（Windows + MSVC）
- [ ] Demo 连接主网：**≥2 条连接稳定保持 5 分钟以上**，不再被种子频繁断开
- [ ] 本地过滤：watch 地址新交易（mempool + 区块）自动入库
- [ ] 历史回扫：`1J3NjfS7eYTddzina6s4bddvEwu4W8UUwc` 185136 高度交易自动出现
- [ ] Prune 生效：区块文件删除后 LevelDB 交易仍可查询
- [ ] `check_blacklist.py` 更新并通过（只禁 rpc/wallet/mining/zmq/qt/http/init）
- [ ] `check_symbols.sh` 仍只导出 `mvc_light_*`
- [ ] 旧自研 P2P 代码标记 deprecated/移除，README 更新

## 6. 里程碑估算

| 阶段 | 工期 |
|------|------|
| A 依赖与底座 | 1~2 天 |
| B 网络内核启动 | 2~3 天 |
| C 本地过滤+LevelDB+Prune | 2~3 天 |
| D C ABI 适配 | 1~2 天 |
| E Demo+测试 | 1~2 天 |
| **合计** | **约 7~12 天** |

## 7. 备注

- 上游为 MIT 协议，可合规引入，需保留版权声明
- 原 `src/light/light_peer.*`、`light_sync.*`、`light_connman.*` 等自研 P2P 保留为 legacy，新内核就绪后逐步下线
- 本方案已按用户要求：**网络行为完全与全节点一致、过滤本地做、交易入库后删区块文件**
