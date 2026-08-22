// mvclight Windows 原生桌面 Demo（不依赖 Web/第三方 GUI）
//
// 功能：
//   1. 连接真实 MVC 主网种子，握手
//   2. getheaders/headers 同步，实时显示进度
//   3. 内置 Checkpoint 锚定展示
//   4. Watch 地址管理（演示交易入库）
//   5. 日志面板（SDK 回调 + P2P 消息流）
//
// 自检模式：demo_mvclight.exe --selftest
//   不创建窗口，执行连接+同步核心路径，写入 demo_selftest.log
//   成功时日志包含 VISUAL_DEMO_READY

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>
#include <commctrl.h>

// Win32 的 SendMessage 宏会误展开 CLightPeer::SendMessage，这里取消宏并显式使用 SendMessageA
#undef SendMessage

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "light/light_chainstore.h"
#include "light/light_checkpoints.h"
#include "light/light_header.h"
#include "light/light_message.h"
#include "light/light_peer.h"
#include "light/light_validation.h"
#include "light/light_watchstore.h"
#include "light/mvc_light.h"

using mvclight::CLightChainStore;
using mvclight::CLightPeer;
using mvclight::CLightWatchStore;
using mvclight::CheckCheckpoint;
using mvclight::GetBuiltinCheckpoint;
using mvclight::LightBlockHeader;
using mvclight::LightMessage;
using mvclight::MainnetMagic;
using mvclight::TxRecord;
using mvclight::ValidateHeader;
using mvclight::uint256;
using mvclight::uint256S;

namespace {

constexpr int kMaxBatches = 64;
constexpr int kCheckpointHeight = 21256;
constexpr const char* kDefaultPeer = "47.242.24.63:9883";

// ---- 控件 ID ----
constexpr int IDC_PEER_EDIT = 101;
constexpr int IDC_CONNECT_BTN = 102;
constexpr int IDC_DISCONNECT_BTN = 103;
constexpr int IDC_STATE_TEXT = 104;
constexpr int IDC_HEIGHT_TEXT = 105;
constexpr int IDC_HEADER_TEXT = 106;
constexpr int IDC_CP_TEXT = 107;
constexpr int IDC_PROGRESS = 108;
constexpr int IDC_WATCH_EDIT = 109;
constexpr int IDC_WATCH_ADD = 110;
constexpr int IDC_WATCH_DEL = 111;
constexpr int IDC_WATCH_LIST = 112;
constexpr int IDC_TX_LIST = 113;
constexpr int IDC_LOG_EDIT = 114;
constexpr int IDC_RESET_BTN = 115;
constexpr int IDC_CLEARLOG_BTN = 116;
constexpr int WM_APP_STATE = WM_APP + 1;

struct TxItem {
    std::string txid;
    std::string block_hash;
    int64_t height = -1;
    bool verified = false;
};

struct DemoState {
    std::mutex mu;
    bool connected = false;
    bool syncing = false;
    std::string peer_state = "INIT";
    int64_t synced_height = -1;
    std::string latest_hash;
    uint32_t latest_time = 0;
    uint32_t latest_bits = 0;
    bool checkpoint_ok = false;
    std::string last_error;
    std::vector<std::string> logs;
    std::vector<std::string> watch_addresses;
    std::vector<TxItem> txs;
    bool dirty = false;
};

std::atomic<bool> g_stop{false};
std::atomic<bool> g_worker_running{false};
std::thread g_worker;
DemoState g_state;
CLightWatchStore g_store;
HWND g_hwnd = nullptr;

void AppendLog(DemoState& s, const std::string& line) {
    std::lock_guard<std::mutex> lock(s.mu);
    if (s.logs.size() > 2000) s.logs.erase(s.logs.begin());
    s.logs.push_back(line);
    s.dirty = true;
}

void AppendLogNoLock(DemoState& s, const std::string& line) {
    if (s.logs.size() > 2000) s.logs.erase(s.logs.begin());
    s.logs.push_back(line);
    s.dirty = true;
}

void AppendLE32(std::vector<uint8_t>& out, uint32_t v) {
    for (int i = 0; i < 4; ++i) out.push_back(static_cast<uint8_t>(v >> (i * 8)));
}

void AppendCompactSize(std::vector<uint8_t>& out, uint64_t v) {
    if (v < 253) {
        out.push_back(static_cast<uint8_t>(v));
    } else {
        out.push_back(253);
        out.push_back(static_cast<uint8_t>(v));
        out.push_back(static_cast<uint8_t>(v >> 8));
    }
}

uint64_t ReadCompactSize(const uint8_t*& p, const uint8_t* end, bool& ok) {
    if (p >= end) { ok = false; return 0; }
    uint8_t first = *p++;
    if (first < 253) return first;
    if (first == 253) {
        if (p + 2 > end) { ok = false; return 0; }
        uint64_t v = p[0] | (uint64_t(p[1]) << 8);
        p += 2;
        return v;
    }
    if (first == 254) {
        if (p + 4 > end) { ok = false; return 0; }
        uint64_t v = 0;
        for (int i = 0; i < 4; ++i) v |= uint64_t(p[i]) << (i * 8);
        p += 4;
        return v;
    }
    if (p + 8 > end) { ok = false; return 0; }
    uint64_t v = 0;
    for (int i = 0; i < 8; ++i) v |= uint64_t(p[i]) << (i * 8);
    p += 8;
    return v;
}

std::vector<uint8_t> MakeGetHeaders(const uint256& locator) {
    std::vector<uint8_t> p;
    AppendLE32(p, 70016);
    AppendCompactSize(p, 1);
    p.insert(p.end(), locator.begin(), locator.end());
    p.insert(p.end(), 32, 0); // hashstop = 0
    return p;
}

bool SplitHostPort(const std::string& peer, std::string& host, int& port) {
    auto pos = peer.rfind(':');
    if (pos == std::string::npos || pos + 1 >= peer.size()) return false;
    host = peer.substr(0, pos);
    port = atoi(peer.c_str() + pos + 1);
    return port > 0;
}

std::string kDefaultPeerHost();
int kDefaultPeerPort();

// 从 demo 地址生成一个稳定 txid（演示用）
uint256 DemoTxidForAddr(const std::string& addr) {
    uint256 txid;
    auto* p = txid.begin();
    size_t i = 0;
    for (char c : addr) {
        p[i % 32] ^= static_cast<uint8_t>(c);
        ++i;
    }
    p[0] ^= 0xAA;
    p[1] ^= 0x55;
    return txid;
}

void CommitDemoTx(const std::string& addr) {
    TxRecord rec;
    rec.txid = DemoTxidForAddr(addr);
    rec.height = kCheckpointHeight + 1;
    rec.block_hash = GetBuiltinCheckpoint().hash;
    rec.tx_blob = {1, 2, 3};
    rec.script_verified = true;
    if (g_store.CommitTx(addr, rec)) {
        std::lock_guard<std::mutex> lock(g_state.mu);
        g_state.txs.clear();
        std::vector<mvclight::uint256> txids;
        g_store.GetTxidsByAddr(addr, txids);
        for (const auto& txid : txids) {
            TxRecord out;
            if (g_store.GetTx(txid, out)) {
                TxItem item;
                item.txid = txid.GetHex();
                item.block_hash = out.block_hash.GetHex();
                item.height = out.height;
                item.verified = out.script_verified;
                g_state.txs.push_back(item);
            }
        }
        g_state.dirty = true;
    }
}

// 核心同步逻辑（GUI 工作线程与 --selftest 共用）
bool RunSyncCore(const std::string& host, int port, std::string& err_out) {
    CLightPeer peer;
    AppendLog(g_state, "[sync] connecting to " + host + ":" + std::to_string(port));
    if (!peer.ConnectAndHandshake(host, port, 10000, 10000)) {
        err_out = "connect/handshake failed";
        AppendLog(g_state, "[sync] " + err_out);
        return false;
    }
    {
        std::lock_guard<std::mutex> lock(g_state.mu);
        g_state.connected = true;
        g_state.syncing = true;
        g_state.peer_state = mvclight::ToString(peer.GetState());
        g_state.dirty = true;
    }
    AppendLog(g_state, "[sync] MAINNET_HANDSHAKE_OK");

    CLightChainStore chain;
    uint256 locator;
    int64_t last_batch_height = -1;
    bool reached = false;

    for (int batch = 0; batch < kMaxBatches && !g_stop.load(); ++batch) {
        std::vector<uint8_t> getheaders = MakeGetHeaders(locator);
        if (!(peer.SendMessage)("getheaders", getheaders)) {
            err_out = "send getheaders failed";
            AppendLog(g_state, "[sync] " + err_out);
            break;
        }
        bool got = false;
        for (int attempt = 0; attempt < 60; ++attempt) {
            if (g_stop.load()) break;
            LightMessage msg;
            if (!peer.ReadMessage(msg)) {
                err_out = "read failed";
                AppendLog(g_state, "[sync] " + err_out);
                break;
            }
            if (msg.command != "headers") continue;
            const uint8_t* p = msg.payload.data();
            const uint8_t* end = p + msg.payload.size();
            bool ok = true;
            uint64_t count = ReadCompactSize(p, end, ok);
            if (!ok || count == 0 || count > 2000 ||
                p + count * (LightBlockHeader::kHeaderSize + 1) > end) {
                err_out = "bad headers format";
                AppendLog(g_state, "[sync] " + err_out);
                break;
            }
            int64_t processed = 0;
            for (uint64_t i = 0; i < count; ++i) {
                const uint8_t* hp = p + i * (LightBlockHeader::kHeaderSize + 1);
                LightBlockHeader h;
                if (!h.Deserialize(hp, LightBlockHeader::kHeaderSize)) {
                    err_out = "bad header deserialize";
                    break;
                }
                if (hp[LightBlockHeader::kHeaderSize] != 0) {
                    err_out = "bad headers txcount";
                    break;
                }
                int64_t height = chain.TipHeight() + 1;
                LightBlockHeader prev;
                const LightBlockHeader* prev_ptr = nullptr;
                if (height > 0 && chain.GetHeaderAtHeight(height - 1, prev)) {
                    prev_ptr = &prev;
                }
                std::string reason;
                int64_t mtp = -1;
                if (prev_ptr != nullptr) {
                    mtp = chain.GetMedianTimePast(height - 1);
                }
                // ≤ Checkpoint 为历史段：只验 prev 连续（+ Checkpoint 哈希/工作量锚定）
                bool historical = (height <= kCheckpointHeight);
                if (!ValidateHeader(h, prev_ptr, height, historical,
                                    static_cast<int64_t>(std::time(nullptr)), reason, mtp)) {
                    err_out = "bad header " + reason;
                    AppendLog(g_state, "[sync] bad header at " + std::to_string(height) +
                              ": " + reason);
                    break;
                }
                chain.AddHeader(h, height);
                chain.AddWork(h.nBits);
                ++processed;
                {
                    std::lock_guard<std::mutex> lock(g_state.mu);
                    g_state.synced_height = height;
                    g_state.latest_hash = h.GetHash().GetHex();
                    g_state.latest_time = h.nTime;
                    g_state.latest_bits = h.nBits;
                    g_state.dirty = true;
                }
                if (height == kCheckpointHeight) {
                    reached = true;
                }
            }
            got = true;
            {
                LightBlockHeader tip_hdr;
                if (!chain.GetTip(tip_hdr)) break;
                locator = tip_hdr.GetHash();
            }
            AppendLog(g_state, "[sync] batch " + std::to_string(batch) + " headers=" +
                      std::to_string(chain.TipHeight()) + " tip=" +
                      (locator.IsNull() ? std::string("-") : locator.GetHex()));
            last_batch_height = chain.TipHeight();
            break;
        }
        if (!got) break;
        if (reached || chain.TipHeight() >= kCheckpointHeight) {
            if (CheckCheckpoint(chain, GetBuiltinCheckpoint())) {
                std::lock_guard<std::mutex> lock(g_state.mu);
                g_state.checkpoint_ok = true;
                g_state.dirty = true;
            }
            AppendLog(g_state, reached ? "[sync] CHECKPOINT_ANCHORED" :
                      "[sync] checkpoint chainwork ok");
        }
        if (chain.TipHeight() >= kCheckpointHeight && reached) {
            break;
        }
    }

    {
        std::lock_guard<std::mutex> lock(g_state.mu);
        g_state.syncing = false;
        g_state.connected = false;
        g_state.peer_state = "DISCONNECTED";
        g_state.dirty = true;
    }
    peer.Disconnect();

    bool ok = reached && g_state.checkpoint_ok;
    if (!ok && err_out.empty()) {
        err_out = "did not reach checkpoint";
    }
    AppendLog(g_state, ok ? "[sync] MAINNET_E2E_OK" : "[sync] FAILED: " + err_out);
    return ok;
}

void WorkerMain(const std::string& host, int port) {
    g_worker_running = true;
    std::string err;
    RunSyncCore(host, port, err);
    g_worker_running = false;
    g_stop.store(false);
    if (g_hwnd) PostMessage(g_hwnd, WM_APP_STATE, 0, 0);
}

// ---- UI 辅助 ----

void SetText(HWND h, int id, const std::string& s) {
    if (h) SetDlgItemText(h, id, s.c_str());
}

void RefreshUI(HWND hwnd) {
    std::lock_guard<std::mutex> lock(g_state.mu);
    SetText(hwnd, IDC_STATE_TEXT, "Peer: " + g_state.peer_state +
            (g_state.connected ? " [connected]" : " [disconnected]"));
    SetText(hwnd, IDC_HEIGHT_TEXT, "Synced height: " +
            std::to_string(g_state.synced_height));
    SetText(hwnd, IDC_HEADER_TEXT, "Latest: " + g_state.latest_hash +
            " time=" + std::to_string(g_state.latest_time) +
            " bits=0x" + [&]() {
                char buf[16];
                snprintf(buf, sizeof(buf), "%08x", g_state.latest_bits);
                return std::string(buf);
            }());
    const auto& cp = GetBuiltinCheckpoint();
    SetText(hwnd, IDC_CP_TEXT, std::string("Checkpoint: h=") + std::to_string(cp.height) +
            " " + cp.hash.GetHex() + (g_state.checkpoint_ok ? " [ANCHORED]" : " [pending]"));

    HWND wl = GetDlgItem(hwnd, IDC_WATCH_LIST);
    SendMessageA(wl, LB_RESETCONTENT, 0, 0);
    for (const auto& a : g_state.watch_addresses) {
        SendMessageA(wl, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(a.c_str()));
    }

    HWND tl = GetDlgItem(hwnd, IDC_TX_LIST);
    SendMessageA(tl, LVM_DELETEALLITEMS, 0, 0);
    for (size_t i = 0; i < g_state.txs.size(); ++i) {
        const auto& t = g_state.txs[i];
        char height[32];
        snprintf(height, sizeof(height), "%lld", static_cast<long long>(t.height));
        LVITEMA item{};
        item.mask = LVIF_TEXT;
        item.iItem = static_cast<int>(i);
        item.pszText = const_cast<char*>(t.txid.c_str());
        SendMessageA(tl, LVM_INSERTITEMA, 0, reinterpret_cast<LPARAM>(&item));

        auto set_sub = [&](int col, const char* text) {
            LVITEMA sub{};
            sub.mask = LVIF_TEXT;
            sub.iItem = static_cast<int>(i);
            sub.iSubItem = col;
            sub.pszText = const_cast<char*>(text);
            SendMessageA(tl, LVM_SETITEMTEXTA, static_cast<WPARAM>(i),
                         reinterpret_cast<LPARAM>(&sub));
        };
        set_sub(1, height);
        set_sub(2, t.block_hash.c_str());
        set_sub(3, t.verified ? "true" : "false");
    }

    HWND le = GetDlgItem(hwnd, IDC_LOG_EDIT);
    std::string all;
    for (const auto& l : g_state.logs) all += l + "\r\n";
    SetWindowTextA(le, all.c_str());
    SendMessageA(le, EM_SETSEL, static_cast<WPARAM>(-1), static_cast<LPARAM>(-1));
    SendMessageA(le, EM_SCROLLCARET, 0, 0);
    g_state.dirty = false;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        g_hwnd = hwnd;
        HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        auto mk = [&](const char* cls, const char* text, DWORD style, int x, int y,
                      int w, int h, int id) {
            HWND c = CreateWindowA(cls, text, WS_CHILD | WS_VISIBLE | style,
                                   x, y, w, h, hwnd,
                                   reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                                   GetModuleHandle(nullptr), nullptr);
            if (font) SendMessageA(c, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
            return c;
        };
        mk("STATIC", "Peer:", 0, 10, 12, 60, 20, 0);
        mk("EDIT", kDefaultPeer, WS_BORDER | ES_AUTOHSCROLL, 70, 10, 180, 22, IDC_PEER_EDIT);
        mk("BUTTON", "Connect", BS_PUSHBUTTON, 260, 10, 80, 24, IDC_CONNECT_BTN);
        mk("BUTTON", "Disconnect", BS_PUSHBUTTON, 345, 10, 90, 24, IDC_DISCONNECT_BTN);
        mk("BUTTON", "Reset", BS_PUSHBUTTON, 440, 10, 70, 24, IDC_RESET_BTN);
        mk("BUTTON", "Clear Log", BS_PUSHBUTTON, 515, 10, 80, 24, IDC_CLEARLOG_BTN);

        mk("STATIC", "Status:", 0, 10, 45, 60, 20, 0);
        mk("STATIC", "", SS_SUNKEN, 70, 45, 525, 20, IDC_STATE_TEXT);
        mk("STATIC", "", SS_SUNKEN, 10, 75, 585, 20, IDC_HEIGHT_TEXT);
        mk("STATIC", "", SS_SUNKEN, 10, 100, 585, 20, IDC_HEADER_TEXT);
        mk("STATIC", "", SS_SUNKEN, 10, 125, 585, 20, IDC_CP_TEXT);
        mk("PROGRESS_CLASS", "", 0, 10, 150, 585, 18, IDC_PROGRESS);

        mk("STATIC", "Watch address:", 0, 10, 180, 100, 20, 0);
        mk("EDIT", "", WS_BORDER | ES_AUTOHSCROLL, 110, 180, 220, 22, IDC_WATCH_EDIT);
        mk("BUTTON", "Add", BS_PUSHBUTTON, 340, 180, 60, 24, IDC_WATCH_ADD);
        mk("BUTTON", "Remove", BS_PUSHBUTTON, 405, 180, 70, 24, IDC_WATCH_DEL);
        mk("LISTBOX", "", WS_BORDER | WS_VSCROLL, 10, 210, 265, 120, IDC_WATCH_LIST);

        mk("STATIC", "Transactions:", 0, 285, 300, 100, 20, 0);
        HWND tx = mk("SysListView32", "", WS_BORDER | LVS_REPORT | LVS_SINGLESEL,
                     10, 305, 585, 130, IDC_TX_LIST);
        LVCOLUMNA col{};
        col.mask = LVCF_TEXT | LVCF_WIDTH;
        col.pszText = const_cast<char*>("txid");
        col.cx = 220;
        SendMessageA(tx, LVM_INSERTCOLUMNA, 0, reinterpret_cast<LPARAM>(&col));
        col.pszText = const_cast<char*>("height");
        col.cx = 70;
        SendMessageA(tx, LVM_INSERTCOLUMNA, 1, reinterpret_cast<LPARAM>(&col));
        col.pszText = const_cast<char*>("block_hash");
        col.cx = 220;
        SendMessageA(tx, LVM_INSERTCOLUMNA, 2, reinterpret_cast<LPARAM>(&col));
        col.pszText = const_cast<char*>("script_verified");
        col.cx = 80;
        SendMessageA(tx, LVM_INSERTCOLUMNA, 3, reinterpret_cast<LPARAM>(&col));

        mk("EDIT", "", WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY |
           WS_VSCROLL, 10, 445, 585, 160, IDC_LOG_EDIT);
        SetTimer(hwnd, 1, 200, nullptr);
        return 0;
    }
    case WM_COMMAND: {
        int id = LOWORD(wParam);
        if (id == IDC_CONNECT_BTN) {
            if (g_worker_running.load()) break;
            char buf[256];
            GetDlgItemTextA(hwnd, IDC_PEER_EDIT, buf, sizeof(buf));
            std::string host;
            int port = 0;
            if (!SplitHostPort(buf, host, port)) {
                AppendLog(g_state, "[ui] invalid peer address");
                break;
            }
            g_stop.store(false);
            {
                std::lock_guard<std::mutex> lock(g_state.mu);
                g_state.checkpoint_ok = false;
                g_state.dirty = true;
            }
            g_worker = std::thread([host, port]() { WorkerMain(host, port); });
            AppendLog(g_state, "[ui] connect requested " + std::string(buf));
        } else if (id == IDC_DISCONNECT_BTN) {
            g_stop.store(true);
            AppendLog(g_state, "[ui] disconnect requested");
        } else if (id == IDC_WATCH_ADD) {
            char buf[256];
            GetDlgItemTextA(hwnd, IDC_WATCH_EDIT, buf, sizeof(buf));
            std::string addr = buf;
            if (!addr.empty()) {
                std::lock_guard<std::mutex> lock(g_state.mu);
                g_state.watch_addresses.push_back(addr);
                g_state.dirty = true;
            }
            CommitDemoTx(addr);
            AppendLog(g_state, "[ui] watch add " + addr);
        } else if (id == IDC_WATCH_DEL) {
            HWND wl = GetDlgItem(hwnd, IDC_WATCH_LIST);
            int sel = static_cast<int>(SendMessageA(wl, LB_GETCURSEL, 0, 0));
            if (sel != LB_ERR) {
                std::lock_guard<std::mutex> lock(g_state.mu);
                if (sel < static_cast<int>(g_state.watch_addresses.size())) {
                    g_state.watch_addresses.erase(
                        g_state.watch_addresses.begin() + sel);
                    g_state.dirty = true;
                }
            }
            AppendLog(g_state, "[ui] watch remove");
        } else if (id == IDC_RESET_BTN) {
            g_store.ClearMemory();
            {
                std::lock_guard<std::mutex> lock(g_state.mu);
                g_state.txs.clear();
                g_state.synced_height = -1;
                g_state.checkpoint_ok = false;
                g_state.dirty = true;
            }
            AppendLog(g_state, "[ui] force_reset_chain");
        } else if (id == IDC_CLEARLOG_BTN) {
            std::lock_guard<std::mutex> lock(g_state.mu);
            g_state.logs.clear();
            g_state.dirty = true;
        }
        return 0;
    }
    case WM_TIMER:
        if (wParam == 1) {
            bool dirty = false;
            bool syncing = false;
            {
                std::lock_guard<std::mutex> lock(g_state.mu);
                dirty = g_state.dirty;
                syncing = g_state.syncing;
                g_state.dirty = false;
            }
            if (dirty || syncing) {
                RefreshUI(hwnd);
            }
            HWND pb = GetDlgItem(hwnd, IDC_PROGRESS);
            SendMessageA(pb, PBM_SETMARQUEE, syncing ? TRUE : FALSE, 30);
        }
        return 0;
    case WM_APP_STATE:
        RefreshUI(hwnd);
        return 0;
    case WM_DESTROY:
        KillTimer(hwnd, 1);
        g_stop.store(true);
        if (g_worker.joinable()) g_worker.join();
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcA(hwnd, msg, wParam, lParam);
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

void InitCommon() {
    INITCOMMONCONTROLSEX icc{};
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_PROGRESS_CLASS | ICC_LISTVIEW_CLASSES;
    InitCommonControlsEx(&icc);
}

int SelftestMain() {
    std::ofstream log("demo_selftest.log");
    auto line = [&](const std::string& s) {
        log << s << std::endl;
        AppendLog(g_state, s);
    };

    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        line("WSAStartup FAILED");
        return 1;
    }

    std::string err;
    bool ok = RunSyncCore(kDefaultPeerHost(), kDefaultPeerPort(), err);
    WSACleanup();

    if (ok) {
        line("VISUAL_DEMO_READY");
        log.close();
        return 0;
    }
    line("SELFTEST FAILED: " + err);
    log.close();
    return 1;
}

std::string kDefaultPeerHost() {
    std::string host;
    int port = 0;
    SplitHostPort(kDefaultPeer, host, port);
    return host;
}

int kDefaultPeerPort() {
    std::string host;
    int port = 0;
    SplitHostPort(kDefaultPeer, host, port);
    return port;
}

} // namespace

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR cmd, int nShow) {
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);

    if (cmd && strstr(cmd, "--selftest")) {
        int rc = SelftestMain();
        WSACleanup();
        return rc;
    }

    InitCommon();

    WNDCLASSA wc{};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = "MvcLightDemoWnd";
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    RegisterClassA(&wc);

    HWND hwnd = CreateWindowA(wc.lpszClassName, "MVCLight Light Node Demo",
                              WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                              620, 660, nullptr, nullptr, hInst, nullptr);
    if (!hwnd) {
        WSACleanup();
        return 1;
    }
    ShowWindow(hwnd, nShow);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessageA(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    WSACleanup();
    return static_cast<int>(msg.wParam);
}
