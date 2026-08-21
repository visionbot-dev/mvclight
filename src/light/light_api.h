#ifndef MVC_LIGHT_LIGHT_API_H
#define MVC_LIGHT_LIGHT_API_H

/*
 * SDK 入口层（Phase 0 骨架）。
 * 后续 Phase 将在此接入 light_peer / light_sync / light_watchstore 等模块。
 */

#include "light/mvc_light.h"

namespace mvclight {

struct LightContext {
    bool initialized = false;
    // Phase 4 起填充：peer / sync / watchstore / callback dispatcher
};

} // namespace mvclight

#endif // MVC_LIGHT_LIGHT_API_H
