# mvclight

MVC SPV 轻节点 SDK（`libmvclight.{so,dll,dylib}`），基于 BIP-37 布隆过滤器 + 默克尔证明，直连指定 MVC 主网全节点同步关注地址相关交易。

- 设计文档：`doc/light-node-design.md`
- 实施计划：`doc/light-node-execution-plan.md`
- 上游全节点源码（只读引用）：`D:\Project\Sample\microvisionchain`（v0.2.1.0）

## 当前状态

- Phase 0 ~ Phase 4：完成（仓库骨架、P2P 状态机、Bloom/Header 同步、MERKLEBLOCK/交易同步、C ABI 业务集成）
- Phase 5：桌面端加固完成（深重组阈值、畸形数据防护、发布检查、真实 MTP）；**Android/iOS 移动端交叉编译已按用户决定移出当前范围**
- Windows 桌面可视化 Demo（`demo/`）已可用，可直连真实主网体验 Header 同步 / Checkpoint / Watch 地址
- 当前共 19 个测试，全部通过

## 构建

```bash
cmake -S . -B build -DBUILD_MVC_LIGHT=ON -DBUILD_MVC_LIGHT_DEMO=ON
cmake --build build --target mvclight -j
cmake --build build --target mvclight_demo -j
ctest --test-dir build --output-on-failure
```

## Windows 桌面 Demo

`demo/` 目录提供 Win32 原生桌面可视化 Demo（无 Web 前端、无第三方 GUI）。

### 构建

```bash
cmake -S . -B build -DBUILD_MVC_LIGHT_DEMO=ON
cmake --build build --target mvclight_demo --config Debug
```

产物：`build/demo/Debug/demo_mvclight.exe`

### 使用

1. 双击运行 `demo_mvclight.exe`
2. 默认连接真实主网种子 `47.242.24.63:9883`（可在顶部编辑框修改）
3. 点击 **Connect**：显示握手状态、Header 同步进度、最新区块哈希、Checkpoint 锚定状态
4. 在 **Watch address** 输入地址后点击 **Add**：地址进入 watch 列表；同步线程会重建 Bloom 过滤器并发送 `FILTERLOAD`，此后命中该地址的新交易会通过 `MERKLEBLOCK + TX` 配对验证后入库展示
5. 点击 **Remove** 删除选中地址；**Reset** 重置链状态（清空 LevelDB 与断点状态）；**Clear Log** 清空日志
6. 点击 **Disconnect** 停止同步
7. **Backfill N**：对最近 N 个区块做 P2P 回扫（独立临时连接，逐块 `getdata(MSG_BLOCK)` 下载完整区块并在本地按 watch scriptPubKey 过滤），可把添加 watch 之前的最近 N 块历史交易也扫出来；默认 100 块，上限 10000

### 持久化与断点续传

- 交易：保存到运行目录 `demo_store/`（LevelDB），重启后自动加载并显示
- Watch 地址：保存到 `demo_watch.txt`，重启后自动加载
- Header 同步位置：保存到 `demo_sync_state.bin`（上次 tip 高度+区块头），下次 Connect 从上次 tip 继续，不会从 genesis 重头拉取

### 自检模式

```bash
cd build/demo/Debug
./demo_mvclight.exe --selftest
```

成功时当前目录生成 `demo_selftest.log`，内容包含 `VISUAL_DEMO_READY`（会真实连接主网种子，校验 Checkpoint 高度 21256 后继续同步至链尖或达到批次上限）。

## 目录结构

```text
src/light/      自研 SDK 代码（C ABI、light_txpool、后续 P2P/存储模块）
src/tests/      单元/集成/端到端测试与 CI 脚本
demo/           Windows 原生桌面可视化 Demo
third_party/    vendored 依赖与上游 patch
```
