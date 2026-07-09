---
AIGC:
  ContentProducer: '001191110102MAD55U9H0F10002'
  ContentPropagator: '001191110102MAD55U9H0F10002'
  Label: '1'
  ProduceID: 'da1892de-637a-4d9f-9ce4-2a69ecfbb891'
  PropagateID: 'da1892de-637a-4d9f-9ce4-2a69ecfbb891'
  ReservedCode1: 'bb5f3904-04c1-4627-9e20-58b26cbed6ab'
  ReservedCode2: 'bb5f3904-04c1-4627-9e20-58b26cbed6ab'
---

---
AIGC:
  ContentProducer: '001191110102MAD55U9H0F10002'
  ContentPropagator: '001191110102MAD55U9H0F10002'
  Label: '1'
  ProduceID: '169e3f99-ed98-4b6b-a204-01e59df67d5b'
  PropagateID: '169e3f99-ed98-4b6b-a204-01e59df67d5b'
  ReservedCode1: '8c158494-eddd-4311-8ee3-9074c4b1e6ce'
  ReservedCode2: '8c158494-eddd-4311-8ee3-9074c4b1e6ce'
---

---
AIGC:
  ContentProducer: '001191110102MAD55U9H0F10002'
  ContentPropagator: '001191110102MAD55U9H0F10002'
  Label: '1'
  ProduceID: 'a4ee9162-2d19-4659-b870-4af4df573e76'
  PropagateID: 'a4ee9162-2d19-4659-b870-4af4df573e76'
  ReservedCode1: 'b596a38f-d27d-44e7-abee-0ac53674459f'
  ReservedCode2: 'b596a38f-d27d-44e7-abee-0ac53674459f'
---

# P24-08: COM错误传播 — HRESULT→VB6 Err.Raise

日期: 2026-07-07
阶段: P24-08 (COM错误传播)
关联: P24-05 (frxParse OLEAUT32崩溃修复)

## 问题背景

frxParse中 `dic.Add "hello", "wo" & Now` 每次点击按钮都会调用,
第二次Add同一个key时, VB6应该报Error 457 "This key is already associated with
an element of this collection", 但c3编译的程序静默忽略了错误。

## 根因分析

`vb6_ComCall`及所有COM调用函数(vb6_ComGetProp, vb6_ComSetProp等)在
`FAILED(hr)`时只执行 `fwprintf(stderr, ...)` 然后返回NULL — COM错误被静默忽略,
没有转换为VB6运行时错误(`vb6_RaiseError`)。

VB6的正确行为: COM方法失败时自动触发Err.Raise, 可被On Error GoTo/Resume Next捕获。

## 修复内容

### 新增辅助函数 `vb6_ComCheckError`

```c
static void vb6_ComCheckError(HRESULT hr, EXCEPINFO* excep, const wchar_t* context) {
    // 1. 从EXCEPINFO.scode提取VB6错误号 (HRESULT低16位)
    int32_t errNum = (int32_t)(excep->scode ? (excep->scode & 0xFFFF) : (hr & 0xFFFF));
    // 2. 从EXCEPINFO.bstrDescription获取错误描述, 转移所有权给vb6_RaiseError
    // 3. 清理EXCEPINFO其他BSTR (bstrSource, bstrHelpFile)
    // 4. 调用vb6_RaiseError(errNum, desc)
    //    - On Error Resume Next: 返回继续执行
    //    - On Error GoTo: longjmp跳到错误处理器
    //    - 无错误处理: exit(errNum)
}
```

### 修改7处FAILED(hr)路径

| 函数 | 原行为 | 新行为 |
|------|--------|--------|
| vb6_ComCall | fwprintf→return NULL | vb6_ComCheckError→return NULL |
| vb6_ComGetProp | fwprintf→return NULL | vb6_ComCheckError→return NULL |
| vb6_ComGetPropArg | fwprintf→return NULL | vb6_ComCheckError→return NULL |
| vb6_ComSetProp | fwprintf→return | vb6_ComCheckError→return |
| vb6_ComSetRef | fwprintf→return | vb6_ComCheckError→return |
| vb6_CreateObject (CLSIDFromProgID) | fwprintf→return NULL | vb6_RaiseError(429,"ActiveX component can't create object")→return NULL |
| vb6_CreateObject (CoCreateInstance) | fwprintf→return NULL | vb6_RaiseError(429,"ActiveX component can't create object")→return NULL |

### COM错误码→VB6错误号映射

COM的`IDispatch::Invoke`在方法失败时返回`DISP_E_EXCEPTION (0x80020009)`,
实际错误码在`EXCEPINFO.scode`中:
- `EXCEPINFO.scode & 0xFFFF` = VB6错误号
- 例: Dictionary.Add重复key → scode=0x800A01DF → VB6 Error 457
- `EXCEPINFO.bstrDescription` = 错误描述文本

## 验证结果

- Dictionary.Add重复key测试: 退出码457 ✅ (之前静默忽略)
- frxParse编译+运行: ✅ 正常
- 76/76回归测试: ✅ 零失败

## 修改文件清单

| 文件 | 改动 |
|------|------|
| src/rtl/core/vb6com.c | 新增vb6_ComCheckError辅助函数; 7处FAILED(hr)替换为COM错误传播 |
| ai/004-进度表.md | 新增P24-08行 |

> AI生成

> AI生成

> AI生成