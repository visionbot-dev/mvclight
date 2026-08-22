#ifndef MVC_LIGHT_LIGHT_VALIDATION_H
#define MVC_LIGHT_LIGHT_VALIDATION_H

/*
 * Header 链验证（设计文档 §4.3）。
 *
 * Phase 2 实现检查项：
 *   1. PoW（hash <= target）
 *   2. prev 哈希连续
 *   3. 时间戳 > 前块时间（简化 MTP）
 *   4. 时间戳 <= 调整后当前时间 + 2h
 *   5. nVersion >= 4（简化）
 *   6. 两阶段：历史段（<= Checkpoint）只校验 prev 连续性 + Checkpoint 累计工作量；
 *      新区段（> Checkpoint）执行全量检查
 *
 * TODO(后续)：ASERT 难度连续性、真实 MTP、BIP34/65/66/113 高度阈值。
 */

#include "light/light_chainstore.h"
#include "light/light_checkpoints.h"
#include "light/light_header.h"

#include <string>

namespace mvclight {

// 校验单个 header；prev 为前一高度 header（可为 nullptr）
// mtp >= 0 时使用真实 11 块中位时间戳校验 time-too-old；否则回退 prev->nTime（简化）
bool ValidateHeader(const LightBlockHeader& h, const LightBlockHeader* prev,
                    int64_t height, bool historical_segment, int64_t adjusted_time_now,
                    std::string& reason, int64_t mtp = -1);

// 校验 Checkpoint 累计工作量（及可选的 Checkpoint 哈希锚定）
bool CheckCheckpoint(const CLightChainStore& store, const LightCheckpoint& cp);

} // namespace mvclight

#endif // MVC_LIGHT_LIGHT_VALIDATION_H
