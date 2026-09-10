# C3Fix - COM 标记边界 / With 成员解析 / 字段名大小写规范化 / ParamArray (092m-q)


**日期**: 2026-09-10
**里程碑**: 092m-q
**回归**: 无窗体 125 模块 bisect 23 → **16**（每步单独验证，均无回归）
**提交**: 53da196(092m) / 7757dee(092n) / d6a4dcf(092o) / 8217f5f(092p) / ff133f4(092q)

本轮 5 项全部围绕同一主题：**成员/属性解析与 `isComMarker_` 全局状态的边界**。三者互相关联，
共享一条判读：*`isComMarker_` 是「生成器全局状态」，任何语句或子表达式边界都必须显式消费或清除它，
否则会跨语句/跨子表达式错配*。

## 092m（23→22）With 类字段写不消费 COM 标记
```c
_vb6_with_6->IP       = me->m_oServer;                                  /* 属性名丢失 */
_vb6_with_6->Port     = me->m_oServer;                                  /* 属性名丢失 */
_vb6_with_6->ConnectAt = vb6_ComCall(me->m_oServer, L"RemotePort", NULL, 0);  /* 串位 + C2440 */
```
源码（cHttpServer.cls 429-431）：
```vb
With CI
    .IP = m_oServer.RemoteHostIP
    .Port = m_oServer.RemotePort
    .ConnectAt = Now()
End With
```
`m_oServer` 被识别为 **typed COM** → `MemberAccessExpr` 只返回对象并设置
`comObjExpr_/comMemberName_/isComMarker_`，契约是「下游语句消费标记补 COM 调用」。
但 cgen_stmt 的「With class field write」路径直接 `tempVar->member = lastExpr_`，**不消费** →
① 本语句丢属性名（编译能过，**语义错**）；② 标记残留到下一条语句（`.ConnectAt` 位置出现 `RemotePort`）。

修法：字段写路径补 `resolveComValue(hint)`。难点是 hint 需要**目标字段类型**，而类符号原本只有
方法/属性表，没有字段表 → 新增 `Symbol::memberFieldTypes`（语义分析 `VariableDecl` 分支登记
+ driver 跨模块拷贝），并加 `classFieldComUnpackHint()` 映射：
String→`ComGetStringProp` / Long→`ComGetIntProp` / Date|Double→`ComGetDoubleProp` / Object→`ComGetObjectProp`。
修复后：
```c
_vb6_with_6->IP       = vb6_ComGetStringProp(me->m_oServer, L"RemoteHostIP");
_vb6_with_6->Port     = vb6_ComGetIntProp(me->m_oServer, L"RemotePort");
_vb6_with_6->ConnectAt = vb6_Now();
```
**判读**：编译错误只是冰山一角 —— 同一处还藏着 2 个静默语义错误，修「标记未消费」时要把三条语句一起验证。

## 092n（22→21）残留标记被 UnaryExpr 误消费
```c
int32_t i_end  = 1;
int32_t i_step = (-vb6_ComGetStringProp(_vb6_with_2, L"Count"));   /* Step 变成 -<Count> */
i              = _vb6_with_2  /* With COM .Count */;               /* 初值丢属性 */
```
源码 `For i = .Count To 1 Step -1`（clsSubClass.cls 654-655）。`ForStmt` 依次生成 start/end/step，
start 设置标记后**未消费**，step 的 `-1` 生成时 `UnaryExpr` 里既有的
`if (isComMarker_) resolveComValue()` 把残留标记消费掉并取负 → C2171。

修法：For 的三个子表达式「生成前清标记、生成后按 Long 消费」（`emitForExpr092n` lambda）。
修复后 `i = vb6_ComGetIntProp(_vb6_with_2, L"Count"); i_step = (-1);`。

**判读**：`UnaryExpr`/`IfStmt` 里已有「顺手消费标记」的代码 —— 这类"防御式消费"在标记泄漏时会把
错误传播到完全无关的表达式。根治方向是**在语句边界清除**，而不是到处消费。

## 092o（21→20）With 目标表达式用错了栈帧
```c
vb6_cls_cJson* _vb6_with_4 = (vb6_cls_cJson*)_vb6_with_3->ReturnJson  /* With class .ReturnJson field */();
```
源码 `With .ReturnJson()`（嵌套在 `With Http` 内，cAliyunCaptcha.cls 327）。
`WithStmt` 生成顺序是 **先 push 本次 `withInfo`，再 `emitExpr(*node.object)`** →
生成目标表达式时栈顶已是**内层** info（className = 目标表达式的推断类型 `cJson`），
于是 `.ReturnJson`（实为 cHttpClient 的成员）在 cJson 上解析失败 → 退化为字段访问。

诊断手段（值得复用）：在 `visit(MemberAccessExpr)` 的成员解析处临时打印
`className / clsFound / memberNames.size() / hasMember / resolved / asCallCallee`，
一次 bisect 就锁定「className='cJson'」这一关键事实（否则很容易往 Phase A/B 符号匹配方向走偏）。

修法：目标表达式生成期间保持**外层**栈顶，之后再 push 本帧（BuiltinObject 分支无目标表达式，单独 push）。

## 092p（20→18）VB6 大小写不敏感 vs C 结构体成员名
```c
vb6_cTlsReMaster_SendData((void*)me->Client->socket, ...);   /* C2039: 成员是 Socket */
```
源码 `Client.socket.SendData Head & vbCrLf`（cHttpServerResponse.cls 487，小写 `socket`）。
C 结构体成员按**声明**生成（`Socket`），而访问点直接 `cIdent(源码名)`。

修法：新增 `Symbol::memberFieldNames`（lower → 声明原名）+ `canonicalClassFieldName()`，
在类字段访问/With 字段写生成点规范化为声明名。
**遗留**：全仓还有 6 处 `"->" + cIdent(node.memberName)` 未覆盖（见 handoff 提醒②）。

## 092q（18→16）For Each 遍历 ParamArray
```c
vElem = vb6_VariantFromValue(VB6_SA_AT(vb6_VARIANT, a, _fe_i112));   /* C2039: tagSAFEARRAY 无 data */
```
源码 `Private Sub pvArrayByte(baRetVal() As Byte, ParamArray a() As Variant)` 内 `For Each vElem In a`
（mdTlsThunks.bas 5236+）。ParamArray 形参的 C 类型是 `SAFEARRAY*`（cgen 参数映射），
而 `vb6_LBound/vb6_UBound/VB6_SA_AT` 都要求 `vb6_SafeArray1D*`（宏展开访问 `->data`/`->lBound`）→
C2039 ×2，且 `vb6_LBound(a,1)` 是**静默运行时错**（把 Windows SAFEARRAY 当 vb6 结构解析）。

修法：For Each 的集合是当前过程的 ParamArray 参数时改用 `vb6_PA_LBound`/`vb6_PA_UBound`/
`vb6_PA_GetVariant`（判定方式与 091k 一致：遍历 `currentProc_->params` 找 `isParamArray`）。
第二版修正：`vb6_PA_GetVariant` 返回 Windows `VARIANT`，与 `vb6_VARIANT` 不是同一类型（首版直接赋值 C2440），
需经 `vb6_VariantFromComResult(&tmp)` 桥接 —— 生成两行（临时量 + 赋值），临时量在循环块内声明。

## 方法学沉淀
1. **先定位生成路径再动手**：092h 的失败（改对了逻辑但内建调用不走那条路径 → 30→81）与本轮 092o
   都说明，动手前必须确认「目标代码实际经过哪个分支」。临时诊断打印（一次 bisect）通常比反复读代码快。
2. **静默错误优先**：本轮 092m/092n/092q 各修掉 1-2 处「编译通过但语义错」（属性丢失、Step 错位、
   上下界语义错）。这类问题在编译错误清零后会变成运行时 bug，越早修越省事。
3. **全局状态要有边界契约**：`isComMarker_`/`lastExpr_` 这类生成器全局状态，需要明确「谁设置、谁消费、
   何处清除」。当前是「谁需要谁消费」的松散约定，已被证明会在语句/子表达式之间串位。

## 残留（16，真实 9）与下一步
见 `C3_FIX_HANDOFF.md`：C2440×4（cTlsSocket 4093 Variant→void*、Demo_Database 506 属性参数个数、
pvSubClass 543/544 `TypeName(Me)` 需内建实参包装）、C2198×3（cHttpServerResponse 452 类方法 Optional 未补齐）、
C2037/C2186/C2106/C2197 各 1（COM 接口方法调用、`Debug.Assert` 赋值解析、参数化属性赋值、C2197）。
