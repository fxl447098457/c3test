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


#ifdef __cplusplus
}
#endif
