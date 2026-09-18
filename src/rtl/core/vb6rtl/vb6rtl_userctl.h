#pragma once
// vb6rtl_userctl.h - UserControl/PropertyPage 宿主内建对象 (Fix 105)
//
// 背景: VB6 UserControl 工程 (Charts 2020 等) 的 .ctl/.pag 代码里直接引用
// UserControl.* / Ambient.* / Extender.* / PropertyPage.* 内建对象。
// 生成 C 把它们发射为裸标识符 vb6_UserControl_ScaleWidth / vb6_Ambient_Font /
// vb6_Extender_Left / vb6_PropertyPage_hwnd 等, 此前无任何声明 → C2065。
//
// 本头提供「每进程单实例」的宿主状态: 对单窗体 + 同屏多实例不精确 (多实例
// 共享一份宿主状态), 但让真实工程先可编译运行; 实例化宿主是后续特性块。
// 同时提供 StdFont 具体类 (生成代码把 UserControl.Font 强转为 vb6_cls_StdFont*
// 做 With 成员写) 与 Picture1.Line 的 B/BF 模式常量、HitResult 常量。

#include "vb6rtl_bstr.h"

#ifdef __cplusplus
extern "C" {
#endif

// --- Font 对象 (VB6 stdole.StdFont 的最小 C 形态) ---
// Fix 109: 生成代码对 UserControl.Font 既有对象用法 (裸名作为 vb6_ComIface_Font*
// 实参传给控件绘制函数), 又有成员用法 (`.Bold` / `.Size + 8` / `.Name`)。
// 因此这里给出**完整结构体**, 并把 vb6_ComIface_Font 定义为它 —— 生成代码对该
// 类型只有前向 typedef, 这里补齐定义不冲突. 成员用数值/字符串真实类型, 与
// 生成代码 bBold(int16_t) = Font.Bold; Font.Bold = -1; Font.Size + 8 一致.
typedef struct vb6_ComIface_Font {
    BSTR    Name;
    float   Size;
    int16_t Bold;
    int16_t Italic;
    int16_t Underline;
    int16_t Strikethrough;
    int32_t Weight;
    int32_t Charset;
} vb6_ComIface_Font;

// 旧名保留 (Fix 105 引入), 供既有生成代码/注释引用.
typedef vb6_ComIface_Font vb6_cls_StdFont;

// --- UserControl 宿主 ---
extern int32_t vb6_UserControl_ScaleWidth;   // 用户坐标宽度
extern int32_t vb6_UserControl_ScaleHeight;  // 用户坐标高度
extern int32_t vb6_UserControl_ScaleMode;    // 1=Twip 3=Pixel
extern void*   vb6_UserControl_hDC;          // 绘制 DC (Windowless: 容器客户区 DC)
extern int32_t vb6_UserControl_ContainerHwnd;// 容器 HWND
extern int16_t vb6_UserControl_Enabled;
extern int32_t vb6_UserControl_MousePointer;
extern void*   vb6_UserControl_MouseIcon;
extern int32_t vb6_UserControl_OLEDropMode;
// Fix 109: Font 是**对象指针** (生成代码把它作为 vb6_ComIface_Font* 实参传递,
// 同时对它做 `.成员` 访问 —— 成员访问由生成端改写为 '->', 见 CodeEmitter::emitLine).
extern vb6_ComIface_Font* vb6_UserControl_Font;
struct vb6_UserControl_Ambient_Type { vb6_ComIface_Font* Font; };
extern struct vb6_UserControl_Ambient_Type vb6_UserControl_Ambient;

void vb6_UserControl_Refresh(void);
// Fix 110z: UserControl.Size width, height — 设置控件尺寸 (单位同 ScaleMode).
// Charts 2020 LabelPlus.ctl:1371 `UserControl.Size (lWidth + 1) * ..., ...`
// 生成 vb6_UserControl_Size(w, h), 此前无声明 → C2065/C2064.
void vb6_UserControl_Size(double width, double height);
void vb6_UserControl_CancelAsyncRead(BSTR propName);
int32_t vb6_UserControl_TextWidth(BSTR text);
int32_t vb6_UserControl_TextHeight(BSTR text);

// Fix 111: UserControl 的其余内建方法 (.ctl 里常以裸名书写,
// 生成 C 端由 cgen 的宿主伪对象成员表映射为 vb6_UserControl_<Member>).
//   ScaleX/ScaleY(x, fromScale, toScale) → 单位换算
//   AsyncRead(url, asyncType, propertyName, flags) → 异步读取 (编译形态下空操作)
//   PropertyChanged(propName) → 通知容器属性已变 (触发容器端的 Changed/属性刷新)
double vb6_UserControl_ScaleX(double x, int32_t fromScale, int32_t toScale);
double vb6_UserControl_ScaleY(double x, int32_t fromScale, int32_t toScale);
void   vb6_UserControl_AsyncRead(BSTR url, int32_t asyncType, BSTR propertyName,
                                 int32_t flags);
void   vb6_UserControl_PropertyChanged(BSTR propName);

// --- Ambient 宿主环境 ---
extern vb6_ComIface_Font* vb6_Ambient_Font;  // 容器默认 Font
extern int16_t vb6_Ambient_UserMode;         // -1=运行期 0=设计期
extern BSTR    vb6_Ambient_DisplayName;      // 控件实例名
extern int32_t vb6_Ambient_ForeColor;
extern int32_t vb6_Ambient_BackColor;

// --- Extender (容器提供的扩展对象) ---
extern int32_t vb6_Extender_Left;
extern int32_t vb6_Extender_Top;

// --- PropertyPage 设计器页 ---
extern void*   vb6_PropertyPage_hwnd;
extern void*   vb6_PropertyPage_hWnd;        // 生成代码两种拼写并存
extern int32_t vb6_PropertyPage_ScaleMode;
extern int32_t vb6_PropertyPage_ScaleHeight;
extern int16_t vb6_PropertyPage_Changed;     // PropertyPage 内建 Changed 属性
                                               // (生成 C 直接引用裸名 Changed)

// --- HitTest 常量 (VB6 HitResult) ---
#define vbHitResultOutside     0
#define vbHitResultTransparent 1
#define vbHitResultHit         2

// --- PropertyPage 内建 Changed 属性 ---
// VB6 PropertyPage 代码惯用裸名 `Changed = True` (生成 C 亦为裸标识符),
// 见 Charts 2020 PropPagLP.pag. 项目自定义的 Changed 只会以类字段
// (me->m_Changed) 或模块限定名 (vb6_<Mod>_Changed) 出现, 不会占用裸键.
extern int16_t Changed;

// --- AsyncProperty / Picture 类型常量 (VB6 内建, 此前缺失) ---
#define vbAsyncTypePicture     0
#define vbAsyncTypeFile        1
#define vbAsyncTypeByteArray   2
#define vbAsyncReadSynchronous 0
#define vbAsyncReadAsynchronous 2
#define vbAsyncReadForceUpdate 4
#define vbPicTypeNone          0
#define vbPicTypeBitmap        1
#define vbPicTypeMetafile      2
#define vbPicTypeIcon          3
#define vbPicTypeEMetafile     4

// --- Picture.Line 模式常量 (Fix 102 把 `, B` / `, BF` 原样作为实参发射) ---
extern const int32_t B;   // 画方框
extern const int32_t BF;  // 实心方框

#ifdef __cplusplus
}
#endif
