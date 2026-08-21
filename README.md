# mvclight

MVC SPV 轻节点 SDK（`libmvclight.{so,dll,dylib}`），基于 BIP-37 布隆过滤器 + 默克尔证明，直连指定 MVC 主网全节点同步关注地址相关交易。

- 设计文档：`doc/light-node-design.md`
- 实施计划：`doc/light-node-execution-plan.md`
- 上游全节点源码（只读引用）：`D:\Project\Sample\microvisionchain`（v0.2.1.0）

## 当前状态

Phase 0（工程环境与构建骨架）实施中。

## 构建

```bash
cmake -S . -B build -DBUILD_MVC_LIGHT=ON
cmake --build build --target mvclight -j
ctest --test-dir build --output-on-failure
```

## 目录结构

```text
src/light/      自研 SDK 代码（C ABI、light_txpool、后续 P2P/存储模块）
src/tests/      单元/集成/端到端测试与 CI 脚本
third_party/    vendored 依赖与上游 patch
```
