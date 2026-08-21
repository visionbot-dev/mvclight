#include "light/light_api.h"

#include <cstdlib>
#include <cstring>
#include <new>

struct mvc_light_ctx {
    mvclight::LightContext impl;
};

extern "C" {

mvc_light_ctx* mvc_light_init(const mvc_light_config* cfg) {
    if (cfg == nullptr) {
        return nullptr; // ERR_PARAM_INVALID（init 返回 ctx*，NULL 表示失败）
    }
    if (cfg->network == nullptr || cfg->peer == nullptr || cfg->store_path == nullptr) {
        if (cfg->on_log != nullptr) {
            cfg->on_log(cfg->user_data, MVC_LIGHT_LOG_ERROR,
                        "mvc_light_init: network/peer/store_path must not be null");
        }
        return nullptr; // ERR_PARAM_INVALID
    }

    mvc_light_ctx* ctx = new (std::nothrow) mvc_light_ctx();
    if (ctx == nullptr) {
        return nullptr; // ERR_INTERNAL（分配失败）
    }
    ctx->impl.initialized = true;
    return ctx;
}

int mvc_light_start(mvc_light_ctx* ctx) {
    if (ctx == nullptr) return MVC_LIGHT_ERR_PARAM_INVALID;
    // Phase 0：网络层未实现，返回 ERR_PEER_DISCONNECTED（首连失败占位）
    return MVC_LIGHT_ERR_PEER_DISCONNECTED;
}

void mvc_light_stop(mvc_light_ctx* ctx) {
    // Phase 0：无运行线程，空实现
    (void)ctx;
}

void mvc_light_destroy(mvc_light_ctx* ctx) {
    delete ctx;
}

int mvc_light_switch_peer(mvc_light_ctx* ctx, const char* address) {
    (void)address;
    if (ctx == nullptr) return MVC_LIGHT_ERR_PARAM_INVALID;
    return MVC_LIGHT_ERR_NOT_RUNNING;
}

int mvc_light_watch_add(mvc_light_ctx* ctx, const char* address) {
    (void)address;
    if (ctx == nullptr) return MVC_LIGHT_ERR_PARAM_INVALID;
    return MVC_LIGHT_ERR_NOT_RUNNING;
}

int mvc_light_watch_remove(mvc_light_ctx* ctx, const char* address) {
    (void)address;
    if (ctx == nullptr) return MVC_LIGHT_ERR_PARAM_INVALID;
    return MVC_LIGHT_ERR_NOT_RUNNING;
}

char* mvc_light_watch_list(mvc_light_ctx* ctx) {
    if (ctx == nullptr) return nullptr;
    return nullptr; // Phase 0 未实现
}

char* mvc_light_watch_get(mvc_light_ctx* ctx, const char* address,
                          int64_t from_height, int64_t to_height) {
    (void)address;
    (void)from_height;
    (void)to_height;
    if (ctx == nullptr) return nullptr;
    return nullptr; // Phase 0 未实现
}

char* mvc_light_send_raw_tx(mvc_light_ctx* ctx, const char* hex) {
    (void)hex;
    if (ctx == nullptr) return nullptr;
    return nullptr; // Phase 0 未实现
}

char* mvc_light_sync_status(mvc_light_ctx* ctx) {
    if (ctx == nullptr) return nullptr;
    return nullptr; // Phase 0 未实现
}

int mvc_light_force_reset_chain(mvc_light_ctx* ctx) {
    if (ctx == nullptr) return MVC_LIGHT_ERR_PARAM_INVALID;
    return MVC_LIGHT_ERR_NOT_RUNNING;
}

int mvc_light_is_running(mvc_light_ctx* ctx) {
    if (ctx == nullptr) return 0;
    return 0; // Phase 0 未启动
}

int mvc_light_get_peer_state(mvc_light_ctx* ctx, char* buf, size_t len) {
    if (ctx == nullptr || buf == nullptr || len == 0) {
        return MVC_LIGHT_ERR_PARAM_INVALID;
    }
    const char* state = "DISCONNECTED";
    size_t n = strlen(state) + 1;
    if (len < n) {
        return MVC_LIGHT_ERR_PARAM_INVALID;
    }
    memcpy(buf, state, n);
    return 0;
}

void mvc_light_free_string(char* s) {
    free(s);
}

} // extern "C"
