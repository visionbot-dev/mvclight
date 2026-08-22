#ifndef MVC_LIGHT_LIGHT_PEER_POLICY_H
#define MVC_LIGHT_LIGHT_PEER_POLICY_H

/*
 * P2P 防拉黑策略（参考上游 net/net_processing / bloom / validation 常量）。
 *
 * 只读研究上游后自研，绝不链接 net_processing.cpp 等黑名单模块。
 * 关键上游依据：
 *   - DEFAULT_BANSCORE_THRESHOLD = 100（validation.h:184）
 *   - MAX_HEADERS_RESULTS = 2000（validation.h:123）
 *   - PING_INTERVAL = 120s（net/net.h:78）
 *   - MAX_BLOOM_FILTER_SIZE = 36000 / MAX_HASH_FUNCS = 50（bloom.h:18-19）
 * 本模块采用更保守的 BIP-37 常量：20000 元素 / 32768 字节。
 */

#include <cstddef>
#include <cstdint>

namespace mvclight {

class CLightPeerPolicy {
public:
    static constexpr int64_t kGetHeadersMinIntervalMs = 100;
    static constexpr int64_t kFilterReloadMinIntervalMs = 5000;
    static constexpr size_t kMaxBloomElements = 20000;
    static constexpr size_t kMaxBloomBytes = 32768;
    static constexpr uint8_t kMaxBloomHashFuncs = 50;
    static constexpr int64_t kPingIntervalMs = 120000;
    static constexpr int kBanScoreThreshold = 100;
    static constexpr int kMaxGetHeadersBatch = 2000;

    // getheaders 限速：同一窗口内只允许一批
    bool AllowGetHeaders(int64_t now_ms);

    // FILTERLOAD 整体重建限速：避免频繁重建过滤器
    bool AllowFilterReload(int64_t now_ms);

    // Bloom 过滤器参数预检（超限拒绝，避免被节点断开/拉黑）
    bool IsValidBloom(size_t elements, size_t bytes, uint8_t hash_funcs,
                      uint8_t flags) const;

private:
    int64_t m_last_getheaders_ms = -1;
    int64_t m_last_filter_reload_ms = -1;
};

} // namespace mvclight

#endif // MVC_LIGHT_LIGHT_PEER_POLICY_H
