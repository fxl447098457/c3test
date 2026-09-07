# C3Fix - 只写属性 LHS 参数多传 (090ad: Dictionary.key C2197 x2)

**日期**: 2026-09-07
**里程碑**: 090ad（+ 090w 死代码激活）
**回归**: 109 → 107*（*全量被 C3 崩溃阻塞，Dictionary 单模块 0 错）
**提交**: 2415265

## 背景

Dictionary.c 两处 C2197（110/162）：

```c
vb6_Dictionary_prop_let_key(me->m_Dict, OldKey, vb6_BSTR_Empty(), NewKey);
/* Property Let via prop_get_ rewrite (Pattern C/D2) */
```

VB 源（Dictionary.cls RenameKey/内部 Let key 体）：

```vb
Public Property Let key(ByVal OldKey As String, ByVal NewKey As String)
    m_Dict.key(OldKey) = NewKey   ' Scripting.Dictionary COM 带参属性写(重命名键)
```

对比同文件其他写属性（Item/Value 等）均正常 3 参——它们都有 `Property Get`
（读上下文选 Get → prop_get_Item(obj, 索引) 恰好无补齐 → rewrite 追加 value）。

## 根因

`key` 属性**只有 Property Let 无 Get**。emitExpr 处理 LHS `m_Dict.key(OldKey)`
时，resolveClassMemberCall 读上下文 fallback 到 Let（prop_let_key），
IndexOrCallExpr 参数补齐（`args.size() < calleeParams.size()` → 逐形参 pad
默认值）把缺失的 **value 形参 NewKey** 也 pad 成 `vb6_BSTR_Empty()` →
target = `prop_let_key(obj, OldKey, vb6_BSTR_Empty())`。随后
tryRewriteCOMLvalue Pattern C/D2 认为 target 是 prop_get 读形式，**追加** RHS
value → 4 实参 vs 声明 3 形参 → C2197。

## 修复（cgen_util.cpp tryRewriteCOMLvalue Pattern C/D2）

matchedVerb != prop_get_ 分支（emit 已按 Let/Set 生成 target——只写属性场景）：
1. `findClassMemberCallParams(clsCd2, afterPg)` 取业务形参表 paramsCd2；
2. `splitTopLevelArgs(argsStr)` 顶层分割括号实参（新增 lambda，跳过嵌套括号）；
3. 若段数 == paramsCd2.size() + 1（对象 + 全部形参含被 pad 的 value 位）→
   pop 末段，finalArgs 拼回，再拼真实 valArg。

## 附带 bug：090w/090ad 的 prefix.substr(5)

`prefix` 形如 `vb6_Dictionary_`。去掉 `vb6_` 应 `substr(4)`，代码误写 `substr(5)`
（多去类名首字符 → `ictionary_`/`Json_`→`Json`？不——`vb6_cJson_` substr(5)=`Json_`，
与 className 比对失败）。后果：**090w（prop_let Variant 末参打包）此前从未生效**
（死代码，findClassMemberCallParams 恒 false）。修正 substr(4) 后激活：
- 值实参 → `vb6_VariantFromValue(v)`（_Generic：标量→Long/Double、BSTR→String、
  SafeArray→Array、vb6_VARIANT→VariantIdentity no-op、指针→Object）——对已打包
  实参是 Identity，无二次包装风险。
- 影响面：仅 prop_let 改写且末形参 As Variant 的调用点（cJson.Item Dat、
  cCsv.Value 等），打包只会消除 C2440。

## 验证

- fix_tool（ASAN C3）单模块 Dictionary.cls：**0 error**；生成
  `vb6_Dictionary_prop_let_key(me->m_Dict, OldKey, NewKey)` 3 参匹配声明。
- cCsv.cls 裁剪：4 错（`d(0).Root` C2039、VariantToObjectVal 参数太少、
  void*→vb6_VARIANT）与 HEAD 全量基线同形——非本修复引入，留待下轮。
- **全量回归受阻**：release C3 崩于 cVBMAN.c 前收尾；ASAN C3 崩点漂移
  （Dictionary 模块后 ↔ Form Picture1.Align），内存损坏特征（无 ASAN report）。
  见 HANDOFF「阻塞项」。

## 残留（下轮候选）

1. C3 崩溃 bug（阻塞全量回归，优先级最高）
2. cCsv.c `d(0).Root`/`Set d(0)` COM/成员（C2039 Root 非成员、VariantToObjectVal 参数）
3. cAliyunCaptcha.c `With .ReturnJson()` 字段函数化（With 目标为方法返回对象）
4. cHttpServerResponse.c VBMAN.Version 参数形状 / `->socket` 成员
5. cCollection prop_get_Item x1、C2440 SafeArray* ↔ vb6_VARIANT 各族
