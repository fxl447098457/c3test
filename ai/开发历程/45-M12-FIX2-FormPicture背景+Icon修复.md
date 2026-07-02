# M12-FIX2: Form.Picture背景渲染 + Form.Icon句柄修复

日期: 2026-07-02

## 问题

用户反馈 Form.Picture 背景图片和 Form.Icon 窗体图标仍然没有正确显示。

## 根因分析

### Bug 1: Form.Icon — ICO 句柄悬空指针

`vb6_LoadPictureFromMemory()` 中，对 ICO 类型 (picType==3) 直接借用 IPicture 的 HICON 句柄：

```c
} else {
    /* Icon or metafile - just use the handle directly */
    result = (HANDLE)(LONG_PTR)hHandle;   // 借用IPicture的句柄
}
pPicture->lpVtbl->Release(pPicture);      // 释放IPicture → HICON被销毁！
return (void*)result;                      // 返回悬空句柄
```

IPicture 拥有 HICON 的所有权，Release 后句柄失效。BMP 类型已正确复制，但 ICO 类型遗漏了。

### Bug 2: Form.Picture — 没有渲染代码

cgen_form.cpp 中，Form.Picture 图片在 WM_CREATE 中加载并存入窗口属性 `VB6_Picture`：
```c
SetPropW(hwnd, L"VB6_Picture", (HANDLE)vb6_bgpic);
```

但 WndProc 中 **完全没有 WM_ERASEBKGND 或 WM_PAINT 处理** 来绘制这个图片。注释写着 "for WM_PAINT" 但只是空头支票。

## 修复

### Fix 1: vb6_LoadPictureFromMemory — DuplicateIcon

```c
} else if (picType == 3) {
    /* PICTURE_TYPE_ICON - must duplicate because IPicture owns it */
    HICON hSrc = (HICON)(LONG_PTR)hHandle;
    if (hSrc) {
        result = (HANDLE)DuplicateIcon(NULL, hSrc);
    }
} else {
    /* Metafile - use handle directly (rare for .frx) */
    result = (HANDLE)(LONG_PTR)hHandle;
}
```

### Fix 2: cgen_form.cpp — WM_ERASEBKGND 平铺渲染

在 WM_CREATE 之后、WM_COMMAND 之前添加 WM_ERASEBKGND case：

```c
case WM_ERASEBKGND: {
    HANDLE vb6_bg = GetPropW(hwnd, L"VB6_Picture");
    if (vb6_bg && GetObjectType((HGDIOBJ)vb6_bg) == OBJ_BITMAP) {
        HDC hdc = (HDC)(WPARAM)wParam;
        RECT rc; GetClientRect(hwnd, &rc);
        HDC memDC = CreateCompatibleDC(hdc);
        HBITMAP hBmp = (HBITMAP)vb6_bg;
        BITMAP bm; GetObjectW(hBmp, sizeof(bm), &bm);
        HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, hBmp);
        for (int y = 0; y < rc.bottom; y += bm.bmHeight) {
            for (int x = 0; x < rc.right; x += bm.bmWidth) {
                BitBlt(hdc, x, y, bm.bmWidth, bm.bmHeight, memDC, 0, 0, SRCCOPY);
            }
        }
        SelectObject(memDC, oldBmp);
        DeleteDC(memDC);
        return 1;
    }
    break;
}
```

VB6 的 Form.Picture 行为是平铺(Tile)显示，不是拉伸。用双重循环 BitBlt 实现平铺。

### Fix 3: WM_DESTROY 清理

在 PostQuitMessage(0) 之前添加：

- Form.Picture: `DeleteObject(vb6_bg); RemovePropW(hwnd, L"VB6_Picture");`
- Form.Icon: `DestroyIcon((HICON)vb6_ico);`

## 修改文件

| 文件 | 修改内容 |
|------|---------|
| src/rtl/core/vb6forms.c | picType==3 分支用 DuplicateIcon 替代直接借用 |
| src/backend/cgen_form.cpp | 添加 WM_ERASEBKGND 平铺渲染 + WM_DESTROY 清理 |

## 验证

- frxParse 工程编译成功，Form1.c 从 7083→8283 字节
- 工程1.exe 从 160768→161792 字节
- 74/74 回归测试零失败

## ImageList 决策

用户明确：ImageList 是 MSCOMCTL.OCX 第三方控件，不属于 VB6 内置标准工具箱，c3 不负责实现。第三方 ActiveX 控件的支持范围另行讨论。
