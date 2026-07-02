# M12-FIX2: Form.Picture背景渲染 + Form.Icon句柄修复 (v2)

日期: 2026-07-02

## 问题

用户反馈：
1. Form.Picture 背景图片重复显示（平铺），应该只显示一次
2. Form.Icon 窗体图标仍然不显示

## 根因分析

### Bug 1: Form.Icon — OleLoadPicture 对 ICO 返回 HBITMAP 而非 HICON

`vb6_LoadPictureFromMemory` 用 `OleLoadPicture` 加载 ICO 数据，对 `picType==3` 用 `DuplicateIcon` 复制句柄。但 OleLoadPicture 加载 ICO 时可能返回 picType==1 (BITMAP) 而非 picType==3 (ICON)，导致返回的是 HBITMAP 而非 HICON。`WM_SETICON` 只接受 HICON，传 HBITMAP 无效。

**根本修复**：对 Form.Icon 使用专门的 `vb6_LoadIconFromMemory` 函数，直接用 Win32 ICO 解析 API（`LookupIconIdFromDirectoryEx` + `CreateIconFromResourceEx`），确保始终返回 HICON。

### Bug 2: Form.Picture — 平铺 vs 单次显示

WM_ERASEBKGND 中用双重 for 循环 BitBlt 实现平铺(Tile)。用户要求只显示一次。

## 修复

### Fix 1: vb6_LoadIconFromMemory（新增函数）

用 Win32 原生 ICO 解析 API：
1. `LookupIconIdFromDirectoryEx` 查找最佳匹配的图标索引
2. 从 ICO 目录项中读取 dataOffset/dataSize
3. `CreateIconFromResourceEx` 创建 HICON

如果不是有效 ICO 格式，回退到 `vb6_LoadPictureFromMemory`。

cgen_form.cpp 中 Form.Icon 改用 `vb6_LoadIconFromMemory`。

### Fix 2: Form.Picture — 单次 BitBlt

将平铺循环改为单次 BitBlt：
```c
BitBlt(hdc, 0, 0, bm.bmWidth, bm.bmHeight, memDC, 0, 0, SRCCOPY);
```

### Fix 3: rebuild_rtl_full.bat — 编译 vb6forms.c

之前重建脚本不编译 vb6forms.c，导致 vb6rtl_gui.lib 缺少新函数。添加编译和打包步骤。

## 修改文件

| 文件 | 修改内容 |
|------|---------|
| src/rtl/core/vb6forms.h | 声明 vb6_LoadIconFromMemory |
| src/rtl/core/vb6forms.c | 实现 vb6_LoadIconFromMemory |
| src/backend/cgen_form.cpp | Form.Icon 用 vb6_LoadIconFromMemory；Form.Picture 单次 BitBlt |
| .temp/rebuild_rtl_full.bat | 添加 vb6forms.c 编译和 vb6rtl_gui.lib 打包 |

## 验证

- frxParse 编译成功，74/74 回归测试零失败
