#include "light/light_peer_policy.h"

namespace mvclight {

bool CLightPeerPolicy::AllowGetHeaders(int64_t now_ms) {
    if (m_last_getheaders_ms < 0 ||
        now_ms - m_last_getheaders_ms >= kGetHeadersMinIntervalMs) {
        m_last_getheaders_ms = now_ms;
        return true;
    }
    return false;
}

bool CLightPeerPolicy::AllowFilterReload(int64_t now_ms) {
    if (m_last_filter_reload_ms < 0 ||
        now_ms - m_last_filter_reload_ms >= kFilterReloadMinIntervalMs) {
        m_last_filter_reload_ms = now_ms;
        return true;
    }
    return false;
}

bool CLightPeerPolicy::IsValidBloom(size_t elements, size_t bytes,
                                    uint8_t hash_funcs, uint8_t flags) const {
    if (elements > kMaxBloomElements) return false;
    if (bytes > kMaxBloomBytes) return false;
    if (hash_funcs > kMaxBloomHashFuncs) return false;
    // BIP-37 flags：0=BLOOM_UPDATE_NONE, 1=ALL, 2=ALL 的节点兼容值；其余拒绝
    if (flags > 2) return false;
    return true;
}

} // namespace mvclight
