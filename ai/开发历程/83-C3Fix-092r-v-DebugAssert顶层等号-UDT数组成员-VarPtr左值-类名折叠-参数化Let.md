# C3Fix - Debug.Assert 顶层等号 / UDT 数组成员元素类型 / VarPtr 左值 / 类名折叠 / 参数化 Let (092r-v)


**日期**: 2026-09-11
**里程碑**: 092r-v
**回归**: 无窗体 125 模块 bisect 16 → **11**（每步单独验证，均无回归）
**提交**: f799e55(092r) / b6ff125(092s) / 7559ad2(092t) / 50de9b7(092u) / bad7b27(092v)

本轮 5 项覆盖三个层次：**解析层**（092r）、**类型推断/生成层**（092s/092t）、**调用形态生成**（092u/092v）。
共同点：都是「VB6 语法里看似简单、但 C 侧类型/调用约定与之不同」的地方。

## 092r（16→15）Debug.Assert 的条件可含顶层 `=`
```c
vb6_DebugAssert(((int32_t)(lSignatureScheme) & (int32_t)(255))) = ((int32_t)(513) & (int32_t)(255));
```
源码（mdTlsThunks.bas）：
```vb
Debug.Assert (lSignatureScheme And &HFF&) = (&H201 And &HFF&)
```
语句级先 `parseExpression(9)`（minBp 阻止把 `=` 当比较）→ 已吃掉 `Debug.Assert (<cond>)`，
随后 1090 的 `match(TokenKind::Equals)` 把它当**赋值**，生成 `vb6_DebugAssert(<cond>) = <rhs>` → C2186
（`=` 左侧是 void）。

修法：在赋值检测**之前**判断 `expr` 是否 `IndexOrCallExpr(callee = MemberAccess(Debug.Assert), 1 个实参)`
且当前 token 是 `=` → 消费 `=`、解析右侧、把两者合成 `BinaryExpr(BinaryOp::Eq)` 作为**唯一实参**，
交回调用路径生成 `vb6_DebugAssert(<cond>);`。
无括号形式 `Debug.Assert x = 1` 无需处理 —— 那里的 `parseExpression()` 本就会把 `=` 当比较。

**判读**：VB6 的「语句关键字 + 表达式」结构（Debug.Assert/Print、Set、Call）容易与赋值语句的
`=` 检测互相干扰；凡此类关键字，都要在赋值检测前判定。

## 092s（15→14）UDT 数组成员元素类型 + VarPtr 左值宏
```c
(*uOutput).rgRDNAttr = (intptr_t)&(void*){VB6_SA_AT(vb6_VARIANT, (*uOutput).Buffer, 0)};   /* C2440 */
CopyMemory((void*)&(VB6_SA_AT(vb6_VARIANT, (*uOutput).Buffer, lPos)), ...);               /* 静默错位 */
```
源码 `uOutput.rgRDNAttr = VarPtr(uOutput.Buffer(0))`，`Buffer() As CERT_RDN_ATTR`。

两个独立缺陷：
1. **元素类型退化**：`mapSaElemCType(Vb6Type::UserDefinedType)` → `vb6_VARIANT`，
   而 UDT 数组成员的元素应是 `vb6_type_<UDT>`（`mi.typeRefName`）。第二行是**静默语义错**
   （元素大小不同 → 拷贝/取址全部错位），比编译错误更危险。补在 MemberAccess 链与 With 两条生成点。
2. **VarPtr 误判非左值**：`VB6_SA_AT`/`VB6_SA_ND_ATn` 宏展开是 SafeArray 元素，属**左值**，
   却因「以标识符开头且含 `(`」被当作函数调用 → `&(void*){...}` 复合字面量。加白名单走 `&(...)`。
   注意 `vb6_VariantArrayGet` **按值**返回，不能入白名单（否则 C2102）。

## 092t（14→12）TypeName(Me) 折叠为类名
```c
vb6_TypeName(me)          /* me 是 vb6_cls_pvSubClass*, 形参是 vb6_VARIANT → C2440 */
```
`TypeName(Me)` 两次。**类名在编译期就是已知的**（vbp 的 `Class=` 名 == `moduleName_`，这里 = `pvSubClass`），
所以直接折叠：`vb6_BSTR_FromStr(L"pvSubClass")`。

这不只是绕过类型错误的权宜之计 —— 它是**语义上唯一正确**的形态：
把类实例包成 Variant 再交给 `vb6_TypeName`，VT_DISPATCH 只能得到 `"Object"`，而 VB6 要求返回真实类名。

## 092u（12→11）类内参数化 Property Let 赋值
```c
Extend(&_arr_1, &(vb6_VARIANT){0}) = vb6_Split(...);   /* C2106: "=" 左侧必须为左值 */
```
源码 `Extend(Array(A, C)) = Split(...)`（cToolsArray.cls 97，`Sub test` 内隐式 me）。
`emitExpr(target)` 走「读方向」生成了裸函数名 + 补的 value 占位；AssignmentStmt 再拼 ` = <RHS>`。

修法：在 AssignmentStmt 里识别「target 是 `Ident(args...)`、该名解析为**当前类**的写方向属性
（`findClassMemberWriteParams(moduleName_, name, isSet, out)`）、形参数 == 实参数 + 1」→ 按写方向形参表生成：
```c
vb6_VARIANT _pv1_0 = vb6_VariantFromValue(_arr_2);
vb6_VARIANT _pv1_1 = vb6_VariantFromValue(vb6_Split(...));
vb6_cToolsArray_prop_let_Extend((void*)me, &_pv1_0, &_pv1_1);
```
**两次踩坑（都在同一步 bisect 内暴露，值得记住）**：
1. ByRef 实参必须是可寻址对象 —— `&(vb6_VARIANT){<结构体值>}` 触发 C2440，改落**栈变量**再取址；
   非左值（函数结果/字段链）同样落栈变量，否则 `&` 报 C2102。
2. **含 Optional 形参时必须放弃本路径**：Optional 在 C 侧另有存在标志参数
   （`Property Let ThunkPrivateData(pThunk, Optional ByVal Index, ByVal lValue)` →
   `(me, IUnknown**, int32_t, int32_t, int)`），只按符号表拼参会 C2198（cAsyncSocket 2611）。
   收窄条件后该处回退到原有的 `prop_get_` 重写路径（Pattern C/D2），保持原正确输出。

## 092v（无回归）`Mod.Member(...)` 的前缀应取限定模块名
`Common.Version()` 生成 `vb6_cVBMAN_Version(&(void*){0}, 0)`（缺 me → C2198）。
诊断打印（`fprintf` + bisect + grep）一次给出关键事实：
```
[DBG092v] objSym=(null) kind=-1 | memSym=Version kind=3 src=cVBMAN ext=1 | commonExt=0()
```
- `objSym = null` → `Common` **不在符号表**；
- `memSym` 命中的是 `cVBMAN.Version`（全局首个同名 Function）；
- `commonExt = 0` → 也没有 `sourceModule == "Common"` 的 external 符号。

查证：`Common` 是**外部 ActiveX 引用**的模块名（`Common4DLL.bas`/`Common4EXE.bas` 的 `VB_Name` 都是 `Common`），
且**不在 vbman 任何 vbp 内** → 属「Reference/TypeLib 级模块命名空间符号缺失」的配置缺口，
不是本项目代码缺陷（`SymbolKind::ComModule`/`ComGlobalNs` 枚举已存在，缺导入）。

补丁仍保留：把 `sourceMod` 的决策改为「限定符对应 external 符号所属模块的规范名」优先 ——
这修掉了一类真实隐患（限定名与 `memSym->sourceModule` 不一致时取错模块），本场景只是暂无符号可用。
**约定**：这类「无效果但方向正确、且零回归」的改动单独提交并在信息里说明，不混入其它修复。

## 方法学沉淀
1. **诊断打印优先于读代码**：092o/092u/092v 都是 `fprintf(stderr, ...)` + 一次 bisect + grep
   `vbman/src/_fix/bisect/run.log`（注意：**C3.exe 的输出在 run.log，cl.exe 的错误在 c3-error.log**）
   就锁定了分支，比翻 cgen 的两万行快得多。
2. **同一处改动可能同时暴露两个缺陷**：092u 首版引发 C2102+C2198（取址方式 + Optional 形参），
   修完才收敛。收窄触发条件（`!hasOptional`）比"补齐所有细节"更安全。
3. **区分三类失败**：编译错误（真错）／静默语义错（更危险，如 092m/092s 的属性丢失与元素错位）／
   配置假阳性（窗体 7 个、Common 452）。基线数字只有在标注了类型后才有意义。

## 残留（11，真实 2 + 缺口 1）与下一步
见 `C3_FIX_HANDOFF.md`：
1. Demo_Database 506（C2440+C2197）— `TestDB.Rows(1)("score")` 连续索引的外层实参被并入内层
   `prop_get_Item`（3 实参 vs 2 形参）；方向是按内层结果类型生成
   `vb6_ComCall(vb6_VariantToObjectVal(<inner>), L"Item", ...)`；
2. cHttpServer 821（C2037）— `Request->Header->Exists(...)`，COM 成员方法应走后期绑定；
3. cHttpServerResponse 452 — 见 092v 的配置缺口说明。
