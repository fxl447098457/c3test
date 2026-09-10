# C3Fix - 返回变量提取 / 字面量数组实参 / String+BSTR 拼接 / With 成员路径 / NULL 取址 (092f-l)


**日期**: 2026-09-10
**里程碑**: 092f-l
**回归**: 无窗体 125 模块 bisect 32 → **23**（092f/092g/092i/092j/092k/092l 各步单独验证；092h 试探失败已回退）
**提交**: 866e814(092f) / 288c4ea(092g) / a29240f(092i) / 504e86b(092j) / aa98df1(092k) / 5c5bf65(092l)

## 现象与修复

### 092f（32→31）返回变量不在任何「目标类型」判定内
```c
vb6_SafeArray1D* vb6_ret_ReturnBody = 0;
vb6_ret_ReturnBody = vb6_VariantFromComResult(vb6_ComCall(...));  /* C2440 → vb6_SafeArray1D* */
```
赋值提取路径的判据是「UDT 字段 (`inferUdtFieldVb6Type`) / 数组元素 (`VB6_SA_AT`)」，
`vb6_ret_ReturnBody` 两者都不是 → `inferUdtFieldVb6Type` 返回 Unknown → 直接赋值。
补「目标是 `currentReturnVar_` → 按 `currentReturnCType_` 提取」（SafeArray1D*/BSTR/void*/double/整型）。

### 092g（31→30）已由生成器构造好的数组又提取一次
```c
vb6_Join(vb6_VariantToSafeArray1D(_arr_0), ...)   /* C2440: _arr_0 已经是 SafeArray1D* */
```
`Join(Array(a,b,c), " ")` 的 `Array(...)` 由 cgen 展开为匿名 `_arr_0` + `vb6_ArraySetBSTR` 逐项填充
（C 侧类型已是 `vb6_SafeArray1D*`），却被形参类型判定按「Variant 数组」再提取。加 `_arr_` 前缀排除。

### 092h（试探失败，已回退）把「可接受 Variant」误当成「需要包成 Variant」
`TypeName(Me)` 需 `vb6_VariantFromValue(me)`。第一版按
`getRuntimeParamCType(callee, i) == "vb6_VARIANT"` 泛化触发 → bisect **30 → 81**：
```c
vb6_CStrLong(vb6_VariantFromValue(Index))   /* vb6_CStrLong 形参是 int32_t */
```
根因：表中 `{"vb6_CStr", {"vb6_VARIANT"}}` 的语义是「该函数**可接受** Variant 实参」，
被反用为「实参**需要**包装成 Variant」，且 `CStr` 的实参在后续还会被按类型替换为 `CStrLong/CStrBSTR`。
收窄为 `vb6_TypeName`/`vb6_VarType` 白名单后不再回归，但**内建调用不经过该实参分支**（`calleeParams` 为空、
生成走内建识别分支）→ 对 pvSubClass 543/544 无效，遂整体回退。

**教训**：① 复用查表的语义方向要明确（输入转换 vs 输出转换）；
② 改动前先确认目标调用**走的是哪条生成路径**（用户函数实参段 ≠ 内建识别分支），否则改对了逻辑也无效。

### 092i（30→28）类型推断「未知」导致拼接退化为算术
```c
vb6_BSTR_Assign(&GB_UrlDecode, (GB_UrlDecode + vb6_Chr(d)));   /* C2110 指针相加 + C2198 */
```
源码 `GB_UrlDecode + Chr$(d)`（`GB_UrlDecode As String`）。`inferExprType(Add)` 要求**整体**推断为 String，
右侧是内建调用时推断失败 → 落到算术 `+`。而相邻的 `GB_UrlDecode + c`（c As String）正常。
补「左 String + 右侧 BSTR 返回调用白名单 → 按拼接处理」。

**教训**：VB6 `+` 兼具算术/拼接双重语义，依赖类型推断；只要一侧类型推断失败就会静默走错分支
（这里恰好报错，若目标是字符串变量则可能静默生成指针相加）。

### 092j（28→27）With 对象引用的「类型推断」与「C 级表达式」不一致
```c
void* _vb6_with_2 = (void*)vb6_VariantFromComResult(vb6_ComCall(...));   /* C2440 */
```
`inferClassTypeOfExpr(With 目标)` 非空（按 VB 声明推成项目类）→ 走 `(void*)` 硬转兜底；
但 C 级表达式实际是 Variant。兜底分支补 `cExprIsVariant(lastExpr_)` → `vb6_VariantToObjectVal(...)`。

### 092k（27→24）两级以上成员路径被 `cIdent` 拍平
```c
vb6_SafeArrayDestroy1D(_vb6_with_30->MessBuffer_Data);  /* C2039: 无此成员 */
```
`Erase .MessBuffer.Data` → `"->" + cIdent("MessBuffer.Data")` → `cIdent` 把 `.` 规范化为 `_`
（该规范化是为「标识符含点」的历史场景设计）→ 成员名被拍平。改为逐段展开（首段 `->` 后续 `.`）。

**教训**：`cIdent` 会修改 `.`；任何「先拼路径再调 cIdent」的写法对多级成员都是错的。

### 092l（24→23）`NULL` 不是常量标识符
```c
vb6_cTlsSocket_FireOnCertificate((void*)me, &NULL);   /* C2101 常量上的 & */
```
ByRef 形参取址路径已处理「常量标识符」(`isConstIdent` → `wrapConstArgForByRef`)，但 `NULL` 不在常量表 →
落到 `"&" + argVal`。补 `NULL`/`nullptr` → 按形参 C 类型生成复合字面量 `(&(<cType>){0})`。

## 本轮副产物与判读
- 再次确认 **cLogs×5 / cLayer×2 共 7 个错误是窗体假阳性**（`.frm` 模块不在 bisect 编译集），
  真实剩余目标 **16 个**。
- 发现两处**静默语义错误**（不报编译错但行为错，优先级应高于编译错误）：
  ① cHttpServer 372/373 `Server.LocalIP/LocalPort` → 生成成 `me->m_oServer`（成员访问整体丢失）；
  ② pvSubClass 708 `For .. To .Count` 的 With COM 属性 → 生成成 `_vb6_with_2` 本身（同时 `i = _vb6_with_2`）。
- 定型「赋值右值 Variant → 具体目标类型」的提取矩阵（cgen_stmt）：数组元素 / UDT 字段 / **返回变量(092f)**；
  反向「具体类型 → Variant 形参」的包装矩阵仍缺内建调用入口（092h 遗留）。

## 残留（23）与下一步
见 `C3_FIX_HANDOFF.md`：C2440×5（含上述 2 处静默错误 + pvSubClass 543/544 + Demo_Database 506 参数个数）、
C2039×4（COM 方法调用 / 成员名大小写规范化 / 类方法被当字段 / `tagSAFEARRAY` 字段）、
C2198×4（类方法可选参数未补齐 + 窗体假阳性）、零散 C2106/C2171/C2186/C2037。
