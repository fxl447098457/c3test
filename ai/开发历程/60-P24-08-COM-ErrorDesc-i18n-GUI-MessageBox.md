---
AIGC:
  ContentProducer: '001191110102MAD55U9H0F10002'
  ContentPropagator: '001191110102MAD55U9H0F10002'
  Label: '1'
  ProduceID: '09c84d7f-8e15-4370-9b60-ccdd0fa37431'
  PropagateID: '09c84d7f-8e15-4370-9b60-ccdd0fa37431'
  ReservedCode1: '3f33f23f-7d6d-4a52-9ad8-51d599892f56'
  ReservedCode2: '3f33f23f-7d6d-4a52-9ad8-51d599892f56'
---

# P24-08 补充: COM错误描述i18n + GUI错误弹窗

日期: 2026-07-08
阶段: P24-08 (COM错误传播补充)
关联: 59-P24-08-COM-Error-Propagation.md

## 问题背景

P24-08实现了COM错误传播, 但存在两个遗留问题:

1. **GUI程序错误不可见**: `vb6_RaiseError`在无On Error处理器时直接`exit(errNum)`,
   GUI程序没有控制台, 用户只看到窗口闪退, 无任何错误提示。

2. **COM错误描述缺失**: Dictionary.Add重复key时, VB6应显示
   "此键已经与该集合中的一个元素关联", 但c3编译的程序显示
   "COM error in ComCall: 0x80020009" 或 "发生意外"。

## 根因分析

### 问题1: GUI程序闪退

`vb6_RaiseError`未处理错误路径只有`fwprintf(stderr)+exit()`,
GUI程序没有stderr, 用户看不到任何提示。

### 问题2: COM错误描述丢失 (关键发现)

通过32位测试程序`test_dic_error2.c`实证分析Dictionary.Add重复key的COM错误:

```
HRESULT: 0x80020009              ← DISP_E_EXCEPTION (通用异常)
HRESULT & 0xFFFF = 9             ← hr低16位是9不是457!
EXCEPINFO.scode: 0x800A01C9      ← 真正的VB6错误码
EXCEPINFO.scode & 0xFFFF = 457   ← scode低16位才是VB6错误号
EXCEPINFO.bstrDescription: (null) ← COM对象不填描述!
GetErrorInfo: S_FALSE             ← 没有IErrorInfo!
FormatMessageW(0x80020009): "发生意外" ← 系统通用描述, 无用
FormatMessageW(0x800A01C9): (not found) ← VB6应用级错误号系统查不到
```

**结论**: scrrun.dll (Dictionary) 只填EXCEPINFO.scode, 不填bstrDescription,
也不通过SetErrorInfo提供IErrorInfo。VB6真实行为是从MSVBVM60.DLL内部错误表
按错误号查找本地化描述。

### FormatMessageW vs VB6错误表优先级问题

初始实现中FormatMessageW在VB6错误表之前, 导致:
1. `FormatMessageW(0x80020009)` 返回"发生意外"(中文系统通用描述)
2. VB6错误表457→"此键已经与该集合中的一个元素关联"永远不会被查到

修复: 当EXCEPINFO.scode有效(有VB6错误号)时, VB6查表必须优先于FormatMessageW。

## 修复内容

### 修复1: vb6_RaiseError区分GUI/CLI (vb6rtl.c)

```c
// 未设置错误处理: GUI程序弹MessageBox, CLI程序输出stderr
if (GetConsoleWindow()) {
    // CLI程序: 输出到stderr
    fwprintf(stderr, L"Unhandled VB6 Error #%d: %ls\n", ...);
} else {
    // GUI程序: 弹出VB6风格错误对话框
    wchar_t msg[512];
    swprintf(msg, 512, L"Run-time error '%d':\n%ls", errNum, ...);
    MessageBoxW(NULL, msg, L"VB6 Runtime Error", MB_ICONERROR | MB_OK);
}
exit(errNum);
```

新增`#pragma comment(lib, "user32.lib")`和`"kernel32.lib"`。

### 修复2: vb6_ComCheckError五级fallback (vb6com.c)

最终确定的错误描述来源优先级:

| 优先级 | 来源 | 说明 |
|--------|------|------|
| 1 | EXCEPINFO.bstrDescription | COM对象直接提供 |
| 2 | IErrorInfo::GetDescription | COM错误接口 |
| 3 | VB6标准错误号中文查表 | **scode有效时优先**, 60+常见错误号 |
| 4 | FormatMessageW(FROM_SYSTEM) | 仅当VB6表查不到时(纯系统HRESULT) |
| 5 | 默认HRESULT | "COM error in \<context\>: 0xXXXXXXXX" |

关键调整: VB6查表(级3)在FormatMessageW(级4)之前, 因为:
- DISP_E_EXCEPTION(0x80020009)的FormatMessageW返回"发生意外"等无用通用描述
- 当scode有效时, VB6错误表才有正确的错误描述

### 修复3: VB6标准错误号中文查表 (vb6com.c)

新增`vb6_StdErrorDesc()`函数, 覆盖60+常见VB6运行时错误号:
- 错误5: "无效的过程调用或参数"
- 错误9: "下标越界"
- 错误13: "类型不匹配"
- 错误91: "对象变量或With块变量未设置"
- 错误429: "ActiveX组件不能创建对象"
- 错误457: "此键已经与该集合的一个元素关联"
- ... (完整覆盖VB6标准错误5~746)

### 修复4: vb6_CreateObject Error 429增强 (vb6com.c)

Error 429消息现在包含ProgID和HRESULT:
```
ActiveX组件不能创建对象 (ProgID: XXXXX, HRESULT: 0xXXXXXXXX)
```
CoCreateInstance失败时也尝试IErrorInfo获取更详细描述。

### 修复5: vb6forms.c _vb6_forms_trace未定义

`_vb6_forms_trace`调试函数未定义导致链接错误,
替换为`fwprintf(stderr, ...)`。

## i18n方案决策

用户提出i18n需求后, 讨论了四种方案:

| 方案 | 优点 | 缺点 |
|------|------|------|
| A. MSVBVM60.dll LoadString | 自动跟随系统语言 | 不是所有系统都有此DLL |
| B. RTL资源STRINGTABLE | 标准Win32做法 | 工程量大 |
| **C. FormatMessageW+中文表兜底** | 系统HRESULT自动本地化+VB6表中文兜底 | 当前只支持中文 |
| D. 暂时英文表 | 最简 | 用户体验差 |

**选定方案C**: FormatMessageW(FROM_SYSTEM)提供系统级HRESULT本地化描述,
VB6应用级错误号用内置中文查表兜底, 后续可扩展为STRINGTABLE多语言。

## 验证结果

- frxParse窗口正常显示 ✅ (不再闪退429)
- frxParse第一次点击Add → MsgBox显示字典值 ✅
- frxParse第二次点击Add → 弹出"Run-time error '457': 此键已经与该集合的一个元素关联" ✅
- Dictionary.Add重复key → 退出码457 ✅
- 76/76回归测试 → 零失败 ✅

## 修改文件清单

| 文件 | 改动 |
|------|------|
| src/rtl/core/vb6rtl.c | vb6_RaiseError区分GUI/CLI; 新增user32/kernel32 pragma |
| src/rtl/core/vb6com.c | vb6_StdErrorDesc中文查表(60+错误号); vb6_ComCheckError五级fallback; CreateObject Error 429增强 |
| src/rtl/core/vb6forms.c | _vb6_forms_trace→fwprintf |

> AI生成