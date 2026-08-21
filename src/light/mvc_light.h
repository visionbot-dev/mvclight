#ifndef MVC_LIGHT_MVC_LIGHT_H
#define MVC_LIGHT_MVC_LIGHT_H

/*
 * MVC SPV 轻节点 SDK 公共 C ABI。
 *
 * 基线：doc/light-node-design.md v1.1
 * 线程安全契约（§5.2）：
 *   - 所有回调在 SDK 工作线程串行触发；
 *   - 回调内严禁调用获取 SDK 内部锁的 API（watch_add / watch_remove /
 *     switch_peer / force_reset_chain / send_raw_tx / start / stop / destroy）；
 *   - is_running / get_peer_state 为无锁原子读，可在任意线程调用。
 *
 * Phase 0：本头文件为可编译骨架，除 init/destroy 外，其余 API 暂返回
 * ERR_NOT_RUNNING / ERR_PARAM_INVALID；后续 Phase 逐个填充实现。
 */

#include "mvc_light_export.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 错误码：完整枚举见设计文档附录 C */
typedef enum mvc_light_error {
    MVC_LIGHT_OK = 0,
    /* 参数/状态 */
    MVC_LIGHT_ERR_PARAM_INVALID = -1,
    MVC_LIGHT_ERR_NOT_RUNNING = -2,
    MVC_LIGHT_ERR_ALREADY_RUNNING = -3,
    /* 网络 */
    MVC_LIGHT_ERR_NET_TIMEOUT = -4,
    MVC_LIGHT_ERR_PEER_NO_BLOOM = -5,
    MVC_LIGHT_ERR_PEER_DISCONNECTED = -6,
    MVC_LIGHT_ERR_DEEP_REORG = -7,
    /* 存储 */
    MVC_LIGHT_ERR_STORE_OPEN_FAILED = -8,
    MVC_LIGHT_ERR_STORE_FAILED = -9,
    MVC_LIGHT_ERR_DISK_FULL = -10,
    /* 过滤/交易 */
    MVC_LIGHT_ERR_FILTER_FULL = -11,
    MVC_LIGHT_ERR_INVALID_TX = -12,
    /* 共识 */
    MVC_LIGHT_ERR_CP_CHECKPOINT_FAILED = -13,
    MVC_LIGHT_ERR_INTERNAL = -99,
} mvc_light_error;

typedef enum mvc_light_log_level {
    MVC_LIGHT_LOG_DEBUG = 0,
    MVC_LIGHT_LOG_INFO = 1,
    MVC_LIGHT_LOG_WARN = 2,
    MVC_LIGHT_LOG_ERROR = 3,
} mvc_light_log_level;

/* 发送结果回调 code（§4.1.3） */
enum {
    MVC_LIGHT_SEND_OK = 0,                  /* 仅表示已发送至 Socket */
    MVC_LIGHT_SEND_REJECTED = 1,            /* 对端回 REJECT */
    MVC_LIGHT_SEND_TIMEOUT_UNCONFIRMED = 2, /* 120 分钟未上链 */
};

typedef struct mvc_light_config {
    const char* network;               /* "main"/"test" */
    const char* peer;                  /* 初始直连节点 ip:port */
    int64_t     start_height;          /* 起始同步高度（-1=从创世） */
    int         dbcache_mb;            /* 默认 8 */
    int         store_proof;           /* 默认 0 */
    const char* store_path;            /* 存储目录 */

    /* 回调 */
    void (*on_watch_tx)(void* user, const char* addr, const char* txid,
                        int64_t height, int64_t confirmations, int script_verified);
    void (*on_sync_progress)(void* user, int64_t synced_height, int64_t target_height);
    void (*on_peer_state)(void* user, const char* peer, int connected, const char* event);
    void (*on_send_result)(void* user, const char* txid, int code, const char* reason);
    void (*on_log)(void* user, int level, const char* msg);

    void* user_data;
} mvc_light_config;

typedef struct mvc_light_ctx mvc_light_ctx;

MVCLIGHT_API mvc_light_ctx* mvc_light_init(const mvc_light_config* cfg);
MVCLIGHT_API int mvc_light_start(mvc_light_ctx* ctx);
MVCLIGHT_API void mvc_light_stop(mvc_light_ctx* ctx);
MVCLIGHT_API void mvc_light_destroy(mvc_light_ctx* ctx);

MVCLIGHT_API int mvc_light_switch_peer(mvc_light_ctx* ctx, const char* address);
MVCLIGHT_API int mvc_light_watch_add(mvc_light_ctx* ctx, const char* address);
MVCLIGHT_API int mvc_light_watch_remove(mvc_light_ctx* ctx, const char* address);
MVCLIGHT_API char* mvc_light_watch_list(mvc_light_ctx* ctx);
MVCLIGHT_API char* mvc_light_watch_get(mvc_light_ctx* ctx, const char* address,
                                       int64_t from_height, int64_t to_height);
MVCLIGHT_API char* mvc_light_send_raw_tx(mvc_light_ctx* ctx, const char* hex);
MVCLIGHT_API char* mvc_light_sync_status(mvc_light_ctx* ctx);
MVCLIGHT_API int mvc_light_force_reset_chain(mvc_light_ctx* ctx);

/* 同步查询（不依赖回调，线程安全） */
MVCLIGHT_API int mvc_light_is_running(mvc_light_ctx* ctx);
MVCLIGHT_API int mvc_light_get_peer_state(mvc_light_ctx* ctx, char* buf, size_t len);
MVCLIGHT_API void mvc_light_free_string(char* s);

#ifdef __cplusplus
}
#endif

#endif /* MVC_LIGHT_MVC_LIGHT_H */
