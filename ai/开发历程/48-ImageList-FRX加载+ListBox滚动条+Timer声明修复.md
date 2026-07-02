# 48 - ImageList FRX图片加载 + ListBox滚动条 + Timer前向声明修复

> 日期: 2026-07-03
> 关联: 47-ImageList-Picture切换+Graphical禁用修复

## 问题背景

frxParse 测试项目三个运行时 bug：
1. ListBox 没有自动滚动条
2. ImageList 图片未从 .frx 加载（CoCreateInstance 创建空对象，ListImages.Count=0）
3. ImageList1.ListImages.Count 返回 0

额外发现：Timer 回调函数缺少前向声明导致编译失败。

## 修复内容

### Bug 1: ListBox 自动滚动条

**文件**: `src/backend/cgen_form.cpp`

ListBox 样式中缺少 `WS_VSCROLL`。VB6 的 ListBox 在项目超出可见区域时自动显示垂直滚动条。

```cpp
// 修复前
style |= kLbsNotify | kWsBorder;
// 修复后
style |= kLbsNotify | kWsBorder | kWsVscroll;  // VB6 ListBox always shows vertical scrollbar
```

### Bug 2: ImageList 图片未从 .frx 加载（核心修复）

**根因**: `parsePropertyBlock` 只解析扁平属性行，不支持嵌套 `BeginProperty`。ImageList 的 `BeginProperty Images` 块内嵌套了 `BeginProperty ListImage1..5` 块，每个 ListImage 有 `Picture = "Form1.frx":偏移`。

**方案**（不使用专用 RTL 函数，走通用 COM 运行时）：

#### 2a. 数据结构扩展 (`frm_parser.hpp`)

`FrmPropertyBlock` 新增：
- `std::string blockGuid` — 提取 GUID（如 `{2C247F25-8591-11D1-B16A-00C0F0283628}`）
- `std::vector<FrmPropertyBlock> nestedBlocks` — 嵌套 BeginProperty 块

#### 2b. 递归解析 (`frm_parser.cpp`)

`parsePropertyBlock` 改为：
- 解析 `BeginProperty Name {GUID}` 行，分离 blockName 和 blockGuid
- 块体内遇到 `BeginProperty` 时递归调用自身，结果存入 `nestedBlocks`
- 属性行支持带点号的键（如 `Object.Tag`）

#### 2c. 通用 RTL 函数 (`vb6forms.c` + `vb6forms.h`)

新增 `vb6_LoadPictureAsCom(data, size)` — 从内存加载图片返回 `IPictureDisp*`（IDispatch*）：
- 使用 `OleLoadPicture` + `IID_IPictureDisp`（而非 `IID_IPicture`）
- 返回的 IDispatch* 可直接传给 COM 方法的 Picture 参数
- 调用方负责 Release（通过 `vb6_ReleaseObject`）

这是**通用函数**，不是 ImageList 专用——任何 COM 控件需要图片参数都能用。

#### 2d. 代码生成 (`cgen_form.cpp`)

ImageList 的 CoCreateInstance 后不再直接 `continue`，改为：

1. 通过 `vb6_ComSetProp` 设置 `ImageWidth`/`ImageHeight`（必须在 Add 之前设置）
2. 遍历 `propertyBlocks` 找 "Images" 块
3. 对每个嵌套 `ListImage` 块：
   - `FrxReader::readPicture(offset, true)` 读取 ImageList 格式图片数据（28字节头部含16字节GUID）
   - `bytesToHexArray()` 生成 .h 中的静态字节数组
   - 运行时通过通用 COM 调用链加载：
     ```c
     void* lstImgs = vb6_ComGetObjectProp(imgList, L"ListImages");
     void* picCom = vb6_LoadPictureAsCom(data, size);
     void* args[] = { vb6_ComPackInt(index), vb6_ComPackBSTR(L"key"), vb6_ComPackObject(picCom) };
     vb6_ComCall(lstImgs, L"Add", args, 3);
     vb6_ReleaseObject(&picCom);
     vb6_ReleaseObject(&lstImgs);
     ```

### Bug 3: ListImages.Count 返回 0

Bug 2 修复后 Count 应正确返回 5。但实测发现 **MSCOMCTL.OCX 是纯 32 位 COM 组件**，64 位进程的 CoCreateInstance 返回 `REGDB_E_CLASSNOTREG (0x80040154)`。这是环境限制，不是编译器 bug。

### Bug 4 (额外): Timer 回调前向声明缺失

**文件**: `src/backend/cgen_form.cpp`

Timer 创建时引用回调函数（如 `vb6_Timer1_Timer`）但没有前向声明，C 编译器报 "undeclared identifier"。

```cpp
// 修复前
c_.emitLine("vb6_SetTimer(" + interval + ", (void*)" + timerFn + ");");
// 修复后
c_.emitLine("{ extern void " + timerFn + "(); vb6_SetTimer(" + interval + ", (void*)" + timerFn + "); }");
```

## 修改文件清单

| 文件 | 变更 |
|------|------|
| `src/project/frm_parser.hpp` | FrmPropertyBlock 新增 blockGuid + nestedBlocks |
| `src/project/frm_parser.cpp` | parsePropertyBlock 支持嵌套递归 + GUID 解析 |
| `src/rtl/core/vb6forms.c` | 新增 vb6_LoadPictureAsCom(通用) |
| `src/rtl/core/vb6forms.h` | 新增 vb6_LoadPictureAsCom 声明 |
| `src/backend/cgen_form.cpp` | ListBox 加 WS_VSCROLL; ImageList 加载图片; Timer 加 extern |

## 测试结果

- **74/74 回归测试零失败**
- frxParse 项目编译成功
- ListBox 滚动条正常显示
- ImageList 代码生成正确（5 个 ListImage 的字节数组 + COM 加载链路）
- MSCOMCTL.OCX 的 32 位限制需另案处理（64 位进程无法加载 32 位 COM 组件）

## 已知遗留问题

1. **MSCOMCTL.OCX 32 位限制**: 64 位 EXE 无法加载 32 位 ActiveX 组件。解决方案：
   - 编译 32 位 EXE（需要 c3 支持 x86 目标）
   - 使用进程外 COM 代理（surrogate）
2. **LSet 固定字符串中文乱码**: `LSet a = "等等"` 后的字符串在 ListBox 中显示乱码，疑为 `vb6_BSTR_FromStr` 编码链路问题（非本次引入）
