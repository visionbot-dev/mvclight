# Agent 提示词：mvclight Windows 桌面可视化 Demo

> 使用方法：将下文从“开始”到“结束”整段复制给 agent，或在 Deep Code 中直接粘贴执行。

---

## 开始

请作为资深 C++/Windows 桌面工程师，为 `D:\Project\Sample\mvclight` 项目制作一个 **Windows 原生桌面可视化 Demo**（不使用 Web 前端），让用户能直观体验 MVCLight SDK 的轻节点能力：连接真实 MVC 主网、同步区块头、Checkpoint 锚定、watch 地址管理、交易入库展示。

## 工作环境

- 仓库根目录：`D:\Project\Sample\mvclight`（已实现 Phase 0~5 桌面端）
- 上游全节点源码（只读参考）：`D:\Project\Sample\microvisionchain`
- 工具链：Windows + VS2022 + MSVC + CMake + Ninja
- 当前状态：19 个测试全绿；C ABI 已可用；主网端到端 `test_mainnet_sync` 可连接真实种子节点；LevelDB 持久化已接入；内置真实 Checkpoint（height=21256）

## 第一步：阅读核验

先阅读以下文件，确认架构约束与可复用能力：

1. `README.md` —— 当前状态、构建方式
2. `doc/light-node-design.md` —— 重点：§4.1 P2P 状态机、§4.3 Header 同步、§4.8 双表存储、附录 B 存储编码、C ABI 设计
3. `doc/light-node-execution-plan.md` —— 注意范围更正：**移动端已移出当前范围，不做 Android/iOS**
4. `src/light/mvc_light.h` —— C ABI 全部 API 与错误码
5. `src/tests/integration/test_mainnet_sync.cpp` —— 真实主网 P2P 同步参考实现（握手、getheaders、headers 解析、MTP/ASERT/Checkpoint 校验）
6. `src/light/light_peer.h/.cpp`、`src/light/light_sync.h/.cpp`、`src/light/light_chainstore.h/.cpp`、`src/light/light_watchstore.h/.cpp` —— 可直接复用的内部组件
7. `src/light/light_socket.h/.cpp` —— 跨平台 socket 封装，可参考其 Winsock 用法

硬性约束（必须遵守）：

- 不得链接/包含上游黑名单模块：`validation.cpp`、`txmempool.cpp`、`rpc/`、`wallet/`、`mining/`、`zmq/`、`qt/`、`http/`、`addrman/`、`net_processing.cpp`、`init.cpp`
- 不引入 Qt、Electron、Boost、第三方 GUI 库；**只允许 Win32 API + C++ 标准库**
- 不修改 SDK 核心业务逻辑（`src/light/` 下现有文件）除非确有必要；如需新增 C ABI 函数，必须保持 `mvc_light_*` 前缀并说明理由
- 不实现移动端；不做跨平台打包；不做 Web/HTTP 服务

## 第二步：架构决策（推荐方案）

采用 **C++17 + Win32 API 原生窗口程序**：

- 新增 `demo/` 目录，独立于 SDK 核心：
  - `demo/demo_win32.cpp` —— 主程序（WinMain、窗口过程、控件、工作线程、自检模式）
  - `demo/CMakeLists.txt`（或在根 `CMakeLists.txt` 增加 `BUILD_MVC_LIGHT_DEMO` 选项）
- Demo 链接 `mvclight_objects`（可直接调用 `CLightPeer`、`CLightSync`、`CLightChainStore`、`CLightWatchStore` 等内部组件，也可以调用 C ABI）
- 线程模型：
  - UI 线程：Win32 消息循环，创建控件，接收自定义消息刷新界面
  - 工作线程：执行连接、握手、getheaders/headers 同步循环
  - 工作线程通过 `PostMessage(hWnd, WM_APP + n, ...)` 向 UI 线程推送状态、进度、日志、交易数据
  - 严禁在 UI 线程内执行阻塞网络操作
- 自检模式：`demo_mvclight.exe --selftest` 时不创建窗口（或创建后自动关闭），执行连接+同步核心路径，将结果写入 `demo_selftest.log`，必须包含标记 `VISUAL_DEMO_READY`

## 第三步：实现要求

### 必须实现的真实功能

1. **连接管理**
   - 默认主网种子：`47.242.24.63:9883`（界面可编辑 IP:端口）
   - 按钮：连接 / 断开
   - 展示 Peer 状态机迁移：`INIT → HANDSHAKE → FILTER_SENT → ... → STEADY / DISCONNECTED`
   - 展示握手结果（协议版本、服务位、user agent 如有）

2. **Header 同步进度**
   - 从 genesis 开始发送 `getheaders`，循环读取 `headers` 消息（注意：真实节点每头 80 字节后跟 1 字节 tx-count=0，步长 81）
   - 界面显示：已同步高度、批次、最新区块哈希 / 时间 / nBits
   - 同步完成后显示“已锚定 Checkpoint”状态

3. **Checkpoint 展示**
   - 显示内置 Checkpoint：height `21256`、hash、nChainWork
   - 同步通过后打勾；失败显示错误码

4. **Watch 地址管理**
   - 添加 / 删除地址
   - 显示当前 watch 数量、Bloom 过滤器元素上限提示（BIP-37：20000 元素 / 32768 字节）
   - 地址列表实时刷新

5. **交易展示**
   - 至少用 Demo 内置的演示交易数据触发 `CommitTx`，展示 `tx_store + addr_tx_index` 双表入库效果
   - 交易列表字段：txid、height、block_hash、script_verified
   - 若真实 MERKLEBLOCK/TX 同步链路已能在 Demo 中跑通，优先展示真实数据；跑不通则用演示数据并在界面标注“演示数据”

6. **状态与日志**
   - 展示 `sync_status` 字段：running、peer_state、watch_count、tx_count、checkpoint 字段
   - 日志面板：展示 SDK 回调（`on_log`、`on_peer_state`、`on_send_result`）与 P2P 消息流
   - 按钮：`force_reset_chain`（重置）、清空日志

### UI 要求（Win32 原生控件）

- 主窗口建议布局（可滚动/可调整大小）：
  - 顶部：连接设置（IP:端口 Edit + Connect/Disconnect Button）+ 状态栏
  - 中部左侧：Checkpoint 信息、Header 同步进度条（ProgressBar + 高度文本）
  - 中部右侧：Watch 地址（Edit + Add/Remove + ListBox/ListView）、交易列表（ListView：txid/height/block_hash/script_verified）
  - 底部：日志（多行 Edit 或 RichEdit）
- 深色主题可选（能用 Win32 控件实现就做，做不了不强求）；状态颜色：绿=正常、黄=同步中/警告、红=错误
- 必须处理窗口关闭时停止工作线程并清理资源

### 工程要求

- CMake 增加选项 `BUILD_MVC_LIGHT_DEMO`（默认 ON）
- Demo 目标为 `WIN32` 子系统可执行程序，链接 `mvclight_objects` + `ws2_32` + `user32` + `gdi32` + `comctl32`
- 不得破坏现有白名单/黑名单校验；新增 demo 源文件放在 `demo/`，不进 `src/CMakeLists.txt` 白名单
- 保持现有测试全部通过

## 第四步：验收 DoD（全部满足后才算完成）

- [ ] `cmake -S . -B build -DBUILD_MVC_LIGHT_DEMO=ON` 配置 0 error
- [ ] `cmake --build build --config Debug` 构建 0 error
- [ ] 现有 19 个测试 `ctest -C Debug` 全绿
- [ ] 黑名单检查 `check_blacklist.py` PASS；符号检查 `check_symbols.sh` PASS
- [ ] 运行 `demo_mvclight.exe` 出现原生 Windows 窗口，控件完整可见
- [ ] 点击“连接”能连上真实主网种子并显示 `MAINNET_HANDSHAKE_OK`
- [ ] Header 同步进度条/计数持续增长；Checkpoint 显示“已锚定”
- [ ] 添加 watch 地址后列表实时更新；演示交易可入库并展示
- [ ] 运行 `demo_mvclight.exe --selftest` 后 `demo_selftest.log` 包含 `VISUAL_DEMO_READY`
- [ ] 更新 `README.md` 增加 Demo 使用说明
- [ ] git commit 并 push，commit message 如 `feat: windows visual light node demo`

## 第五步：沟通要求

- 每完成一步先汇报再继续；遇到环境缺失/网络不可达等问题先停下提问，不要自行改变架构决策
- 若发现 C ABI 缺少 Demo 所需能力（如“开始同步”接口），先说明缺口和补法，经确认后再实现
- 禁止使用 emoji 装饰代码/文档（界面文案除外，如需可用文字）
- 完成后按“交付物清单”逐项确认

## 交付物清单

1. `demo/demo_win32.cpp`（Win32 GUI + 工作线程 + --selftest 模式）
2. CMake 集成：根 `CMakeLists.txt` 增加 `BUILD_MVC_LIGHT_DEMO` 选项
3. `README.md` 新增 Demo 章节（如何构建、启动、操作步骤、--selftest 说明）
4. 如新增 C ABI：`src/light/mvc_light.h` / `light_api.cpp` 的改动与说明
5. git commit + push

---

## 结束
