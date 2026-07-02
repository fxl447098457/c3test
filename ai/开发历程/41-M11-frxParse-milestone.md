# M11 - frxParse 工程解析+编译成功

**日期**: 2026-07-02
**提交**: (待commit)
**回归测试**: 74/74 PASS
**里程碑**: M11 达成

## 背景

用户创建了 frxParse 测试工程，包含 .frm + .frx + .vbp，用于验证 C3 编译器对真实 VB6 工程的解析和编译能力。该工程包含：
- Form 的 Icon + Picture（2 个 frx 引用）
- ImageList1（5 个 ListImage，每个有 Picture）
- TextBox 的 Text（frx 中存多行文本）
- ListBox 的 ItemData + List（frx 中存列表项）
- Command1/Option1/Option2/Check1(0/1/2).Picture（6 个图形按钮）
- Picture1.Picture、Image1.Picture
- Object= MSCOMCTL.OCX 引用

## 修复项

### 1. frm_parser: 跳过 Object= 行 (根因修复)
- **问题**: .frm 文件在 VERSION 行后、Begin 行前有 `Object = "{831FDD16...}"; MSCOMCTL.OCX` 行，frm_parser 不认识这行，导致 Begin...End 块未被解析，窗体描述泄漏到 codeSection
- **修复**: frm_parser.cpp 在 VERSION 解析后添加循环跳过 Object= 行和空行
- **效果**: --dump-frm 显示 11 个控件全部正确识别，CodeSection 从 5107 缩减到 176 字符

### 2. RC winver.h 依赖修复
- **问题**: 生成的 version_info.rc 包含 `#include <winver.h>`，rc.exe 没有 SDK include 路径导致编译失败
- **修复**: 移除 `#include`，将 VOS_NT_WINDOWS32/VFT_APP/VFT2_UNKNOWN 替换为数值常量

### 3. RC 资源名修复
- **问题**: RC 文件使用 `VS_VERSION_INFO VERSIONINFO`，资源名是字符串 "VS_VERSION_INFO"，Windows API 期望数字 ID 1
- **修复**: 改为 `1 VERSIONINFO`
- **效果**: GetFileVersionInfoSizeW 正确返回非零值，VersionInfo 字段全部可读

### 4. RC UTF-8 编码修复
- **问题**: RC 文件含中文字符(工程1)，rc.exe 默认按 ANSI 解码
- **修复**: RC 文件头部添加 `#pragma code_page(65001)`

### 5. msvc_driver: GUI/Console 分支 .res 链接修复
- **问题**: .res 文件追加只在 isDll 分支实现，isGui 和控制台分支缺失
- **修复**: 在三个分支(isDll/isGui/console)都添加 typelibResFile/versionInfoResFile/userResFile 追加逻辑

## 验证结果

- frxParse 工程编译成功，生成 142KB EXE
- EXE 运行正常，窗口标题 "Form1"
- 版本信息正确嵌入：
  - CompanyName: home
  - FileDescription: 工程1
  - ProductVersion: 1.0.0.0
  - FileVersion: 1.0.0.0
  - OriginalFilename: 工程1.exe
- 74/74 回归测试零失败

## 文件变更清单
| 文件 | 变更 |
|------|------|
| src/project/frm_parser.cpp | +12行 Object= 跳过逻辑 |
| src/driver/driver.cpp | RC 生成: 去 winver.h, 数值常量, code_page pragma, 资源名改1 |
| src/backend/msvc_driver.cpp | +18行 isGui/console 分支 .res 链接 |
