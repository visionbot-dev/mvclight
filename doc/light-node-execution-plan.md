# MVC SPV 轻节点 SDK 实施执行计划

> 输入基线：`doc/light-node-design.md` v1.1（可编码实施版）
> 上游全节点源码（只读引用）：`D:\Project\Sample\microvisionchain`（microvisionchain v0.2.1.0，已本地核验 `configure.ac` 版本号）
> 交付形态：`libmvclight.{so,dll,dylib}` 动态库 SDK（C ABI）
> 排程口径：1 名全职 C++ 开发；总预估 **512 人时（64 人日）**，按 5 日/周串行约 13 周，含缓冲排 **14 周**

---

## 第一章 架构决策（必读）

### 1.1 工程策略：独立仓库（白名单复制 + Facade 隔离）

**决策：采用独立仓库，不基于全节点源码树直接裁剪。**

理由：

- 全节点源码树内 `validation.cpp`、`txmempool.cpp`、`rpc/`、`wallet/` 存在深度耦合，直接裁剪将形成长期维护负担与链接地狱；
- 独立仓库可实现零侵入上游、可复现构建、最小二进制体积（设计目标 5~15 MB，§6）；
- 以 `D:\Project\Sample\microvisionchain`（microvisionchain v0.2.1.0，已本地核验）为上游基线，按白名单**复制**所需文件到本仓库；上游目录一律只读，禁止在源树内修改；确需裁剪时以 `third_party/patches/` 下的 diff 文件管理，不直接改写复制件；
- 自研业务代码集中在 `src/light/`（Facade），上游升级时只更新白名单复制件 + patch，冲突面最小（§3.4）。

### 1.2 目录结构标准

```text
mvclight/
├── CMakeLists.txt                      # 顶层：project() + add_subdirectory(src)
├── cmake/
│   └── toolchains/                     # Android NDK / iOS toolchain wrapper
├── third_party/                        # 只读第三方依赖与上游 patch
│   ├── leveldb/                        # vendored LevelDB（版本锁定）
│   ├── secp256k1/                      # vendored secp256k1（版本锁定）
│   ├── univalue/                       # vendored univalue（版本锁定）
│   └── patches/                        # 上游白名单文件裁剪/适配 patch
│       └── upstream_manifest.json      # 导入清单：来源路径、版本、SHA256、patch 引用
├── src/
│   ├── CMakeLists.txt                  # SDK 核心构建（白名单模式，黑名单校验）
│   ├── light/                          # 自研 SDK 代码（唯一业务逻辑写入区）
│   │   ├── mvc_light.h                 # 公共 C ABI 头（安装/导出）
│   │   ├── mvc_light_export.h          # MVCLIGHT_API 符号可见性宏
│   │   ├── light_api.h / light_api.cpp          # C ABI 实现、生命周期、查询 API
│   │   ├── light_peer.h / light_peer.cpp        # 单连接、握手、心跳、重连、切换
│   │   ├── light_state.h / light_state.cpp      # P2P 状态机（严格迁移表）
│   │   ├── light_message.h / light_message.cpp  # 消息帧读写与长度防护
│   │   ├── light_sync.h / light_sync.cpp        # headers/交易同步编排
│   │   ├── light_validation.h / light_validation.cpp  # header 验证 + CheckTransactionCommon
│   │   ├── light_merkle.h / light_merkle.cpp    # MERKLEBLOCK 解析与根校验
│   │   ├── light_filter.h / light_filter.cpp    # BIP-37 过滤器封装与容量上限
│   │   ├── light_watchstore.h / light_watchstore.cpp        # 存储抽象 + 双表逻辑
│   │   ├── light_watchstore_leveldb.h / light_watchstore_leveldb.cpp  # LevelDB 后端
│   │   ├── light_watchstore_sqlite.h / light_watchstore_sqlite.cpp    # SQLite 后端
│   │   ├── light_chainstore.h / light_chainstore.cpp  # header tip/meta 缓存
│   │   ├── light_pendingtx.h / light_pendingtx.cpp    # PendingTxMap 乱序配对
│   │   ├── light_sendmonitor.h / light_sendmonitor.cpp # REJECT/120min 兜底监控
│   │   ├── light_checkpoints.h / light_checkpoints.cpp # 内置 Checkpoint 表
│   │   ├── light_txpool.h / light_txpool.cpp        # 轻量广播池（替代 txmempool）
│   │   └── light_common.h / light_common.cpp        # 错误码/日志/回调分发
│   ├── net/                            # 上游白名单 P2P 模块（只读 + patch）
│   │   ├── net.h / net.cpp
│   │   ├── netaddress.h / netaddress.cpp
│   │   ├── netbase.h / netbase.cpp
│   │   ├── protocol.h / protocol.cpp
│   │   ├── net_message.h / net_message.cpp
│   │   └── stream.h / stream.cpp
│   ├── consensus/                      # params/merkle 等白名单
│   ├── primitives/                     # block/transaction
│   ├── script/                         # interpreter/script/standard/sigcache 子集
│   ├── crypto/                         # 哈希/加密白名单
│   ├── util/ support/                  # 全局基础依赖
│   ├── bloom.h / bloom.cpp
│   ├── merkleblock.h / merkleblock.cpp
│   ├── chain.h / chain.cpp             # 精简为 header 索引
│   ├── chainparams.h / chainparams.cpp
│   ├── pow.h / pow.cpp                 # ASERT/EDA/DAA 分支
│   ├── dbwrapper.h / dbwrapper.cpp
│   ├── hash.h / hash.cpp
│   ├── uint256.h / arith_uint256.h
│   └── tests/
│       ├── unit/                       # Boost.Test 单元测试
│       ├── integration/                # 本地 mock peer / testnet 集成测试
│       ├── e2e/                        # 主网端到端测试
│       └── ci/                         # 黑名单/符号/发布检查脚本
└── src/examples/                       # Android/iOS Demo（可选，收尾验收）
```

说明：为满足“文件路径以 `src/` 或 `third_party/` 为根”的约束，测试与示例也统一放在 `src/` 下；公共头在 `src/light/mvc_light.h`，发布安装时可再复制到平台 include 目录。

### 1.3 CMake 核心构建逻辑（白名单模式）

```cmake
# src/CMakeLists.txt —— 核心逻辑（示意）
option(BUILD_MVC_LIGHT "Build mvclight SDK" ON)
set(MVC_LIGHT_DB_BACKEND "leveldb" CACHE STRING "leveldb|sqlite")

set(MVCLIGHT_WHITELIST_SOURCES
    # ---- 自研 Facade（业务逻辑唯一写入区）----
    light/light_api.cpp
    light/light_peer.cpp
    light/light_state.cpp
    light/light_message.cpp
    light/light_sync.cpp
    light/light_validation.cpp
    light/light_merkle.cpp
    light/light_filter.cpp
    light/light_watchstore.cpp
    light/light_chainstore.cpp
    light/light_pendingtx.cpp
    light/light_sendmonitor.cpp
    light/light_checkpoints.cpp
    light/light_txpool.cpp
    light/light_common.cpp
    # ---- 上游白名单复制件（只读 + third_party/patches/ 管理裁剪）----
    net/net.cpp net/netbase.cpp net/netaddress.cpp net/protocol.cpp
    net/net_message.cpp net/stream.cpp
    bloom.cpp merkleblock.cpp consensus/merkle.cpp
    primitives/block.cpp primitives/transaction.cpp
    chain.cpp chainparams.cpp pow.cpp
    script/interpreter.cpp script/script.cpp script/standard.cpp script/sigcache.cpp
    dbwrapper.cpp hash.cpp
    crypto/*.cpp util/*.cpp
)

if(MVC_LIGHT_DB_BACKEND STREQUAL "sqlite")
    list(APPEND MVCLIGHT_WHITELIST_SOURCES light/light_watchstore_sqlite.cpp)
else()
    list(APPEND MVCLIGHT_WHITELIST_SOURCES light/light_watchstore_leveldb.cpp)
endif()

add_library(mvclight SHARED ${MVCLIGHT_WHITELIST_SOURCES})
target_compile_options(mvclight PRIVATE -fvisibility=hidden)
target_include_directories(mvclight
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}          # 暴露 src/light/mvc_light.h
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/third_party)
target_link_libraries(mvclight PRIVATE
    secp256k1 leveldb univalue
    ${Boost_LIBRARIES} ${OPENSSL_LIBS} ${EVENT_LIBS})

# ---- 黑名单防回归：白名单中出现以下路径直接 FATAL ----
set(MVCLIGHT_BLACKLIST_REGEX
    "(^|/)(validation|txmempool|mempooltxdb|net_processing|init)\\.cpp$"
    "(^|/)(rpc|wallet|mining|zmq|qt|http|addrman)/")
foreach(src IN LISTS MVCLIGHT_WHITELIST_SOURCES)
    foreach(pat IN LISTS MVCLIGHT_BLACKLIST_REGEX)
        if(src MATCHES "${pat}")
            message(FATAL_ERROR "BLACKLISTED source: ${src}")
        endif()
    endforeach()
endforeach()
```

**明确排除（绝不链接）的模块**：`validation.cpp`、`txmempool.cpp`、`mempooltxdb.cpp`、`net_processing.cpp`（仅作行为参考，不编译）、`init.cpp`、`rpc/`、`wallet/`、`mining/`、`zmq/`、`qt/`、`httpserver.cpp`/`httprpc.cpp`、`addrman.cpp`/DNS 种子、多连接管理 `CConnman` 主体。

### 1.4 上游源码核验与白名单导入映射（依据本地源码）

已对 `D:\Project\Sample\microvisionchain` 实际目录核验，关键结论：

- 版本基线确认为 **microvisionchain v0.2.1.0**（`configure.ac` 中 `_CLIENT_VERSION_MAJOR/MINOR/REVISION/BUILD = 0.2.1.0`）；
- 设计文档引用的 `bloom.cpp/h`、`merkleblock.cpp/h`、`pow.cpp/h`、`checkpoints.cpp/h`、`chain.cpp/h`、`chainparams.cpp/h`、`dbwrapper.cpp/h`、`consensus/merkle.cpp/h`、`primitives/block.cpp/h`、`primitives/transaction.cpp/h` 均存在；
- `src/net/` 实际包含 `net.cpp/h`、`netbase`、`netaddress`、`protocol`、`net_message`、`stream` 之外，还包含 `association.*`、`block_download_tracker.*`、`node_state.*`、`stream_policy*`、`validation_scheduler.*`、`net_processing.*` 等**多连接/全节点专用文件**，一律不导入；
- `src/rpc/`、`src/wallet/`、`src/mining/`、`src/zmq/`、`src/txmempool.cpp/h`、`src/validation.cpp/h`、`src/init.cpp` 均存在，全部列入黑名单；
- 上游自带 `src/CMakeLists.txt` 与根 `CMakeLists.txt`，但**本计划不复用**，独立仓库内新建自己的白名单 `src/CMakeLists.txt`。

白名单导入映射（上游 → 本仓库）：

| 上游（`D:\Project\Sample\microvisionchain\src\...`） | 本仓库目标 | 说明 |
|-----------------------------------------------------|-----------|------|
| `net/netbase.*`、`net/netaddress.*`、`net/protocol.*`、`net/net_message.*`、`net/stream.*` | `src/net/` | 原样导入 |
| `net/net.*` | `src/net/` | 仅取单连接收发所需；裁剪 diff 记录于 `third_party/patches/net-single-peer.patch`；依赖过深时使用 `src/light/light_socket.cpp` 回退 |
| `bloom.*`、`merkleblock.*`、`pow.*`、`chain.*`、`chainparams.*`、`dbwrapper.*`、`hash.*` | `src/` | 原样导入；`chain.*` 按需精简为 header 索引 |
| `consensus/merkle.*`、`consensus/params.h`、`consensus/consensus.h` | `src/consensus/` | 原样导入 |
| `primitives/block.*`、`primitives/transaction.*` | `src/primitives/` | 原样导入 |
| `script/interpreter.*`、`script/script.*`、`script/standard.*`、`script/sigcache.*`、`script/mvcconsensus.*`、`script/script_flags.h`、`script/sighashtype.h` | `src/script/` | 按 §3.1 最小集合导入；`ismine/sign/scriptcache/opcodes` 视编译依赖决定 |
| `crypto/*`、`uint256.h`、`arith_uint256.*`、`streams.h`、`serialize.h`、`version.h`、`util*`、`support/*` | `src/crypto/`、`src/` | 全局基础依赖 |
| `leveldb/`、`secp256k1/`、`univalue/` | `third_party/leveldb/`、`third_party/secp256k1/`、`third_party/univalue/` | vendored 依赖，只读导入 |

导入动作统一登记到 `third_party/patches/upstream_manifest.json`，字段至少包含：上游相对路径、本仓库目标路径、版本号、SHA256、关联 patch 文件名（如有）。CI 校验导入文件与上游一致（除已记录 patch）。

---

## 第二章 总体原则（全程遵守）

以下为贯穿 Phase 0~5 的硬性约束，任何阶段不得违反：

1. **零侵入全节点源码**
   上游文件以只读复制件进入仓库；确需裁剪时通过 `third_party/patches/` 的 diff 记录，禁止直接修改复制件写业务逻辑；业务逻辑只能写在 `src/light/`。

2. **白名单构建 + 黑名单自动化校验**
   所有编译单元必须出现在 `src/CMakeLists.txt` 的白名单列表中；CI 每次构建执行黑名单扫描，出现 `validation.cpp`、`txmempool.cpp`、`rpc/`、`wallet/` 等即构建失败。

3. **存储层数据类型统一**
   `txid`/`block_hash` 一律以 32 字节原始二进制（BLOB）存储，内存布局与 `uint256::begin()` 一致；`addr` 保持 TEXT；SQLite 与 LevelDB 使用同一编码约定（附录 B.1/B.2），跨端数据必须可互读。

4. **状态机流转严格**
   所有 P2P 状态迁移必须经过唯一入口（`light_state.cpp` 的迁移表），禁止业务线程直接改写连接状态；非法迁移必须被拦截并记录日志。

5. **单阶段交付原则**
   每个 Phase 的 DoD 必须全部可自动化验证；未通过 DoD 不得进入下一阶段；DoD 命令写入 `src/tests/ci/` 供 CI 与本地重复执行。

6. **符号可见性控制**
   编译选项 `-fvisibility=hidden`；仅 `mvc_light_*` 函数通过 `MVCLIGHT_API` 导出；发布前 `nm -D` 校验不得出现其他符号。

7. **线程安全契约（防死锁）**
   回调在 SDK 工作线程串行触发；**回调内严禁调用任何获取 SDK 内部锁的 API**（`watch_add`、`watch_remove`、`switch_peer`、`force_reset_chain`、`send_raw_tx`、`start/stop/destroy`）；同步查询 `is_running`/`get_peer_state` 为无锁原子读。

8. **错误码唯一出口**
   C ABI 所有错误必须返回 `mvc_light_error` 枚举（附录 C），禁止裸 `-1`、`errno`、异常或 `nullptr` 直接透出到业务层；内部异常必须在 ABI 边界捕获并映射为 `ERR_INTERNAL`。

### 绝不链接模块黑名单

| 模块 | 文件（禁止加入任何 CMake 目标） | 替代方案 |
|------|--------------------------------|----------|
| 区块体/UTXO 验证 | `src/validation.cpp`、`src/validation.h` | `src/light/light_validation.cpp` |
| 全节点交易池 | `src/txmempool.cpp/h`、`src/mempooltxdb.cpp` | `src/light/light_txpool.cpp` |
| P2P 消息处理（参考） | `src/net/net_processing.cpp` | `src/light/light_peer.cpp` + `light_sync.cpp` |
| 节点初始化 | `src/init.cpp` | `src/light/light_api.cpp` |
| RPC | `src/rpc/**` | C ABI + univalue JSON 返回值 |
| 钱包 | `src/wallet/**` | 无（SDK 不托管私钥） |
| 挖矿 | `src/mining/**` | 无 |
| 消息推送 | `src/zmq/**` | 无 |
| GUI | `src/qt/**` | 无 |
| HTTP | `src/httpserver.cpp/h`、`src/httprpc.cpp` | 无 |
| 节点发现/多连接 | `src/addrman.cpp/h`、`src/dnsseed.cpp` | 单 peer 直连，业务层切换 |

---

## 第三章 分阶段实施任务清单

### Phase 0：工程环境与构建骨架

**阶段目标**：独立仓库 + CMake 白名单构建可产出空壳动态库；`light_txpool` 落地；符号可见性与黑名单校验生效。

**预估工时**：5 人日

**任务清单**

| 编号 | 任务描述 | 文件路径 | 说明 |
|------|---------|---------|------|
| 0.1 | 独立仓库初始化、顶层 CMake、.gitignore | 仓库根 `CMakeLists.txt`、`src/CMakeLists.txt`、`.gitignore` | 顶层仅 `project()` + `add_subdirectory(src)`；提供 `BUILD_MVC_LIGHT` 开关 |
| 0.2 | 上游白名单基础模块导入 | `src/crypto/*.cpp`、`src/hash.h/.cpp`、`src/uint256.h`、`src/arith_uint256.h`、`src/streams.h`、`src/serialize.h`、`src/util/*.cpp`、`src/support/*.h` | 从 `D:\Project\Sample\microvisionchain\src` 按白名单复制（版本 0.2.1.0），只读；导入动作记录到 `third_party/patches/upstream_manifest.json`（见 §1.4） |
| 0.3 | 实现 light_txpool 替代 txmempool | `src/light/light_txpool.h/.cpp` | `CLightTxPool`：`m_relayed`/`m_pending`、`Enqueue/PopNext/IsRelayed/PendingCount`、`MAX_PENDING=1024` FIFO；禁止 include `validation.h/coins.h/txmempool.h`（§4.6） |
| 0.4 | C ABI 空壳与错误码枚举 | `src/light/mvc_light.h`、`src/light/mvc_light_export.h`、`src/light/light_api.h/.cpp` | 完整错误码枚举（附录 C）、配置结构、5 类回调类型；`init/destroy` 可运行，其余 API 暂返回 `ERR_NOT_RUNNING`/`ERR_PARAM_INVALID` |
| 0.5 | CMake 白名单目标 + 符号可见性 | `src/CMakeLists.txt` | `add_library(mvclight SHARED ...)`；`-fvisibility=hidden`；仅 `MVCLIGHT_API` 导出 |
| 0.6 | vendored 依赖构建 | `third_party/leveldb/CMakeLists.txt`、`third_party/secp256k1/CMakeLists.txt`、`third_party/univalue/CMakeLists.txt` | 版本按附录 D.1 锁定；静态库子目标；符号不随 SDK 导出 |
| 0.7 | 单元测试骨架 | `src/tests/unit/test_light_txpool.cpp`、`src/tests/unit/test_mvc_light_api.cpp` | Boost.Test；覆盖 Enqueue 去重/FIFO 溢出/IsRelayed；空参数 init 返回 `ERR_PARAM_INVALID` |
| 0.8 | 黑名单/符号校验脚本 | `src/tests/ci/check_blacklist.py`、`src/tests/ci/check_symbols.sh` | 扫描构建产物不含黑名单目标；`nm -D` 仅 `mvc_light_*` 导出 |
| 0.9 | 上游导入清单与差异校验 | `third_party/patches/upstream_manifest.json`、`src/tests/ci/check_upstream_manifest.py` | 记录上游相对路径/目标路径/版本/SHA256/patch；CI 校验复制件与 `D:\Project\Sample\microvisionchain` 一致（除已记录 patch） |

**验收标准（DoD）**

- [ ] `cmake -S . -B build -DBUILD_MVC_LIGHT=ON && cmake --build build --target mvclight -j4` 编译 0 error
- [ ] `ctest --test-dir build -R unit` 全部通过
- [ ] `src/tests/ci/check_blacklist.py build` 通过：产物不含 validation/txmempool/rpc/wallet/mining/zmq/qt/http/addrman/net_processing/init 目标
- [ ] `src/tests/ci/check_upstream_manifest.py` 通过：白名单复制件与 `D:\Project\Sample\microvisionchain` 一致（patch 已登记）
- [ ] `src/tests/ci/check_symbols.sh build` 通过：`nm -D` 仅导出 `mvc_light_*` 符号
- [ ] `mvc_light_init(NULL)` 返回 `MVC_LIGHT_ERR_PARAM_INVALID`；合法配置 init/destroy 无崩溃
- [ ] light_txpool 单测覆盖：重复 Enqueue 返回 false；溢出 FIFO 淘汰最旧；IsRelayed 正确

**风险与依赖**

- 风险：上游基础模块隐式依赖（`util` 依赖 logging/fs/boost/OpenSSL）；LevelDB/secp256k1 在 Windows/NDK 上的构建差异。
- 缓解：白名单最小集迭代编译；所有裁剪以 patch 形式记录；依赖版本锁定后不再升级。
- 依赖设计文档：§3.1、§3.2、§3.3、§3.4、§4.6、附录 C、附录 D.1-D.3。

---

### Phase 1：P2P 网络层与状态机

**阶段目标**：单 TCP 连接、消息帧读写、VERSION/VERACK 握手、心跳 ping/pong、重连退避骨架可用；`test_phase1` 输出 `HANDSHAKE_COMPLETE`。

**预估工时**：10 人日

**任务清单**

| 编号 | 任务描述 | 文件路径 | 说明 |
|------|---------|---------|------|
| 1.1 | 网络白名单模块导入/裁剪 | `src/net/net.h/.cpp`、`src/net/netbase.h/.cpp`、`src/net/netaddress.h/.cpp`、`src/net/protocol.h/.cpp`、`src/net/net_message.h/.cpp`、`src/net/stream.h/.cpp`、`third_party/patches/net-single-peer.patch` | 从 `D:\Project\Sample\microvisionchain\src\net` 导入并移除 `CConnman` 多连接与 `addrman`；**不导入** `association.*`、`block_download_tracker.*`、`node_state.*`、`stream_policy*`、`validation_scheduler.*`、`net_processing.*`；若 `net.cpp` 依赖过深则启动 1.2 回退方案 |
| 1.2 | 轻量 TCP Socket 封装（回退/兜底路径） | `src/light/light_socket.h/.cpp` | connect/read/write/close、非阻塞 + 超时、断线检测；作为不链接 `net.cpp` 时的连接底座 |
| 1.3 | 消息帧读写 | `src/light/light_message.h/.cpp` | magic/command/len/checksum/payload 编解码；长度上限按 `GetMaxMessageLength`；反序列化异常归类 |
| 1.4 | P2P 状态机 | `src/light/light_state.h/.cpp` | INIT/HANDSHAKE/FILTER_SENT/WAIT_FILTER_ACK/SYNCING_HEADERS/SYNCING_TXS/STEADY；迁移表 + 非法迁移拦截 |
| 1.5 | 握手实现 | `src/light/light_peer.cpp` | 发 VERSION/收 VERSION/校验 `NODE_BLOOM`/发 VERACK/收 VERACK；协商失败断开 |
| 1.6 | 心跳实现 | `src/light/light_peer.cpp` | `PING_INTERVAL=120s`；pong 60s 超时；连续 2 次超时断连；更新 RTT |
| 1.7 | 重连退避骨架 | `src/light/light_peer.cpp` | 指数退避 `min(5s×2^n, 300s)` ±20% 抖动；连续 10 次失败 `MAX_RETRY_EXCEEDED`；重连期间写操作返回 `ERR_PEER_DISCONNECTED` |
| 1.8 | 单元测试 | `src/tests/unit/test_light_state.cpp`、`src/tests/unit/test_light_message.cpp`、`src/tests/unit/test_light_peer.cpp` | 状态迁移/消息帧/超时与退避算法 |
| 1.9 | 集成测试 | `src/tests/integration/test_phase1.cpp` | 本地 mock peer：握手完成输出 `HANDSHAKE_COMPLETE`；不发 VERACK 输出 `HANDSHAKE_TIMEOUT`；断连重连输出 `RECONNECTED` |

**验收标准（DoD）**

- [ ] `cmake --build build --target mvclight test_phase1` 0 error
- [ ] `test_phase1` 输出 `HANDSHAKE_COMPLETE`
- [ ] mock 不发 VERACK：10s 内输出 `HANDSHAKE_TIMEOUT`
- [ ] mock 不回 pong：连续 2 次超时后断开并进入重连退避
- [ ] 重连退避单测：attempt 0 间隔 5s（±20%）、封顶 300s、10 次后 `MAX_RETRY_EXCEEDED`
- [ ] 消息长度超限/截断：不崩溃且断开连接
- [ ] 5 组非法状态迁移单测全部被拦截

**风险与依赖**

- 风险：`net.cpp` 与 `validation.h`/Boost.Asio 隐式依赖；`CNode` 生命周期复杂。
- 缓解：预留回退方案（`light_socket.cpp` 自研连接层），保留 `netbase/protocol/net_message/streams`；DoD 以集成测试输出为准。
- 依赖设计文档：§4.1.1、§4.1.2、§4.1.4、§4.1.5、附录 A、附录 D.1-D.2。

---

### Phase 2：布隆过滤器与 Header 链同步

**阶段目标**：BIP-37 过滤器封装 + 内置 Checkpoint 锚定 + Header 链同步与验证；`test_phase2` 输出 `CHECKPOINT_ANCHORED` 与 `HEADERS_SYNCED`。

**预估工时**：12 人日

**任务清单**

| 编号 | 任务描述 | 文件路径 | 说明 |
|------|---------|---------|------|
| 2.1 | bloom 导入与封装 | `src/bloom.h/.cpp`、`src/light/light_filter.h/.cpp` | `CBloomFilter`；`MAX_FILTER_ELEMENTS=20000`、`MAX_FILTER_BYTES=32768`；`nFlags=BLOOM_UPDATE_ALL`；定期重建接口 |
| 2.2 | 关注地址管理与过滤器重建 | `src/light/light_filter.cpp`、`src/light/light_common.h/.cpp` | 内存版关注集合；超限返回 `ERR_FILTER_FULL` + `WATCH_FILTER_FULL` 回调；持久化由 Phase 3 接入 |
| 2.3 | Header 链基础模块导入 | `src/primitives/block.h/.cpp`、`src/chain.h/.cpp`、`src/pow.h/.cpp`、`src/consensus/params.h`、`src/chainparams.h/.cpp` | `chain` 精简为 header 索引；`pow` 仅保留 ASERT 主路径 + EDA/DAA 激活前分支（§4.3） |
| 2.4 | 内置 Checkpoint 表 | `src/light/light_checkpoints.h/.cpp` | 硬编码 `(height, hash, nChainWork)`；提供 `CheckpointHeight()/Lookup()`；发布前填真实值 |
| 2.5 | Header 链验证 | `src/light/light_validation.h/.cpp` | §4.3 七项检查；两阶段：历史段只校验 prev + `nChainWork`，新区段全量逐块验证；`ERR_CP_CHECKPOINT_FAILED` |
| 2.6 | Headers 同步流程 | `src/light/light_sync.h/.cpp`、`src/light/light_peer.cpp` | WAIT_FILTER_ACK→SYNCING_HEADERS；GETHEADERS/HEADERS 循环；首个 HEADERS 前丢弃 INV/MERKLEBLOCK；30s 无 HEADERS 触发重连 |
| 2.7 | Header 链缓存 | `src/light/light_chainstore.h/.cpp` | tip/height/checkpoint meta 持久化（LevelDB `0xFF` 空间）；可选 `header_cache/` |
| 2.8 | 单元测试 | `src/tests/unit/test_light_filter.cpp`、`src/tests/unit/test_light_validation.cpp`、`src/tests/unit/test_light_checkpoints.cpp` | 过滤器容量、7 项 header 检查、Checkpoint 比较 |
| 2.9 | 集成测试 | `src/tests/integration/test_phase2.cpp` | mock 返回 Checkpoint→tip 合法 headers；注入 5 组非法 header 断言拒绝 |

**验收标准（DoD）**

- [ ] `test_phase2` 输出 `CHECKPOINT_ANCHORED`
- [ ] `test_phase2` 输出 `HEADERS_SYNCED`
- [ ] 非法 header 5 组（PoW/bits/时间戳/version/prev）全部拒绝并输出对应 `bad-*`
- [ ] 累计工作量不足内置 `nChainWork` → 返回 `ERR_CP_CHECKPOINT_FAILED`
- [ ] 第 20001 个关注地址 → `ERR_FILTER_FULL`
- [ ] WAIT_FILTER_ACK 30s 无 HEADERS → 自动重连
- [ ] 历史段不逐块重算难度：10000 header 处理耗时低于 CI 阈值（记录基线）

**风险与依赖**

- 风险：ASERT 锚点/激活参数需与主网源码核对；`nChainWork` 大数比较；`chainparams` 依赖面过大。
- 缓解：Checkpoint 真实值由可信全节点导出脚本生成；`pow.cpp` 先行单测锁定 ASERT 行为。
- 依赖设计文档：§4.1.1、§4.2、§4.3、§7、附录 B.2。

---

### Phase 3：MERKLEBLOCK 与交易同步 + 双表原子存储

**阶段目标**：MERKLEBLOCK 解析/根校验、PendingTxMap 乱序配对、交易结构校验、LevelDB 双表原子存储；`test_phase3` 输出 `TX_PAIRED` 与 `STORE_COMMITTED`。

**预估工时**：12 人日

**任务清单**

| 编号 | 任务描述 | 文件路径 | 说明 |
|------|---------|---------|------|
| 3.1 | merkleblock 模块导入 | `src/merkleblock.h/.cpp`、`src/consensus/merkle.h/.cpp` | `CPartialMerkleTree`/`CMerkleBlock`/`ExtractMatches` |
| 3.2 | light_merkle 封装 | `src/light/light_merkle.h/.cpp` | 根校验；篡改失败按 `fDisconnect` 语义断开；`store_proof=1` 时提取 proof |
| 3.3 | PendingTxMap | `src/light/light_pendingtx.h/.cpp` | TX/MERKLEBLOCK 配对；30s 超时重传；重试上限 3；FIFO 上限 4096 |
| 3.4 | 交易结构校验 | `src/light/light_validation.cpp` | CheckTransactionCommon 7 项：vin/vout 非空、`maxTxSize=32MB`、输出非负/上限/总额、sigops；`bad-txns-*` |
| 3.5 | 存储抽象层 | `src/light/light_watchstore.h/.cpp`、`src/light/light_chainstore.cpp` | TxRecord 序列化；后端接口（LevelDB/SQLite 可插拔）；原子事务抽象 |
| 3.6 | LevelDB 后端 | `src/dbwrapper.h/.cpp`、`src/light/light_watchstore_leveldb.h/.cpp` | 前缀 `0x01~0x04`/`0xFF`；`WriteBatch` 原子；写前 `statvfs` 剩余 <50MB → `ERR_DISK_FULL` |
| 3.7 | 交易同步流程 | `src/light/light_sync.cpp`、`src/light/light_peer.cpp` | SYNCING_TXS：GETDATA(MSG_FILTERED_BLOCK)、MERKLEBLOCK/TX 配对、原子写库、txid 去重、内部 `on_watch_tx` hook（Phase 4 接 C 回调） |
| 3.8 | 单元测试 | `src/tests/unit/test_light_merkle.cpp`、`src/tests/unit/test_light_pendingtx.cpp`、`src/tests/unit/test_light_watchstore.cpp` | ExtractMatches 根匹配/篡改；配对超时；原子事务/去重/磁盘满 |
| 3.9 | 集成测试 | `src/tests/integration/test_phase3.cpp` | 输出 `TX_PAIRED`/`STORE_COMMITTED`；TX 先到与 MERKLEBLOCK 先到两条路径 |

**验收标准（DoD）**

- [ ] `test_phase3` 输出 `TX_PAIRED`、`STORE_COMMITTED`
- [ ] 篡改 MERKLEBLOCK 根/叶子 → mock 断言 `DISCONNECTED`
- [ ] 原子性故障注入：COMMIT 前中断无半写状态；txid 去重后仅新增 `addr_tx_index`
- [ ] 30s 未配对 → 重传 getdata；3 次后输出 `TX_RECONCILE_TIMEOUT`
- [ ] PendingTxMap 4096 FIFO 淘汰
- [ ] 7 项非法交易全部拒绝（`bad-txns-*`）
- [ ] 磁盘剩余 <50MB 时写操作返回 `ERR_DISK_FULL`

**风险与依赖**

- 风险：`ExtractMatches` 与上游行为差异；LevelDB/SQLite 编码一致性；跨后端原子事务。
- 缓解：存储接口先行，双后端共享同一测试套件；编码严格按附录 B.1/B.2 单一约定。
- 依赖设计文档：§4.4、§4.5.1、§4.5.2、§4.8、附录 B。

---

### Phase 4：C ABI 接口封装与业务集成

**阶段目标**：全部导出函数可用，回调分发、广播/REJECT/120 分钟兜底、节点切换深重组拒绝完成；`test_phase4` 输出 `API_ALL_GREEN`。

**预估工时**：10 人日

**任务清单**

| 编号 | 任务描述 | 文件路径 | 说明 |
|------|---------|---------|------|
| 4.1 | 完整 C ABI 实现 | `src/light/mvc_light.h`、`src/light/light_api.h/.cpp` | init/start/stop/destroy/switch_peer/watch_add/remove/list/get/send_raw_tx/sync_status/force_reset_chain/is_running/get_peer_state/free_string；错误码矩阵（附录 C） |
| 4.2 | 回调分发器 | `src/light/light_common.h/.cpp` | 5 类回调；同实例串行触发；回调内调用 SDK API 的检测/断言（§5.2） |
| 4.3 | 广播链路 | `src/light/light_txpool.cpp`、`src/light/light_api.cpp`、`src/light/light_peer.cpp` | `send_raw_tx`：hex≤16MB → CheckTransactionCommon → Enqueue → 网络线程 PopNext → `PushMessage(TX)` → MarkRelayed |
| 4.4 | 发送结果监控 | `src/light/light_sendmonitor.h/.cpp` | REJECT 监听 30s 窗口；120 分钟链上确认兜底；`SEND_OK/REJECTED/TIMEOUT_UNCONFIRMED`；可注入时钟 |
| 4.5 | 查询 API 与 JSON | `src/light/light_api.cpp`、`src/light/light_watchstore.cpp` | `watch_list`/`watch_get`/`sync_status` 输出 univalue JSON；暴露 `script_verified`、checkpoint 字段 |
| 4.6 | 节点切换与深重组拒绝 | `src/light/light_peer.cpp`、`src/light/light_api.cpp` | 断旧连新、增量 getheaders；分叉深度 >144 → `ERR_DEEP_REORG` 并保持旧连接；`m_switching` 丢弃旧残留 |
| 4.7 | force_reset_chain 与同步查询 | `src/light/light_api.cpp`、`src/light/light_sync.cpp` | 清链重建；`is_running`/`get_peer_state` 无锁原子读 |
| 4.8 | 单元测试 | `src/tests/unit/test_light_api.cpp`、`src/tests/unit/test_light_sendmonitor.cpp` | 错误码矩阵、发送状态机、定时器 |
| 4.9 | 集成测试 | `src/tests/integration/test_phase4.cpp` | 全 API 调用；回调死锁检测；输出 `API_ALL_GREEN` |

**验收标准（DoD）**

- [ ] `test_phase4` 输出 `API_ALL_GREEN`
- [ ] 广播路径：PushMessage 成功 → `SEND_OK`；对端 REJECT → `SEND_REJECTED`；注入时钟 120min 未上链 → `SEND_TIMEOUT_UNCONFIRMED`
- [ ] `switch_peer` 深重组 >144 → `ERR_DEEP_REORG` 且旧连接状态保持
- [ ] 回调内调用 `watch_add` 被测试检测（超时/断言），符合 §5.2
- [ ] `watch_get` JSON 字段完整：addr/txid/height/confirmations/script_verified
- [ ] 附录 C 各 API 返回码矩阵测试通过
- [ ] `is_running`/`get_peer_state` 任意线程可调（TSAN 无数据竞争）

**风险与依赖**

- 风险：回调线程与锁顺序死锁；univalue JSON 大对象性能；定时器线程安全。
- 缓解：回调串行队列 + TSAN；可注入时钟单测；`sync_status` 提供内存环形日志。
- 依赖设计文档：§4.1.3、§4.1.5、§4.1.6、§4.2、§4.5.2、§4.6、§4.7、§5、附录 C。

---

### Phase 5：生产级加固与移动端交付

**阶段目标**：深重组完整处理、异常防护、SQLite 后端、Android/iOS 交叉编译、主网端到端验证与发布包。

**预估工时**：15 人日

**任务清单**

| 编号 | 任务描述 | 文件路径 | 说明 |
|------|---------|---------|------|
| 5.1 | SQLite 存储后端 | `src/light/light_watchstore_sqlite.h/.cpp`、`src/light/light_watchstore.h` | 附录 B.1 DDL；WAL；txid/block_hash 为 32B BLOB；`BEGIN IMMEDIATE` 原子事务；`MVC_LIGHT_DB_BACKEND=sqlite` |
| 5.2 | 深重组完整处理 | `src/light/light_peer.cpp`、`src/light/light_sync.cpp` | 浅重组 ≤144 回滚并重确认；深重组暂停同步 + 回调；`force_reset_chain` 决策路径 |
| 5.3 | 异常防护 | `src/light/light_message.cpp`、`src/light/light_peer.cpp` | 畸形/超长消息断开；时间偏差 >70min 告警；磁盘/权限错误路径全覆盖 |
| 5.4 | 资源优化 | `src/light/light_pendingtx.cpp`、`src/light/light_txpool.cpp`、`src/light/light_filter.cpp`、`src/light/light_chainstore.cpp` | Pending 4096、TxPool 1024、过滤器定期重建、header cache 裁剪、内存峰值控制 |
| 5.5 | Android 交叉编译 | `src/CMakeLists.txt`、`third_party/leveldb/CMakeLists.txt`、`third_party/secp256k1/CMakeLists.txt`、`third_party/univalue/CMakeLists.txt` | NDK r25+；arm64-v8a/armeabi-v7a/x86_64；`c++_shared` |
| 5.6 | iOS 交叉编译 | 同上 + `src/light/light_watchstore_sqlite.cpp` | ios-cmake `OS64`/`SIMULATOR64`；framework 打包 |
| 5.7 | 主网端到端测试 | `src/tests/e2e/test_e2e_mainnet.cpp` | 真实 Checkpoint；测试关注地址；真实交易；`sync_status` 暴露 checkpoint 字段 |
| 5.8 | 发布检查自动化 | `src/tests/ci/check_release.sh`、`src/tests/ci/check_blacklist.py`、`src/tests/ci/check_symbols.sh` | 附录 E.3 清单脚本化 |
| 5.9 | Demo App 集成 | `src/examples/android/`、`src/examples/ios/` | 最小 App 调用全 API；可作为并行任务提前准备 |

**验收标准（DoD）**

- [ ] Android arm64-v8a/armeabi-v7a/x86_64 交叉编译通过，产出 `libmvclight.so`
- [ ] iOS 真机 arm64 + 模拟器构建通过，产出 framework
- [ ] SQLite 与 LevelDB 后端共享同一测试套件全绿；同一 txid 跨端读取结果一致
- [ ] 模拟 >144 深重组 → `ERR_DEEP_REORG` + 暂停同步；`force_reset_chain` 可恢复
- [ ] 模糊/畸形消息注入 1 小时无崩溃
- [ ] 主网端到端通过：Header 同步至内置 Checkpoint，关注交易出现
- [ ] `nm -D` 仅 `mvc_light_*`；发布检查清单全绿
- [ ] 版本号 `MAJOR.MINOR.PATCH`、Change Log、Checkpoint 发布说明更新

**风险与依赖**

- 风险：NDK 下旧 Boost/LevelDB 编译；iOS bitcode/签名；SQLite 在 iOS 的编译集成；主网固定节点可达性。
- 缓解：CI 双端构建机；SQLite 编译选项预研；端到端使用固定可信节点并允许重试。
- 依赖设计文档：§4.1.6、§4.7、§4.8、§6、§7、§8、附录 B.1/B.3、附录 D.4、附录 E。

---

## 第四章 各阶段时间线总览

**总预估**：512 人时（64 人日）。按 1 名全职开发（5 日/周）串行约 13 周；排程含 1 周缓冲，共 14 周。

| 阶段 | 预估 | 周次 | 累计人日 |
|------|------|------|---------|
| Phase 0 | 5 人日 | W1 | 5 |
| Phase 1 | 10 人日 | W2-W3 | 15 |
| Phase 2 | 12 人日 | W4-W6 | 27 |
| Phase 3 | 12 人日 | W7-W9 | 39 |
| Phase 4 | 10 人日 | W10-W11 | 49 |
| Phase 5 | 15 人日 | W12-W14 | 64 |

```text
周次          W1  W2  W3  W4  W5  W6  W7  W8  W9  W10 W11 W12 W13 W14
Phase 0       ████
Phase 1           ████ ████
Phase 2                    ████ ████ ████
Phase 3                                 ████ ████ ████
Phase 4                                            ████ ████
Phase 5                                                        ████ ████ ████
CI/文档并行    ████ ████ ████ ████ ████ ████ ████ ████ ████ ████ ████ ████ ████ ████
Demo App 准备                                  ████ ████ ████ ████ ████ ████ ████ ████
```

若投入 2 名开发，可将 Phase 5 的 SQLite 后端提前至 Phase 4 并行、Demo/CI 完全并行，总工期可压缩至 10~11 周；但关键路径（见第六章）不变。

---

## 第五章 并行工作建议

| 并行任务 | 内容 | 启动条件 | 与主流程关系 |
|---------|------|---------|-------------|
| CI 搭建 | 多平台编译、黑名单/符号检查、`ctest`、ASAN/TSAN job、构建产物归档 | Phase 0 仓库骨架完成后 | 全程跟随，不阻塞主流程 |
| 本地 testnet 私有节点准备 | 按附录 E.1 搭降难度 testnet 私有链，提供固定 peer 地址与 mock 数据 | Phase 1 开始前 | 支撑 Phase 1~4 集成测试 |
| API 文档编写 | `mvc_light.h` Doxygen/Markdown、错误码矩阵、线程安全契约说明 | Phase 0 产出 `mvc_light.h` 初版后 | 随 Phase 4 API 冻结 |
| Demo App 准备 | Android/iOS 空壳 UI、JNI/Swift 绑定骨架 | Phase 4 前可做 UI 壳；正式对接需 Phase 4 API 冻结 | 收尾验收用 |
| 内置 Checkpoint 导出工具 | 从可信主网全节点导出 `(height, hash, nChainWork)` 并生成 `light_checkpoints.h` | Phase 2 开始前 | 供 Phase 2/5 使用 |
| 性能/体积基准 | 首同步流量、内存峰值、动态库体积测量 | Phase 2 header 同步可用后 | 供 Phase 5 优化与发布基线 |
| 安全审查 | 威胁模型复核、回调死锁/畸形数据 review、模糊测试 | Phase 5 发布前 | 发布 gate |

---

## 第六章 关键路径提醒

**关键路径（不可压缩的串行链）**：

```text
Phase 0 → Phase 1 → Phase 2 → Phase 3 → Phase 4 → Phase 5
```

| 阶段 | 是否关键路径 | 延期影响范围 |
|------|-------------|-------------|
| Phase 0 | 是（起点） | 所有阶段顺延；仓库/构建/黑名单校验未就绪则无开发底座 |
| Phase 1 | 是 | P2P 层是 Phase 2/3/4/5 的硬前置；延期直接推后 Header 与交易同步 |
| Phase 2 | 是 | Header/过滤器未完成则 Phase 3 的 MERKLEBLOCK 无链可锚定 |
| Phase 3 | 是 | 存储与交易配对未完成则 Phase 4 的业务 API 无数据可查 |
| Phase 4 | 是 | API 冻结延迟，影响 Phase 5 Demo、文档与发布 |
| Phase 5 | 是（终点） | 直接决定可交付日期；任何前面阶段延期都会在此暴露 |

**非关键但重要**：CI、API 文档、Demo、testnet 节点、Checkpoint 导出工具均可在并行窗口完成，作为关键路径的缓冲垫。

**关键路径缓解措施**：

- 接口先行：Phase 0 定义存储后端接口、Phase 2 定义 `light_sync` 接口、Phase 3 定义 `light_sendmonitor` 接口，使后续阶段可并行开发；
- Phase 5 的 SQLite 后端可在 Phase 4 尾段并行启动（存储接口在 Phase 3 已冻结）；
- 关键路径上禁止插入非必要重构；任何影响 ABI 的变更必须走 Phase 4 变更评审。

---

## 附录：参考文档索引

| 本计划章节/阶段 | 设计文档 `doc/light-node-design.md` 对应章节 |
|----------------|---------------------------------------------|
| 第一章 架构决策 | §1.4 设计决策、§3.4 Facade 隔离编译、附录 D 构建系统配置 |
| 第二章 总体原则 | §3.2 移除/简化模块、§4.8 存储设计、§5.2 线程安全契约、附录 C 错误码 |
| Phase 0 | §3.1/3.2/3.3/3.4、§4.6 light_txpool、附录 C、附录 D.1-D.3 |
| Phase 1 | §4.1.1 P2P 状态机、§4.1.2 心跳、§4.1.4 重连、§4.1.5 乱序与超时、附录 A |
| Phase 2 | §4.1.1 状态机过渡态、§4.2 布隆过滤器、§4.3 Header 链验证、§7 指定高度与 Checkpoint、附录 B.2 |
| Phase 3 | §4.4 MERKLEBLOCK、§4.5.1 PendingTxMap、§4.5.2 交易校验、§4.8 存储设计、附录 B |
| Phase 4 | §4.1.3 reject/兜底、§4.1.5 超时、§4.1.6 节点切换/深重组、§4.6 广播、§5 C ABI、附录 C |
| Phase 5 | §4.1.6 深重组、§4.7 重组与确认数、§4.8 存储、§6 资源预算、§7 信任模型、§8 风险与异常、附录 B.1/B.3、附录 D.4、附录 E |
| 测试与验收 | 附录 E 三层测试方案、E.2 异常场景、E.3 发布检查清单 |
| 升级与兼容 | 附录 E.4 升级策略（MAJOR/MINOR/PATCH、硬分叉应对、Checkpoint 过期策略） |

### 源码位置索引（仅参考，不链接）

| 能力 | 设计文档中的源码位置 |
|------|---------------------|
| CBloomFilter（BIP-37） | `src/bloom.h:45` |
| CPartialMerkleTree / CMerkleBlock | `src/merkleblock.h:57/170` |
| NODE_BLOOM | `src/init.cpp:2701`、`net_processing.cpp:3173`（参考，不链接） |
| headers 收发 | `net_processing.cpp:2472/3702`（参考，不链接） |
| PING_INTERVAL | `src/net/net.h:78`（=120s） |
| CheckBlockHeader（PoW） | `validation.cpp:5026`（参考，不链接） |
| CheckTransactionCommon | `validation.cpp:512`（参考，不链接） |
| ASERT 难度 | `src/pow.cpp:132` |
| ForkId 脚本规则 | `script_flags.h:92`、`interpreter.cpp:258/285/1341/2061` |
| LevelDB 封装 | `src/dbwrapper.cpp/h` |

### 实际本地上游路径索引（`D:\Project\Sample\microvisionchain`）

| 能力 | 上游本地路径（只读参考） |
|------|--------------------------|
| 版本基线 | `D:\Project\Sample\microvisionchain\configure.ac`（v0.2.1.0） |
| CBloomFilter（BIP-37） | `D:\Project\Sample\microvisionchain\src\bloom.h` |
| CPartialMerkleTree / CMerkleBlock | `D:\Project\Sample\microvisionchain\src\merkleblock.h` |
| 网络消息协议 | `D:\Project\Sample\microvisionchain\src\net\protocol.cpp/h`、`net\net_message.cpp/h`、`net\stream.cpp/h` |
| 单连接网络实现（需裁剪） | `D:\Project\Sample\microvisionchain\src\net\net.cpp/h` |
| P2P 消息处理（参考，不链接） | `D:\Project\Sample\microvisionchain\src\net\net_processing.cpp` |
| NODE_BLOOM（参考） | `D:\Project\Sample\microvisionchain\src\init.cpp`、`src\net\net_processing.cpp` |
| PING_INTERVAL | `D:\Project\Sample\microvisionchain\src\net\net.h` |
| CheckBlockHeader（参考，不链接） | `D:\Project\Sample\microvisionchain\src\validation.cpp` |
| CheckTransactionCommon（参考，不链接） | `D:\Project\Sample\microvisionchain\src\validation.cpp` |
| ASERT 难度 | `D:\Project\Sample\microvisionchain\src\pow.cpp` |
| ForkId 脚本规则 | `D:\Project\Sample\microvisionchain\src\script\script_flags.h`、`src\script\interpreter.cpp` |
| 脚本通用接口 | `D:\Project\Sample\microvisionchain\src\script\mvcconsensus.h` |
| LevelDB 封装 | `D:\Project\Sample\microvisionchain\src\dbwrapper.cpp/h` |
| vendored 依赖 | `D:\Project\Sample\microvisionchain\src\leveldb\`、`src\secp256k1\`、`src\univalue\` |
