#include "light/light_connman.h"

#include "test_framework.h"

using mvclight::CLightConnMan;

int main() {
    CLightConnMan cm(100, 100);

    // 候选添加
    CHECK(cm.AddPeer("127.0.0.1", 1));
    CHECK(cm.AddPeer("127.0.0.1", 2));
    CHECK(!cm.AddPeer("", 0));
    CHECK(cm.Count() == 2);

    // 未连接
    CHECK(!cm.IsConnected(0));
    CHECK(cm.Peer(0) == nullptr); // 尚未创建连接对象
    CHECK(cm.PeerName(0) == "127.0.0.1:1");

    // 连接失败（端口不可达）不应崩溃；失败后 peer 对象已创建
    CHECK(!cm.ConnectPeer(0));
    CHECK(!cm.IsConnected(0));
    CHECK(cm.Peer(0) != nullptr);

    // 断开未连接 peer 不应崩溃
    cm.DisconnectPeer(0);
    cm.DisconnectPeer(99); // 越界安全

    // 提升未连接 peer 失败
    CHECK(!cm.PromoteToPrimary(1));

    TEST_MAIN_RETURN();
}
