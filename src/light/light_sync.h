#ifndef MVC_LIGHT_LIGHT_SYNC_H
#define MVC_LIGHT_LIGHT_SYNC_H

/*
 * Header/交易同步编排（Phase 2：Header 同步）。
 *
 * 流程（设计文档 §4.1.1 / §4.3）：
 *   WAIT_FILTER_ACK -> SYNCING_HEADERS -> SYNCING_TXS
 *   - 首个 HEADERS 之前收到的 INV/MERKLEBLOCK 一律丢弃
 *   - 30s 无 HEADERS 由 socket recv timeout 触发重连（Phase 1 已实现超时）
 */

#include "light/light_checkpoints.h"
#include "light/light_chainstore.h"
#include "light/light_peer.h"

#include <cstdint>
#include <string>

namespace mvclight {

class CLightSync {
public:
    // 构造 GETHEADERS 并发送；locator 优先使用 store tip，否则使用 Checkpoint
    static bool SendGetHeaders(CLightPeer& peer, const LightCheckpoint& cp,
                               const CLightChainStore& store);

    // 读取并处理 HEADERS 消息；返回成功处理的 header 数量，失败返回 -1
    static int ProcessHeaders(CLightPeer& peer, CLightChainStore& store,
                              const LightCheckpoint& cp, int64_t start_height,
                              int64_t adjusted_time_now, std::string& last_reason);
};

} // namespace mvclight

#endif // MVC_LIGHT_LIGHT_SYNC_H
