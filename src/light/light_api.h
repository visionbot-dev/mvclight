#ifndef MVC_LIGHT_LIGHT_API_H
#define MVC_LIGHT_LIGHT_API_H

/*
 * SDK 入口层（Phase 4：接入 peer / watchstore / txpool / send monitor）。
 */

#include "light/light_filter.h"
#include "light/light_peer.h"
#include "light/light_sendmonitor.h"
#include "light/light_txpool.h"
#include "light/light_watchstore.h"
#include "light/mvc_light.h"

#include <string>
#include <vector>

namespace mvclight {

struct LightContext {
    bool initialized = false;
    bool running = false;
    std::string network;
    std::string peer;
    std::string store_path;

    mvc_light_config callbacks{}; // 保存回调与 user_data
    CLightPeer peer_conn;
    CLightWatchStore store;
    CLightTxPool txpool;
    CLightSendMonitor send_monitor;
    std::vector<std::string> watch_addresses;
};

} // namespace mvclight

#endif // MVC_LIGHT_LIGHT_API_H
