#pragma once
// vb6forms_webview.h - WebView2 宿主 (P7.9)
// 由 vb6forms.h 伞头按固定顺序 include，不要单独使用
// 内容 = 拆分前 vb6forms.h 第 522~567 行，逐行未改

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// WebView2宿主 (P7.9)
// ============================================================
// VB6 WebBrowser控件 → Windows WebView2 (Edge Chromium) 映射
// 替代原有SHDocVw.WebBrowser (IE内核)

// 创建WebView2控件
// hParent: 父窗口句柄
// x, y, width, height: 位置和大小 (像素)
// controlName: 控件名 (用于标识)
// 返回: 控件窗口句柄 (HWND), WebView2异步初始化完成后可Navigate
void* vb6_CreateWebView(void* hParent, int x, int y, int width, int height, const char* controlName);

// 导航到URL
// hwnd: CreateWebView返回的句柄
// url: 要导航的URL
// 返回: 0=成功, -1=WebView2未初始化, -2=参数无效
int vb6_WebViewNavigate(void* hwnd, const char* url);

// 获取当前URL (返回BSTR, 调用者负责释放)
void* vb6_WebViewGetUrl(void* hwnd);

// 获取WebView2就绪状态
// 返回: 1=就绪(Navigate可用), 0=正在初始化, -1=初始化失败
int vb6_WebViewIsReady(void* hwnd);

// 设置WebView2大小 (响应式调整)
void vb6_WebViewResize(void* hwnd, int width, int height);

// 执行JavaScript
// script: JavaScript代码
// 返回: 0=成功, -1=未就绪, -2=参数无效
int vb6_WebViewExecuteScript(void* hwnd, const char* script);

// WebView2事件回调类型
// DocumentComplete: 导航完成
typedef void (*vb6_WebViewEventCallback)(void* hwnd, const char* url);

// 设置DocumentComplete事件回调
void vb6_WebViewSetDocumentCompleteCallback(void* hwnd, vb6_WebViewEventCallback callback);

// P20-50: WebBrowser导航方法占位
int vb6_WebViewGoBack(void* hwnd);
int vb6_WebViewGoForward(void* hwnd);
int vb6_WebViewRefresh(void* hwnd);

// ============================================================
// P20-37 / C29-1b: DriveListBox / DirListBox / FileListBox
// ============================================================
// 这三类在 C29-1b 之前只有定义、没有任何头声明: 生成代码里的调用点因此按
// "返回 int" 的隐式原型编译, 字符串句柄被截成 32 位 —— 真编译真跑才暴露 (CF3 崩)。
// 原生窗口: Drive = COMBOBOX, Dir/File = LISTBOX。
// 注: 本头不引 oleauto.h。BSTR 就是 wchar_t* (OLECHAR=wchar_t), 因此这里一律写
// wchar_t*, 与 .c 里的 BSTR 定义同一类型、不冲突。

wchar_t* vb6_DriveListBoxDrive(void* hwnd);
void    vb6_DriveListBoxSetDrive(void* hwnd, wchar_t* drive);
void    vb6_DriveListBoxRefresh(void* hwnd);

wchar_t* vb6_DirListBoxPath(void* hwnd);
void    vb6_DirListBoxSetPath(void* hwnd, wchar_t* path);
void    vb6_DirListBoxRefresh(void* hwnd);
// 把当前选中的 [条目] 下钻成新的 Path; 没有选中或非目录条目时返回 0。
int32_t vb6_DirListBoxDescendSelected(void* hwnd);

wchar_t* vb6_FileListBoxPath(void* hwnd);
void    vb6_FileListBoxSetPath(void* hwnd, wchar_t* path);
wchar_t* vb6_FileListBoxPattern(void* hwnd);
void    vb6_FileListBoxSetPattern(void* hwnd, wchar_t* pattern);
wchar_t* vb6_FileListBoxFileName(void* hwnd);
void    vb6_FileListBoxSetFileName(void* hwnd, wchar_t* fileName);
void    vb6_FileListBoxRefresh(void* hwnd);


#ifdef __cplusplus
}
#endif
