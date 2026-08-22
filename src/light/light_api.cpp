#include "light/light_api.h"

#include "light/light_sha256.h"

#include <cstdlib>
#include <cstring>
#include <new>

namespace {

int HexVal(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

bool HexToBytes(const char* hex, std::vector<uint8_t>& out) {
    out.clear();
    if (hex == nullptr) return false;
    size_t len = strlen(hex);
    if (len % 2 != 0) return false;
    for (size_t i = 0; i < len; i += 2) {
        int hi = HexVal(hex[i]);
        int lo = HexVal(hex[i + 1]);
        if (hi < 0 || lo < 0) return false;
        out.push_back(static_cast<uint8_t>((hi << 4) | lo));
    }
    return true;
}

std::string BytesToHex(const uint8_t* data, size_t len) {
    static const char* hex = "0123456789abcdef";
    std::string s(len * 2, '0');
    for (size_t i = 0; i < len; ++i) {
        s[i * 2] = hex[data[i] >> 4];
        s[i * 2 + 1] = hex[data[i] & 15];
    }
    return s;
}

bool SplitHostPort(const std::string& peer, std::string& host, uint16_t& port) {
    auto pos = peer.rfind(':');
    if (pos == std::string::npos || pos + 1 >= peer.size()) return false;
    host = peer.substr(0, pos);
    port = static_cast<uint16_t>(atoi(peer.c_str() + pos + 1));
    return port != 0;
}

char* StrDup(const std::string& s) {
    char* p = static_cast<char*>(malloc(s.size() + 1));
    if (p) {
        memcpy(p, s.c_str(), s.size() + 1);
    }
    return p;
}

void EmitPeerState(mvclight::LightContext* c, const char* peer, int connected,
                   const char* event) {
    if (c->callbacks.on_peer_state != nullptr) {
        c->callbacks.on_peer_state(c->callbacks.user_data, peer, connected, event);
    }
}

void EmitLog(mvclight::LightContext* c, int level, const char* msg) {
    if (c->callbacks.on_log != nullptr) {
        c->callbacks.on_log(c->callbacks.user_data, level, msg);
    }
}

void EmitSendResult(mvclight::LightContext* c, const std::string& txid, int code,
                    const std::string& reason) {
    if (c->callbacks.on_send_result != nullptr) {
        c->callbacks.on_send_result(c->callbacks.user_data, txid.c_str(), code,
                                    reason.c_str());
    }
}

} // namespace

struct mvc_light_ctx {
    mvclight::LightContext impl;
};

extern "C" {

mvc_light_ctx* mvc_light_init(const mvc_light_config* cfg) {
    if (cfg == nullptr) return nullptr;
    if (cfg->network == nullptr || cfg->peer == nullptr || cfg->store_path == nullptr) {
        if (cfg->on_log != nullptr) {
            cfg->on_log(cfg->user_data, MVC_LIGHT_LOG_ERROR,
                        "mvc_light_init: network/peer/store_path must not be null");
        }
        return nullptr;
    }

    mvc_light_ctx* ctx = new (std::nothrow) mvc_light_ctx();
    if (ctx == nullptr) return nullptr;

    ctx->impl.initialized = true;
    ctx->impl.network = cfg->network;
    ctx->impl.peer = cfg->peer;
    ctx->impl.store_path = cfg->store_path;
    ctx->impl.callbacks = *cfg;
    return ctx;
}

int mvc_light_start(mvc_light_ctx* ctx) {
    if (ctx == nullptr) return MVC_LIGHT_ERR_PARAM_INVALID;
    if (ctx->impl.running) return MVC_LIGHT_ERR_ALREADY_RUNNING;

    std::string host;
    uint16_t port = 0;
    if (!SplitHostPort(ctx->impl.peer, host, port)) {
        return MVC_LIGHT_ERR_PARAM_INVALID;
    }

    // Phase 4：阻塞式连接+握手；后续改为异步线程
    if (!ctx->impl.peer_conn.ConnectAndHandshake(host, port, 3000, 3000)) {
        EmitPeerState(&ctx->impl, ctx->impl.peer.c_str(), 0, "CONNECT_FAILED");
        return MVC_LIGHT_ERR_PEER_DISCONNECTED;
    }
    ctx->impl.running = true;
    EmitPeerState(&ctx->impl, ctx->impl.peer.c_str(), 1, "CONNECTED");
    return MVC_LIGHT_OK;
}

void mvc_light_stop(mvc_light_ctx* ctx) {
    if (ctx == nullptr) return;
    ctx->impl.running = false;
    ctx->impl.peer_conn.Disconnect();
    EmitPeerState(&ctx->impl, ctx->impl.peer.c_str(), 0, "STOPPED");
}

void mvc_light_destroy(mvc_light_ctx* ctx) {
    delete ctx;
}

int mvc_light_switch_peer(mvc_light_ctx* ctx, const char* address) {
    if (ctx == nullptr || address == nullptr) return MVC_LIGHT_ERR_PARAM_INVALID;
    if (!ctx->impl.running) return MVC_LIGHT_ERR_PEER_DISCONNECTED;

    std::string host;
    uint16_t port = 0;
    if (!SplitHostPort(address, host, port)) return MVC_LIGHT_ERR_PARAM_INVALID;

    ctx->impl.peer_conn.Disconnect();
    if (!ctx->impl.peer_conn.ConnectAndHandshake(host, port, 3000, 3000)) {
        // 深重组拒绝（Phase 5 完善）：此处仅报告失败
        return MVC_LIGHT_ERR_PEER_DISCONNECTED;
    }
    ctx->impl.peer = address;
    EmitPeerState(&ctx->impl, address, 1, "SWITCHED");
    return MVC_LIGHT_OK;
}

int mvc_light_watch_add(mvc_light_ctx* ctx, const char* address) {
    if (ctx == nullptr || address == nullptr) return MVC_LIGHT_ERR_PARAM_INVALID;
    if (!ctx->impl.running) return MVC_LIGHT_ERR_PEER_DISCONNECTED;
    if (ctx->impl.watch_addresses.size() >= mvclight::kMaxFilterElements) {
        return MVC_LIGHT_ERR_FILTER_FULL;
    }
    ctx->impl.watch_addresses.push_back(address);
    return MVC_LIGHT_OK;
}

int mvc_light_watch_remove(mvc_light_ctx* ctx, const char* address) {
    if (ctx == nullptr || address == nullptr) return MVC_LIGHT_ERR_PARAM_INVALID;
    auto& v = ctx->impl.watch_addresses;
    for (auto it = v.begin(); it != v.end(); ++it) {
        if (*it == address) {
            v.erase(it);
            return MVC_LIGHT_OK;
        }
    }
    return MVC_LIGHT_OK;
}

char* mvc_light_watch_list(mvc_light_ctx* ctx) {
    if (ctx == nullptr) return nullptr;
    std::string json = "[";
    for (size_t i = 0; i < ctx->impl.watch_addresses.size(); ++i) {
        if (i) json += ",";
        json += "\"" + ctx->impl.watch_addresses[i] + "\"";
    }
    json += "]";
    return StrDup(json);
}

char* mvc_light_watch_get(mvc_light_ctx* ctx, const char* address,
                          int64_t from_height, int64_t to_height) {
    if (ctx == nullptr || address == nullptr) return nullptr;
    std::vector<mvclight::uint256> txids;
    ctx->impl.store.GetTxidsByAddr(address, txids);
    std::string json = "[";
    bool first = true;
    for (const auto& txid : txids) {
        mvclight::TxRecord rec;
        if (!ctx->impl.store.GetTx(txid, rec)) continue;
        if (rec.height < from_height || (to_height >= 0 && rec.height > to_height)) continue;
        if (!first) json += ",";
        first = false;
        json += "{\"txid\":\"" + txid.GetHex() + "\",\"height\":" +
                std::to_string(rec.height) +
                ",\"script_verified\":" + (rec.script_verified ? "true" : "false") + "}";
    }
    json += "]";
    return StrDup(json);
}

char* mvc_light_send_raw_tx(mvc_light_ctx* ctx, const char* hex) {
    if (ctx == nullptr || hex == nullptr) return nullptr;
    if (strlen(hex) > 16 * 1024 * 1024 * 2) return nullptr; // 16MB hex 上限

    std::vector<uint8_t> raw;
    if (!HexToBytes(hex, raw)) return nullptr;

    uint8_t hash[32];
    mvclight::SHA256D(raw.data(), raw.size(), hash);
    mvclight::uint256 txid(std::vector<uint8_t>(hash, hash + 32));

    if (!ctx->impl.txpool.Enqueue(txid)) {
        return StrDup(txid.GetHex()); // 重复，仍返回 txid
    }
    if (ctx->impl.running) {
        ctx->impl.peer_conn.SendMessage("tx", raw);
        ctx->impl.send_monitor.RecordSent(txid, 0); // Phase 4 简化：now=0 由外部 CheckTimeouts 注入
        EmitSendResult(&ctx->impl, txid.GetHex(), 0, "");
    }
    return StrDup(txid.GetHex());
}

char* mvc_light_sync_status(mvc_light_ctx* ctx) {
    if (ctx == nullptr) return nullptr;
    std::string json = "{\"running\":" + std::string(ctx->impl.running ? "true" : "false") +
                       ",\"peer_state\":\"" + mvclight::ToString(ctx->impl.peer_conn.GetState()) +
                       "\",\"watch_count\":" + std::to_string(ctx->impl.watch_addresses.size()) +
                       ",\"tx_count\":" + std::to_string(ctx->impl.store.TxCount()) +
                       ",\"checkpoint_height\":0,\"start_height\":-1}";
    return StrDup(json);
}

int mvc_light_force_reset_chain(mvc_light_ctx* ctx) {
    if (ctx == nullptr) return MVC_LIGHT_ERR_PARAM_INVALID;
    ctx->impl.store = mvclight::CLightWatchStore();
    return MVC_LIGHT_OK;
}

int mvc_light_is_running(mvc_light_ctx* ctx) {
    if (ctx == nullptr) return 0;
    return ctx->impl.running ? 1 : 0;
}

int mvc_light_get_peer_state(mvc_light_ctx* ctx, char* buf, size_t len) {
    if (ctx == nullptr || buf == nullptr || len == 0) return MVC_LIGHT_ERR_PARAM_INVALID;
    const char* state = mvclight::ToString(ctx->impl.peer_conn.GetState());
    size_t n = strlen(state) + 1;
    if (len < n) return MVC_LIGHT_ERR_PARAM_INVALID;
    memcpy(buf, state, n);
    return 0;
}

void mvc_light_free_string(char* s) {
    free(s);
}

} // extern "C"
