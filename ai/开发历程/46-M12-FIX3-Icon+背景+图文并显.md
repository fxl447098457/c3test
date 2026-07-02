# M12-FIX3: Icon直接解析 + 背景灰色 + Graphical按钮图文并显

日期: 2026-07-02

## 问题

用户截图对比 c3(左) vs VB6(右)：
1. Form.Icon 仍不显示
2. Form.Picture 显示后非图片区域变白色，VB6是灰色(COLOR_BTNFACE)
3. Graphical按钮只显示图片，不显示Caption文字
4. Graphical按钮获得焦点/选中后图片消失
5. Graphical按钮快速点击时图片闪烁

## 根因分析

### Bug 1: Icon — LookupIconIdFromDirectoryEx 期望资源目录格式

`LookupIconIdFromDirectoryEx` 期望 PE 资源目录格式(RT_GROUP_ICON)，其中目录项最后一个字段是 WORD ID(资源标识符)。而 .ico 文件格式中目录项最后一个字段是 DWORD ImageOffset(文件偏移)。两者不同，导致解析错误返回 0 或错误值。

### Bug 2: 背景 — WM_ERASEBKGND return 1 后未填充非图片区域

WM_ERASEBKGND 返回 1 表示"我已处理背景擦除"，DefWindowProc 不再填充。但 BitBlt 只画了图片大小的区域，其余部分保持 DC 的默认白色。

### Bug 3: Graphical按钮 — BS_BITMAP 只画图片不画文字

Win32 的 BS_BITMAP 样式下，按钮只显示 BM_SETIMAGE 设置的位图，不显示 SetWindowText 设置的文字。VB6 的 Graphical 按钮是图片在上、文字在下。

### Bug 4: 焦点/选中后图片消失

默认按钮窗口过程在处理 WM_SETFOCUS、WM_KILLFOCUS、BM_SETCHECK、BM_SETSTATE 等消息时，直接操作 DC 绘制按钮（不经过 InvalidateRect → WM_PAINT 流程），绕过子类化 WM_PAINT，覆盖自绘内容。

### Bug 5: 快速点击闪烁

拦截状态消息后先 CallWindowProcW（默认窗口过程画一次无图片按钮），再 InvalidateRect 触发 WM_PAINT（画一次带图片按钮），中间有一帧空白。

## 修复

### Fix 1: vb6_LoadIconFromMemory — 直接解析 .ico 文件格式

不再使用 LookupIconIdFromDirectoryEx，改为：
1. 验证 .ico 魔数 (00 00 01 00)
2. 遍历目录项，找最佳匹配(尺寸最接近 SM_CXSMICON，优先高 bpp)
3. 读取 dataOffset/dataSize
4. `CreateIconFromResourceEx` 直接创建 HICON

### Fix 2: WM_ERASEBKGND — 先 FillRect 灰色再 BitBlt 图片

```c
RECT rc; GetClientRect(hwnd, &rc);
HBRUSH hBr = (HBRUSH)(COLOR_BTNFACE+1); FillRect(hdc, &rc, hBr);
// ... then BitBlt the picture at (0,0)
```

### Fix 3: Graphical 按钮 — 子类化 WM_PAINT 自己画

去掉 BS_BITMAP，保留 BS_PUSHLIKE。添加 `vb6_GraphicalBtn_SetImage` 函数：
- 存储位图到窗口属性 VB6_GfxBtn_Bmp
- 子类化按钮，替换 WndProc 为 vb6_GraphicalBtnSubclassProc
- WM_PAINT 处理：
  1. BM_GETSTATE 获取 push/focus 状态
  2. DrawFrameControl 画按钮框架(3D raised/sunken)
  3. BitBlt 画位图(居中偏上)
  4. DrawTextW 画 Caption 文字(居中偏下)
  5. DrawFocusRect 画焦点框

### Fix 4: 拦截状态变更消息 + InvalidateRect

拦截 WM_SETFOCUS/WK_KILLFOCUS/BM_SETCHECK/BM_SETSTATE/BM_SETSTATE，先 CallWindowProcW 处理状态变更，再 InvalidateRect 强制子类化 WM_PAINT 重绘。

### Fix 5: WM_SETREDRAW 消除闪烁

在 CallWindowProcW 前后用 SendMessageW(WM_SETREDRAW, FALSE/TRUE) 禁止/恢复自动重绘，默认窗口过程不会直接画到屏幕，只有 InvalidateRect → WM_PAINT 统一画一次。

## 修改文件

| 文件 | 修改内容 |
|------|---------|
| src/rtl/core/vb6forms.c | 重写 vb6_LoadIconFromMemory；新增 vb6_GraphicalBtnSubclassProc + vb6_GraphicalBtn_SetImage；WM_SETREDRAW 防闪烁 |
| src/rtl/core/vb6forms.h | 声明 vb6_GraphicalBtn_SetImage |
| src/backend/cgen_form.cpp | WM_ERASEBKGND 先 FillRect 灰色；Graphical 按钮去掉 BS_BITMAP，用 vb6_GraphicalBtn_SetImage |

## 验证

- frxParse 编译成功，74/74 回归测试零失败
- Form.Icon 正确显示自定义图标
- Form.Picture 背景图片只显示一次，非图片区域保持灰色
- Graphical 按钮图片+文字并显，凹陷/选中/焦点状态均正确
- 快速点击无闪烁
