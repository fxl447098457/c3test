#pragma once
// vb6forms_prop_pic.h - Picture 加载与属性 (P13.13/P24)、Image.Stretch (P17.2)、控件子类化基础设施 (P18-F)
// 由 vb6forms.h 伞头按固定顺序 include，不要单独使用
// 内容 = 拆分前 vb6forms.h 第 329~378 行，逐行未改

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// P13.13: Picture (PictureBox/Image)
// vb6_LoadPictureFromFile: load picture from file (BMP/ICO/CUR/WMF/EMF)
// Returns HBITMAP/HICON (as void*), or NULL on failure
void* vb6_LoadPictureFromFile(const char* filePath);
// vb6_LoadPictureFromResource: load picture from resource
// Returns handle from resource, or NULL on failure
void* vb6_LoadPictureFromResource(void* hInstance, int resourceId, const char* type);
// P24: Load picture from memory buffer (JPEG/BMP/ICO/PNG/GIF via OleLoadPicture + IStream)
// Returns HBITMAP/HICON handle, or NULL on failure

void* vb6_LoadPictureFromMemory(const void* data, int size);
// Load picture from memory as COM IDispatch* (IPictureDisp)
// For passing pictures to COM controls (e.g. ImageList.ListImages.Add)
// Caller must Release the returned IDispatch* (via vb6_ReleaseObject)
void* vb6_LoadPictureAsCom(const void* data, int size);

void* vb6_LoadIconFromMemory(const void* data, int size);

void vb6_GraphicalBtn_SetImage(void* hwnd, void* hBitmap);
// P24: Convert UTF-8 string to wide string (caller must free())
wchar_t* vb6_Utf8ToWide(const char* utf8);
// Set/get Picture property on PictureBox/Image controls
void* vb6_GetControlPicture(void* hwnd);
void vb6_SetControlPicture(void* hwnd, void* hPicture);
// Set Picture from COM IPictureDisp object (extracts HBITMAP via IPicture::get_Handle)
void vb6_SetControlPictureFromCom(void* hwnd, void* pPictureDisp);
// AutoSize for PictureBox: resize to fit picture
int vb6_GetPictureAutoSize(void* hwnd);
void vb6_SetPictureAutoSize(void* hwnd, int autoSize);

// P17.2: Image.Stretch property

int vb6_GetImageStretch(void* hwnd);

void vb6_SetImageStretch(void* hwnd, int stretch);

// P17.2: Image subclass for WM_PAINT (StretchBlt rendering)

void vb6_InstallImageSubclass(void* hwnd);

// P18-F: 控件子类化基础设施 (GotFocus/LostFocus/MouseEnter/MouseLeave/控件级事件)
// 通用控件子类化安装 (复用VB6_OrigProc属性模式)
// subclassProc: 子类化窗口过程 (由cgen生成)
void vb6_InstallControlSubclass(void* hwnd, void* subclassProc);
// 获取原始窗口过程 (子类化Proc内调用CallWindowProc用)
void* vb6_GetOriginalWndProc(void* hwnd);
// 移除控件子类化 (WM_DESTROY时调用)
void vb6_RemoveControlSubclass(void* hwnd);
// 启动鼠标跟踪 (TrackMouseEvent封装, 用于MouseEnter/MouseLeave)
void vb6_StartMouseTracking(void* hwnd);

#ifdef __cplusplus
}
#endif
