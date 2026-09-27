// vb6forms_winsock.c — ai/029 C29-WS-a: VB6 Winsock 控件的原生落点（Winsock2）第一档
//
// 本格范围 = 控件身份（不可见窗 + **真在收通知的 WndProc**）+ 实例表 + 事件槽 + 状态与属性面
//   + **UDP 一整轮**（Bind / SendData / GetData / PeekData / Close + DataArrival / StateChanged）。
// TCP 的 Listen / Accept / Connect 留 WS-b（服务端与客户端互为前提，拆不开就一起做），
// Error / SendProgress / SendComplete / Byte 数组那一形 GetData 留 WS-c。
//
// 三条决定这里形状的测量（理由与读数在 vb6forms_winsock.h 头上与 029 §九）：
//   ① 事件种类读消息 lParam 低字，不读 WSAEnumNetworkEvents；
//   ② FD_CLOSE 会带着未读的尾巴到 ⇒ 先 drain 再发 Close；
//   ③ 还剩数据没读时 Winsock 自己会重投 FD_READ ⇒ RTL 一次读到干净，用户不调 GetData
//      也不会被反复打扰（老控件那条"DataArrival 里必须 GetData"的坑就填在这里）。

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
// winsock2.h 必须在 windows.h 之前（否则 windows.h 先把 v1 的 winsock.h 拽进来，两套
// fd_set / TIMEVAL 打架）。同仓库的先例是 src/rtl/core/di/vb6_di_net_stubs.c。
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#endif

#include "vb6forms.h"
#include "vb6forms_internal.h"
#include <oleauto.h>   /* SysAllocString / SysFreeString / SysReAllocStringLen */
#include <string.h>
#include <wchar.h>
#include <stdlib.h>

#ifdef _WIN32

#pragma comment(lib, "ws2_32.lib")

// WSAAsyncSelect 投的私有消息。仓库里另外两条私有号是 0x7FF0（延迟 Form_Load）与
// 0x7FFF（恢复焦点），都排在 WM_APP 之外；这里用 WM_APP 段，控件自己的窗口只收自己那份通知，
// 所以**一枚号就够**（不需要 per-control 的消息号 —— 实测两枚控件各配各的窗口互不串）。
#define WM_WS_NOTIFY (WM_APP + 0x15)

#define VB6_WS_MAX           64     // 一个工程里的 Winsock 控件上限（含数组展开后的下标）
#define VB6_WS_RX_MAX        (1u << 20)   // 单枚控件接收缓冲的天花板（1 MB），超了就丢新的

typedef void (*vb6_WsCbVoid)(void);
typedef void (*vb6_WsCbLong)(int32_t);          // DataArrival / ConnectionRequest（ByVal As Long）
typedef void (*vb6_WsCbInt)(int16_t);           // StateChanged（ByVal As Integer）

struct vb6_WsInstance {
    HWND     hwnd;                 // 身份窗（也是通知的目标窗）；NULL = 空槽
    SOCKET   sock;
    int32_t  protocol;             // VB6_WS_TCP / VB6_WS_UDP
    int32_t  state;                // VB6_WS_* 那一族
    int32_t  lastErr;              // 最近一次 WSA 错误码（State=sckError 时的编号）
    int32_t  localPort;
    wchar_t  localIP[64];
    wchar_t  remoteHost[256];
    int32_t  remotePort;
    int32_t  family;               // AF_INET / AF_INET6（Bind 或第一次发送时定）
    // 接收缓冲：FD_READ 里一次读干净放这儿，GetData 从这里取（②③ 两条测量的落点）
    char*    rx;
    uint32_t rxHead, rxLen;
    uint32_t rxCap;
    int32_t  bytesReceived;        // 本次事件新到的字节数（VB6 的 bytesTotal 与 BytesReceived）
    int32_t  byteTransferred;
    void*    cb[VB6_WS_EV_COUNT];
};

static struct vb6_WsInstance g_ws[VB6_WS_MAX];
static int g_wsSockStarted = 0;

static void vb6_WsEnsureWsa(void) {
    if (!g_wsSockStarted) {
        WSADATA d;
        if (WSAStartup(MAKEWORD(2, 2), &d) == 0) g_wsSockStarted = 1;
    }
}

static struct vb6_WsInstance* vb6_WsOf(HWND h) {
    int i;
    if (!h) return NULL;
    for (i = 0; i < VB6_WS_MAX; i++) if (g_ws[i].hwnd == h) return &g_ws[i];
    return NULL;
}

static struct vb6_WsInstance* vb6_WsSlot(HWND h) {
    int i;
    for (i = 0; i < VB6_WS_MAX; i++) if (!g_ws[i].hwnd) {
        ZeroMemory(&g_ws[i], sizeof(g_ws[i]));
        g_ws[i].hwnd = h;
        g_ws[i].sock = INVALID_SOCKET;
        g_ws[i].protocol = VB6_WS_TCP;      // VB6 的默认
        g_ws[i].state = VB6_WS_CLOSED;
        return &g_ws[i];
    }
    return NULL;
}

static void vb6_WsSetState(struct vb6_WsInstance* e, int32_t st) {
    if (!e || e->state == st) return;
    e->state = st;
    // 与 Timer 的 winmm 线程同一条纪律：事件只在消息循环里发，且**发完再调**，
    // 处理器里再动这枚控件（Close / SendData）不会踩到半成品状态。
    if (e->cb[VB6_WS_EV_STATECHANGED]) ((vb6_WsCbInt)e->cb[VB6_WS_EV_STATECHANGED])((int16_t)st);
}

// BSTR -> 定长 UTF-16 缓冲（NULL / 空串都当空）
static void vb6_WsCopyBstr(wchar_t* dst, size_t cap, const void* bstr) {
    const wchar_t* s = (const wchar_t*)bstr;
    size_t n;
    dst[0] = 0;
    if (!s) return;
    n = (size_t)SysStringLen((BSTR)s);
    if (n > cap - 1) n = cap - 1;
    if (n) memcpy(dst, s, n * sizeof(wchar_t));
    dst[n] = 0;
}

// 线格式 = ANSI(CP_ACP)，与 VB6 那颗 OCX 一致（String 那一形）。要传非 ASCII 的字节流
// 得用 Byte 数组那一形（WS-c）。
static void vb6_WsRxAppend(struct vb6_WsInstance* e, const char* p, int n) {
    uint32_t need;
    char* grown;
    if (n <= 0) return;
    need = e->rxLen + (uint32_t)n;
    if (need > VB6_WS_RX_MAX) return;                  // 超天花板：丢掉新的，保住已排队的那些
    if (need > e->rxCap) {
        uint32_t cap = e->rxCap ? e->rxCap : 4096u;
        while (cap < need) cap *= 2u;
        grown = (char*)realloc(e->rx, cap);
        if (!grown) return;
        e->rx = grown;
        e->rxCap = cap;
    }
    if (e->rxHead + e->rxLen > e->rxCap) {             // 环形留白不够就整体前移一次
        memmove(e->rx, e->rx + e->rxHead, e->rxLen);
        e->rxHead = 0;
    }
    memcpy(e->rx + e->rxHead + e->rxLen, p, (size_t)n);
    e->rxLen += (uint32_t)n;
}

// UDP 收到东西时把**来包地址**写回 RemoteHost / RemotePort —— 这是 VB6 那颗 OCX 的既有语义
// （"收到之后 RemoteHost 就是发件人"），回信那条路靠它才成立，不是 C3 新发明的口径。
static void vb6_WsRememberPeer(struct vb6_WsInstance* e, struct sockaddr* from, int flen) {
    wchar_t txt[80];
    char  ansi[80];
    ULONG n = sizeof(ansi);
    u_short port = 0;
    int i;
    if (WSAAddressToStringA(from, flen, NULL, ansi, &n) != 0) return;
    for (i = 0; ansi[i] && i < 79; i++) txt[i] = (wchar_t)(unsigned char)ansi[i];
    txt[i] = 0;
    port = (from->sa_family == AF_INET6) ? ntohs(((struct sockaddr_in6*)from)->sin6_port)
                                         : ntohs(((struct sockaddr_in*)from)->sin_port);
    // "127.0.0.1:54241" / "[::1]:54241" → 地址与端口分开存（LocalIP 那处同一截法）
    {
        wchar_t* colon = wcsrchr(txt, L':');
        if (colon) {
            wchar_t* lb = wcsrchr(txt, L'[');
            if (lb && lb < colon) {   // IPv6：[addr]:port —— 取括号里那截，且端口是括号之后的冒号
                wchar_t* rb = wcschr(lb, L']');
                if (rb) { *rb = 0; memmove(txt, lb + 1, (wcslen(lb + 1) + 1) * sizeof(wchar_t)); }
            } else {
                *colon = 0;
            }
        }
    }
    for (i = 0; txt[i] && i < 255; i++) e->remoteHost[i] = txt[i];
    e->remoteHost[i] = 0;
    e->remotePort = (int32_t)port;
}

// 把 FD_READ 之后"还能读的"全部读进缓冲：一次读干净 = ③ 那条坑不再复发。
static void vb6_WsDrain(struct vb6_WsInstance* e) {
    char tmp[8192];
    int got, total = 0;
    for (;;) {
        if (e->protocol == VB6_WS_UDP) {
            struct sockaddr_storage fs;
            int fl = sizeof(fs);
            got = recvfrom(e->sock, tmp, (int)sizeof(tmp), 0, (struct sockaddr*)&fs, &fl);
            if (got > 0) vb6_WsRememberPeer(e, (struct sockaddr*)&fs, fl);
        } else {
            got = recv(e->sock, tmp, (int)sizeof(tmp), 0);
        }
        if (got == 0) { total = -1; break; }
        if (got < 0) {
            int err = WSAGetLastError();
            if (err != WSAEWOULDBLOCK) e->lastErr = err;
            break;
        }
        vb6_WsRxAppend(e, tmp, got);
        total += got;
    }
    if (total > 0) e->bytesReceived = total;
}

static void vb6_WsRaiseData(struct vb6_WsInstance* e) {
    if (!e->cb[VB6_WS_EV_DATAARRIVAL]) return;
    ((vb6_WsCbLong)e->cb[VB6_WS_EV_DATAARRIVAL])(e->bytesReceived);
}

static void vb6_WsRaiseClose(struct vb6_WsInstance* e) {
    vb6_WsSetState(e, VB6_WS_CLOSED);
    if (e->cb[VB6_WS_EV_CLOSE]) ((vb6_WsCbVoid)e->cb[VB6_WS_EV_CLOSE])();
}

// UDP 的"对端"：没 Connect 这一步，发往 RemoteHost:RemotePort；收到谁的算谁的（VB6 同）
static int vb6_WsUdpTarget(struct vb6_WsInstance* e, struct sockaddr_storage* ss, int* slen) {
    ZeroMemory(ss, sizeof(*ss));
    if (e->family == AF_INET6) {
        struct sockaddr_in6* a = (struct sockaddr_in6*)ss;
        a->sin6_family = AF_INET6;
        a->sin6_port = htons((u_short)e->remotePort);          /* 网络字节序 */
        if (e->remoteHost[0] == 0 || wcscmp(e->remoteHost, L"*") == 0) {
            a->sin6_addr = in6addr_any;
        } else if (!InetPtonW(AF_INET6, e->remoteHost, &a->sin6_addr)) {
            return 0;
        }
        *slen = sizeof(struct sockaddr_in6);
        return 1;
    } else {
        struct sockaddr_in* a = (struct sockaddr_in*)ss;
        a->sin_family = AF_INET;
        a->sin_port = htons((u_short)e->remotePort);          /* 网络字节序 */
        if (e->remoteHost[0] == 0 || wcscmp(e->remoteHost, L"*") == 0) {
            a->sin_addr.s_addr = INADDR_ANY;
        } else if (!InetPtonW(AF_INET, e->remoteHost, &a->sin_addr)) {
            return 0;
        }
        *slen = sizeof(struct sockaddr_in);
        return 1;
    }
}

static LRESULT CALLBACK vb6_WsWndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    struct vb6_WsInstance* e;
    if (m == WM_WS_NOTIFY) {
        e = vb6_WsOf(h);
        if (!e) return 0;
        switch (LOWORD(l)) {           // ① 事件种类只认这里
            case FD_READ:
                vb6_WsDrain(e);
                vb6_WsRaiseData(e);
                break;
            case FD_CLOSE:
                // ② FD_CLOSE 允许带着没读完的尾巴到（实测还剩 7 字节）⇒ 先 drain、
                //   把这一批作为一次 DataArrival 交出去，再发 Close。VB6 那颗 OCX 在这里丢数据。
                vb6_WsDrain(e);
                if (e->bytesReceived > 0) vb6_WsRaiseData(e);
                if (e->sock != INVALID_SOCKET) { closesocket(e->sock); e->sock = INVALID_SOCKET; }
                vb6_WsRaiseClose(e);
                break;
            default:
                break;                          // FD_ACCEPT / FD_CONNECT / FD_WRITE 归 WS-b
        }
        return 0;
    }
    if (m == WM_DESTROY) { vb6_Ws_Destroy((void*)h); return 0; }
    return DefWindowProcW(h, m, w, l);
}

void vb6_RegisterWinsockClass(void* hInstance) {
    static int done = 0;
    WNDCLASSW wc;
    if (done) return;
    ZeroMemory(&wc, sizeof(wc));
    wc.lpfnWndProc = vb6_WsWndProc;
    wc.hInstance = (HINSTANCE)hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = L"VB6_WINSOCK";
    if (RegisterClassW(&wc)) done = 1;
}

void* vb6_Ws_Create(void* hwnd) {
    vb6_WsEnsureWsa();
    if (hwnd) vb6_WsSlot((HWND)hwnd);
    return hwnd;
}

void vb6_Ws_Destroy(void* hwnd) {
    struct vb6_WsInstance* e = vb6_WsOf((HWND)hwnd);
    if (!e) return;
    if (e->sock != INVALID_SOCKET) { closesocket(e->sock); e->sock = INVALID_SOCKET; }
    free(e->rx);
    e->rx = NULL; e->rxCap = e->rxLen = e->rxHead = 0;
    e->hwnd = NULL;                       // 槽位回收
}

void vb6_Ws_SetEventHandler(void* hwnd, int32_t kind, void* fn) {
    struct vb6_WsInstance* e;
    if (kind < 0 || kind >= VB6_WS_EV_COUNT) return;
    e = vb6_WsOf((HWND)hwnd);
    if (!e) { e = vb6_WsSlot((HWND)hwnd); }
    if (e) e->cb[kind] = fn;
}

// ---------------- 属性面 ----------------
int32_t vb6_Ws_GetProtocol(void* hwnd) {
    struct vb6_WsInstance* e = vb6_WsOf((HWND)hwnd);
    return e ? e->protocol : VB6_WS_TCP;
}

void vb6_Ws_SetProtocol(void* hwnd, int32_t v) {
    struct vb6_WsInstance* e = vb6_WsOf((HWND)hwnd);
    if (!e) return;
    if (e->sock != INVALID_SOCKET) return;   // VB6：已建套接字后改 Protocol 会报错，这里保持不动
    e->protocol = (v == VB6_WS_UDP) ? VB6_WS_UDP : VB6_WS_TCP;
}

int32_t vb6_Ws_GetState(void* hwnd) {
    struct vb6_WsInstance* e = vb6_WsOf((HWND)hwnd);
    return e ? e->state : VB6_WS_CLOSED;
}

int32_t vb6_Ws_GetLocalPort(void* hwnd) {
    struct vb6_WsInstance* e = vb6_WsOf((HWND)hwnd);
    return e ? e->localPort : 0;
}

const void* vb6_Ws_GetLocalIP(void* hwnd) {
    struct vb6_WsInstance* e = vb6_WsOf((HWND)hwnd);
    return (const void*)SysAllocString(e ? e->localIP : L"");
}

const void* vb6_Ws_GetRemoteHost(void* hwnd) {
    struct vb6_WsInstance* e = vb6_WsOf((HWND)hwnd);
    return (const void*)SysAllocString(e ? e->remoteHost : L"");
}

void vb6_Ws_SetRemoteHost(void* hwnd, const void* bstr) {
    struct vb6_WsInstance* e = vb6_WsOf((HWND)hwnd);
    if (!e) return;
    vb6_WsCopyBstr(e->remoteHost, sizeof(e->remoteHost) / sizeof(wchar_t), bstr);
}

int32_t vb6_Ws_GetRemotePort(void* hwnd) {
    struct vb6_WsInstance* e = vb6_WsOf((HWND)hwnd);
    return e ? e->remotePort : 0;
}

void vb6_Ws_SetRemotePort(void* hwnd, int32_t v) {
    struct vb6_WsInstance* e = vb6_WsOf((HWND)hwnd);
    if (e) e->remotePort = v;
}

int32_t vb6_Ws_GetBytesReceived(void* hwnd) {
    struct vb6_WsInstance* e = vb6_WsOf((HWND)hwnd);
    return e ? (int32_t)e->rxLen : 0;
}

int32_t vb6_Ws_GetByteTransferred(void* hwnd) {
    struct vb6_WsInstance* e = vb6_WsOf((HWND)hwnd);
    return e ? e->byteTransferred : 0;
}

// ---------------- 方法面 ----------------
// Bind(port[, ip])：UDP 的入口（TCP 也走这条定 LocalPort，服务端那一档在 WS-b 里接）。
// 实测：port=0 ⇒ 系统自动分配，从 getsockname 读回（VB6 的"LocalPort 留 0 让它自己挑"）；
// 从没绑过的 socket 问 getsockname 是**失败**的 ⇒ localPort/localIP 保持 0 / 空串。
void vb6_Ws_Bind(void* hwnd, int32_t port, const void* ip) {
    struct vb6_WsInstance* e = vb6_WsOf((HWND)hwnd);
    SOCKET s;
    int slen, family = AF_INET, on = 1;
    wchar_t ipw[64];
    struct sockaddr_storage ss;

    if (!e) return;
    vb6_WsEnsureWsa();
    if (e->sock != INVALID_SOCKET) { closesocket(e->sock); e->sock = INVALID_SOCKET; }

    ZeroMemory(&ss, sizeof(ss));
    vb6_WsCopyBstr(ipw, 64, ip);
    if (ipw[0] && wcschr(ipw, L':')) family = AF_INET6;
    e->family = family;

    s = socket(family, (e->protocol == VB6_WS_UDP) ? SOCK_DGRAM : SOCK_STREAM,
               (e->protocol == VB6_WS_UDP) ? IPPROTO_UDP : IPPROTO_TCP);
    if (s == INVALID_SOCKET) { e->lastErr = WSAGetLastError(); vb6_WsSetState(e, VB6_WS_ERROR); return; }
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char*)&on, sizeof(on));

    if (family == AF_INET6) {
        struct sockaddr_in6* a = (struct sockaddr_in6*)&ss;
        a->sin6_family = AF_INET6;
        a->sin6_port = htons((u_short)port);
        if (ipw[0] && wcscmp(ipw, L"*") && !InetPtonW(AF_INET6, ipw, &a->sin6_addr)) {
            closesocket(s); vb6_WsSetState(e, VB6_WS_ERROR); return;
        }
        slen = sizeof(struct sockaddr_in6);
    } else {
        struct sockaddr_in* a = (struct sockaddr_in*)&ss;
        a->sin_family = AF_INET;
        a->sin_port = htons((u_short)port);
        if (ipw[0] && wcscmp(ipw, L"*") && !InetPtonW(AF_INET, ipw, &a->sin_addr)) {
            closesocket(s); vb6_WsSetState(e, VB6_WS_ERROR); return;
        }
        slen = sizeof(struct sockaddr_in);
    }
    if (bind(s, (struct sockaddr*)&ss, slen) == SOCKET_ERROR) {
        e->lastErr = WSAGetLastError();               // 撞已用端口 = 10048（实测）
        closesocket(s);
        vb6_WsSetState(e, VB6_WS_ERROR);
        return;
    }
    {
        int l = slen;
        if (getsockname(s, (struct sockaddr*)&ss, &l) == 0) {
            e->localPort = ntohs(family == AF_INET6 ? ((struct sockaddr_in6*)&ss)->sin6_port
                                                    : ((struct sockaddr_in*)&ss)->sin_port);
            {
                char txt[64];
                ULONG n = (ULONG)sizeof(txt);
                txt[0] = 0;
                if (WSAAddressToStringA((struct sockaddr*)&ss, l, NULL, txt, &n) == 0) {
                    int i;
                    // WSAAddressToString 给的是 "127.0.0.1:54241" / "[::1]:54241" 这种形状，
                    // VB6 的 LocalIP 只要地址那一半 ⇒ 从最后一个冒号截断（IPv6 的组冒号都在括号里）。
                    for (i = 0; txt[i] && i < 63; i++) e->localIP[i] = (wchar_t)(unsigned char)txt[i];
                    while (i > 0 && e->localIP[i-1] != L':') i--;
                    if (i > 0) { e->localIP[i-1] = 0; if (i-1 > 0 && e->localIP[i-2] == L']') e->localIP[i-2] = 0; }
                }
            }
        }
    }
    e->sock = s;
    WSAAsyncSelect(s, e->hwnd, WM_WS_NOTIFY,
                   FD_READ | FD_CLOSE /* WS-b 再加 FD_ACCEPT / FD_CONNECT / FD_WRITE */);
    // UDP 没有"连接"一说，绑完就是 sckOpen（实测：这也是 VB6 对 UDP 的观感）
    vb6_WsSetState(e, (e->protocol == VB6_WS_UDP) ? VB6_WS_OPEN : VB6_WS_CLOSED);
}

void vb6_Ws_Listen(void* hwnd) { (void)hwnd; }      // WS-b
void vb6_Ws_Accept(void* hwnd, int32_t id) { (void)hwnd; (void)id; }
void vb6_Ws_Connect(void* hwnd) { (void)hwnd; }      // WS-b

void vb6_Ws_SendData(void* hwnd, const void* data) {
    struct vb6_WsInstance* e = vb6_WsOf((HWND)hwnd);
    const wchar_t* s = (const wchar_t*)data;
    char* bytes;
    int need, n, slen = 0;
    struct sockaddr_storage ss;

    if (!e || e->sock == INVALID_SOCKET || !s) return;
    need = WideCharToMultiByte(CP_ACP, 0, s, -1, NULL, 0, NULL, NULL);
    if (need <= 0) return;
    bytes = (char*)malloc((size_t)need);
    if (!bytes) return;
    n = WideCharToMultiByte(CP_ACP, 0, s, -1, bytes, need, NULL, NULL);
    if (n > 0) n -= 1;                       // 去掉结尾 NUL：线格式里不带它
    if (n > 0) {
        if (e->protocol == VB6_WS_UDP) {
            if (vb6_WsUdpTarget(e, &ss, &slen)) {
                n = sendto(e->sock, bytes, n, 0, (struct sockaddr*)&ss, slen);
            } else {
                free(bytes);
                return;
            }
        } else {
            n = send(e->sock, bytes, n, 0);
        }
        e->byteTransferred = (n > 0) ? n : 0;
        if (n == SOCKET_ERROR) e->lastErr = WSAGetLastError();
    }
    free(bytes);
}

// 从缓冲里取：maxLen<=0 = VB6 的"全给我"（默认那条）
static BSTR vb6_WsTake(struct vb6_WsInstance* e, int32_t maxLen) {
    uint32_t take = e->rxLen;
    if (maxLen > 0 && (uint32_t)maxLen < take) take = (uint32_t)maxLen;
    if (take) {
        int wn = MultiByteToWideChar(CP_ACP, 0, e->rx + e->rxHead, (int)take, NULL, 0);
        BSTR out = SysAllocStringLen(NULL, wn > 0 ? wn : 0);
        if (out) {
            if (wn > 0) MultiByteToWideChar(CP_ACP, 0, e->rx + e->rxHead, (int)take, out, wn);
            e->rxHead += take;
            e->rxLen -= take;
            if (!e->rxLen) e->rxHead = 0;
        }
        return out;
    }
    return (BSTR)SysAllocStringLen(NULL, 0);   /* 空串：与 vb6_BSTR_Empty 同物，这枚 TU 没引那个头 */
}

int32_t vb6_Ws_GetData(void* hwnd, void* outBstr, int32_t type, int32_t maxLen) {
    struct vb6_WsInstance* e = vb6_WsOf((HWND)hwnd);
    BSTR got;
    (void)type;                              // Byte 数组那一形在 WS-c；现在只有 String
    if (!e || !outBstr) return 0;
    got = vb6_WsTake(e, maxLen);
    *(BSTR*)outBstr = got;
    return (int32_t)SysStringLen(got);
}

int32_t vb6_Ws_PeekData(void* hwnd, void* outBstr, int32_t type, int32_t maxLen) {
    struct vb6_WsInstance* e = vb6_WsOf((HWND)hwnd);
    uint32_t head, len;
    BSTR out;
    (void)type;
    if (!e || !outBstr) return 0;
    head = e->rxHead; len = e->rxLen;
    if (maxLen > 0 && (uint32_t)maxLen < len) len = (uint32_t)maxLen;
    {
        int wn = MultiByteToWideChar(CP_ACP, 0, e->rx + head, (int)len, NULL, 0);
        out = SysAllocStringLen(NULL, wn > 0 ? wn : 0);
        if (out && wn > 0) MultiByteToWideChar(CP_ACP, 0, e->rx + head, (int)len, out, wn);
    }
    *(BSTR*)outBstr = out;                   // **不消费**：这是 PeekData 与 GetData 唯一的差别
    return (int32_t)len;
}

void vb6_Ws_Close(void* hwnd) {
    struct vb6_WsInstance* e = vb6_WsOf((HWND)hwnd);
    if (!e) return;
    if (e->sock != INVALID_SOCKET) { closesocket(e->sock); e->sock = INVALID_SOCKET; }
    e->rxLen = e->rxHead = 0;
    e->bytesReceived = 0;
    vb6_WsSetState(e, VB6_WS_CLOSED);
    // VB6 的 Close **不**再触发 Close 事件（那是"对端关了"才有的）⇒ 这里只翻状态。
}

#else  /* !_WIN32 */

void vb6_RegisterWinsockClass(void* hInstance) { (void)hInstance; }

#endif /* _WIN32 */
