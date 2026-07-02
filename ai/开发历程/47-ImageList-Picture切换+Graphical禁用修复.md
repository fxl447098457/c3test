# 47 - ImageList Picture切换 + Graphical按钮禁用状态修复

## 日期
2026-07-02

## 问题

### Bug 1: ImageList图片切换不工作
`Set Picture2.Picture = ImageList1.ListImages(i).Picture` 编译失败/运行时错误

**根因分析**：
1. **SetStmt路径缺失**：VB6中 `Set Picture2.Picture = expr` 走 SetStmt，但 SetStmt 对 `ctrl.Property = value` 没有控件属性写入路径，直接 `emitExpr` 左侧产生 `vb6_GetControlPicture(hwnd)`（函数返回值，非左值），导致 `error C2106: "=": 左操作数必须为左值`
2. **COM marker未resolve**：RHS `ImageList1.ListImages(i).Picture` 的 `.Picture` 设置了 `isComMarker_=true`，但赋值处理没有消费此marker，导致COM对象表达式泄漏
3. **类型不匹配**：即使resolve了COM marker，`vb6_ComGetObjectProp(..., L"Picture")` 返回 `IPictureDisp*`，而 `vb6_SetControlPicture` 期望 `HBITMAP`

### Bug 2: Graphical按钮禁用状态（之前已修复代码，本次验证构建）
Command1禁用时WM_PAINT不显示禁用视觉效果

## 修复

### 1. RTL: 新增 `vb6_SetControlPictureFromCom` (vb6forms.c/h)
```c
void vb6_SetControlPictureFromCom(void* hwnd, void* pPictureDisp) {
    IPicture* pPic = (IPicture*)pPictureDisp;
    pPic->lpVtbl->get_Type(pPic, &nType);   // C风格vtable调用
    pPic->lpVtbl->get_Handle(pPic, &hOle);  // 提取OLE_HANDLE
    if (nType == 1) vb6_SetControlPicture(hwnd, (void*)(HANDLE)hOle); // Bitmap
    // Icon: SS_ICON + STM_SETIMAGE
}
```

### 2. cgen_stmt.cpp: AssignmentStmt ctrl.Property COM marker处理
在 `ctrl.Property = value` 的 `emitExpr(*node.value)` 后：
- 检查 `isComMarker_`，当 `comMemberName_=="Picture"` 时 `resolveComValue("Object")`
- 当 `rhsIsComPicture && writeFn.find("SetControlPicture")` 时改用 `vb6_SetControlPictureFromCom`

### 3. cgen_stmt.cpp: SetStmt ctrl.Property写入路径
在 SetStmt 的 Nothing处理后、`emitExpr(*node.target)` 前：
- 检查 `node.target` 是否为 `MemberAccessExpr` 且对象是已知控件
- 若 `getControlPropWriteFn` 有对应写函数，直接走写入路径
- 同样处理COM marker resolve和 `SetControlPictureFromCom`

### 4. IPicture vtable: C风格调用
`pPic->get_Type()` → `pPic->lpVtbl->get_Type(pPic, &nType)`（C编译模式不支持C++虚方法语法）

### 5. RTL重建: /MT静态CRT
重建脚本改用 `/MT`（静态CRT）而非 `/MD`（动态CRT），避免 `__imp_` 前缀的链接错误

## 验证
- frxParse编译成功，生成代码正确：
  ```c
  vb6_SetControlPictureFromCom(vb6_hwnd_Picture2, 
      vb6_ComGetObjectProp(
          vb6_ComCallObject(
              vb6_ComGetObjectProp(vb6_com_ImageList1, L"ListImages"),
              L"Item", (void*[]){vb6_ComPackInt(i)}, 1),
          L"Picture"));
  ```
- frxParse运行成功，窗口正常显示
- 74/74回归测试全通过

## 修改文件
- `src/rtl/core/vb6forms.c` — 新增 `vb6_SetControlPictureFromCom` 函数
- `src/rtl/core/vb6forms.h` — 新增 `vb6_SetControlPictureFromCom` 声明
- `src/backend/cgen_stmt.cpp` — AssignmentStmt COM marker resolve + SetStmt ctrl.Property写入路径
- `src/rtl/lib/vb6rtl_gui.lib` — 重新编译
- `src/rtl/lib/vb6rtl.lib` — 重新编译
- `src/rtl/lib/vb6rtl_dll.lib` — 重新编译
