# Phase A: vendored MVC 全节点源码（只读副本）
# 本文件暂作为独立构建入口；与 mvclight 主工程集成时需解决
# 目标名冲突（leveldb/secp256k1/univalue）后通过 add_subdirectory 或 ExternalProject 引入。
cmake_minimum_required(VERSION 3.5)
project(microvisionchain-vendored)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 关闭不需要的组件
set(BUILD_MVC_WALLET OFF CACHE BOOL "" FORCE)
set(BUILD_MVC_ZMQ OFF CACHE BOOL "" FORCE)
set(BUILD_MVC_CLI OFF CACHE BOOL "" FORCE)
set(BUILD_MVC_TX OFF CACHE BOOL "" FORCE)

add_subdirectory(src)

# mvc_core 目标：完整 server 库（含 net/validation/txmempool/addrman）
# TODO: 后续按设计文档 §11 裁剪 rpc/mining/http/init 后仍以 mvc_core 导出
add_library(mvc_core ALIAS server)
