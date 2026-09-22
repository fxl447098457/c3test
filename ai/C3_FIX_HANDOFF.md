# C3 编译器错误修复 — 任务交接文档

> 用途：新会话恢复上下文用。新开会话后直接说「读取 C3_FIX_HANDOFF.md 并继续修复」。
> 更新日期：2026-09-11（091a-092z：无窗体 125 模块 bisect 基线 65 → **7**、含窗体全量 129 条目 → **0**；提交 aeaa95f/05793ac/ae72bf2/d8f11aa/e97a90d/02205f5/ad62a09/96d123a/7e58595/ac78b3c/36d83a3/1c8daf7/db139c9/d5806d8/821e0ea/02e49a3/8a8eac2/866e814/288c4ea/a29240f/504e86b/aa98df1/5c5bf65/53da196/7757dee/d6a4dcf/8217f5f/ff133f4/f799e55/b6ff125/7559ad2/50de9b7/bad7b27/ac69fc0/e0df780/ec7fa10/4a03e23/97eb89c。**剩余 7 全部为窗体假阳性**（cLogs/cLayer 引用的 `.frm` 不在 bisect 编译集）→ 无窗体面已收敛。**092z 起新增「含窗体」窗口**：`-Forms` 开关 + 排除陈旧对象后，**含窗体全量（129 条目，含全部 4 个 `.frm`）error C = 0** —— 编译期全绿。**窗体 Release 崩溃「阻塞项」已解除**：根因并非未初始化/UB，而是 `.build` 的**陈旧对象**（`cmake --build .build --clean-first` 全量重建后该崩溃即消失）→ 不必再上 cdb 抓栈；下一步转入**链接期**（LNK2005 重定义 + 由源码 `Declare ... Lib "msvbvm60"` 生成的 `msvbvm60.lib` 引用；后者 x64 无库可补，须改道 RTL））

### 最近修复摘要（092r-z：无窗体 bisect 基线 16 → **7**、含窗体全量 → **0**；提交 f799e55/b6ff125/7559ad2/50de9b7/bad7b27/ac69fc0/e0df780/ec7fa10/4a03e23/97eb89c，2026-09-11）

- **092r（16→15）Debug.Assert 条件的顶层 `=` 不再被当赋值**。`Debug.Assert (a And &HFF&) = (&H201 And &HFF&)`：语句级 `parseExpression(9)` 已吃掉 `Debug.Assert (<cond>)`，随后的 `=` 被当赋值 → `vb6_DebugAssert(<cond>) = <rhs>` C2186（ToolsTlsThunks 4733）。修法：在赋值检测前识别 `IndexOrCallExpr(callee = Debug.Assert, 1 个实参)` + 当前 token 是 `=`，把 `=` 右侧并回断言条件（`BinaryExpr(Eq)` 作为唯一实参）。
- **092s（15→14）UDT 数组成员元素类型 + VarPtr 左值宏白名单**。① `mapSaElemCType(UserDefinedType)` 退化 `vb6_VARIANT` → `uOutput.Buffer(i)`（`Buffer() As CERT_RDN_ATTR`）元素大小/取址全错（cTlsSocket 4086/4093，其中 4086 是**静默语义错**）；改用 `vb6_type_<UDT>`（`mi.typeRefName`），覆盖 MemberAccess 链与 With 两条生成点。② `VarPtr(uOutput.Buffer(0))` 生成 `(intptr_t)&(void*){VB6_SA_AT(...)}` C2440 — `VB6_SA_AT`/`VB6_SA_ND_ATn` 展开是**左值**，加入白名单走 `&(...)`（`vb6_VariantArrayGet` 按值返回，不入白名单）。
- **092t（14→12）TypeName(Me) 折叠为类名字面量**。`Me` 在 C 侧是 `vb6_cls_X*`，`vb6_TypeName` 形参是 `vb6_VARIANT` → C2440（pvSubClass 543/544）。类名编译期已知（vbp 的 `Class=` 名 == `moduleName_`）→ 生成 `vb6_BSTR_FromStr(L"pvSubClass")`；这也是语义上唯一正确形态（经 Variant 包装对象只会得到 `"Object"`）。
- **092u（12→11）类内参数化 Property Let 赋值**。`Extend(Array(A, C)) = Split(...)`（cToolsArray.cls 97，隐式 me）此前落到通用路径：`emitExpr(target)` 生成裸名 + pad 的 value 占位 `Extend(&_arr_1, &(vb6_VARIANT){0})`，再拼 ` = <RHS>` → C2106。修法：AssignmentStmt 中识别「target 是 `Ident(args...)`、该名是**当前类**的写方向属性、形参数 == 实参数+1」→ 生成 `vb6_<Cls>_prop_let_<Name>((void*)me, &Vars, &Value)`（ByRef Variant 落栈变量取址；非左值 ByRef 也落栈变量以免 C2102）。**收窄条件**：含 Optional 形参时不走此路径（Optional 在 C 侧另有存在标志参数，cAsyncSocket 2611 曾因此 C2198）。
- **092v（无回归）模块限定调用前缀以限定模块名为准**。`Mod.Member(...)` 的 `sourceMod` 原取 `memSym->sourceModule`；当限定名是模块而全局存在同名过程时会取错模块（命中 `cVBMAN.Version` 而非 `Common.Version`）。改为优先用「限定名对应的 external 符号所属模块规范名」。注意 cHttpServerResponse 452 的根因是 `Common`（外部 ActiveX 引用的模块）**不在任何 vbp 内**，符号表无该模块 → 属配置/缺口（见下）。
- **092w（11→9）链式默认属性访问 `X(a)(b)` 走后期绑定**。Demo_Database 506 `TestDB.Rows(1)("score")`：外层 `("score")` 的实参被并入内层 `prop_get_Item` 调用（3 实参 vs 签名 2）→ C2197 + C2440。修法（cgen_expr.cpp `IndexOrCallExpr` 拆链 Fix 086 分支）：① 内层 `_prop_get_` 调用已自带实参（`classMethodObjArg` 顶层段数 ≥ 2，即 this + ≥1 实参）时判定内层自足，外层索引改走返回对象的默认成员 `Item` 后期绑定；② 内层函数在 `variantReturnFuncs_` 中（返回 `vb6_VARIANT`，如 `cCollection.prop_get_Item`）时先经 `vb6_VariantToObjectVal` 提取对象指针再传 `vb6_ComCall`（否则 Variant 直传 void* C2440）；③ `wrapToBSTR` 增 `vb6_VariantFromComResult(` 前缀识别（须在 `vb6_BSTR` 子串检查之前，实参可能含 `vb6_BSTR_FromStr`）。
- **092x（9→8）COM 接口字段链式成员访问走后期绑定**。cHttpServer 821 `Request.Header.Exists("Cookie")`：`Header As Dictionary` 是 COM 接口字段，生成 `Request->Header->Exists(...)` 结构体成员访问 → C2037（`vb6_ComIface_IDictionary` 无结构定义）。根因：内层 MAE 值上下文字段访问仅查 `classVoidFieldMap_` 决定是否加 `voidptr` 标记，而 driver 预扫描把 COM 接口字段记入 `classTypedFieldMap_`（值前缀 `COM:<Interface>`）且**不入** `classVoidFieldMap_` → 无标记 → 外层 MAE Fix 088b `inferClassTypeOfExpr` 遇 `COM:` 前缀返回空串 → 落入 `obj->member`。修法：内层 MAE 追加 `voidptr` 标记时同时查 `classTypedFieldMap_` 的 `COM:` 前缀条目 → 外层 Fix 023 识别 → 生成 `vb6_ComCall(Request->Header, L"Exists", ...)`，与 `cSSE.c` `RootItem.Exists` 既有形态一致（注：`vb6_ComCall(...) == (-1)` 比较指针属**既有**架构形态，非本修复引入）。
- **092y（8→7）模块限定调用前缀兜底取工程模块规范名**。cHttpServerResponse 440 `Common.Version()` 生成 `vb6_cVBMAN_Version(&(void*){0}, 0)`（缺 `me`）→ C2198。**根因（修正 092v 的判读）**：`Common` **不是**外部 ActiveX 引用，它就是 `VBMAN.vbp` 第 65 行的标准模块（`Module=Common; Common\Common4DLL.bas`，`Public Function Version(Optional HostApp As Object)`）。真实根因是 `storageKey(Function) == lowerName` → `Common.Version`/`Common.Path` 与 `cVBMAN.Version`/`cVBMAN.Path` **四个成员两两同键**，`defineExternal` 先到先得 → Common 的成员**全部**被丢弃 → 消费模块作用域内不存在任何 `sourceModule=="Common"` 的 external 符号 → 092v 的 external 扫描落空 → `sourceMod` 退回 `memSym->sourceModule`("cVBMAN")。修法（cgen_expr.cpp 模块限定调用前缀判定）：092v 扫描失败时**兜底查 `externalModules_`**（工程内除本模块外的全部模块名、规范大小写）→ 命中且 `symTab_.lookup(extMod)` 非 `SymbolKind::Class` 时采用其规范名（与既有 3 处 `toLower(extMod)` 比对惯例一致）。仅对非类模块生效，不改 `类名.成员` / `窗体名.成员` 语义。结果 `vb6_Common_Version(&(void*){0}, 0)` 与签名 `BSTR(void**, int)` 匹配。
- **092z（含窗体全量 1→0）内置全局对象成员不再走同名项目成员形参兜底**。`FLogs.frm` 175 `Clipboard.SetText Text1.Text` 生成 `vb6_Clipboard_SetText(vb6_VariantFromValue(vb6_GetControlText(vb6_hwnd_Text1)))` → C2440（`vb6_VARIANT`→`BSTR`）。**根因**：`cQRcode.cls:34` 定义 `Public Function SetText(ByVal Content As Variant)`，IndexOrCallExpr 的 **class-unaware 兜底** `lookupModule("SetText")` 命中该 external 符号 → `calleeParams` 被填成 `[ByVal Variant]` → Fix 086 / Fix 024P2 把实参包装成 Variant；但 callee 早已由 `Clipboard` 硬编码分支解析为 RTL 函数 `vb6_Clipboard_SetText(BSTR)`，**根本不存在 VB 侧形参表**。修法：对象为 VB6 保留全局对象（`clipboard/screen/printer/forms/debug/err/app`）时跳过该兜底，`calleeParams` 保持为空。**教训：与 092y 同族 —— 「名字撞车」是本地最高频根因**（092y = `storageKey(Function)==lowerName` 使 `Common.Version` 与 `cVBMAN.Version` 撞键；092z = 跨类同名 `SetText` 撞车）。
- **窗体 Release 崩溃「阻塞项」解除（重大纠正）**：此前 092v 记录的「release 独崩 0xC0000005、最小批次 38 = FLayer.frm、疑似未初始化/UB、建议上 cdb」**结论有误**。实测：`.build` 增量构建的二进制崩，而 ① `cmake --build .build --clean-first` 全量重建后**含窗体全量直接跑到 0 错**；② RelWithDebInfo（`/Ob1`）与 Release+PDB（`/O2 /Ob2 /Zi`）都不崩 —— 即崩因是**陈旧对象**（本文件「注意事项」第 344/385 行早有同类记录：增量构建产生陈旧对象 → 运行期 0xC0000409，clean-first 解决）。**因此不必再上 cdb**，含窗体回归可直接作为常规验证面。另：`--dump-frm` 单测 `FLayer.frm` 正常（exit 0）→ 窗体**解析**阶段无问题，崩点在其后。

### 历史：092m-q（无窗体 bisect 基线 23 → **16**；提交 53da196/7757dee/d6a4dcf/8217f5f/ff133f4，2026-09-10）

- **092m（23→22）With 类字段写消费 COM 标记 + 类字段类型表**。`With CI` 内 `.IP = m_oServer.RemoteHostIP` 走「数据字段写」路径但**不消费** `isComMarker_` → `lastExpr_` 只含对象（`.IP = me->m_oServer`，静默丢属性）+ 标记泄漏到下一条语句（`.ConnectAt = ... L"RemotePort"` C2440，cHttpServer 372/373/374）。修法：字段写路径调 `resolveComValue(hint)`，hint 取自新增 `Symbol::memberFieldTypes`（语义分析登记类字段 VB 类型 + driver 跨模块拷贝）→ String→`ComGetStringProp` / Long→`ComGetIntProp` / Date→`ComGetDoubleProp`。
- **092n（22→21）For 子表达式独立消费 COM 标记**。`For i = .Count To 1 Step -1`：start 只得到对象本身，且**残留标记被 Step 表达式里 UnaryExpr 的 `if (isComMarker_) resolveComValue()` 消费** → `i_step = (-vb6_ComGetStringProp(_vb6_with_2, L"Count"))` C2171（pvSubClass 708/709）。改为每个子表达式生成前清标记、生成后按 Long 消费。
- **092o（21→20）With 目标表达式在外层上下文中生成**。`With .ReturnJson()`（嵌套 With）：`withObjectInfoStack_.push_back(withInfo)` 发生在 `emitExpr(*node.object)` **之前**，生成目标表达式时栈顶已是**内层** info（className=cJson）→ `.ReturnJson`（实为 cHttpClient 成员）解析失败退化为数据字段（cAliyunCaptcha 222 C2039 + 实参丢失）。改为目标表达式生成后再入栈（BuiltinObject 分支单独入栈）。
- **092p（20→18）类字段名规范化回声明名**。VB6 大小写不敏感：源码 `Client.socket.SendData`（小写）直接 `cIdent` → `me->Client->socket`，而 C 结构体成员名按声明是 `Socket` → C2039 + C2198（cHttpServerResponse 508）。新增 `Symbol::memberFieldNames`（lower → 声明原名）+ `canonicalClassFieldName()`，用于类字段访问与 With 字段写生成点。
- **092q（18→16）For Each 遍历 ParamArray 用 PA 专用 API**。ParamArray 形参 C 类型是 `SAFEARRAY*`，而 `vb6_LBound/vb6_UBound/VB6_SA_AT` 都要求 `vb6_SafeArray1D*`（`(arr)->data` / `->lBound`）→ C2039 ×2 且上下界取值语义错（ToolsTlsThunks 5468，`mdTlsThunks.bas` 的 `For Each vElem In a`）。改用 `vb6_PA_LBound`/`vb6_PA_UBound`/`vb6_PA_GetVariant`；Windows `VARIANT` 需经 `vb6_VariantFromComResult(&tmp)` 转 `vb6_VARIANT`（首版直接赋值触发 C2440）。

### 历史：092f-l（无窗体 bisect 基线 32 → **23**；提交 866e814/288c4ea/a29240f/504e86b/aa98df1/5c5bf65，2026-09-10）

- **092f（32→31）返回变量赋值按返回 C 类型提取 Variant**。`Function ReturnBody() As Byte()` 内 `ReturnBody = Inst.ResponseBody` → `vb6_ret_ReturnBody = vb6_VariantFromComResult(...)` C2440（cHttpClient 399）。`inferUdtFieldVb6Type` 对 `vb6_ret_X` 返回 Unknown、返回变量不落入任何目标类型判定 → 在赋值提取路径补「目标是当前返回变量 → 按 `currentReturnCType_`（SafeArray1D*/BSTR/void*/double/整型）提取」。
- **092g（31→30）字面量数组实参跳过 Variant 数组提取**。`Join(Array(a,b,c), " ")` 的 `Array(...)` 生成为匿名 `_arr_0`（C 侧已是 `vb6_SafeArray1D*`），却被按「Variant 数组形参」提取 → `vb6_Join(vb6_VariantToSafeArray1D(_arr_0))` C2440（cPLI 56）。形参为数组且实参前缀 `_arr_` 时跳过提取。
- **092h（尝试，已回退）TypeName/VarType 实参包装**。`TypeName(Me)`（pvSubClass 543/544）需 `vb6_VariantFromValue(me)`，但内建调用不经过该实参分支，改动无效；且**泛用** `getRuntimeParamCType(...) == "vb6_VARIANT"` 会把 `CStr(Index)` 提前包成 `vb6_CStrLong(vb6_VariantFromValue(Index))`（bisect 30→**81**）。**教训：该表的 "vb6_VARIANT" 语义是「该函数可接受 Variant 实参」，不能反用为「实参需要包装成 Variant」。** 已 `git checkout` 回退。
- **092i（30→28）String + BSTR 返回调用视为拼接**。源码 `GB_UrlDecode + Chr$(d)`（`GB_UrlDecode As String`）—— `inferExprType(Add)` 因右侧是内建调用而推不出 String → 退化为算术 `+` → `(GB_UrlDecode + vb6_Chr(d))` 触发 C2110 指针相加 + C2198（cToolsHttp 280）。左操作数为 String 且右侧是 BSTR 返回调用（`vb6_Chr(`/`vb6_Mid(`/`vb6_BSTR_` 等白名单）时按拼接处理。
- **092j（28→27）With 对象引用为 Variant 值**。`With me->LangInfo.Item("LangList").Item(Idx+1)` —— `inferClassTypeOfExpr` 非空（按声明推断为项目类）走 `(void*)` 硬转兜底，但 C 级表达式是 `vb6_VariantFromComResult(...)` → C2440（cLang 144）。兜底分支补 `cExprIsVariant(lastExpr_)` 判定 → 改用 `vb6_VariantToObjectVal(...)`。
- **092k（27→24）With 多级成员路径逐段展开**。`Erase .MessBuffer.Data` 经 `cIdent("MessBuffer.Data")` 把 `.` 变 `_` → `_vb6_with_N->MessBuffer_Data` C2039（ToolsTlsThunks 1780，同时 C2198）。`resolveArrayTargetIdent` 的 With 分支改为逐段展开（首段 `->`，后续段保留 `.`）。
- **092l（24→23）NULL 实参改用复合字面量取址**。`FireOnCertificate(me, NULL)` 的 ByRef 形参取址生成 `&NULL` → C2101（cTlsSocket 440）。`NULL`/`nullptr` 与 `isConstIdent` 同等处理，按形参 C 类型生成 `(&(<cType>){0})`。

### 历史：092a-e（无窗体 bisect 基线 36 → **32**；提交 d5806d8/821e0ea/02e49a3/8a8eac2，2026-09-10）

- **092a（36→35）BSTR 形参提取跳过条件：子串 → 顶层**。`paramBase == String` 分支原用 `argVal.find("vb6_BSTR")` 子串判定"已是 BSTR"→ `StrConv(LoadResData("AES.CBC","JSCRIPT"), 64, 0)` 的实参内部含 `vb6_BSTR_FromStr(` → 整体跳过提取 → C2440（cAesCBC 25）。改为 4 项**顶层前缀**白名单（`vb6_BSTR_` / `VB6_SA_AT(BSTR,` / `vb6_VariantToString(` / `vb6_BSTR_FromStr(`），与 5080 处同源。
- **092b（35→34）Object/COM 变量的属性写不再命中他类 Property Let**。`Rs As Object` 的 `Rs.Filter = <COM 属性>` 被全局 `lookupModuleByKind("Filter", PropertyLet)` 命中 cDialog.Filter，生成 `vb6_cDialog_prop_let_Filter(Rs, ...)`（C2440 + 丢 COM 语义）。在 cgen_stmt 的类属性写路径加 `objClass.empty()` 时的 Object/COM 变量判定（`knownObjectVars_` / `knownTypedComVars_`）→ 跳过，交回 COM 写路径（收窄 Fix 084g-2 的"isExternal 即生成"）。
- **092c（34→33）RHS 为 Variant 返回变量**。`Function ShowOpen(...) As Variant` 内 `mData.mstrFileName = ShowOpen` 的 RHS 生成 `vb6_ret_ShowOpen`（裸返回变量名）—— `cExprIsVariant` 只认函数前缀、`knownVariantVars_` 只认 VB 名 → 未识别为 Variant → 不做提取（cDialog 393 C2440）。在赋值通用提取路径加"`value == currentReturnVar_ && currentReturnCType_ == "vb6_VARIANT"`"判定。
- **092e（33→32）ByRef 数组形参的复合字面量类型**。`Decode(ByRef Utf() As Byte)` 的 C 形参是 `vb6_SafeArray1D**`（Fix 078 rev2），但调用点 `mapType(Byte|Array)` 给出 `uint8_t*` → `(&(uint8_t*){Variant})` C2440（cHttpClient 418）。ByRef 数组形参统一按 `vb6_SafeArray1D**` 生成复合字面量，并在 Variant 提取映射中补 `vb6_SafeArray1D** → vb6_VariantToSafeArray1D`。
- **重要判读：cLogs/cLayer 的 7 个 C2065/C2198 是无窗体 bisect 假阳性**（`FLogs`/`FLayer` 是 `.frm` 窗体：`vb6_FLogs_Visible`、`vb6_cCsv_ShowTo(&Content, ...)` 漏 me 等），窗体模块本就不在 bisect 编译集，不应作为修复目标。
- **两个窗口的残留**：① **无窗体 125 = 7**（C2065×5 + C2198×2，092y 后已无真实代码缺陷）—— 7 个**全部**是 cLogs/cLayer 的窗体假阳性：cLogs×5（`vb6_FLogs_Visible` 未声明 ×3、`FLogs` 未声明 ×1、`vb6_cTimeUse_Show` 参数少 ×1）、cLayer×2（`FLayer` 未声明、`vb6_cCsv_ShowTo` 参数少）。二者引用的 `FLogs`/`FLayer` 都是 `.frm` 窗体，**窗体模块不在无窗体 bisect 编译集内** → 无窗体面已收敛，这 7 错**不必再修**（纳入窗体后自动消失）。② **含窗体全量 129 = 0 error C**（2026-09-11，092z 后）→ **编译期全绿**；继续推进应转向**链接期**：
  1) ~~**Demo_Database 506（C2440 + C2197）**：`TestDB.Rows(1)("score")` 连续索引（默认属性链）~~ → **已修（092w）**：外层索引不再并入内层 `prop_get_Item`，改为对返回 Variant 结果做 `vb6_ComCall(vb6_VariantToObjectVal(<inner>), L"Item", ...)` 后期绑定；
  2) ~~**cHttpServer 821（C2037）**：`Request->Header->Exists(...)` — `Header` 是 COM 接口字段，成员方法调用应走后期绑定~~ → **已修（092x）**：内层 MAE 追加 `voidptr` 标记时同时识别 `classTypedFieldMap_` 的 `COM:<Interface>` 条目 → 外层走 COM dispatch；
  3) ~~**cHttpServerResponse 452（C2198，原判为「配置缺口」）**：`Common.Version()`~~ → **已修（092y）**，且**原判读有误**：`Common` 并非外部 ActiveX 引用，而是 `VBMAN.vbp` 第 65 行的标准模块 `Module=Common; Common\Common4DLL.bas`；真实根因是 `storageKey(Function)==lowerName` 造成 `Common.{Version,Path}` 与 `cVBMAN.{Version,Path}` 全部同键、`defineExternal` 先到先得丢弃 Common 成员 → 092v 扫描落空。故**无需** Reference/TypeLib 级命名空间符号（`SymbolKind::ComModule`/`ComGlobalNs` 导入仍可作为独立可选项，但已不再是本错误的必要条件）；
  4) **方法学提醒**：① 内建调用实参路径 ≠ 用户函数实参路径（092h 未生效即此因），先在 `typeNameMap` 等**内建识别分支**定位；② VB6 大小写不敏感 — 所有 `cIdent(成员名)` 都需按声明规范化，092p 只覆盖了 3 个生成点，其余 6 处 `->" + cIdent(node.memberName)` 仍是同类隐患；③ `isComMarker_` 是全局状态，语句/子表达式边界必须显式消费或清除（092m/n 已覆盖 With 字段写与 For，If/While/Select/Call 实参等边界待查）；④ 泛用的实参包装改动极易引发大面积回归（092h：30→81），务必先用小集合验证并用 `ParameterInfo` 上下文收窄；⑤ 改生成器前先加**一次性诊断打印**（`fprintf(stderr, "[DBG...]")` + bisect + grep `run.log`）—— 092o/092u/092v 都靠它一次定位路径，比反复读代码快得多；⑥ **判断某生成形态是否「既有可接受」时，先在生成产物目录（temp 的 `C3C\<pid>\*.c`，非仓库内、不受 .gitignore 影响）用 `search_content` 搜同类形态**——092w/092x 都靠搜到 `cSSE.c` 现有 `RootItem.Exists(...)`/`vb6_ComCall(...) == (-1)` 快速确认与既有行为一致、无需再扩大改动面。注意 `run.log`（C3.exe 输出）与 `c3-error.log`（cl.exe 错误）分属两个文件；⑦ **交接文档里的「根因」结论也要复核** —— 092v 把 cHttpServerResponse 452 记为「`Common` 不在任何 vbp 内的配置缺口」，实际它就写在 `VBMAN.vbp` 第 65 行；误判让该错误被搁置了整整一轮。凡「外部模块 / 配置缺口」类结论，归档前先用 `search_content` 在 `.vbp` 中核实条目，并优先怀疑「同名符号 storageKey 冲突 / 跨类同名成员」这类更常见的内部原因；⑧ **任何「崩溃」结论落档前必须先排除陈旧对象** —— 本次「窗体 Release 崩溃（0xC0000005）」经查只是 `.build` 增量构建的陈旧对象，`--clean-first` 全量重建后含窗体回归直接跑到 0 错；092v 却把它当成「未初始化/UB」并计划上 cdb。规则：**见到 release-only 崩溃，先 `--clean-first` 全量重建再复现**，仍崩才考虑 UB。

### 历史：091p-s 摘要（无窗体 bisect 基线 41 → **36**；提交 ac78b3c/36d83a3/1c8daf7/db139c9，2026-09-10）

- **091p（42→41）类字段 Variant 判定**：`Set m_vUserData = Value`（cWinsock Property Set，字段 `Private m_vUserData As Variant`）被误判为 typed 目标 → 生成 `vb6_VariantToObjectVal(Value)` C2440。根因同 091m：`knownVariantVars_` 每过程 `clear()`，类字段不在其中；但字段访问带明确 `me->` 前缀，故新增 `classVariantFields_`（类模块字段声明时登记，**不回灌裸名集合**），Set 判定中按 `target.rfind("me->",0)==0` 单独查询。
- **091q（41→40）ForEach COM 集合源**：`For Each x In me.LangInfo.Item("LangList")` 生成 `vb6_ForEach_Init(vb6_VariantToObjectVal(vb6_ComCall(...)))` C2440（void*→vb6_VARIANT）—— `vb6_ComCall/vb6_ComCallObject` 已返回对象指针，`isDefinitelyVariantExpr` 的 fallback 不该再提取。加 `collIsObjPtr091q` 前缀排除（两个分支都加）。
- **091r（40→38）Debug.Print Variant 兜底**：`Debug.Print x`（x 是 Variant 循环变量）落入整数分支生成 `vb6_DebugWriteLong((int32_t)(x))` → C2440 + C2198。Debug 参数判定原来只查 `cExprIsVariant`（C 表达式级），新增 `isVariantVal091r` 兜底 `knownVariantVars_` / `classVariantFields_`（含 `me->`/`(*p)` 剥离与 `/* */` 注释后缀处理）。
- **091s（38→36）ByVal Variant 收 Object 实参**：`ToolsJsonVba.ConvertToJson(Json)`（`Json As Object` → void*，形参 `ByVal JsonValue As Variant`）未打包 → C2440。Fix 084d 的跳过条件（`inferClassTypeOfExpr` 非空即跳过）过宽：① 收窄为**项目类**（`symTab_.lookup(类名)->kind == SymbolKind::Class`）；② 实参是 `knownObjectVars_/knownTypedComVars_` 的 Object/COM 变量时**强制打包**（`inferClassTypeOfExpr` 对 Object 变量可能返回内建 "Object" 类名）。
- **残留（36 = C2440×12 + C2039×6 + C2198×6 + C2065×5 + 其它 7）**，下一步候选（按族）：
  1) **少 me 的跨模块方法调用**（C2198×3）：`vb6_cLayer` 的 `vb6_cCsv_ShowTo(&Content, &(int32_t){800}, 0)`、`vb6_cTimeUse_Show(&(BSTR){...}, 0)`、`vb6_cVBMAN_Version(&(void*){0}, 0)` —— 调用点漏传对象指针 me（VB 里像 `cCsv.ShowTo ...` / `Me.Version` 这类限定写法），同时 cHttpServerResponse 508 把对象表达式错传为 `(void*)me->Client->socket`；
  2) **窗体/全局对象引用**（C2065×5）：cLogs 的 `FLogs` / `vb6_FLogs_Visible`、cLayer 的 `FLayer` —— 窗体变量名未解析（应走全局单例或 `me->` 字段）；
  3) **Variant → 具体类型提取仍缺**（C2440×6）：cDialog 393（`me->mData.mstrFileName = vb6_ret_ShowOpen`）、cAesCBC 25（`vb6_StrConv(vb6_LoadResData(...), 64, 0)`）、cToolsList 25（属性 Let 的 BSTR 形参，091n 未命中）、cHttpClient 399/418（函数返回 SafeArray1D*/uint8_t* ← `VariantFromComResult`）、Demo_Database 506（`cCollection_prop_get_Item` 参数个数）；
  4) **BSTR 拼接误用指针算术**（C2110+C2198）：cToolsHttp 280 `vb6_BSTR_Assign(&GB_UrlDecode, (GB_UrlDecode + vb6_Chr(d)))` → 应 `vb6_BSTR_Concat`；
  5) **RTL 签名不匹配**：ToolsTlsThunks 1780 `vb6_SafeArrayDestroy1D(_vb6_with_30->MessBuffer_Data)`（参数太少）＋ 1780/5468 的 C2039（`MessBuffer_Data`/`data`/`lBound` 成员不存在）；cTlsSocket 4093、cLang 144、cHttpServer 374、cHttpClient 418 的 void*/double 桥接。

### 历史：091k-o 摘要（无窗体 bisect 基线 52 → **42**；提交 e97a90d/02205f5/ad62a09/96d123a，2026-09-10）

- **091k（52→48）ParamArray/Variant 参数边界三处**：
  - 通用实参转换中实参是当前过程的 **ParamArray 参数**时不再按 Variant 数组提取（此前 `UBound(OutVars)` → `vb6_PA_UBound(vb6_VariantToSafeArray1D(OutVars))` C2440，cToolsArray DeArray）；
  - **ByRef Variant 参数索引免取址**：`v(i)` 曾生成 `vb6_VariantArrayGet(&v, i)`，而 ByRef Variant 形参的 C 类型已是 `vb6_VARIANT*` → 改传裸名（cToolsArray prop_let_Extend/DeArray）；
  - **IsMissing 语义/表项修正**：RTL `int32_t vb6_IsMissing(SAFEARRAY*)` 是 ParamArray 专用（判断是否未传实参），cgen 此前把任意 Variant 实参传给它 → 现：ParamArray 名 → `vb6_IsMissing(pa)`；其它复合表达式（数组/ParamArray 元素）→ 常量 `(0)`（VB6 IsMissing 仅对 Optional Variant 可能为真）。运行时表项 `vb6_VARIANT*` → `SAFEARRAY*`。
- **091l/091m（48→46）Variant 目标赋值包装 + 模块级 Variant 回灌**：赋值 `Variant ← 具体类型值`（`Array(...)` 物化的 `_arr_N` SafeArray1D* 等）补 `vb6_VariantFromValue`；判定依赖 `knownVariantVars_`，而该集合在每个过程开始时被 `clear()` → 模块级 `Dim x As Variant` 在过程体内不可见 → 新增 `moduleVariantVars_`（模块级声明时登记，过程开始回灌，**必须排除类模块/窗体成员**——否则裸名生成 C2065，曾 +7 回归）。
- **091n（46→44）属性 Let 值参按写方向末参类型适配**：`obj.Prop = value` 的 prop_let_ 调用此前完全不打包 → 用 091a 的 `findClassMemberWriteParams(类, 成员, isSet, out)` 取末参：Variant 形参 + **数组载体**值 → `packLetValueArg`；BSTR 形参 + Variant 值 → `wrapToBSTR`。**不能用裸 `propLetSym->params`**（`lookupModuleByKind` 全局同名命中他类属性 → 误打包 +36 C2440）；也不要 `Unknown` 类型兜底（同样 +36）；值形态未确认是数组载体时不打包（否则 COM 属性 `Dictionary.CompareMode = 1` 被误打包 → 大回归）。清 cWinsock 1949/1965。
- **091o（44→42）命名实参 ByVal 适配**：`Name:=expr` 路径此前只做 `applyByRef`（ByRef 处理），漏了 ByVal 形参类型适配 → `oListenSocket.Accept(m_oSocket, UseTls:=bUseTls)`（第 4 参 `Optional ByVal UseTls As Variant`）C2440 int16_t→vb6_VARIANT。类对象实参保持指针直传（同 Fix 084d）。
- **残留（42 = C2440×17 + C2198×7 + C2039×6 + C2065×5 + 其它 7）**，下一步候选：
  1) ForEach COM 源 `vb6_VariantToObjectVal(vb6_ComCall(...))` → 应为 `vb6_VariantFromComResult`（cLang 26）；
  2) `vb6_Join(vb6_VariantToSafeArray1D(_arr_N))` — 091g 物化的 `_arr_N` 临时未登记 knownArrays_（cPLI 56）；
  3) 类 Variant 字段 `me->f = <void* COM 结果>`（cWinsock 172、Demo 857/859）；
  4) COM 对象属性 Let 的 BSTR 提取（cToolsList 25，091n 未命中：写方向表对 COM 类的处理需确认）；
  5) `vb6_cDialog_prop_let_Filter(rs, vb6_VariantFromComResult(...))` 类归属（cDialog 目标 vs rs 实际类）；
  6) Debug 强转调用参数量（pvSubClass 543/544，C2198 族）。

### 历史：091e-i 摘要（58 → 52，2026-09-10）

### 最近修复摘要（091e-i：无窗体 bisect 基线 58 → **52**；提交 d8f11aa 等，2026-09-10）

- **091e（未观测到变化，保留）**：`rtParamTypeUsable = i >= calleeParams.size() || calleeParams[i].type 是 Variant/Empty`（此前仅参数表为空才查运行时参数类型表）+ 参数表补 `{"vb6_StrConv", {"BSTR","int32_t","int32_t"}}`。逻辑正确但单独验证 58→58。
- **091f/091g（58→55→55）ForEach 数组源**：`For Each` 源为返回 `vb6_SafeArray1D*` 的内置调用时走**数组迭代路径**而非 COM 路径（此前生成 `vb6_ForEach_Init(vb6_VariantToObjectVal(vb6_Split(...)))` → C2440）。白名单：`Split/Filter`（元素 BSTR，`Vb6Type::String`）、`Array(...)`（vb6_ArrayCreate 建 **Variant 数组**，元素 `vb6_VARIANT`）。表达式集合先物化到 `_fe_arrN` 临时变量，避免 LBound/UBound/`VB6_SA_AT` 处重复求值（重新分配数组）。清 ToolsTlsThunks 754/2913/3178。
- **091h（55→54）BSTR 判定前缀修正**：`bstrTopPrefixes` 移除 `"vb6_VariantFromComResult("` — 该函数返回 **vb6_VARIANT**（vb6rtl.h:918）而非 BSTR，旧前缀把 COM 属性结果误判为"已 BSTR"而跳过提取。清 cTlsSocket.c 200（`vb6_LenB(vb6_VariantFromComResult(ComGetProp(...)))`）。
- **091i（54→52）wrapToBSTR 识别项目 Variant 返回函数**：`wrapToBSTR()` 的"其他 vb6_ 函数假定为 BSTR"直通分支前，增加 `variantReturnFuncs_` 检查（driver 预扫描，含 `vb6_<cls>_prop_get_<name>`）→ 命中则 `vb6_VariantToString(...)`。清 Demo_Database 310/678（`vb6_BSTR_Concat(L"...", vb6_cDataBase_LastInsertId(...))`）。
- **残留（52 = C2440×26 + C2198×7 + C2039×6 + C2065×5 + 其它 8）**：
  1) **形参为按值 Variant 的运行时函数**（表项类型 `"vb6_VARIANT"`，如 `vb6_VariantToLong/ObjectVal/CStr/CBool/CByte/CDate/CCur`、`vb6_IIfVariant`、`vb6_CLngV/CIntV`）实参为具体类型时未包装：`vb6_VariantToObjectVal(vb6_ComCall(...))`（cLang 26 void*）、`vb6_PA_UBound(vb6_VariantToSafeArray1D(...))`（cToolsArray 147 SAFEARRAY*，注意 PA_UBound 若要数组变体需 `vb6_VariantFromValue` 无法产生数组变体语义）、`vb6_Join(vb6_VariantToSafeArray1D(...))`（cPLI 56）、`vb6_cTlsSocket_Accept(..., bUseTls, ...)`（cWinsock 1604 int16_t）、`cWinsock 1949/1965`（baBuffer Byte 数组 → ByVal Variant）、`pvSubClass 543/544`（cls*）、`cDelay 74`（SAFEARRAY*）。注意 `vb6_CLngV/CIntV` 有 5993-6010 的反向剥离后处理，包装改动需与其对齐。
  2) **赋值路径目标类型识别**：`EnumLevelNames = _arr_0`（ToolsLogs 44，Variant 目标 ← SafeArray1D*，疑 knownVariantVars_ 未注册）、`me->m_vUserData = vb6_VariantToObjectVal(Value)`（cWinsock 172）、cHttpClient 399/418、cLang 144、Demo 671、cTlsSocket 4093（Variant → 具体类型目标）。
  3) **UDT 嵌套字段**：`me->mData.mstrFileName = vb6_ret_ShowOpen`（cDialog 393，嵌套 UDT 成员类型推断未覆盖）。
  4) `Demo_Database 506`：`vb6_BSTR_Concat(L"...", vb6_cCollection_prop_get_Item(...))` — prop_get 返回 Variant 未被 091i 命中（需确认 cCollection.Item 的 returnType 与集合名是否一致）。
  5) cAesCBC 25 / cToolsList 25（`vb6_StrConv(LoadResData(...))`、`prop_let_Filter(rs, ComGetProp(...))`）、cToolsArray 98/107（`vb6_IsMissing(Variant 值)`，RTL 形参是 SAFEARRAY*）。


## 1. 项目与目标

- **C3**：VB6→C 转换器（工作区 `c:/Users/vi/Desktop/c3.vb6.pro`）
- **目标**：减少 `vbman/dist/c3-error.log` 中 MSVC 编译错误数
- **进度**：777 → 588 → 552 → 216 → 204 → 176 → 165 → 155 → 142 → 132 → 109 → 107* → 103* → **100***（090ad 清 Dictionary 2、090ae/090af 清 cCsv 4、090ag 清 ToolsJsonVba 3；`*`=全量回归被 C3 自身崩溃阻塞，100 为推断值，未全量跑通）
- **当前状态**：**cZipArchive 簇 error C = 0、Demo With/COM 链清零（090s/090t）**；090u/v/w/x/y/aa/ab/ac 降 23（132→109）；**090ad 修 Dictionary.key(OldKey)=NewKey 只写属性 LHS 多传被 pad 的 value 实参（C2197 x2，单模块 0 错，附带激活 090w Variant 末参打包死代码——substr(5)→substr(4) bug）**。残余错误分布极散：C2440 各类方向 30+ 处（每类 1-7 处）+「参数太少/太多」调用形态 16 处 + 成员不存在/SAFEARRAY 成员 10 处（含 cCsv `d(0).Root` C2039 + VariantToObjectVal 实参、cAliyunCaptcha With .ReturnJson() 字段函数化、cHttpServerResponse VBMAN.Version/->socket 等）+ cCollection prop_get_Item x1。下轮候选：见「残留错误族明细」节。
- **阻塞项（C3 自身崩溃）**：release `.build\C3.exe` 与 ASAN `.build-asan\C3.exe` 在全量 vbman 收尾/后段均崩溃（release 崩于 cVBMAN.c 生成前收尾、ASAN 崩点漂移 Dictionary 模块后 ↔ Form Picture1.Align/FToastDrawer 处理 → 内存损坏特征，无 ASAN report）。**2026-09-07 验证：ASAN clean-first 全量重建后仍在同位置崩（179733 行日志、Picture1.Align VB4001 后）→ 排除「陈旧对象」假说**（旧注意事项 2026-09-05 结论对此崩溃不适用，勿再浪费 clean 重建）；FToastDrawer 单 frm 裁剪不崩 → 需多模块组合才能复现，建议用「逐步追加模块 bisect」定位（从 VBMAN.vbp 头部 Class 依 vbp 顺序追加到 Form）。全量回归依赖修复该 bug（HANDOFF 090z TODO：With void* ClassInstance 0xC0000409；0x100EE/0xE2A72 偏移 map 解析命中 std::string 移动赋值，疑似未初始化读取/越界写）。修复前只能用 fix_tool 单模块/裁剪簇验证（注意 c3-error.log 现被 ASAN 崩溃前 31 文件快照污染，需 `git checkout -- vbman/dist/c3-error.log` 恢复 109 基线）。
- **快速验证**：修复 C3 后不要直接全量编 vbman（2-3 分钟），先跑 `scripts/fix_tool.ps1` 裁剪出含错误模块的最小 vbp 工程（约 5-7 秒/簇）复现/验证，最后才全量回归。
- **cluster 注意**：小工程缺依赖模块时部分跨模块代码分支不执行，可能零错但全量仍报（cCollection/cToolsStr 案例），须以全量回归为准。

## 2. 构建 / 验证工作流

1. `scripts/build.bat` — 重编 C3.exe
2. **快速迭代（推荐）**：`powershell -File scripts/fix_tool.ps1 -Modules <相对 vbman/src 路径>[,<更多>...]`
   - 从 VBMAN.vbp 裁剪只含指定模块的最小工程（产物 `vbman/src/FIX_<Name>.vbp` + `vbman/src/_fix/<Name>/out/c3-error.log`）
   - 注意：多模块参数需写成 wrapper .ps1（`& fix_tool.ps1 -Modules @(...)`）或单模块直传；错误复现数与全量一致（已验证 ToolsJsonVba 7/7、cZipArchive 4 模块簇 23/23）
3. 全量回归：`vbman/build_vbman.bat`（产物 `vbman/dist/c3-error.log`）
4. `scripts/_tmp_count.ps1` — 统计错误总数
5. **注意**：PowerShell 内联 `$_` 会被转义，复杂逻辑必须写进 .ps1 脚本文件再执行

## 2. 构建 / 验证工作流

1. `scripts/build.bat` — 重编 C3.exe
2. `vbman/build_vbman.bat` — 用新 C3.exe 重新生成 VBMAN 的 C 代码并触发 MSVC 编译
   - C3 生成成功时打印新临时目录（如 `113776838989600`），exit 1 是警告所致，属正常
   - C3 失败时自动写 `vbman/dist/c3-error.log`
3. `scripts/_tmp_count.ps1` — 统计错误总数
4. 辅助脚本（按需重写）：`scripts/_tmp_grep.ps1`（模式统计）、`scripts/_tmp_peek.ps1`（看生成代码）
5. **含窗体回归（092z 起）**：`powershell -File scripts/_tmp_bisect.ps1 -N 999 -Forms` — 纳入 `VBMAN.vbp` 的 `Form=` 条目（默认不纳入，行为不变），产物 `vbman/src/FIX_bisectfrm.vbp` + `_fix/bisect/outfrm/c3-error.log`。**当前含窗体全量基线 = 0 error C**
6. **改过 cgen/头文件后务必全量重建**：`cmake --build .build --clean-first`。增量构建会产生**陈旧对象** → release 二进制运行期崩溃（0xC0000005 / 0xC0000409），极易误判为 UB（本次「窗体 Release 崩溃」即此）
7. **崩溃栈定位**：`set C3_CRASH_TRACE=1` + 带 PDB 的构建（`.build-rwdi` / Debug）→ 崩溃时 stderr 输出 `[C3-CRASH]` 异常码/地址 + 符号化调用栈（dbghelp；`c3` 目标已链接 dbghelp）
8. **注意**：PowerShell 内联 `$_` 会被转义，复杂逻辑必须写进 .ps1 脚本文件再执行

## 3. 错误演进史

777 → 774 → 748 → 723（VBA 子集常量）→ 700（VBA 全量 + 枚举）→ 638（ReDim/Erase + 枚举）→ 594（C2102 常量取址修复）→ 589 → 588（C2099 静态初始化清零）→ 216（088e-089h）→ 204（089i/j/k）→ 176（090a/c/d/e + P25b）→ 165（090f/g）→ 155（090h-n）→ 142（090o-p）→ 132（090s/090t）→ 109（090u/v/w/x/y/aa/ab/ac）→ 107*（090ad）→ 103*（090ae/090af）→ **100***（090ag）

### 最近修复摘要（091a/091c/091d：无窗体 bisect 基线 65 → **58**；提交 aeaa95f / 05793ac / ae72bf2，2026-09-10）

- **091a（属性写方向签名表，65→63）**：`Symbol` 新增 `memberLetParams`/`memberSetParams`（Let/Set 各自唯一，无条件填充于 Pass1/Pass2；driver.cpp 跨模块拷贝）。新函数 `findClassMemberWriteParams(className, member, isSet, out)`（Phase A 模块作用域 Let/Set 符号 → Phase B Class 符号写方向表）。090w 的 C/D2 打包改用它 —— 修复 `memberParams` 读优先（Get > Func > Sub > Let > Set）对「Get 有参 + Let 末参才是 value」属性的遮蔽：`item$pl` 等跨模块 storageKey 冲突时 Phase A 找不到本类符号，旧逻辑回退 Get 的 `[key]` → 不打包 → C2440（Demo.c 766/786 `prop_let_Item(json, key, double)`，766/767/768 现均字段式打包 ✓）。
- **091c（ParamArray 元素赋值，63→61）**：`OutVars(i) = value` 生成 `vb6_PA_GetLong(OutVars, i) = ...` → 取值函数作左值 C2106 + Variant 值传具体类型 Set 形参 C2440。cgen_stmt 赋值路径新增 `vb6_PA_Get*` 目标识别：改写为 `vb6_PA_SetLong/Double/BSTR(args, <按类型提取的 value>)`（Variant 值 → `vb6_VariantToLong/ToDouble/ToString`）。清 cToolsArray.c 158/167。
- **091d（常量类型推断/整型折叠，61→58）**：
  (a) semantic_analyzer `LocalDeclStmt` 的 ConstDecl 分支此前被"简化"为 Variant，现直接复用 `registerConstant`（完整字面量/一元负号推导，define 到局部作用域）；
  (b) cgen_stmt 局部 ConstDecl 无 `As 类型` 时按字面量推断 C 类型（Integer/Long→int32_t、Single/Double→double、String→BSTR、Boolean→VBABOOL）—— 此前一律 `const vb6_VARIANT SW_SHOWNORMAL = 1;` → C2440 初始化（cToolsSystem.c 11/13）；
  (c) cgen_decl 模块级 ConstDecl 生成前先 `tryEvalConstInt` 折叠整型表达式 → `#define BIF_USENEWUI (80)`，避免 Variant 语义把字面量操作数包装成 `vb6_VariantToLong(64)` → C2440（cDialog.c 36，报错在使用处 532）。
- **未采用（已回退）091b**：对象属性赋值路径（cgen_stmt.cpp 578 `/* Property Let */`）值参打包。首版按 `propLetSym->params.back()` 无条件打包 → +36 C2440（`lookupModuleByKind` 全局同名属性命中错类）；改用 `findClassMemberWriteParams(objClass, ...)` 后回归消失，但 ByVal Variant 场景（cWinsock 1949/1965 `UserData Let(ByVal Value As Variant) ← baBuffer`）仍不生效：`inferExprType(baBuffer)` 与 `symTab_.lookup("baBuffer")` 均拿不到数组位（局部数组类型信息缺失）。且枚举类型（`Dictionary.CompareMode As CompareMethod`）在符号表被解析为 Variant 而 cgen 生成 int32_t 形参 → 打包即 C2440（Dictionary.c 9/27）。结论：该路径需要"实参 C 类型"与"符号表类型"一致性的前置条件，待后续。
- **残留错误族（58 = C2440×32 + C2198×7 + C2039×6 + C2065×5 + 其它 8）**：
  1) **X → Variant 实参/赋值包装缺失（约 14 处）**：`vb6_ForEach_Init(vb6_VariantToObjectVal(vb6_Split(...)))`（ToolsTlsThunks 754/2913/3182）、`vb6_ComCall` 结果直接传 VariantToObjectVal（cLang 26）、跨模块方法实参（cWinsock 1604 int16_t→Variant）、`EnumLevelNames = _arr_0`（ToolsLogs 44，SafeArray→Variant 目标，疑未注册 knownVariantVars_）、Set 属性对象→Variant 字段（cWinsock 172）、`vb6_PA_UBound(vb6_VariantToSafeArray1D(...))`（cToolsArray 147）、pvSubClass 543/544（cls*→Variant）。
  2) **Variant → 具体类型提取缺失（约 7 处）**：内置/运行时函数实参（cTlsSocket 200、cAesCBC 25、cToolsList 25、Demo_Database 310/506/678 均为 `Variant → BSTR`；cDialog 393 UDT 嵌套字段）、cHttpClient 399/418、cLang 144、cToolsArray 98/107、Demo 671。
  3) cToolsArray 130 `Extend(&_arr_1, &(vb6_VARIANT){0}) = vb6_Split(...)`（过程调用作赋值目标，C2106）、cHttpServer 374（void*→double）。

### 最近修复摘要（090w 修正+090h/090x：无窗体 bisect 基线 68 → **65**；提交 18c58eb / 087e203，2026-09-10）

- **090w 修正（cgen_util.cpp，ByRef Variant 值参字段式打包）**：旧实现 ByRef 分支生成 `(&(vb6_VARIANT){vb6_VariantFromValue(x)})` —— 结构体复合字面量不能用另一个结构值初始化，命中即 C2440「vb6_VARIANT→vb6_vartype」。激活后（substr(4) 修正使 classLower 匹配成功）cJson.Items 等案例爆 8 条此类新错。改为**字段式**（.vt=VT_BSTR/.bstrVal、VT_I4/.lVal、VT_UI1/.bVal、VT_R8/.dblVal、VT_BOOL/.boolVal，default 对齐 M22：变体表达式 vb6_VariantToString、其余按 BSTR 兜底），依据 `inferExprType(valueExpr)` 选字段。
- **090x（cgen.hpp/cgen_util.cpp helper `packLetValueArg`）**：把 090w 打包逻辑抽成共享 helper，供两处使用——(a) cgen_util Pattern C/D2（prop_get_ LHS rewrite 追加 value 实参）；(b) cgen_stmt.cpp With 块 prop_let_ 调用（090x-With：With 循环记录 PropertyLet 符号，末参 Variant 时对 lastExpr_ 打包 → 清 cHttpServer.c 913 `.Expires = DateAdd(...)` C2440 double→vb6_VARIANT*）。
- **090h（cgen_stmt.cpp，With 块类属性写识别）**：跨模块 storageKey 冲突（如 CookieAttr.Value 的 value$pl 被先注入的其它类同名属性覆盖）使 moduleScope 扫描漏 PropertyLet/Set → With 内 `.Value =` 误走数据字段写（`_vb6_with->Value`）→ C2039（CookieAttr 无 Value 字段）。补 findClassMemberCallParams（Class 符号 memberParams 精确到类）二次确认属性存在 → 生成 `vb6_cHttpServerCookieAttr_prop_let_Value(...)`。清 cHttpServer.c 908 C2039。
- **净效果**：C2440 41→39（清 Demo 592、cHttpServer 913，修 cJson.Items 系列回归为合法字段式）、C2039 7→6，无新增回归。总 68 → 65。
- **残留同类**：Demo 766/786（cJson.Item）与 cWinsock 1949 等——消费模块 moduleScope 中该类 PropertyLet 符号被跨模块同名($pl)冲突覆盖/缺失，Get 方向参数表（memberParams，Get>Func>Sub>Let>Set 单一 read 优先）无法给出 Let 末参签名 → 090w/090x 不打包仍 C2440。需语义层为属性写方向（Let/Set）提供完整签名（或跨模块符号按 sourceModule 多保留）——下一轮候选。
- **注意**：090x 命名与 9/7 handoff「090x Debug.Print」重复，本会话 090x = Let 值参打包 helper，勿混淆。

### 最近修复摘要（090bx/by：无窗体 125 模块基线 84 → **79**；提交 f3ed79c）

- **ToolsJs 源修复（vbman）**：`NewObj` 函数体内误写 `Set NewArr = MSSC.Eval("{};")`（复制粘贴笔误，应 `Set NewObj = ...`）→ 清 ToolsJs.c(41) C2106+C2198。VB6 中跨函数名作赋值左侧非法，属源码 bug 非 cgen 问题。
- **090bx（cgen_stmt.cpp）**：With 块 `void*` 目标恢复 COMObject 分发（090y 设计；090z 为定位 0xC0000409 崩溃临时还原 ClassInstance，现崩溃已定位到窗体主题）→ 清 Demo.c(509) `.Item("abc") = 123` C2106（此前 classInstance 撞名 cTimers.Item）。With 推断两处 `kind == Unknown && tempType == "void*"` → COMObject。
- **090by（cgen_expr.cpp + cgen_base.cpp）**：`resolveArrayElemType` 对 VB6 内置对象类型（Collection/Forms/ErrObject/App/Screen/Printer/Clipboard，含 VBA. 前缀）返回 Variant 但 C 字段是 void* → 误注册 classVariantMembers_ → 类内 `m_Col(i)` 生成 `vb6_VariantArrayGet(&me->m_Col, i)`（C2172）。修复：cgen_expr 6447 SimpleTypeRef 分支加 vb6BuiltinObjTypes 集合（与 semantic_analyzer.cpp:587 一致）→ Object；cgen_base.cpp 177 分支加防御性排除（非内置名且符号表无条目不注册）。清 C2172 x2（cHttpServer.c 62 / cHttpServerSvr.c 160 的 `m_DefaultDocumentAdditions(i)` Collection Item 调用）。
- **已知运行期缺陷（编译通过但语义错，未修）**：cHttpServer.c AddDefaultDocument 的 `For i = 1 To m_DefaultDocumentAdditions.count` 上界生成 `int32_t i_end = me->m_DefaultDocumentAdditions;`（.count 属性读被忽略）；正向 for 分支条件误用 ComCall("count") 包 Item 结果（d2 反向分支正确）→ 需 Collection `.count` 属性在整数上下文读（vb6_ComGetIntProp）与正向分支修正。

### HttpServer 簇（cHttpServer/cHttpServerResponse/cHttpServerCookies，约 20 错，多独立深机制）

当前 79 错分布：C2440 51 / C2198 8 / C2039 7 / C2065 5 / 单发 8 个码。HttpServer 簇含 5+ 独立机制，逐一深挖：
1. **cHttpServer.c 804 State403 C2198（090ak 曾声称清过但语句调用上下文仍漏）**：`Response.State403` 是 Call 语句（非值上下文），Fix 083d 只覆盖值上下文无括号类方法引用；语句调用 Optional Say 未补参（C 签名 BSTR* Say + int _has_Say）。同族：cHttpServerResponse.c 508 TlsReMaster.SendData C2198、cHttpServer.c 374/821/913、cookies 65/168 的 Variant/COM 值转换。
2. **cHttpServer.c 906 `Cookies.Cookie(x)` COM dispatch**：Cookies 是局部 `Dim As cHttpServerCookies`（项目类，早绑定应 vb6_cHttpServerCookies_prop_get_Cookie），生成却 `vb6_ComCall(Cookies, L"Cookie", ...)`——默认成员（VB_UserMemId=0）调用被 COM 化。致 908 `.Value =` C2039（With 对象 ClassInstance 但 cHttpServerCookieAttr.Value 是 Property 非字段，字段直写失败）。
3. **Variant prop 值转换缺口**：cHttpServerCookieAttr.Expires As Variant——(a) cHttpServerCookies.c 168 `FormatHttpDate(CK.Expires)` ByRef double 参数需 `&(double){vb6_VariantToDouble(prop_get_Expires(CK))}`（当前 vb6_VARIANT 直接塞 double 复合字面量 C2440）；(b) cHttpServer.c 913 `.Expires = DateAdd(...)` prop_let Variant 参数需 vb6_VariantFromValue 打包。同族：cHttpServer.c 374 void*→double、cookies 65、response 152/153。
4. **cHttpServer.c 821 `Request.Header.Exists(...)` C2037**：Header 被解析成 vb6_ComIface_IDictionary* 且 `->Exists` 结构体成员访问（应 vb6_ComCall 或类方法早绑定）。
5. **cHttpServerResponse.c 452 VBMAN.Version C2198**：ComGlobalNs promoted 函数链生成 `vb6_cVBMAN_Version(&(void*){0}, 0)`（当静态模块函数）；应 CreateObject(sGlobal) + ComCall VBMAN + 晚绑定 .Version（cgen_expr 424 分支未命中）。
6. **cHttpServer.c 372-374 With CI 内 `m_oServer.RemoteHostIP/RemotePort` 链式读错乱**：RHS 退化为 me->m_oServer（属性名丢失）、`.ConnectAt = Now()` 错配 COM RemotePort。With ClassInstance 字段写 RHS 含类字段链式属性读的路径缺陷。
7. **cHttpServer.c 382 FireEvent 事件参数打包**：OnAccept(CI, IsDiss) 的 CI 生成 `vb6_BSTR_FromStr(CI)`（对象被当字符串打包，运行时错；编译无错故未计）。

候选低垂：cHttpServerCookies.c 65/168（Variant↔对象/Date 转换，机制同 090ag 089c2/IsEmpty 修复，可先做）。

- **验证基线变更**：用 `scripts/_tmp_bisect.ps1 -N 125`（FIX_bisect.vbp，Class/Module 125 个、无窗体——VBMAN.vbp 的 `Form=Layer\FLayer.frm` 行无分号，bisect 第 14 行正则 `^(Class|Module|Form)=[^;]+;.+$` 要求分号 → 窗体被排除）做快速全量验证，错误日志 `vbman/src/_fix/bisect/out/c3-error.log`。注意此基线与 HANDOFF 上方记录的不同窗口期（当前 84 错 = 无窗体全量）。
- **090aj（cgen_base.cpp）**：Date 类型类字段归入浮点成员集 classDoubleMembers_（此前 UDT/字段映射遗漏 → 相关 C2440）。
- **090ak+090al+090am（cgen_expr.cpp）**：值上下文无括号类方法引用补默认参数（C2198 参数太少）——清 cHttpServerSession 5 错（`If Db.Sql(s).Param(p).QueryParam Then` 链）+ cHttpServer.c(804) State403 + cHttpServerResponse.c(452) VBMAN.Version + cLayer cCsv_ShowTo 等。生效点是 **Fix 083d 分支**（cgen_expr 2189：`node.object->kind != IdentifierExpr && !asCallCallee_` 只发 this 单参）——按 findClassMemberCallParams 形参表补默认值（_prop_ 属性跳过）。090al（1899 段 knownClassVars_ 未命中 → inferClassTypeOfExpr emplace）与 090ak（Fix 088b typed 字段链补参）为同族防御性补丁。
- **090an（36cd62c）**：数组下标为 Variant 时转 Long——`baBuffer(maxLen)`/`ReDim` 边界等生成 `VB6_SA_AT(type, arr, (*maxLen))`，宏内 `[(idx)-lBound]` 对 vb6_VARIANT 做减法 → C2088。修 cTlsReMaster.c(372) C2088+C2198（visit 一维/二维数组元素访问 + toLongIfVariant）。注意 ByRef Variant 参数以 `(*name)` 形态作下标时 toLongIfVariant 依赖 knownVariantVars_/cExprIsVariant 识别，未见漏网。
- **窗体 Release 崩溃验证（既有阻塞项确认）**：VBMAN.vbp 有 4 窗体（FLayer/FLogs/FToastCenter/FToastDrawer），bisect 因正则从未含窗体 → 崩溃从未在回归面。修正正则含窗体后 release `.build\C3.exe` 崩溃（0xC0000005）最小批次 38 = **FLayer.frm**（VBMAN.vbp 顺序 index37）；**.build-asan 版同批次正常完成**（FLayer.c 只报 2 个截断依赖错 ToolsWindow）→ release 独崩 = 未初始化/UB。印证 HANDOFF 上方「ASAN 崩点漂移 ↔ release 崩 cVBMAN 收尾」内存损坏特征。**窗体方法/默认实例跨模块调用（cLayer FLayer.ShowTo 分发到 cCsv_ShowTo、cLogs FLogs.Visible 未声明）为窗体模块缺失的副作用，修复窗体崩溃后才可见真实错误面**。窗体崩溃定位建议：Debug 构建 + cdb 抓 FLayer 崩溃栈（ASAN 不崩无法用 report）。

### 最近修复摘要（090ag ToolsJsonVba 3 错，103* → 100*）

- **C2039/C2037 x2（ParseObject 内 `json_ParseObject.Item(json_Key) = v`）**：`As Dictionary`（项目外 COM 类）函数返回的 C 类型是 `vb6_ComIface_IDictionary*`；MAE visit 的 089c2 分支只匹配 `currentReturnCType_ == "void*"`（As Collection/As Object），字典类型落入 UDT fallback → `vb6_ret_X.Item(...)` C2037（未定义结构 vb6_ComIface_IDictionary）。修复：089c2 条件加 `find("vb6_ComIface_") != npos`，comObjExpr_ 统一 `(void*)currentReturnVar_` → COM dispatch，与函数返回 COM 属性的既有形态（如 CompareMode 的 FnRetObj COM SetProp）对齐。
- **C2083（ConvertToJson `If JsonValue Then`）**：ByVal As Variant 参数作 If 条件 → `if (JsonValue)`（vb6_VARIANT 结构不能直接 bool）。修复：visit(IfStmt) 在条件 emit 后若为 Variant 值（cExprIsVariant 或 knownVariantVars_ 标识符）包 `vb6_VariantToBool(x)`（VB6 真值判定语义）；已含前缀则跳过防重包。
- **验证**：fix_tool 单模块 ToolsJsonVba.bas 3→0；cCsv 回归 0 error；cZipArchive/cJson 单编的错均裁剪缺依赖假错（vb6_type_ZipVfsType/FILETIME 类型定义与 ToolsList 模块缺失），非改动引入（全量基线 0 错）。提交 368d0ee。

### 090ah 候选分析（cToolsArray 6 错，暂缓——多机制点 + 草稿代码语义存疑）

cToolsArray.cls 的 6 错集中在三个草稿/边缘函数（Extend/DeArray/test，「未完待续」注释），涉及独立机制点（各需一处 cgen 改动 + rtl 语义决策），且 VB 源对 IsMissing 数组元素的用法在 VB6 是否合法存疑（非主线收益）：

1. **C98/C107（Extend 内 `IsMissing(Vars(i)) = False Then Vars(i) = Value(i)`）**：vb6_IsMissing 形参是 SAFEARRAY*（ParamArray 用）；对 As Variant 数组元素（vb6_VariantArrayGet 返回 vb6_VARIANT）调用 → C2440。需 Variant 版 missing 判定（VB6 IsMissing 语义 ≈ 参数是否 Empty/未传）——rtl 无 Variant 版（vb6_IsMissing 只做 psa==NULL），要新增或内联 vt==VT_EMPTY（Optional 缺省 C3 表示另查）。
2. **C130（test 内 `Extend(Array(A, C)) = Split(...)`）**：本类 Public Property Let（Vars, Value 双参）经 IdentifierExpr callee 的 LHS 属性赋值——生成 `Extend(&_arr_1, &(vb6_VARIANT){0}) = RHS`（C2106）。与 090ad 场景同族但对象是本类属性（非 COM），prop_let rewrite 未覆盖「callee 为裸标识符属性 Let」的 LHS。
3. **C147（DeArray 内 `All = UBound(OutVars)`）**：ParamArray 形参（SAFEARRAY*）被当作 Variant 数组包了 vb6_VariantToSafeArray1D(OutVars) → C2440。ParamArray 参数体内使用时应直接 SAFEARRAY*（UBound(OutVars) → vb6_PA_UBound(OutVars)）。
4. **C158/C167（DeArray 内 `IsMissing(OutVars(i)) / OutVars(i) = Arr(i)`）**：PA 元素被生成为 vb6_PA_GetLong(...) 作 LHS（C2106，rtl 无 PA_SetLong 形式的写——实际应 PA_SetVariant 或元素是 VARIANT 槽）；IsMissing(PA 元素) 语义存疑。
5. 连带：vb6_PA_SetLong(_pa_0, 0, A) 传 BSTR（test 里 A/C 是 String）可能另报（错误数 6 外）。

**建议**：此簇让位给更高收益错误（cAliyunCaptcha/cHttpServerResponse/依赖重模块多文件 fix）。若做，从 #3（UBound(ParamArray) 最小改动）切入。

### 最近修复摘要（090ae/090af cCsv COM 链写 + Variant 数组元素成员，107* → 103*）

- **现象**：cCsv.cls 生成 C 4 错集中于两形态——`Set Data(LineNumber)(ColumnNumber) = Dat`（prop_set_Value 内，C2197/C2440）与 `If TypeOf d(0) Is cJson Then Set d(0) = d(0).Root`（NewLine，C2039/C2198/C2440）。
- **090ae（链式 COM 写缺失于 SetStmt）**：P25b（链式 COM 默认属性索引赋值）原只存在于 AssignmentStmt，SetStmt 无分支 → LHS 被 emitExpr 按**链式读**生成 `vb6_VariantFromComResult(vb6_ComCall(...)) = value`（非左值）→ C2440。修复：P25b 提取为 `CCodeGen::tryEmitChainedComWrite`（cgen_util.cpp），AssignmentStmt/SetStmt 共用；root 判定扩展「当前类 void* COM 字段」（classVoidFieldMap_ 按 moduleName_+字段名查，`Data As New Dictionary` → me->Data）。生成 `vb6_ComSetPropArg(vb6_ComCallObject(me->Data, L"Item", {Line}, 1), L"Item", {Col}, 1, vb6_ComPackValue(Dat))`。
- **090af（Variant 数组元素对象成员）**：(a) MemberAccessExpr 对 `VB6_SA_AT(vb6_VARIANT, ...)` 前缀表达式此前走通用 fallback → `.Root` 结构体字段访问 C2039；(b) SetStmt 038b-6 对 Variant RHS 一律 ToObjectVal 提取、051 缺「Variant 数组元素 LHS」判定 → void* 赋 vb6_VARIANT 元素 C2440。修复：MemberAccessExpr 对 `VB6_SA_AT(vb6_VARIANT,` 设后期绑定 marker（同 knownVariantVars_ P24-04）：`vb6_ComGetProp(vb6_VariantToObject(&(elem)), L"Root")` → Variant 值；SetStmt 把 targetIsVariant 判定上移共用（含 VB6_SA_AT(vb6_VARIANT, LHS），038b-6 对 Variant 容器跳过（RHS 直接拷贝/FromValue Identity）、051 统一包装。
- **验证**：fix_tool 单模块 cCsv.cls 0 error；回归 Dictionary.cls（090ad 路径）与 cIni.cls（090e 默认成员链写）均 0 error 无破坏。提交 54e2258。

### 最近修复摘要（090ad 只写属性 LHS 参数多传，109 → 107*）

- **现象**：Dictionary.c(110/162) C2197 `vb6_Dictionary_prop_let_key(me->m_Dict, OldKey, vb6_BSTR_Empty(), NewKey)` —— VB `m_Dict.key(OldKey) = NewKey` 中 key 只有 `Property Let`（无 Get）。
- **根因**：只写属性 LHS emit 时（resolveClassMemberCall 读上下文 fallback 到 Let），IndexOrCallExpr 参数补齐逻辑按 prop_let_key 形参 (OldKey, NewKey) 对缺失的 value 形参也 pad 默认值 `vb6_BSTR_Empty()`；随后 Pattern C/D2 rewrite（tryRewriteCOMLvalue）把 target 当 prop_get 形式**追加** RHS value → 4 实参 vs 3 形参。
- **修复**（cgen_util.cpp Pattern C/D2）：matchedVerb != prop_get_ 时用 findClassMemberCallParams(cls, afterPg) 取业务形参表；若 target 括号实参顶层段数 == 形参数+1（对象+全形参=被 pad 完整），丢弃末段由真实 value 取代 → `prop_let_key(me->m_Dict, OldKey, NewKey)`。配套新增 splitTopLevelArgs 顶层逗号分割 lambda。
- **附带修正**：090w/090ad 中 `prefix.substr(5)` → `substr(4)`（"vb6_" 仅 4 字符）。原 bug 使类名被截首字符（`vb6_Dictionary_`→`ictionary_`），findClassMemberCallParams 永远失败 → **090w 的 Variant 末参打包此前是死代码**。修正后激活：值实参走 `vb6_VariantFromValue`（_Generic：标量→Long/Double、BSTR→String、**vb6_VARIANT→Identity 不透传不二次包**），安全。
- **验证**：fix_tool 单模块 Dictionary.cls → 0 error（生成 `prop_let_key(me->m_Dict, OldKey, NewKey)` 正确 3 参）。cCsv.cls 裁剪复现 4 错与全量基线同形（`d(0).Root` 等，属下一轮，非本修复引入）。**全量回归被 C3 崩溃阻塞**（见上「阻塞项」）。

### 最近修复摘要（090u/v/w/x/y/aa/ab/ac，132 → 109）

- **验证**：全量 132 → 109（C2440 73→46）；错误分布由「cZipArchive/Demo 大簇」转为「64 组零散 1-7 个」（根因分散，逐点反查 VB 源）。
- **090ac**（cgen_expr + cgen_stmt + vb6rtl.c/h）：`App.HelpFile` 此前无 App 属性分支 → 生成 `(void*)0.HelpFile` 非法语法（C2059/C2198）。新增 rtl `vb6_App_HelpFile()`（返回空串 BSTR），MemberAccessExpr App 分支、wrapToBSTR 及 CallStmt/Print/Write/Debug 的 BSTR 前缀表全部登记，防二次装箱。
- **090v**（cgen_decl.cpp + cgen.hpp）：模块/类级 `Dim As New` 变量持久注册 `moduleNewVars_`——`knownNewVars_` 在 Sub/Function/Property 入口清空，仅从 moduleNewVars_ 恢复，避免局部同名变量（如 cColl 内 `Dim A As New cCollection` vs JsonStr 内 `Const A`）残留导致 const 变量被注入 auto-instantiate（C2166）。
- **090ab**（cgen_util inferExprType）：UDT 已确认但字段符号不可解析（跨模块 Public Type 符号不可达）时返回 `Unknown` 而非 `Variant`——否则 `COMSTAT.fBitFields` 等 int32 字段被 `isDefinitelyVariantExpr` 误判 Variant → 实参套 `vb6_VariantToLong(int32)` → C2440。
- **090aa**（cgen_stmt ForEachStmt）：For Each 元素是项目类变量（`For Each oClient In colTimedOut`）时 cast `(vb6_cls_X*)vb6_ComUnpackObject(&feVar)`；此前漏查 knownClassVars_ → 走 Variant 分支 `oClient = vb6_VARIANT` → C2440。
- **090u**（cgen_expr MsgBox/BSTR 形参）：实参已是 `vb6_VariantToString(...)`（参数打包段对 Variant 实参按 BSTR 形参已转一次，如 `MsgBox .FindFirst(...) As Variant`）时不再二次包裹 → 消除 `VariantToString(BSTR)` C2440。
- **090x**（cgen_stmt Debug.Print）：Variant 表达式/变量 → `vb6_DebugWriteBSTR(vb6_VariantToString(x))`，此前落入 `DebugWriteLong((int32_t)(x))` → C2440。
- **090w**（cgen_util tryRewriteCOMLvalue）：Property Let 末参 `As Variant`（cJson.Item Dat / cCsv.Value）经 Pattern C/D2 改写为 prop_let_ 时，标量值实参（double/int/BSTR）打包成 vb6_VARIANT（ByVal → `vb6_VariantFromValue`；ByRef → 复合字面量取址）。
- **连带**：090y 注释厘清 With void* 兜底语义（COMObject 后期绑定更贴合 COM，TODO(090z) 还原 ClassInstance 定位崩溃）；Demo `Users.Decode .Rs` 加括号消除解析歧义。

### 最近修复摘要（090s/090t With 块成员链解析，142 → 132）

- **验证**：全量 142 → 132（C2440 79→73）；Demo 簇 21 → 15，修复 5 类 With/COM 链错误（99/104/142/144/502 行级全部清零）。
- **090s**（cgen_util inferClassTypeOfExpr + cgen_stmt WithStmt + cgen_expr WithMemberExpr asCallCallee）三处协同：
  1. WithMemberExpr 推断：`.X` 是 With 目标类的 typed 项目类字段时返回**字段类**（`.Router.Reg` → .Router As cHttpServerRouter），否则外层 findClassMemberCallParams 形参解析失败、实参全丢（C2198 `.Router.Reg "Test"`/`.CBC.Encode("x")`）。
  2. WithStmt MAE 目标（`With Http.RequestDataQuery`）按宿主类字段表分类：RequestDataQuery As New Dictionary = COM void* 字段 → WithObjKind::COMObject；此前 memSym 全局查找捡 Dictionary 符号当 vb6_cls_Dictionary* 类实例 → `.Item(k)=v` 结构体字段调用 C2039（顺带激活 COM 参数化写路径 → vb6_ComSetPropArg）。memSym 兜底限 kind==Unknown。
  3. WithMemberExpr asCallCallee_ 改为 pendingChainObj_ 协议（同 MAE Fix 015/088b）→ CallStmt 补 WithMemberExpr 形参解析 + Optional padding（`.Start` 4 个 Optional 参 → `vb6_cHttpServer_Start((void*)w, &(int32_t){80}, ...)`）。此前生成 funcName(this) 完整调用使 CallStmt bare-call 分支跳过补齐 → C2198。
- **连带修复**：`.Root("data")("coatingWeight")` 嵌套 COM 链读（Fix 086 嵌套对象参数）因推断类修正自动正确。
- **Demo residual 15**（Ini/Reg/Json 等函数，下轮专项候选）：
  1. `Demo.c(513)` C2106：`With ini.Section("App")` 嵌套 With（Section 返回类实例）→ `.Item(k)=v` 误生成 vb6_cTimers_Item(...)=（void* With 目标 + 全局符号撞名 cTimers.Item）
  2. `(534)(550)(600)(601)` C2440：MsgBox 类方法 String 返回被双包 VariantToString（cRegedit.FindFirst As String → BSTR 当 VARIANT）；UDT 数组元素字段 BSTR 写等
  3. `(681)` C2440/C2198：Debug.Print 链
  4. `(697)` C2166：左值指定 const（写只读？）
  5. `(721)` C2198+C2039：`.Rs` 实参（cDataBase COM 字段传参）Decode 参数太少
  6. `(778)(798)(869)(871)` C2440：double/int→VARIANT*（参数打包字面量、ToolsJsonVba.ConvertToJson 跨模块）

### 最近修复摘要（090o-p cZipArchive 簇清零，155 → 142）

- **验证**：全量 155 → 142（C2440 85→79）；cZipArchive 簇 6 → 0（全量 `cZipArchive error C: 0`）。
- **090o**（cgen_stmt.cpp For/GoTo）：VB6 `For i = lo To hi` 方向拆分为两副本时，`GoTo 标签` 的后缀 `_dN` 仅在「目标标签定义于被拆分 For body 内」（两份副本各定义一次）时附加；目标在 body 外（函数级出口标签如 QH/EH，GoTo 跳出循环到过程尾）只定义一份 → 加后缀生成 `goto vb6_label_QH_d2` 指向不存在标签 → C2094（cZipArchive 事件取消出口 683）。新增 `forSplitLabelStack_`/`collectForBodyLabels` 收集 body 内标签集判定。
- **090p**（cgen_stmt.cpp Erase）：Erase 对 UDT 的 `As Variant` 数组字段（`(*uFile).BufferArray`）生成裸 `vb6_SafeArrayDestroy1D((*uFile).BufferArray)` 把 VARIANT 当 SafeArray* → C2440；改 `vb6_VariantClear(&...);`（释放数组并置 VT_EMPTY）。把 090m 的 UDT 字段判定提炼为 `isVariantArrayTarget()`（ReDim 084a/090m 复用同判定，Erase 覆盖）。
- **090r**（cgen_expr.cpp 参数提取）：Variant→double 参数提取分支补 `Vb6Type::Date`（OLE date = double）。`pvToFileTime(me, vb6_VariantArrayGet(&..SourceFileInfo, 6))`（形参 ByVal Date）此前不走 Currency/Double/Single 提取（缺 Date）→ VARIANT 裸传 double 形参 C2440（2439；同行 2437 Currency 有包装形成差异对照）。
- **090q**（cgen_expr.cpp As Any 打包 + cgen_base/hpp pending 行）：Declare 的 ByVal/ByRef As Any 参数实参是「返回 UDT 的函数调用」（`SetFileTime ..., pvToFileTime(...)`，FILETIME）时：`(void*)(intptr_t)(struct 值)` → C2440；MSVC C 也不支持复合字面量 `(T){fnRetUdt()}` 内联初始化（报 C2440 "初始化…无法从 T 转换"）。修复：`generate()` 入口预扫模块 declarations 注册 `funcUdtRetCType_`（小写函数名→C 返回类型，类方法无 VB 顺序约束故不能等 emitFunctionDecl 逐函数注册）；As Any 打包命中时生成 `vb6_type_X _vb6_anytmpN = <call>;` 挂 `CodeEmitter::addPending`（新 pending 行机制），`emitLine`/`emitBlank` 前经 `flushPending` 先落地声明再输出引用行（表达式拼接期无法在行中插语句，声明恒先于引用）。

### 最近修复摘要（090h-n UDT 字段类型推断链，165 → 155）

- **根因链**：cZipArchive 的 VFS 系列函数（`pvVfsOpen`/`pvVfsCreate`/`pvVfsSetEof` 等，返回 UDT `ZipVfsType` 或 ByRef UDT 参数）里字段访问的类型推断失效：① 返回 UDT 的函数名/返回值变量（`vb6_ret_X`）未注册 `knownUdtVars_`；② 推断层 `inferUdtTypeOfExpr` 不认函数名对象；③ UDT 的 `As Variant` 字段（如 `BufferArray`/`SourceFileInfo`）不被识别为 Variant 表达式 → 被当 SafeArray/String/函数处理 → C2440/C2198/C2064 连锁（簇 23 错）。
- **090i**（cgen_decl.cpp）：Function 返回类型 `vb6_type_*` 时注册 `knownUdtVars_[funcRetLower]=retType`（与 Fix 089c 的 `void*` 类对象注册并列）→ 函数体内 `vb6_ret_X.Field` 字段类型可推断。
- **090j**（cgen_util.cpp `inferUdtTypeOfExpr`）：IdentifierExpr 分支增加「函数名 == 当前过程名且本函数返回 UDT」→ 返回 `currentReturnCType_`（与 090i 对称；发射层 084z-4/088d 已有同名机制）。
- **090h**（cgen_expr.cpp VarPtr）：`lastExpr_` 以 `(*` 开头（ByRef 解引用）是左值 → `(intptr_t)&(expr)`，不走 `(int32_t){}` 常量复合字面量（`VarPtr(File)`/`VarPtr((*uFile).BufferArray)` → C2440）。
- **090k**（cgen_expr.cpp As Any 打包）：`(*uFile).BufferArray` 这类「ByRef UDT 解引用 + 字段链」识别为左值取址，而非 `(void*)(intptr_t)(expr)` 值强转（Variant 字段值强转 → C2440）。
- **090l**（cgen_util.cpp `isDefinitelyVariantExpr`）：MemberAccessExpr 分支，对象是 UDT 且字段声明 As Variant → 明确 Variant（`inferUdtTypeOfExpr` 命中 + `inferExprType==Variant`）。
- **090n**（cgen_expr.cpp UBound/LBound）：首参是 Variant 数组表达式时包 `vb6_VariantToSafeArray1D(&arg)`（如 `UBound(vb6_ret_pvVfsCreate.BufferArray, 1)` → C2440）。
- **090m**（cgen_stmt.cpp ReDim）：`isVariantArrayVar` 增加 UDT 字段目标判定——ByRef UDT 参数/返回 UDT 变量的 `As Variant` 字段（`(*uFile).BufferArray`）按 Variant 数组处理（`vb6_VariantToSafeArray1D` + `vb6_VariantFromValue` 回包）。注：`knownUdtVars_` 值为 `vb6_type_` + `cIdent()`（私有 UDT 名前导 `_`，如 `vb6_type__ZipVfsType`），`lookupModule` 前须 strip 前导 `_`。
- **验证**：全量 165 → 155（C2440 91→85）；cZipArchive 簇 23 → 6。
- **cZipArchive 残留 6 错**（090o-p 专项已全部清零，见上节）：
  1. ~~`cZipArchive.c(683)` C2094：事件回调内 `goto vb6_label_QH_d2` 标签未定义~~ → 090o（后缀仅当标签在 For 拆分 body 内时附加）
  2. ~~`(2439)` C2440：Variant 数组取元素未解包 double~~ → 090r（参数提取补 Vb6Type::Date）
  3. ~~`(2658)` C2440 x2：Erase uFile.BufferArray~~ → 090p（Erase 走 isVariantArrayTarget → vb6_VariantClear）
  4. ~~`(2672)` C2440+C2198：Declare FILETIME ByVal UDT 参数强转 struct 值~~ → 090q（As Any 打包 UDT 调用实参 → pending 临时变量取址）

### 最近修复摘要（090f/g 跨模块同名冲突误生成，176 → 165）

- **090f（Function 返回赋值被误拦截为同名跨类 PropertyLet）**：cCollection.Items() 内 `Items = Array()` → 误生成 `vb6_cJson_prop_let_Items((void*)me, _arr_1)`（C2198 参数太少，Items() 无参却被按 2 参带参属性拼调用）。根因：cgen_stmt.cpp P6.11 裸标识符 PropertyLet/Set 分支的 `isAssigningReturnValue` 只覆盖 PropertyGet/Let/Set 过程内的同名赋值，漏普通 Function —— cJson.Items Property Let/Set（带参属性）与 cCollection.Items() 函数同名，`lookupModuleByKind` 全局命中即拦截。修复：返回赋值判定扩展到 `SymbolKind::Function`（VB6 函数体内 `FuncName = expr` 唯一语义即返回赋值，同参照 Keys() 正常路径）
- **090g（函数名对象方法调用形参解析错类）**：cToolsStr.SplitLinesToCollection() As cCollection 内 `SplitLinesToCollection.Add Mid(...)` → 5 实参 `(this, (&(BSTR){Mid}), &(BSTR){empty}, &(int32_t){0}, 0)`（C2197 参数太多 + C2440）。根因：类感知形参解析（Fix 033/084z-3，cgen_expr.cpp ~4170）把函数名对象当模块名 → `findClassMemberCallParams` 失败 → 回退 `lookupModule(storageKey "add")` 命中跨类同名方法抢占的错误类，形参表错（Item 按 ByRef String 打包、多出 Optional int32）。修复：对象名 == 当前函数名时，经 Fix 088c 注册的 `knownClassVars_["vb6_ret_<fn>"]` 解析为返回类（cCollection），`findClassMemberCallParams` 命中正确形参表
- **验证**：全量 176 → 165（C2197 x5→3、C2198 x36→31、C2440 x95→91）；cToolsStr/cCollection 清零。生成对照：`vb6_cCollection_Add((void*)vb6_ret_SplitLinesToCollection, vb6_VariantFromValue(vb6_Mid(...)), vb6_BSTR_FromStr(L""), 0)`、`vb6_ret_Items = _arr_1`（与 Keys() 对称）

### 最近修复摘要（090a/c/d/e + P25b 链式写，204 → 176）

- **P25b（链式 COM 默认属性写）**：`Dic(Section)(Key) = v`（嵌套 Dictionary/默认项链）LHS → `vb6_ComSetPropArg(vb6_ComCallObject(...inner...), L"Item", outer-args, value)`。链层从最深（`chain[last]`）向 `chain[1]` 逆序取默认成员对象，`chain[0]` 为最外层带索引 Put；根须是 COM 变量或（**090e**）模块默认成员 PropertyGet（VB_UserMemId=0，如 cIni.Root 返回 Dictionary）→ emitExpr(Root) 得 `vb6_cIni_prop_get_Root(me)` 作为链起点
- **090a（As New 类字段 cast）**：`Dim FileStream As New cToolsStream` → C 结构体 `void*` 字段；knownClassVars_ fallback 中 trailingLower 命中 knownNewVars_ 且为工程类（kind==Class）→ 成员访问前 cast `((vb6_cls_X*)obj)->member`（外部 COM 仍走 dispatch 不 cast）；driver.cpp classTypedFieldMap 纳入 As New 工程类字段使 inferClassTypeOfExpr 可推断字段类
- **090c（无实参类方法调用的 Optional padding）**：`Trim(FileStream.ReadLine())` 空括号无参调用 → MAE 把 this 拼成完整调用文本 `vb6_cToolsStream_ReadLine(me->FileStream)`，ICE 3984 处"callee 完整调用且无实参"直接 return 跳过 P14.1.4 补齐 → C2198。修复：positional 空时若 callee 是 MemberAccessExpr 且 `findClassMemberCallParams` 非空（方法有参数），置 needsSplitForMissingArgs 落入拆分流程 → calleeParams 解析 → 补默认值 + `_has_` 尾参 `(me->FileStream, 0, 0)`
- **090d（CByte/CSng/CBool 转换适配）**：P8.4 只覆盖 CInt/CLng/CDbl。`CByte("&H" & Mid(...))` 生成 `vb6_CByte(BSTR)` → C2440。修复：P8.4 扩展 CByte/CSng/CBool + V 后缀（新增 inline `vb6_CByteV/vb6_CSngV/vb6_CBoolV`）；BSTR 实参解析从 `vb6_Val` 升级为新 RTL `vb6_NumVal`（VB6 类型转换语义：支持 `&H`/`&O` 前缀 + 十进制前缀数字，Val 不识别 &H）；**改 RTL 后须重跑 `.temp/build_rtl_libs.bat` 并重编 C3.exe（lib 内嵌）**
- **验证**：`scripts/_fix_ini_cluster.ps1`（cIni+cToolsStream+cCollection+cToolsStr 四模块簇）编译错误 204 阶段 6 个 → 0；全量 191 → 176（C2440 110→95）。cluster 残留仅 LNK2019（缺 ToolsFso/ToolsMath 模块，非编译缺陷）

### 最近修复摘要（089i-089k，216 → 204）

- **089i**：用户函数调用返回 Variant 作实参时按形参类型解包（正常符号路径 + 运行时函数超参路径 089i 判定 argIsVariant）
- **089j**：VB6 无 `As` 类型声明成员默认 Variant（打包/解包/初始化）
- **089k**：4622（运行时函数超参 BSTR 分支）把 049b 的 `find("vb6_BSTR")` 子串包含判断收紧为「剥外层括号后的顶层前缀表」（`vb6_BSTR_`/`VB6_SA_AT(BSTR,`/`vb6_VariantToString(`/`vb6_VariantFromComResult(`，前缀长度用 `strlen` 避免硬编码错误）——修复 `json_ParseErrorMessage(...)` 这类内部实参含 `vb6_BSTR_FromStr` 的用户函数调用被误判「已 BSTR」而漏包 `vb6_VariantToString`（ToolsJsonVba ErrRaise 6 个 C2440）；4575 正常形参路径保留子串检查避免 MsgBox 专用逻辑已转换的表达式二次包装
- **遗留**：Demo 104/534 的 COM 链（`cJson.Root.Item("data").Item(...)` 动态 dispatch 参数形态 C2172/BSTR→Variant）与 C2039 `Item` 成员为**既有深层问题**，专项修 Demo 模块时处理

## 4. 已完成的修复（Fix 084 系列）

### cgen_expr.cpp
- **Fix 084x**（~1600 行）：优先级 3 `isVarName` 判定补充 cgen 集合检查（knownUdtVars_/knownClassVars_/knownNewVars_/knownObjectVars_/knownTypedComVars_/knownIfaceVars_），修复 `oCallback` 局部类变量被误判为模块名
- **Fix 084z-2**（IdentifierExpr）：`inferClassTypeOfExpr` 支持 PropertyGet 符号返回类实例（如 `pvSocket() As cTlsSocket`）
- **Fix 084z-3**：Fix 033 分支 className 回退优先用 `inferClassTypeOfExpr`
- **Fix 084o-5**（LHS）：函数名引用返回对象的成员访问 → `vb6_ret_Func->Field`
- **Fix 084z-4**（RHS）：对称读取侧（`NewItem.RootItem` → `vb6_ret_NewItem->RootItem`）
- **Fix 084y-6/084y-7**：VBA 模块成员——常量表内联数值（vbCr/vbLf/vbTab/vbCrLf/vbNullString/vbTrue/vbFalse/VarType 数值），函数生成裸 `vb6_<name>`（去 `$` 后缀）；Fix 033 回退补 `symTab_.lookup(fnName)` 全表查询拿 Optional 参数表
- **Fix 084y-8**（~1760 行）：M22 fallback 前加 EnumType 分支——枚举类型.成员有常量值则内联数值，否则 `vb6_enum_<Enum>_<member>`
- **Fix 084aa 系列**：
  - 位置实参 / 具名实参 ByRef：常量（`isConstIdent` 命中 #define 宏）不可取址 → 按形参类型用 `wrapConstArgForByRef` 生成 `(&(TYPE){CONST})` 复合字面量（Variant 按常量自身类型选字段：VT_BSTR/VT_R8/VT_BOOL/VT_UI1/VT_I4）
  - `VarPtr(函数调用)`：调用结果非左值 → `(intptr_t)&(void*){call}` 复合字面量包装（如 `VarPtr(.Glob(0))` 的 COM 属性调用）
  - VarCmp 比较路径（~1104-1186 行）：`vb6_VarCmpLong<fn>(&left, right)` 的左右操作数左值判断排除常量宏；Variant vs Variant 的 `isLvalue` lambda 排除常量

### cgen_stmt.cpp
- **Fix 084y**（~495 行）：P6.7 Property Let/Set 前加 `objClassMatchesProp` 校验（对象类与属性 sourceModule 不一致时跳过属性路径）
- **Fix 084y-2**（~1164 行）：P6.8 同类匹配校验
- **Fix 084o-5**：`vb6_ret_Func->Field`（类返回值成员访问）
- **Fix 084y-5**：ReDimStmt/EraseStmt 改用 `resolveArrayTargetIdent`（With 块成员 → `_vb6_with_N->Field`；ByRef UDT/数组/Variant 参数 → `(*obj).Field`；UDT 变量 → `obj.Field`）
- **Fix 084aa**：静态局部变量初始化必须是编译期常量——`vb6_VariantEmpty()` → `{0}`，`vb6_BSTR_Empty()` → `NULL`

### cgen_base.cpp / cgen.hpp
- `resolveArrayTargetIdent`：ReDim/Erase 目标解析（cIdent 之后）
- `lookupConstSym` / `isStringConstIdent` / `isConstIdent` / `constIdentType` / `wrapConstArgForByRef`（ByRef 常量包装）
- `inferClassTypeOfExpr` MemberAccessExpr 分支字段表未命中时回退 `getClassMethodReturnType`（属性 Get 返回类）

### cgen_decl.cpp
- **Fix 084aa**：模块级标量 Variant 初始化 `vb6_VariantEmpty()` → `{0}`（既有 BSTR → NULL 处理之外）

## 5. 已完成的修复：m_uData 模块级 UDT 变量（Fix 010n 扩展）

### 症状（vbman/dist/ToolsTlsThunks.c，修复前）
1. **C2064 x24**：`m_uData.Pfn(2)` — UDT 固定数组字段被当函数调用（VB6 `Pfn(2)` 应为 C `Pfn[2]`）
2. **C2224 x37**：`_vb6_with_15->LocalCertificates.Item(lIdx)`、`(*uInput).Stack.Remove(1)` — `.Item`/`.Add`/`.Remove` 左侧非结构体（**独立簇，见第 9 节**）
3. **C2440 x13**：`vb6_SafeArray1D* → vb6_VARIANT`、`(void*)m_uData` UDT 值强转

### 根因（已实证，非推测）
`With m_uData`（模块级 UDT 变量）被误分类为 **COM 对象** → 生成 `(void*)m_uData` 强转 + 所有字段访问变成 COM 调用 → C2440/C2224/C2064 连锁错误。
**实证链**（临时调试输出，C3_DEBUG_UDT 环境变量）：
- `[UDTREG] var=m_uData type=UcsCryptoData sym=0x... kind=8 cType=vb6_type_UcsCryptoData` → **注册代码确实执行**，`knownUdtVars_["m_udata"]` 正常写入
- `[WITHUDT] obj=m_udata inKnown=0 tempType=void*` → **但 With 分类时查不到**！

**真正原因**：`knownUdtVars_` 在**每个过程开头被 `clear()`**（M22-fix，cgen_decl.cpp 三处：visit(FunctionDecl)/visit(SubDecl)/visit(PropertyDecl)，位于 67/293/1252 行附近），clear 后仅从 `classUdtMembers_` 恢复——而 `classUdtMembers_` **只对类模块填充**（cgen_base.cpp generate() 开头 `if (isClassModule_)` 分支）。**普通 .bas 模块的模块级 UDT 变量（如 m_uData）在进入过程后永久丢失**，导致过程内 `With m_uData` 分类失败。

### 修复（对称扩展 classUdtMembers_ 机制）
1. **cgen.hpp**：新增成员 `std::unordered_map<std::string, std::string> moduleUdtMembers_;`（小写 var 名 → UDT C 标识符）
2. **cgen_base.cpp** generate() 开头：
   - `moduleUdtMembers_.clear()`
   - `if (!isClassModule_)` 分支：扫描 `module.declarations` 的 VariableDecl（`asType` 为 `SimpleTypeRef` 且 `lookupDotted` 命中 `UserDefinedType`），写入 `moduleUdtMembers_[lower] = "vb6_type_" + cIdent(name)`
3. **cgen_decl.cpp** 三处过程开头恢复处（classUdtMembers_ 恢复行后）各加：
   `knownUdtVars_.insert(moduleUdtMembers_.begin(), moduleUdtMembers_.end());`

### 验证结果（修复前后对比）
- ToolsTlsThunks.c：`vb6_type_UcsCryptoData* _vb6_with_125 = &(m_uData)`（修复前 `void* _vb6_with_125 = (void*)m_uData`）
- `.hRandomProv` → `_vb6_with_125->hRandomProv`（修复前 `vb6_ComGetIntProp(...)`）
- `m_uData.Pfn[2]`（修复前 `m_uData.Pfn(2)` 函数调用）
- **错误数：588 → 552**；ToolsTlsThunks：74 → 51（C2064 x24 清零、C2440 13→3 均为其他根因）

### 注意事项（踩坑记录）
- **增量构建可能产生陈旧对象 → 运行时 0xC0000409 崩溃**（症状：任何模式跑 VBMAN.vbp 都崩溃，`--dump-symbols` 也在 `=== End Symbol Table ===` 后崩）。本次通过 `cmake --build .build --clean-first` 全量重建解决。**今后改 cgen.hpp 后若运行崩溃，优先怀疑陈旧对象，先 clean 构建**

## 5b. 已完成的修复（Fix 088e/089 系列，2026-09-05，552 → 216）

演进：552 → 502 → 489 → 486 → 301 → 278 → 267 → 260 → 251 → 243 → **216**
- **088d/088e**：类返回实例链上成员访问 (ReturnJson.Decode)、getClassMethodReturnType Phase-B 兜底（278→267）
- **089**：跨模块 COM/Collection 变量分发（267→260）
- **089c/089d**：COM 返回函数变量、控件未知成员分发（260→251）
- **089e**：Friend 成员被错误编译为 static → extern（251→243）
- **089f**：类模块内无括号引用本类 Function（`pvSessionID = GenerateSessionID`，Fix 086 barecall）缺 `me` → 补 `((void*)me)`（与 PropertyGet/CallStmt 对齐）
- **089g**（cgen_util.cpp resolveClassMemberCall）：跨模块同名成员 storageKey 抢占 —— 多个类有同名 Property Get（如 cWebSocketClient/cWebSocketServerClient 都有 State）时 driver 的 globalPublicSyms 按 `$pg` 键只保留先分析模块符号，目标类 Get 缺失而 Let 正常注入 → 读上下文误发 `prop_let_State`。修复：Get 缺失而读到 Let/Set 时查 Class 符号 memberProcKinds（semantic 按读上下文 Get>Function>Sub>Let>Set 写入），按读形式重定向 `prop_get_`
- **089h**（cgen_expr.cpp ×3 处值上下文）：无括号裸方法引用只发 this → C2198 参数太少。三处分支（Fix 015 链式值上下文 / memSym 类变量分支 / class var 分支）补默认参数（Optional/必选 + `_has_` 尾参）。**属性（`_prop_` 前缀函数）不 pad** —— findClassMemberCallParams 对 Get+Let 并存属性返回 Let 参数，pad 会 C2197（经 stash 基线对比确认无回归，C2197 x5 为 pre-existing：cToolsStr Add x2 / Dictionary prop_let_key x2 / Demo_Database Item x1，属默认属性多级链与 COM 写 rewrite 簇）
- 已验证零新增回归（git stash 基线 243 vs 当前 216，C2197/C2440 同桶）

## 6. 剩余错误分布（216，最新确认 2026-09-05）

按错误码（总数 216）：
- C2440 x118（COM/Variant 打包解包簇：`vb6_VARIANT ↔ void*/double/int32_t`、`类实例 → VARIANT`、函数指针调用参数类型、SafeArray）、C2198 x45、C2039 x11、C2172 x5、C2223 x5、C2197 x5（pre-existing：cToolsStr Add / Dictionary prop_let_key / Demo_Database 默认属性链）、C2106 x5、C2094 x4、C2064 x3、C2037 x3、C2083 x2、C2059 x2、C2171/C2063/C2110/C2101/C2186/C2088/C2143/C2166 各 1
- 已清零：C2102、C2099、C2129、C2065、C2045、C2296、C2224（C2224 已全部转译/消除）
- 待办簇（按体量）：① C2440 x118 —— cTlsSocket.c 4093 等 Variant↔具体类型、cHttpServerRouter.c 29 类实例→VARIANT 参数、cHttpServer.c 374/892 double↔void*/VARIANT；② C2039 x11 —— 成员访问到错误类（Dictionary.Item/Value/Rs/ReturnJson 簇）；③ C2198 x45 中 DataBase 链式值上下文已修、余 COM 打包与默认属性链簇

## 7. 关键文件地图

| 文件 | 职责 |
|---|---|
| `src/backend/expr/cgen_expr.cpp` | 表达式生成：IndexOrCallExpr、M22 fallback、ByRef 实参、VarCmp、VarPtr、枚举分支（同目录另有 `_array`/`_binary`/`_call`/`_ident`/`_member`/`_with`） |
| `src/backend/stmt/cgen_stmt.cpp` | 语句生成：With 分类、属性 Let/Set、ReDim/Erase、静态局部（同目录另有 `_assign`/`_call`/`_control`/`_file_io`/`_jumps`/`_loop`/`_redim`/`_select`/`_setlet`/`_with`） |
| `src/backend/decl/cgen_decl.cpp` | 声明生成：模块级变量 + knownUdtVars_ 注册（`decl/cgen_decl_var.cpp` 的 `visit(VariableDecl&)`，152-160 行；同目录另有 `_api`/`_proc`/`_prop`、`cgen_localdecl.cpp`） |
| `src/backend/cgen_base.cpp` | 辅助：lookupConstSym/wrapConstArgForByRef/resolveArrayTargetIdent/inferClassTypeOfExpr |
| `src/backend/cgen_util.cpp` | inferClassTypeOfExpr、inferUdtTypeOfExpr（已按职责拆为 8 文件：主文件 + `_type`/`_scan`/`_com`/`_comwrite`/`_classcall`/`_classtype`/`_ctrl`） |
| `src/backend/cgen.hpp` | 声明 |
| `src/driver/driver.cpp` | 跨模块注入（1094-1128，仅 Public） |
| `src/semantics/symbol_table.cpp` | lookup/lookupModule 作用域链 |
| `vbman/src/Socket/mdTlsThunks.bas` | 当前错误源文件（VB6 侧） |
| `vbman/dist/c3-error.log` | MSVC 错误日志（统计源） |

## 8. 会话纪律

- 每次修复后必须走完整工作流（build.bat → build_vbman.bat → _tmp_count.ps1）验证错误数变化
- 不用内联 PowerShell `$_`，写 .ps1 脚本
- 修复按错误簇批量处理，每轮结束后更新本文档第 5/6 节
- **改 cgen.hpp 后运行崩溃（0xC0000409）先怀疑陈旧对象 → `cmake --build .build --clean-first` 全量重建**
- **改了 RTL 源码/库后必须确认 `c3rtl.rc.res` 已重编**：否则 C3.exe 仍嵌旧库，症状是「桩明明补好了，链接期仍报未解析 `vb6_di_*`」——极具误导性（根因与修法见第 10 节 2.2）；改 RTL 后固定流程是 `scripts/build_rtl_libs.bat` → 重建 C3.exe。

## 10. 链接期（编译期全绿后的新前沿）

`-Forms` 全量 129 条目已达 **error C = 0**。链接阶段原有两类问题，其中**第 2 项已于 2026-09-11 修复**（提交 `03de92f`、`460e621`，实施与验证见 2.1/2.2，历程见 `ai/开发历程/87-*`）。

**当前链接期基线（2026-09-11，含窗体 125 模块 `--dll` x64 链接，日志 `vbman/src/_fix/bisect/outfrm/c3-error.log`）**：`LNK1104 = 0` / `LNK2005 = 27 条` / **未解析外部符号 443 条**（绝大多数是其它模块待补的 `vb6_di_*` 转发桩）—— 这两项即下一步入口。

1. **LNK2005 重定义（疑似真实缺陷，值得修）** —— 同一符号在多个 `.obj` 中被定义：
   - `FToastDrawer.obj : Sad / m_Theme / m_State / PosVal / TopVal / m_ParentToast / m_TagName 已经在 FToastCenter.obj 中定义`
   - `mDelay.obj : Delay 已经在 Tools.obj 中定义`
   - `cModbusTransportTCP.obj / cModbusSlave.obj : vb6_evt_wrap_m_socket_Connect / _CloseEvent / _DataArrival / _Error / m_listensocket_* 已经在 cWebSocketClient.obj / cWebSocketServer.obj 中定义`
   → 指向两类生成问题：① **模块级私有变量未按模块前缀 mangle**（VB6 中各模块 `Private m_Theme` 互不冲突，C 侧必须消歧；`Delay` 同族）；② **事件包装函数 `vb6_evt_wrap_m_*` 被写进每个发送者模块**，应只在其事件源类模块定义、其它模块仅保留声明。
2. ~~**`LNK1104: 无法打开文件 msvbvm60.lib`**~~ —— **已修复（2026-09-11，提交 `03de92f` + `460e621`）**：不是配置缺口，而是生成器缺「无导入库 `Lib` 特判」（2026-09-11 核实，前一轮判读有误）。以下 2.0 为完整追根记录，最终实现与验证见 2.1/2.2。
   - **来源**：`cgen_decl.cpp:1219` 对**每条** VB6 `Declare ... Lib "X"` 都生成 `#pragma comment(lib, "X.lib")`；而 VBMAN 源码确有 **10 个模块 14 处** `Declare ... Lib "msvbvm60"`（`mdTlsThunks.bas:105/106`、`cAsyncSocket.cls:193/203`、`cTlsSocket.cls:160/161`、`mdTlsSodium.bas:77/79`、`mdTlsNative.bas:121/127`、`ToolsJsonUcs.bas:54`、`cHttpRequest.cls:120`、`cHttpDownload.cls:51`、`cToolsArray.cls:18`）→ 生成物中出现 `#pragma comment(lib, "msvbvm60.lib")`（`cAsyncSocket.c:45/55`、`cTlsSocket.c:101/102`、`ToolsTlsThunks.c:252/253`、`cToolsArray.c:5`），链接器遂去找该文件。
   - **只涉 3 类符号**：`Alias "VarPtr"`（源码名 `ArrPtr`，取数组/变量地址）、`Alias "__vbaObjSetAddref"`（对象赋值时的运行时 AddRef）、`Alias "#644"`（**按序号**导入的运行时内部函数，源码名 `SplitLongToBytes`）。
   - **x64 路线上无法「补」这个库**：本机 `C:\Windows\SysWOW64\msvbvm60.dll` 存在（32 位），但 `System32` 下**无** 64 位版本；Windows SDK / VS 安装目录里**不存在任何 `msvbvm60.lib`**。VB6 运行时只有 32 位 → `--arch x64` 没有可链接的导入库，且与 `ai/011`「静态链接 msvbvm60 ❌ 不采用、不支持 x64」及网站「零依赖部署」的定位冲突。
   - **该库无法提供、且提供也无用**（2026-09-11 二次核实）：工程内 / `C:\Windows` / `Program Files*` / 用户目录**均无** `msvbvm60.lib`；VB6 未安装（无 `VB98`）；Windows SDK / VS 均不携带 —— 该文件只随 VB6/VS6 发行，无法凭空取得。
   - **决定性事实（Fix 076）**：Declare 早已不走导入库路线 —— 生成物里 `__declspec(dllimport)` 出现 **0 次**，改为 `extern <ret> __stdcall vb6_di_<name>(...)` + `#define <VB名> vb6_di_<name>`，由 `src/rtl/core/di/vb6_di_stubs.c` 提供转发桩（`vb6_di_ord_12` 即先例，走 `GetProcAddress(shlwapi, 12)`）。因此 `#pragma comment(lib, "msvbvm60.lib")`（`cgen_decl.cpp:1218-1219` 无条件生成）是**纯残留**：唯一后果是制造 fatal `LNK1104`；**即使补上真库也只消 LNK1104、随即变 `LNK2019`**，因为需要的符号是 C3 内部名，MSVBVM60.DLL 导出表里根本没有。
   - **实测待补 RTL 桩 3 个**（`vb6_di_stubs.c` 87 行内目前 0 个 msvbvm60 桩）：`vb6_di_VarPtr`（源码 `ArrPtr`，`cAsyncSocket.h:460` `extern intptr_t __stdcall vb6_di_VarPtr(vb6_SafeArray1D** Ptr)`）、`vb6_di_vb6___vbaObjSetAddref`（源码 `vbaObjSetAddref`，用法 `vbaObjSetAddref((void*)&(oCallback), _vb6_with_60->ClientCertCallback)`）、`vb6_di_ord_644`（源码 `SplitLongToBytes`，`cToolsArray.h:215`，返回 `vb6_type_longByteType`）。
   - **可行方向**：① `cgen_decl.cpp:1219` 特判 `libName == "msvbvm60"`（不分大小写）→ 跳过 pragma；② `vb6_di_stubs.c` 补这 3 个桩，语义按 VBMAN 调用点实现（`ArrPtr` 取 SAFEARRAY 描述符、`__vbaObjSetAddref` = 写对象变量并 AddRef、`#644` = Long→4 字节 UDT）；**序号桩不能照抄 `ord_12` 的 `GetProcAddress`**，x64 下无 32 位 msvbvm60.dll 可加载。
   - `LNK1104` 是 **fatal**，它会挡在真正的「未解析外部符号」清单之前 → **必须先处理它才能看清链接期全貌**。

#### 2.1 已实施（2026-09-11，提交 `03de92f`）

- **生成器**（`cgen_decl.cpp`）：对 VB6/VBA 运行时库（`msvbvm60`/`msvbvm50`/`vbe7`/`vbe6`/`vba7`/`vba6`）与 `cryptdlg` **不再生成** `#pragma comment(lib, ...)`；其余库照旧（实测其余 19 个库在 SDK 中均存在）。
- **RTL**（`src/rtl/core/di/vb6_di_stubs.c`）新增 **4 个原生桩**（除 `cryptdlg` 外均不 LoadLibrary 转发）：
  - `vb6_di_VarPtr`（`ArrPtr`）：`return (intptr_t)Ptr;` —— 数组 ByRef 传递时 C3 传入的实参**已经是**数组变量地址（生成声明 `vb6_SafeArray1D**`），即 VB6 的 `&baBuffer`，故恒等。调用点 `CopyMemory(ArrPtr(a), ArrPtr(b), 4)` 是交换两个 `SAFEARRAY*`，语义核对通过。
  - `vb6_di_vb6___vbaObjSetAddref`：`if (src) src->AddRef(); *(void**)dst = src;` —— **不** Release 旧值，对齐原版 VB6 语义（避免误 Release 非持有引用）。
  - `vb6_di_ord_644`（`SplitLongToBytes`）：**序号 644 经 `dumpbin /exports SysWOW64\msvbvm60.dll` 反查确认就是 `VarPtr`**（350 = `__vbaObjSetAddref`）。x86 下 VarPtr 把入参放回 EAX，而 VB6 对 4 字节 UDT 返回值同样取 EAX，遂成「Long → 4 字节小端位重解释」技巧 → 此处按小端拆字节返回 4 字节结构。
  - `vb6_di_CertSelectCertificateW`：`cryptdlg.dll` **没有导入库**（SDK `10.0.26100.0\um\x64` 实测无 `cryptdlg.lib`），故走 `GetModuleHandleW`/`LoadLibraryW` + `GetProcAddress`（导出序号 15）动态加载。
- **`scripts/build_rtl_libs.bat` 入库**：原脚本只在被 `.gitignore` 忽略的 `.temp\` 里，而 C3 链接依赖这些 `.lib`（经 `c3rtl.rc` 嵌入 C3.exe），必须可复现构建。**该 `.bat` 必须保持纯 ASCII** —— 中文注释在 cp936 控制台下会把命令行切碎（实测报 `'vb6rtl.obj' is not recognized ...`）。
- **验证（含窗体 125 模块，`--dll` x64）**：`error C = 0`；`LNK1104 = 0`（msvbvm60 / cryptdlg 均不再出现）；**本次新增 4 符号 0 条未解析**；未解析外部符号总数 **451 → 443**。

#### 2.2 陷阱：重建 RTL 库后 C3 仍嵌旧库（提交 `460e621` 修复）

`c3rtl.rc` 以 RCDATA 嵌入 10 个文件（4 头 + 3×x64 库 + 3×x86 库），但 CMake 只把 `.rc` 自身当依赖 → **重建 `src/rtl/lib/*.lib` 后 `.res` 不重编**，C3.exe 继续嵌旧库。本次实测中招：`.res` 停留在 12:17 而新库是 19:45；C3 会话目录提取出的 `vb6rtl.lib` = 467468 字节（正好等于 HEAD 版大小，新库 470076），于是链接期仍报 `vb6_di_VarPtr` 等未解析 —— **看起来像桩没生效**，极易误判。
修法：`CMakeLists.txt` 把这 10 个文件显式声明为 `c3rtl.rc` 的 `OBJECT_DEPENDS`（CMake 3.31 + Ninja 实测有效：重建库后 `.res` 时间戳刷新，C3.exe 体积随新库变化 3354624 → 3359744）。

**建议顺序（2026-09-11 更新）**：第 2 项**已完成**（见 2.1/2.2），链接期已能看到全貌。下一步：① 对着新基线批量补 `vb6_di_*` 桩 —— 清单可从 `c3-error.log` 用 `Select-String -Pattern 'LNK(2019|2001)'` + 正则 `vb6_[A-Za-z0-9_]+` 去重导出（当前 443 条）；② 确认第 1 项两簇 LNK2005 在 `vbman/build_vbman.bat` 全量链接下是否重现（bisect 是裁剪工程，需排除「裁剪导致的重复」），然后分别修。

## 9. （历史，2026-09-05 遗留，已非当前下一步）ToolsTlsThunks C2224 x37（COM 集合簇）+ SafeArray 簇

### 症状（生成代码）
1. **C2224 x37**：`_vb6_with_15->LocalCertificates.Item(lIdx)`、`(*uInput).Stack.Remove(1)` — `.Item`/`.Remove` 左侧非结构体
2. **C2039 x4**：`"MessBuffer_Data": 不是 "vb6_type_UcsTlsContext" 的成员`（1786 行）
3. **C2198 x3 / C2440 x3**：`vb6_SafeArrayDestroy1D(...)` 参数太少、`vb6_SafeArray1D* → vb6_VARIANT`（758 行）

### 已确认的根因（C2224）
- VB6 侧：`UcsTlsContext` UDT 内字段 `LocalCertificates As Collection`、`Stack As Collection`（**UDT 内嵌 COM 对象字段**）
- C 侧：UDT 结构体中该字段生成 `void*`；访问 `.Item(...)` 时被当**结构体字段+函数调用**，而非 **COM 方法调用**
- C3 已有 COM 调用生成（`vb6_ComGetIntProp`/`vb6_ComCall*`，用于局部 COM 变量与 With 块 COM 对象），但 **UDT 内嵌 void* 字段上的方法调用未走 COM 路径**

### 建议方向（未实施）
- 在 MemberAccessExpr/IndexOrCallExpr 生成时，若接收者表达式推断为 `void*`（COM 指针，如 UDT 内嵌 Collection/类字段），成员访问走 COM 调用路径（`vb6_ComGetIntProp(obj, L"Item")` 等）
- 需先梳理 cgen_expr.cpp 中 COM 调用判定条件（knownObjectVars_/knownTypedComVars_ 等），确定「UDT 字段类型为 void*」的识别点
- C2039/C2198/C2440（SafeArray 簇）独立，涉及 `MessBuffer_Data` 字段缺失与 SafeArray 参数包装，可另起一轮

## 10. 已完成：VBFlexGridDemo 修复（Fix 158a–160-G，2026-09~，236 → 31）

VB 源码：`D:\vb_yqt4qPac\VBFLXGRD-master\Standard EXE Version\VBFlexGridDemo.vbp`。
复检流程：`vcvars_env2.bat` 逐行 `^([A-Za-z_][A-Za-z0-9_]*)=(.*)$`（**无 `set ` 前缀**）导入 env → `cmake --build .build --config Release` → 重新生成 → 按 `output_vbflex\c3-error.log` 分组统计。
当前状态：**C2440:102 + C2065:43 + C2172:2 = 147**（C2088/C2198/C2101/C2059 均 0）。

### 158a/b/c
空括号成员表达式（`f()` on property）、ByRef 数组实参改写、With 空括号数组字段。

### 158d
`vb6_UserControl_ParentControls(...)` 链式 COM 识别（C2224 清零）。

### 158e + Pattern M
`vb6_Forms_Item(i).Name/.Caption` → `vb6_GetControlText`；无前缀 `forms(i)` → `vb6_Forms_Item`；链式 COM 前缀扩充（`vb6_Common_PtrToObj(`/`vb6_CreateObject(`）。C2224 7→0、C2064 12→0。

### 158f（RTL + 参数发射）
新增 `vb6_VariantArrayElemPtr(vb6_VARIANT*, int32_t)`（`vb6rtl_com.c` 实现、`vb6rtl_variant.h` 声明，仅 `vb6_sa_variant` 返回槽位指针）。`cgen_expr_call_arg_emit.inc` As-Any ByRef 非左值分支：`vb6_VariantArrayGet(`（前缀长 20）改写为 `vb6_VariantArrayElemPtr(...)`。修复 VTableHandle.c:258/262 `VariantCopy ArgListRev(i), ArgList(UBound-i)`。

### 158g
同分支对 Variant 型非左值实参发 `(&(vb6_VARIANT){argVal})`。修复 VBFlexGrid.c:22603 `VariantCopy Cell, .CellTag(...)`。

### 158h
`cgen_assign_prop_write.inc`：Property Let/Set 写路径对入口参数（`findClassMemberWriteParams` 结果非 1 时）pad 默认值 + `_has_` 标志（`padArgs158h`）。修复 `prop_let_InsertMark` C2198（`VBFlexGrid.ctl:9212`）。

### 158j
`cgen_expr_call_builtin_fixup.inc`：`vb6_AppActivate` 单参补 `, 0`。修复 Startup.c:30。

### 158k
RTL `vb6_App_PrevInstance()`（`vb6rtl_system.c`：命名互斥 `VB6_C3_SingleInstance_<exe>`）+ `cgen_expr_member_form_builtin.inc` App 分支映射。修复 Startup.c:24 `(void*)0.PrevInstance` C2059。

### 158l
`cgen_base_naming.cpp` `isConstIdent` 增加 `kRtlConstMacros158l`（vbPicType*/vbAsyncType*/vbAsyncRead*，#define 宏取址 → `&3` C2101×7 清零）。

### 158m / 158m-2（`cgen_expr_binary.cpp`）
Fix 110aa 升级处加 `isVariantOperand158m`（`cExprIsVariant(c) || isDefinitelyVariantExpr(node.left/right.get())`）；VarCmpLong 标量侧裸 `NULL` 包 `(int32_t)(...)`。C2172 24→4。

### 158n / 158o / 158p（本会话重点，Variant 比较 C2088×14 → 0）
- **158n**：`BinaryOp::Is` 加 Variant 分支（此前仅 Eq/Neq/Lt/Gt/Le/Ge 进块，`Is` 映射为裸 `==` 于 cgen_expr_binary_util.cpp:177 → 结构体 C2088×14）：
  - 双 Variant（`If Buffer Is Value`，FindTag VT=vbObject 分支）→ `vb6_VarCmpEq(&L,&R)`
  - Variant vs NULL/0（`If Value Is Nothing`）→ `vb6_IsNothing(vb6_VariantToObject(&X))`
  - 变体 vs 其它标量 → `vb6_VarCmpLongEq(&X,(int32_t)(y))`
  - `variantAddr158n`：左值标识符/成员/`VB6_SA_AT(` 直接 `&`；其余 rvalue 存 `_vcmp_N` 临时再取址。
- **158o**：LongPtr 快速路径加 `lt != Variant && rt != Variant` 门（VTableHandle.bas `If VTableIPAO(0) = NULL_PTR`，LongPtr 数组元素被误发为 `VB6_SA_AT(vb6_VARIANT,...)`，rt=LongPtr 曾抢先裸比较）；Variant-vs-LongPtr 的 VarCmpLong 标量侧列表加入 LongPtr。
- **158p**：`Is` 分支 AST 判定的类门 —— 对非标识符（MemberAccess 等），`isDefinitelyVariantExpr` 为真但 `inferClassTypeOfExpr` 非空（`VBFlexGridFlexDataSource As IVBFlexDataSource` 被符号表误报 Variant）→ 走对象 `(X == NULL)`，避免把对象取址塞 vb6_VARIANT（C2440）。**标识符（`As Variant` 局部/形参）不套类门**（否则 combo 的 Variant 形参误回对象路径 → C2088）。
- 实证：repro2（`VTableIPAO() As Variant` + `Const NULL_PTR=0`）确认多态内置数组元素在 Fix 158o 前正确走 VarCmpLong（未走其数）。**158n/158o/158p 的发射行复检：无任何一条进入 c3-error.log**（无隐性回归）。

### ⚠ 事故记录（2026-09-21）：`vb6rtl_builtin.h` 被 Edit 连带删掉 171 行
某次 Edit 把 `src/rtl/core/vb6rtl/vb6rtl_builtin.h` 从 315 行改成 144 行（原型批量丢失）
并重复定义 `vb6_CCurV` → **RTL 内嵌在 C3.exe 里，于是全量回归 89 项 FAIL(compile)**，
`C2084: 已有主体` + `C4013: vb6_CurDir/DebugPrintStr/FormatCurrency/CCur`。
处置：备份 `.temp/bak158/` → `git checkout` 该文件 → 只重贴自洽的 158q 块（放在头文件
**所有**同名声明之后，避免宏改写后续声明）。
**自检命令**：`git diff --numstat | awk '$2 > $1+5'`（删除数远超新增数即可疑）。
回归一旦「全绿变全红」，先怀疑工具链/RTL 被改坏，而不是被测代码。

### 158q / 158r（RTL `_Generic`：转换函数的 Variant/BSTR 实参分派）
- 仿 `vb6_LenB`(Fix 048) 先例：`vb6rtl_builtin.h` 末尾（全部声明之后）加
  `#define vb6_CStrLong/CLng/CDbl/CInt/CCur(x) _Generic((x), vb6_VARIANT: …V, BSTR: …BSTR, default: …)((x))`，
  `#ifndef __cplusplus` 包裹；各 `.c` 定义处 `#undef`（`vb6rtl_conv.c` / `vb6rtl_misc.c`）。
- 新增 `vb6_CLngBSTR/CDblBSTR/CIntBSTR/CCurBSTR`（走 `vb6_Val`）与 `vb6_CStrLongFromVariant`。
- 消掉 C2440 子簇 ①（`vb6_CStrLong(vb6_VariantFromComResult(vb6_ComGetProp(...)))` 等）17 条。

### 158s（RTL `_Generic`：`vb6_InStr` 的 needle 是 Variant）
`vb6_InStrVar(int32_t, BSTR, vb6_VARIANT)` = `vb6_VariantToString` + 标准 InStr + `vb6_BSTR_Free`；
头内 `_Generic` 按第 3 参分派，`vb6rtl_string.c` 定义处 `#undef vb6_InStr`。
宏 arity 安全已核：2 参 `InStr` 由 `cgen_expr_call_builtin_fixup.inc:206` 补成 `1, …` 后再进宏。

### 158t（codegen：`Mid$/Left$/Right$` 首参收数值/日期）
`MainForm.frm:386 DecStr = Mid$(1.1, 2, 1)`（取本地小数点分隔符的惯用写法）→
`cgen_expr_call_builtin_argtype.inc` 的 `strArgPos108c` 加 `vb6_Mid/vb6_Left/vb6_Right {0}`，
并让类型白名单接受 `Date` → `vb6_CStrDate`（数值仍 `CStrLong/CStrDbl`）。

### 159-A（codegen：Property Let 的 Value 实参槽位）
VB6 把被赋的值放在**声明末尾的业务形参**，而 C 签名把 `int _has_*` 标志追加在所有业务参数
之后 → Value 并非最后一个 C 形参。`tryRewriteCOMLvalue`（Pattern C/D2）此前一律把 valArg 拼在
最后，于是 Value 落进 `_has_Row` 槽、4 个标志整体前移（`MainForm.c` 5 处成对
C2440「int→vb6_VARIANT」+「vb6_VARIANT→int」）。修正：解析到写方向参数表
（`findClassMemberWriteParams`）时，把尾部恰好等于 Optional 形参个数、且全为数字字面量的那几段
摘出，将 valArg 插到业务参数末尾、标志之前；090ad 已弹掉占位 Value 时不介入。
例：`VBFlexGrid1.Cell(FlexCellToolTipText, i, j) = "…"` →
`prop_let_Cell(me, 8, i, j, -1, -1, VariantFromValue(…), 1, 1, 0, 0)` ✓

### 159-B（codegen：ByVal String 形参收日期/数值实参 → VB6 隐式 CStr）
新增 `CCodeGen::isScalarImplicitStr159(Vb6Type)`（Date/Double/Single/Long/Integer/Byte/Boolean/Currency）；
两处使用：`cgen_assign_com_prop.inc` 的 `class prop_let_` 实参发射、
`tryRewriteCOMLvalue` 的 Pattern C/D2。此前只在实参是 `vb6_VARIANT` 时才 `wrapToBSTR`，
标量/日期裸拼 → C2440「double→BSTR」9 条 → 1 条（`TextMatrix(i,3)=DateAdd(...)`、
`EditText = ComboCalendarValue`、`EditText = DateSerial(...)`）。

### 159-C / 159-E（已实施已测）
- **159-C**：`VarPtr(Variant(i))` → `vb6_VariantArrayGet` 按值返回结构体，
  `cgen_expr_call_builtin_pre.inc` 的 `&(void*){…}` 复合字面量把 Variant 塞进 `void*` 槽
  （C2440×7：VTableHandle.c 181/187/267、VBFlexGrid.c 18504~18645）。改为改名调用
  158f 的 `vb6_VariantArrayElemPtr(vb6_VARIANT*, int32_t)`（同参数形态，返回真左值地址）。
- **159-E**：`cgen_setlet_set_prop.inc` 的 Set 实参包装 —— **只在实参确实是 vb6_VARIANT 时**
  才介入（形参也判 Variant → 直传；否则 → `vb6_VariantToObjectVal`）。
  消掉 VBFlexGrid.c 22824/22827 的 `&(void*){Value}`（void**→VARIANT 与 (void*){vb6_VARIANT} 两条）。
  **教训**：第一版还额外按 `_psSym->params.back().type == Variant` 给非 Variant 实参补
  `vb6_VariantFromValue`，结果 `prop_set_DataSource(me, void* Value)` 被塞进结构体 →
  **新出 3 条 C2172「实参不是指针」**。原因：`_psSym` 可能因跨模块 storageKey 冲突命中同名
  别符号，其 `params` 不可信；C 形参真值看 `.h` 里的 `void* Value`。凡"根据符号表补包装"的
  分支，都要以只读实参形态为准，不要主动新增包装。

### 159-F（codegen：`Form.hWnd` 这类"过程级变量的成员"被当成"模块.成员"）
根因（`cgen_expr_member_class_module.inc` 优先级3）：`isVarName` 只查 `symTab_`（**模块作用域**），
当前过程的**形参/局部**看不见 → 走"模块.成员"分支 → `memSym` 命中 VBFlexGrid.ctl 的 `hWnd`
属性 → 发出全局 `vb6_VBFlexGrid_hWnd`（C2065×17）。
修正：对象标识符落在 `knownLongVars_/knownObjectVars_/knownVariantVars_/knownLongPtrVars_`
且**不在** `knownClassVars_`、成员是 `hWnd` 时，直接取该变量的值：
`lastExpr_ = "(intptr_t)(" + emitExpr(object) + ")"`。
依据是运行时的对象表示约定 —— **窗体/控件"对象值"本身就是它的 HWND**：
`vb6_Forms_Register(void* hwnd)`、`Me` → `vb6_hwnd_MainForm`、类实例经
`vb6_UC_InstanceOf(vb6_hwnd_VBFlexGrid1)` 反查。这条约定是本项目最省力的落点，别再回到
"给每个 Form/Control 变量建对象盒"的思路。
定位手段：`C3_DBG159=1` 钩子（`resolveClassMemberCall` 的 enter/089g/014fallback/scopeSym/
nomatch 五条返回路径 + 159-F 命中打印）。实测该 17 条**不走** `resolveClassMemberCall`，
所以钩子当时没输出 —— 靠"复现件（无类项目）发出 `vb6_Form_hWnd`、demo 发出
`vb6_VBFlexGrid_hWnd`"两者对比，才锁定是优先级3 的 `memSym->sourceModule` 前缀。
效果：**62 → 45**（C2065 43→26）。
**同一根因还剩 11 条 C2065**（`Form.Controls` / `Form.ScaleMode` / `CurrControl.Style` /
`VBFlexGrid.Name` / `.BorderStyle` / `.Enabled` / `UserControl.Width·Height` /
`Control.Index`）—— 单纯"拦截"只会把 `vb6_VBFlexGrid_Name` 换成 `vb6_Form_Name`，仍是
C2065；要真消掉得给这些 Extender 属性配运行时访问器（见任务清单）。

### Fix 160-A（LSet 目标漏 `me->` 前缀）：45 → **41**（C2065 26→22），回归进行中
`cgen_assign_stmt_special.inc` 的 LSet/RSet 分支**手写**目标名 `cIdent(tgtId.name)`，绕过了
标识符发射器 `cgen_expr_ident_symbol.inc:109-114` 的类模块字段规则。于是类私有模块级 UDT
（存储在 `vb6_cls_<T>` 结构体内）作为 LSet 目标时丢了 `me->`：
`LSet VBFlexGridComboBoxRect = RC` → `memcpy(&VBFlexGridComboBoxRect, ...)` = C2065。
**源侧**走 `emitExpr` 所以自带前缀，故只有目标侧报错。修复=在目标侧复刻同一条规则
（`isClassModule_ && currentProc_` + 非 external `Variable` + 不在 `knownLocalVars_` +
`lookupLocal` 不是 `Parameter`），顺带覆盖字符串 LSet/RSet 分支。
> **纠正前判**：这 4 条曾被记成"跨模块枚举可见性"（`VBFlexGridDefaultCols` 等）。它们
> 不是枚举成员，是模块级 UDT 变量；`VBFlexGrid.h:1566/1596/1644` 就在类结构体里声明。

### Fix 160-B（`vbSizeWE` 常量名拼错）：41 → **39**（C2065 22→20）
不是"常量值不可猜"的反例，而是**名字**错：语义表 `builtin_consts_ext.inc:175` 把它登记成
`vbSizeEW`，值 9 早已确定 —— RTL `vb6forms_style.c:227` 有 `case 9: return IDC_SIZEWE;`
与之互相印证（VB6 真实常量名是 vbSize**WE**，West-East）。全仓只有该表与此注释用到
`vbSizeEW`，无测试/VB 源码依赖 → 直接改名，不留别名。清掉 `vbSizeWE`×2。

### Fix 160-C（LSet 目标是**函数返回值**时名字与类型双错）：39 → **37**（C2065 −1、C2172 −1）
与 160-A 同根：LSet/RSet 目标手写 `cIdent(名)`。VB 里"给函数名赋值"即给返回值赋值，其 C
存储是本函数局部 `vb6_ret_<Fn>`；而 `knownUdtVars_` 是按 **`vb6_ret_<fn>` 的 lower** 注册的
（`decl/cgen_decl_func.cpp:215-217`），用裸函数名查不到 → 连类型也判错，从 UDT 分支掉进
字符串分支：
```vb
Private Function GetAppVersionInfo() As VS_FIXEDFILEINFO
    LSet GetAppVersionInfo = Value          ' Common.bas:754
```
→ `vb6_BSTR_Assign(&GetAppVersionInfo, vb6_LSet(vb6_CStr(…), SysStringLen(…)))` = C2065 **+** C2172
→ 修复后 `memcpy(&vb6_ret_GetAppVersionInfo, &Value, sizeof(vb6_type_VS_FIXEDFILEINFO));` ✔

### Fix 160-E（Set/Let 属性写的 `me` 实参漏了 UserControl 实例反查）：37 → **36**（C2065 18）
`cgen_setlet_set_prop.inc` 的对象实参是裸 `emitExpr(*_ma.object)`，对**放置在窗体上的
UserControl 实例**只会发出控件名（C 里根本没有这个变量）→ C2065：
```vb
VBFlexGrid1.CellPicture = Picture1.Picture        ' MainForm.frm Command1_Click
```
→ `vb6_VBFlexGrid_prop_set_CellPicture(VBFlexGrid1, …)` ✘，而**同函数下一行**的 Let 属性写
却是正确的 `(vb6_cls_VBFlexGrid*)vb6_UC_InstanceOf(vb6_hwnd_VBFlexGrid1)`。
根因：`cgen_assign_com_prop.inc:249-266`（Fix 152c）早已为"参数化属性写"实现了这条
`knownUserControlCtrlVars_` → `vb6_UC_InstanceOf(hwnd)` 规则，但 Set/Let 分支没套用。
修复=在 Set/Let 分支复用同一套映射与 `makeCtrlHwndArg`，不新造机制。

### Fix 160-F（Property Get 补设 `currentReturnCType_`）——**已重新实施，回滚理由确认为误判**
诊断本身是对的：`cgen_decl_prop.cpp:171` 只设 `currentReturnVar_`，**从不设
`currentReturnCType_`**（只有 `cgen_decl_func.cpp:168` 设）。而返回槽的 VARIANT→标量换算
按该字段判型（`cgen_assign_value_sem.inc` Fix 092f:265-291 / Fix 124:213-242），所以任何
Property Get 内 `X = <返回 Variant 的表达式>` 都不换算：
`CellFontSize = PropFont.Size` → `vb6_ret_CellFontSize = vb6_VariantFromComResult(…)`
→ 补上后**确实**干净地消掉了 VBFlexGrid.c 32210/32212 两条 `VARIANT→float`。

随后回归 **PASS=33 FAIL=55**，我把它归因于 160-F 并当场回滚。**该归因错误**：
回滚 + 重建 + 重跑，**失败集完全相同**（33/55，逐名比对一致），真因是
`src/rtl/core/vb6forms/vb6forms_axsite.c:449` 的 `ERROR_NOENTRY`（未定义常量）把
**内嵌 RTL 整个编译不过**，连 `hello` 都是 `FAIL (compile)` → 见下一节。
2026-09-21 在 RTL 修好、回归重新 **PASS=88 FAIL=0 SKIP=1** 之后单独重试：一行
`currentReturnCType_ = retType;`（放在 `mapTypeRef` 之后，与 `cgen_decl_func.cpp:168` 同型）。
**结论：净收益，保留。** VBFlexGrid.c 32210/32212 两条 `VARIANT→float` 如期消失。
读取方共 6 处（`cgen_util_classtype.cpp:180`、`cgen_with.cpp:214`、
`cgen_expr_member_obj_dispatch.inc:117/141`、`cgen_expr_binary.cpp:530/539`、
`cgen_assign_value_sem.inc:192/214/266`）逐一核过，**全部带前缀/等值守卫**，
补设只是让 Property Get 与 Function 同权，不存在"无条件生效"的读取方。
副作用当场暴露一条：`void*` 返回的 Property Get（`Prop Get DataMember() As MSDATASRC.DataMember`）
经 092f 被套上 `vb6_VariantToObjectVal(...)`，而实参 `me->PropDataMember` 的 C 存储当时是
`void*` → 反向 C2440。这不是 160-F 该回滚的理由，而是它**揭出了一处真实的两侧类型不一致** → 160-G。

> 归因方法的教训仍然成立：一次改动后大规模失败，先做"回滚后是否仍失败"的对照，
> 再下因果结论；只 diff 失败集就能当场推翻假设。

### Fix 160-G（`MSDATASRC.DataSource/DataMember` 是 String 别名）：单点修在 `TypeSystem::resolveTypeName`
`semantic_analyzer_typeref.cpp` 与 `cgen_base_type.cpp::mapTypeRef` 两层对"未识别类型名"的
兜底**方向相反**（语义 → `Variant`，codegen → `void*`，见后者末尾 return）。同名字段于是
一半按 Variant 记账、一半按 void* 声明：
`VBFlexGrid.h:1685 void* PropDataMember;` + `me->PropDataMember = vb6_VariantFromValue(Value)`
→ `C2440 VARIANT→void*`。Fix 157 的注释早已写明"两侧必须一致"，但当时只补了 Object 侧。
做法：在**两层共同的上游** `TypeSystem::resolveTypeName` 里把这两个限定名判为
`Vb6Type::String`（MSDATASRC.tlb 里 `DataSource`/`DataMember` 就是 LPWSTR typedef，
VB6 侧即 String），一次覆盖两层，无需在两处各写一遍集合。
收益不止消错误 —— 整条链改为按 String 走，语义同步转正：
`BSTR PropDataMember` + `vb6_BSTR_Assign` + `vb6_ComPackBSTR(me->PropDataMember)`
+ `vb6_StrPtr(me->PropDataMember)`（原先 `.ReadProperty/.WriteProperty/StrPtr` 三处用法全错位）。
只匹配 `msdatasrc.` 限定形式；本函数检查发生在符号表查找**之前**，收录裸
`DataSource`/`DataMember` 会遮蔽工程里的同名 ComInterface 符号。
合计 160-F + 160-G：**36 → 31**（C2440 17→12）。

> **踩坑记录（值得复看）**：首版写成 `lower.compare(0, 11, "msdatasrc.")` —— `"msdatasrc."`
> 只有 **10** 个字符，长度写错使条件永不成立，改动"编进去了却毫无效果"。当时先用
> `--dump-symbols` 看到 `PropDataMember : Variant` 才排除掉"没重编"的猜测，定位到这个 off-by-one。
> 改用 `lower.rfind("msdatasrc.", 0) == 0`（仓库既有惯用法）可根除这类手数偏移。
> 另：`grep -c <字面量> .build/C3.exe` 判断"改动是否进了二进制"并不可靠（会误命中）；
> 判据应是 **`.obj`/`.exe` 的 mtime 对比 + 构建日志里是否真的编译**。本次就撞上
> `dev.ps1` 报 `[OK] Build succeeded` 而日志实为 `ninja: no work to do`。

### 已修 RTL 中断（`vb6forms_axsite.c:449` `ERROR_NOENTRY`）——**曾使全仓 55/89 编译失败**
`ERROR_NOENTRY` 不是任何 SDK 里的常量，全仓也只有这一处使用 + 一处注释引用它。
经 `git show HEAD:… | grep -c` 确认 **HEAD 里没有**，是工作区**未提交改动**带进来的
（`+` 行）；它此前一直没被编进 `C3.exe`（RTL 内嵌在 exe 里，需重建才生效），是我为了
160-F 的那次重建才让它生效 → 于是 `hello`/`smoke` 这类最简工程全部 `FAIL (compile)`。
按 413 行注释的本意（`LoadLibrary` 成功但 `GetProcAddress(hMod,"DllGetClassObject")` 取不到
导出），正解是真实常量 **`ERROR_PROC_NOT_FOUND`(127) —— "模块已加载但找不到该函数"**，
语义精确且保持原 `HRESULT_FROM_WIN32(...)` 结构，不臆造新的失败码；注释同步改掉。
`tests/hello.bas` 编到独立 output-dir 验证：EXIT=0 且产出 `hello.exe`。


### Fix 161（`VB.`/`VBA.` 库限定名被降级成 Long）：**30 → 16**，一次消掉 12 条
160-F/G/H 各清 1–3 条之后, 剩余 C2065 被记为"14 条 extender 簇, 需要先造
HWND→设计期属性注册表子系统"。**该定性是错的**, 真因只有一行启发式:
`type_system.cpp` 的"Vb 前缀 ⇒ 枚举 ⇒ Long" (`lower.compare(0,2,"vb")==0`)
把 **`VB.Control` / `VB.Form` / `VB.UserControl` / `VB.MDIForm`** 一起吞了 ——
库限定名的头两个字母恰好就是 `vb`。于是
`Dim CurrControl As VB.Control` → `int32_t`, `ByVal Form As VB.Form` → `int32_t`;
而 `resolveTypeName` 是**语义层与 codegen 的共同上游** (codegen `mapTypeRef` 在
返回非 Unknown 时直接 `mapType` 短路, 不再走它自己那条 Vb 兜底), 所以两层一致地错,
表现为"类型对得上、只是成员访问找不到符号"的假象。
标量上的成员访问于是走 M22 降级成不存在的全局名 `vb6_Form_Controls` /
`vb6_CurrControl_Style` / `vb6_Control_Index` / `vb6_VBFlexGrid_Name` ... → C2065;
`CurrControl = <For Each 取到的 Variant>` → C2440 VARIANT→int32_t。
修法: 在 `resolveTypeName` 里把这 4 个已核实为对象类型的末段判 `Vb6Type::Object`
(demo 里 `VB.`/`VBA.` 限定的全部取值就这 4 个, 测试语料 0 命中)。判 Object 后
`mapType=void*`, 成员访问自然回到 `vb6_ComGet*Prop` —— 而 `Name/Index/Style/
Enabled/hWnd/Controls` **宿主模型本来就实现了**, 所以不需要任何新子系统。
语义收益(逐条核对过生成码, 不是只消错误):
`Form.Controls` → `vb6_ComGetObjectProp(Form, L"Controls")`;
For Each 变量 → `vb6_ComUnpackObject`; `CurrControl.hWnd/.Style/.Enabled` →
`vb6_ComGet*Prop`; `ProperControlName` 的 `.Index/.Name` 同理。VisualStyles 的
整条启用/禁用路径由此真正接通。

> **踩坑: 规则顺序是承重的。** 首版把新规则放在 `comparemethod` 那条之后, 也就
> 放在它本该抢跑的"Vb 前缀 ⇒ Long"**后面** → 测量 **30→30 毫无变化**。诊断没错、
> 代码也没错, 纯粹是放置位置。碰到"改了没反应"先查顺序/是否生效, 别急着回头怀疑诊断。
> 另注: 残留同类隐患 —— 末段不在白名单的库限定名 (如 `VBA.Collection`) 仍会命中
> codegen 自己在 `cgen_base_type.cpp` 里的 `Vb` 前缀兜底。本 demo 无此类取值, 未动。

### Fix 162（`Extender/UserControl` 的 Width/Height 从无赋值）：16 → **14**
`vb6_Extender_Width/Height` 在 `vb6rtl_com.c:556-557` 只有定义, **全仓无赋值 → 恒 0**,
注释还写着"设计期对象, 运行期恒不活动 → 初值随意"。该判断是错的:
`VBFlexGrid.ctl:28728` 在运行期用它做命中测试 ——
`If (X >= 0 And X <= UserControl.Width) And (Y >= 0 And Y <= UserControl.Height) Then RaiseEvent Click`
→ 命中测试恒假 → **Click 事件永不触发** (活体静默误编); 而 `UserControl.Width/Height`
连声明都没有 → 2 条 C2065。
两条命名事实: cgen 对宿主伪对象按 `<对象名>_<成员>` 发射, 所以 `UserControl.Width` 与
`Extender.Width` 是**两个不同标识符、同一个 VB 语义值**, 得一起填。
数据来源是精确值而非近似: `uc_host_create.inc` 的 `width/height` 形参本来就是**缇**
(同函数用 `vb6_TwipToX(width)` 换 scaleWidth), 故直接存进 `vb6_UCRec.extWidth/extHeight`,
在 `vb6_uc_push` 同步四处、`vb6_uc_pop` 对称还原 (沿用该文件既有的
save/restore 快照约定, 与 Fix 133u 的 `Extender.Height` 同一手法)。

### Fix 163（`vb6forms_axcontainer.c` 无原型调用返回 `void*` 的跨单元函数）
`vb6_HostObj_At`（返回 `void*`）与 `vb6_UC_ControlsItemByName`（返回 `void*`）在本文件
315/332 被调用, 但该文件没 include 它们的声明头 → C4013 + **MSVC 按隐式 `int` 编译**。
x64 下返回值被截成 32 位, 于是 `Form.Controls("txtDoc(0)", idx)` 拿到的控件句柄高位丢失。
这不是错误数问题（它只有警告）, 是活体静默误编 —— 与 162 同类。
修法：补 `#include "vb6forms_uc_internal.h"`。已随 15:11 的构建进 exe, 待 demo 复测确认
那 3 条 C4013 消失。

### Fix 164（DI `unknown` 族缺导入库 → 全套测试 64 例链接失败）
`output/yqt_regress25.log` = **PASS=24 FAIL=64**, 全量红。逐条 `FAIL (compile)` 但**不是**
代码生成回归：`C3.exe` 退出码非 0 的原因是 `LNK2019`×14 + `LNK1120`。
根因链（两处, 都是 160y 新增 `unknown` 族时带进来的）：
1. `src/rtl/core/di/vb6_di_unknown_stubs.c`（**未跟踪新文件**）被 `driver_link.cpp:160`
   **无条件**加入链接, 桩体转发的是**真实** Win32 API, 而 `scripts/gen_di_stubs.ps1:405`
   给 `New-Banner` 传的是**空** libs（`$usedLibs['unknown']` 恒空 —— 该族就是"没有
   `vb6_di_lib` 标记"的兜底桶, 桶里没有库名）→ `Imm*`×10、`GetFileVersionInfo*`×2、
   `VerQueryValueW`、`TransparentBlt` 共 14 个符号没有导入库。
   修：在收集阶段按 API 名前缀补 `imm32`/`version`/`msimg32`, 并把 `$usedLibs['unknown']`
   传给 `New-Banner`; 已生成的文件同步手工补 3 条 `#pragma comment(lib, ...)`。
2. **更要紧的工具链坑**：`CMakeLists.txt` 的 `C3RTL_EMBEDDED_FILES`（= `c3rtl.rc` 的
   `OBJECT_DEPENDS`）**漏了** `vb6_di_unknown_stubs.c`（100 个 RCDATA 对 99 个依赖）。
   后果：**改这个 RTL 文件不会重新内嵌**, 而 `dev.ps1` 照样打印 `[OK] Build succeeded`
   —— 这正是那条"[OK] 可能配 `ninja: no work to do`"记录的**成因**, 现已补进列表。
   任何人再遇"改了 RTL 但行为没变", 第一嫌疑就是这里没登记。
   验证方式仍是 `.build/C3.exe` 的 mtime, 不是日志里的 `[OK]`。

### Fix 166（`&(void*){...}` 包装多给一层间接 → demo 启动即 AV；**已修**）
**定位手法（可复用，本次全靠它）**：`.temp/yqt_demo_g.bat` ＝ `yqt_demo.bat` 加 `-g`
（C3 的 `-g` 会带 `/MAP` + `/DEBUG`，见 `msvc_driver.cpp:264`）→ 产出 `VBFlexGridDemo.map`；
跑 exe 从 WER 拿"异常代码/故障偏移"，再 `grep 1403xxxxx VBFlexGridDemo.map` 反查符号
（x64 默认基址 `0x140000000`）。**另一条更快**：RTL 里 `vb6com_invoke.c:199/222` 有 gated
trace，带 `C3_COM_TRACE=1` + `Start-Process -RedirectStandardError` 跑，最后一条日志直接点名。
本次实测：WER 偏移 `0x3327b0` 落在 **`.data`**（map 里 section `0003` = .data，`0001` 才是 .text）
的 `g_vb6_UserControl_FontObj` 上 = **执行了数据页**；trace 最后一条是
`GetProp entry: disp=<栈地址> isFont=0 name=hFont` → `fallback to IDispatch`。
**根因**：`cgen_setlet_set_prop.inc:244` 对 `Set obj.Prop = objRef` 无条件套复合字面量
`&(void*){值}`（该写法只对 **ByRef 对象槽 / C 形参 `void**`** 正确）。于是
`VBFlexGrid.c:2082 prop_let_Font → prop_set_Font(me, &(void*){NewFont})` 让
`me->PropFont` 存成**复合字面量的地址**，其首 8 字节才是真字体指针
（恰好 `== &g_vb6_UserControl_FontObj`）→ 后续 `vb6_ComGetObjectProp(Font, L"hFont")`
把该栈地址当 `IDispatch*`、把首字段当 vtable → 跳进 .data。
**为什么编译期完全看不出来**：C 允许 `void**` 传给 `vb6_ComIface_Font*`，只有指针不匹配
**警告**；而 C3 只在**有 error 时**才落 `_c3_msvc_out.txt` —— demo 是"零错误"构建，
所以 0 错误 + 链接成功 + 一跑就崩。**推论：demo 报"0 错误"绝不能当成"没问题"。**
**改法**：用 `ParameterInfo::isByVal` 判别 —— 被调方形参是 ByVal 就不套包装
（`VBFlexGrid.ctl:3258` 确证 `Property Set Font(ByVal NewFont As StdFont)`）。
效果：demo 里 `&(void*){` 18 → 8（剩下 8 处是真正的 ByRef 槽），仍 0 错误、仍链接通过，
**AV 消失（退出码从 `0xC0000005` 变成 0）**。
**下一步（新线索，未查）**：现在进程**跑起来了但 8 秒内正常退出**，不再崩；
trace 里出现新的可疑接收者 `GetProp entry: disp=0x140332800 isFont=0 isHost=0 name=Height`
—— `0x140332800` 是 **`vb6rtl_com.obj` 的 .data 全局区**（`g_vb6_UserControl_FontObj` 是
`0x1403327b0`，+0x50），即"把某个 vb6rtl_com 全局的地址当对象用"，和 Fix 166 同族但
**是另一处发射**（disp 来自 .data 而非栈）。另外 `GetObjectProp obj: disp=...3327B0
prop=hFont vt=0 -> obj=0` 说明 `hFont` 这条路现在走到"字体对象没有 hFont 字段"→
返回 NULL（RTL 侧 Fix 125 的字体表只认 Name/Size/Weight/…，`hFont` 需要按
Name/Size/Weight 现造 HFONT 并缓存，否则 GDI 字体度量仍不等价于 VB6）。

### Fix 167（Sub Main 缺 VB6 驻留语义 → 进程秒退；**已修**）
demo 链接通过后"跑起来又立刻自己退出"（退出码 0，不是崩）：`Startup.c` 的入口模板是
`vb6_Init → 各模块 init → vb6_Startup_Main() → vb6_Exit() → return 0`，
**全仓生成码里 `vb6_MessageLoop` 出现 0 次**（只有 WinMain 分支会发它）。
而 Sub Main 体内是 `vb6_form_show_MainForm(NULL, 0)`（modeless）→ 一返回进程就走完。
VB6 语义是 Sub Main 返回后运行时继续泵消息直到所有窗体关闭。
改法：RTL 加 `int vb6_AnyThreadWindowVisible(void)`（`vb6forms.c`，
`EnumThreadWindows` + `IsWindowVisible`，**不建窗体注册表**；声明在 `vb6forms_window.h`），
两处 main 模板（`cgen_base_generate_entry.inc` 的 ~113「首个 Public Sub」支与
~160「Public Sub Main」支）在调用后插 `if (vb6_AnyThreadWindowVisible()) vb6_MessageLoop();`。
判据选择的原因（务必别改成无条件进循环）：`App.PrevInstance` 那一支只操作**别的进程**的
hwnd，本线程没有窗口 → 自然不驻留；纯 .bas 控制台也没有可见窗口 → 行为不变。
**结果**：不再秒退，但崩在消息循环里的**下一处**（`0xC0000409`），线索即上方
`disp=0x140332800 name=Height` —— **属进展，不是回退**。

### Fix 165（GUI 入口点写死 + 生成器丢桩 → regress26 仍 14 例红）
`yqt_regress26.log` = **PASS=74 FAIL=14**（164 把 64→14）。这 14 例**全是链接阶段**，
分两个根因，都在未提交的 160y 系列里，**与 160-F/G/H、161、162、163 无关**：

**① 13 例：`/ENTRY:mainCRTStartup` 被写死**（`msvc_driver.cpp:232`、
`msvc_driver_incremental.cpp:261`，均为本次未提交新增）。
`/SUBSYSTEM:WINDOWS` 的默认入口是 `WinMainCRTStartup`（找 `WinMain`），加上该 flag 后
变成找 `main` → 所有**窗体启动**工程 `LNK2019: main`。
但不能简单回退：`VBFlexGridDemo.vbp` 是 `Startup="Sub Main"`，codegen 按
`cgen_base_generate_entry.inc:124` 发的是 `int main()`，它**恰恰需要**这个 flag；
而 5 个 GUI 测试工程（Project1/BalloonTooltips/czFormDemo/Test/NewTab）全是
`Startup=<窗体名>` → 发 `WinMain`。**结论：入口点必须跟着启动对象走**，写死任何一边
都会让另一边全红。
修法：`MsvcDriverOptions` 加 `entryIsMain`，`driver_link.cpp` 用与
`cgen_base_generate_entry.inc:54` **同一判定**（`tolower(startupObject_)=="sub main"`）
填它，两处链接命令按该字段决定加不加 `/ENTRY:mainCRTStartup`。
**该判定有两份、必须同改**，已在两边都留了交叉引用注释。

**② 1 例：`test_declare` 缺 `vb6_di_GetTickCount`。** 这不是孤例 —— 与 HEAD 对比，
`gen_di_stubs.ps1` 本次重生成**净增 190 个桩、同时删掉了 39 个原有桩**。
根因是设计性的：生成器只扫**一个编译会话**目录里的 `.h`，本会话没声明到的符号就不在
输出里，而它**整文件覆盖**族文件 → 没被扫到的桩静默消失。
39 个名单（已恢复，记录在此以免下次丢失时无从比对）：
`ChooseColorA CopyImage CreateRoundRectRgn CreateWindowExA DestroyCursor
DwmSetWindowAttribute FindWindowExA GetDesktopWindow GetFileTitleA GetModuleHandleA
GetOpenFileNameA GetParent GetPropA GetTickCount GetWindow GetWindowLongA
IsWindowUnicode IsZoomed LoadCursorA LoadLibraryA MakeSureDirectoryPathExists
PathMatchSpecW PostMessageA PtInRect RemovePropA RtlFillMemory SHBrowseForFolder
SHGetPathFromIDListA SendMessageA SetPropA SetWindowLongA SetWindowRgn TlsAlloc
TlsFree TlsGetValue TlsSetValue VirtualAlloc VirtualFree lstrlenA`
比对方法（可复用，下次再丢一签就能立刻查出来）：
`grep -hoE '^[A-Za-z_][A-Za-z0-9_ ]*\**__stdcall vb6_di_[A-Za-z0-9_]+\(' 目录 | sort -u`
与 `git grep -hoE` 同一模式跑 HEAD，再 `comm -13`。
**注意**：`\w` 在 `[]` 括号类里不是字符类，写 `[\w ]` 会匹配不到东西、得到假的"全丢"。
**已把这 39 个恢复进手写文件 `vb6_di_stubs.c`** —— 那里是安全位置：
生成器 `:162` 的 `$hand` 会跳过本文件已定义的名字，所以再重生成也不会丢。
补桩需要同时补头文件与导入库：新增 `<shlwapi.h> <shlobj.h> <dwmapi.h> <imagehlp.h>`
和 `shell32/dwmapi/imagehlp/comdlg32` 四条 pragma。
- **坑（第一版就踩）**：`RtlFillMemory` 在 `winbase.h` 里只有**函数式宏**、没有声明，
  而桩体写的是 `(void (WINAPI *)(...))RtlFillMemory)` —— 后面没有 `(` 故宏不展开 →
  **C2065 未声明标识符**。RTL 编译失败会连带让**所有**工程链接不过，看起来像全局回归，
  实际只有一条错误。手法同 `vb6_di_unknown_stubs.c` 顶部：`#undef` + 自己声明。
- **待办（尚未做，建议由 160y 的作者定夺）**：把生成器改成**非破坏式** —— 每个族的
  现有文件先解析成"已知桩"再合并，而不是整文件覆盖；否则下次重生成仍会再丢一批。

### 待做 160-D（`With <返回 UDT 的函数>()`，3 条 C2440 + **又一处静默误编**）


`Common.bas:698/708/718` 的 `With GetAppVersionInfo()` → `Common.c:617/630/643`：
```c
void* _vb6_with_3 = (void*)vb6_Common_GetAppVersionInfo()  /* With object ref */;  C2440
vb6_ret_AppMajor = vb6_ComGetStringProp(_vb6_with_3, L"dwFileVersionMSHi");        ← 误编
```
`.dwFileVersionMSHi` 是 **UDT 字段**，却被降成 COM 字符串属性读取，还把 BSTR 指针赋给
`int16_t` 返回值 —— 第二个错是第一个错的**连带**后果：`cgen_with.cpp:425-432` 只在
`tempType=="void*"` 且 `inferUdtTypeOfExpr` 命中时才切到结构体分支，而
`inferUdtTypeOfExpr`（`cgen_util_classtype.cpp:165+`）覆盖了 IdentifierExpr（含本函数名，
Fix 090j）与 IndexOrCallExpr 的**数组元素**，唯独没有"调用一个返回 UDT 的函数"。
修好类型推断即可同时消掉两错。**两个必须注意的坑**：
1. `cgen_with.cpp:448` 的 UDT 分支发的是 `&(<expr>)`，对**函数调用（右值）非法** →
   需要"值临时 + 取其指针"两条语句（VB 语义上 With 绑定的就是返回的临时值，改不回原对象）。
2. 注册表**不能**在 codegen 函数声明时才填：`AppMajor`(697) 在 `GetAppVersionInfo`(726)
   **之前**，按生成顺序取会落空 → 必须走符号表或声明预扫描。模块级 Function 符号当前只存
   `Vb6Type returnType`，命名类型名仅 Class 符号有（`symbol_table.hpp:114-121`
   `memberReturnTypes`），需确认/补齐模块函数的返回类型名可读性。

### 遗留缺陷 160-W（With/StrPtr **跨过程发射泄漏**，优先级高于错误数，见任务 #13）

MainForm.c 两处证据（`.frm` 为 `MainForm.frm:519/527`）：
1. `Command5_Click`: `.Prompt = "Text for Cell R" & VBFlexGrid1.Row & ...`
   → `MainForm.c:617` 首个拼接操作数变成 `vb6_ComGetStringProp(_vb6_with_14, L"Result")`，
   字面量 `"Text for Cell R"` **消失**；而 `_vb6_with_14` 属于**上一个过程** `Command13_Click`。
2. `Command10_Click` 体内只有一句 `VBFlexGrid1.CellEnsureVisible FlexVisibilityCompleteOnly`
   → `MainForm.c:628` 整句变成 `vb6_ComVarFree((void*)vb6_ComCall(_vb6_with_15, L"Result", …))`，
   真正的语句没了。
已排除：`src/backend` 内**没有**按 `ASTNode*` 为键的 memo（grep 无命中）；
`cgen_with.cpp` 的 `withObjectVars_`/`withObjectInfoStack_` 在 437/505 入栈、513/514 出栈，
437→514 之间**无早退**，看起来是平衡的 → 需要最小 `.frm` 复现 + 插桩才能定位，不能靠读码猜。

### 进度：236 → 147 →（修复工具链后重测）91 → 80 → 72 → 62 → 45 → 41 → 39 → 37 → 36 →
### 31 → 30 → **16** → **14**（160-F/G 与 161 已过回归门禁；160-H/162 见下）。
### 分布（162 状态）：C2065×4 + C2440×9 + C2172×1。
### 回归：158q..159-B、159-C/159-E、159-F、160-A、160-B+160-C、**160-E**、回滚 160-F 后
### (＝RTL 修好)、**160-F+160-G**、**160-H**、**161**、**162+163+164+165（含用户 160y 全套）**
### 均 **PASS=88 FAIL=0 SKIP=1**；
### 当时 160-F 的 **PASS=33 FAIL=55** 实为内嵌 RTL 的 `ERROR_NOENTRY` 所致，**非 160-F**（见上）；
### regress25 = **24/64**（unknown 族缺导入库，Fix 164）、regress26 = **74/14**
### （入口点写死 + 39 桩被删，Fix 165）、regress27 = **88/0/1** ← 当前基线。
### 教训：**全量红先分"编译期 vs 链接期"** —— 25/26 两次的红一例代码生成回归都没有，
### 14/64 条全在链接阶段；若只盯错误数会误判成 codegen 塌方。
### **166**（codegen 改动）→ regress28 = **88/0/1**，无回归，当前基线。
### ⚠ 即使 C2440/C2065 清零也仍**链接不过**：有 6 个符号**全仓库无定义**，于是链接期 LNK2019：`vb6_Choose`×3、`vb6_PropertyPage_ValidateControls`×3、`vb6_UserControl_OLEDrag`、
`vb6_Extender_Drag`、`vb6_Extender_SetFocus`、`vb6_Extender_ZOrder`。
（更正记录：先前此处写作"C4013 链接期会变成错误"是**含糊且不准确**的 ——
C4013 本身从不产生链接错误，只要符号**有定义**就能链接通过；真正决定成败的是
"有没有定义"。C4013 的危害是另一回事且**更隐蔽**：无原型时按 `int` 假设返回值，
x64 下 64 位指针被截成 32 位、`double` 被读成 `int` —— 链接通过、运行期错乱。
已确诊一例：`vb6forms_axcontainer.c` 调 `vb6_HostObj_At`/`vb6_UC_ControlsItemByName`
（均返回 `void*`）无原型 → `Form.Controls(name)` 句柄截断，见 Fix 163。）
出 exe 前必须先处理无定义这一族。
### 测量产物：生成码已固化到 `.temp/gen/*.c|h`（C3C 临时目录会被后续编译清掉，别依赖它）；
### 一步完成"编译+快照+分族计数"的脚本：`bash .temp/measure.sh`。

### ★ 里程碑（2026-09-21 17:05）：VBFlexGridDemo **编译 + 链接全通过**
`cmd //c .temp\yqt_demo.bat` → **`C3_EXIT=0`**，产物 `D:\c3.vb6.pro\VBFlexGridDemo.exe`（1.73 MB）。
会话目录里**没有 `_c3_msvc_out.txt`** = MSVC 零诊断；生成码 14 个 `.c`。
**所以：下面这份"遗留 14 条"清单连同整个"消减 MSVC 错误"的框架已经作废**（多半是
160y/161/162/163/164/165 这一批一起把它推过了终点，未逐条归因），保留仅作历史参考。
**新前沿是运行期崩溃**：exe 启动即死。我们看到的退出码是 `0xC0000409`（fastfail），
而 WER 记录的首因是 **`0xc0000005` 访问违例，faulting module 是 exe 自身，RVA `0x199928`**
（不是系统 DLL → 十有八九是生成码或 RTL 的问题，不是 Win32 用法问题）。
exe **没有 PDB/.map** → 下一步要么给链接行加 `/MAP`（或 `/Zi`）把 RVA 映射回源码，
要么在 `vb6_Init` / 启动窗体 `Form_Load` 路径插桩。
**动手前先看任务 #19**：另一名作者当时正在同一工程目录跑 `bisect1..7.vbp`
（17:43–17:46，按模块子集二分）做同一件事，先分工别撞车。

### 遗留（14，162 状态）—— 逐条定位，旧清单见其后的"历史明细"
- **C2065×4**：`_vb6_with_14`/`_vb6_with_15`（MainForm.c 617/628）= **160-W 跨过程发射泄漏**，
  不是命名问题（任务 #13）；`UserControl`（VBFlexGrid.c:1552）= **裸标识符**
  `With UserControl` 的对象值位置没有 C 符号 —— `cgen_with.cpp:322` 的宿主伪对象特判
  只覆盖**成员访问**对象（`With UserControl.Parent`），不覆盖裸标识符；
  `vb6_ctrl_subproc_Picture2`（MainForm.c:281）= **功能缺口**，`Picture2` 是带
  `_Paint/_MouseDown/_MouseMove/_MouseUp` 事件的 VB.PictureBox，codegen 装了子类化过程
  却从未发射它。
- **C2440×9**：`VS_FIXEDFILEINFO→void*`×3（Common.c 617/630/643）= **160-D**（任务 #14）；
  `vb6_UserControl_Extender_Type→void*`（VBFlexGrid.c:1521）= `With UserControl.Extender`
  取的是**结构体值**而非指针，与上一条同源；`VARIANT→BSTR`（Common.c:530）；
  `Font*→float`（MainForm.c:346）；`double→BSTR`（UserEditingForm.c:588）；
  `BITMAPINFOHEADER→BSTR`（VBFlexGrid.c:55327）；`HANDLE→double`（VisualStyles.c:411）。
- **C2172×1**：MainForm.c:697 `OleCreatePropertyFrame` 实参不是指针。
- 另有 3 个 Extender 方法（`vb6_Extender_Drag/ZOrder/SetFocus`）在生成码里是 implicit-extern
  警告（C4013），但**根因是无定义**而非缺原型 → 链接期 LNK2019，见上方 ⚠ 的更正说明。

### 历史明细（161 之前的 31 条定性，多数已被 161/162 消掉，保留以免重复调查）
- **C2065×18**：三个小包。① **同 159-F 根因、成员不是 hWnd 的 14 条**：`vb6_Form_Controls`×2、
  `vb6_Form_ScaleMode`×2、`vb6_CurrControl_Style`×2、`vb6_VBFlexGrid_Name`×2、
  `vb6_VBFlexGrid_BorderStyle`×2、`vb6_VBFlexGrid_Enabled`×1、`vb6_UserControl_Width`/`Height`、
  `vb6_Control_Index` → 任务 #12。
  **① 的可行性已摸清（子代理核查）**：发射点是 M22 `cgen_expr_member_m22_module.inc:103`
  （把对象名当模块前缀）与 `cgen_expr_member_obj_dispatch.inc:206` 之后的"成员命中别处
  注入的同名外部符号"路径（Fix 110e 注释已记载该失效模式）→ `CurrControl.Enabled` 撞到
  VBFlexGrid 的 `Property Get Enabled`。运行时侧 `vb6_ComGetProp` 已经把宿主对象转发给
  `vb6_Host_GetProp`（`vb6com_invoke.c:212`），且 `vb6_Host_IsHostObject` 用 `IsWindow`
  认裸 HWND（`uc_hostmodel.c:75`）→ **路由是安全的、且不需要新子系统**。
  但 `uc_hostmodel_getprop.inc` 只实现 `Name/Index/Enabled/ScaleMode/Controls/Width/Height/
  hwnd/Caption/Text/Font/hDC/Visible`；**`Style`、`BorderStyle` 没有实现**，而子代理确认
  **不存在 HWND→设计期属性 的持久注册表**（`vb6_HostObj_Register` 只存 name/typeName/index/
  isForm/instance；PropBag 只对 `.ctl` 创建且随即释放；窗体的 ScaleMode 在 getprop.inc:25
  对 `.frm` 硬编码为 1）→ 把 `Style`/`BorderStyle`/`Form.ScaleMode` 路由过去会**编得过但
  值恒为 Empty/1**，属静默误编，必须先补设计期属性注册表，不能只改名。
  ③ `vbSizeWE`×2 → 已由 160-B 解决。④ 杂项 4 条，逐个定性：
  `_vb6_with_14`/`_vb6_with_15`（MainForm.c 617/628）= **160-W 误编**，不是命名问题；
  `UserControl`（VBFlexGrid.c:1554）= 宿主伪对象 `UserControl` 出现在**对象值位置**
  （`With UserControl` → `(void*)UserControl`），没有对应 C 符号；其 `.Width/.Height`
  （:60194 两条 `vb6_UserControl_Width/Height`）按 M22 发成全局，而头文件里只有
  `vb6_Extender_Width/Height`（`vb6rtl_userctl.h:122-123`）→ 同一簇需一起定夺；
  `vb6_ctrl_subproc_Picture2`（MainForm.c:281）= **功能缺口**，`Picture2` 是带
  `_Paint/_MouseDown/_MouseMove/_MouseUp` 事件的 VB.PictureBox，codegen 装了子类化过程
  却从未发射它。**常量值不可猜**（猜错即静默误编）。


- **C2440×12**：剩余都是小包，按族：
  **更正一处旧定性**：VBFlexGrid.c 960/1086/2071 的 `VARIANT→void*`×3 曾被记成
  "Ambient 属性赋给对象字段"，实际全部是 `PropDataMember`（`MSDATASRC.DataMember`）
  的两层类型不一致 → 已由 **160-G** 一并解决（Ambient 簇因此根本不存在）。
  `VARIANT→float`×2（VBFlexGrid 32210/32212）已由 **160-F** 解决。剩下 12 条：
  `vb6_type_VS_FIXEDFILEINFO→void*`×3（Common.c 617/630/643）= **上面已单列的 160-D，定性为
  `With <返回 UDT 的函数>()`，与 LSet 无关**，
  `VARIANT→int32_t`×2（VisualStyles 149/184，
  `CurrControl.Style`/`.hWnd` 系）、`VARIANT→vb6_vartype`（VBFlexGrid 22606，
  `&(vb6_VARIANT){<void* 表达式>}` 复合字面量把 `prop_get_CellTag` 的 `void*` 返回值
  当首个成员 `.vt` 初始化 → 应走 `vb6_VariantFromValue`，是包装选择 bug 而非缺类型信息）、
  `VARIANT→BSTR`（Common 530）、
  `BITMAPINFOHEADER→BSTR`（55326）、`UserControl_Extender_Type→void*`（1521）、
  `Font*→float`（MainForm 346）、`HANDLE→double`（VisualStyles 411）、
  `double→BSTR`×1（UserEditingForm 588，`ComboCalendarValue` 的返回类型未被识别为 Date →
  159-B 白名单没命中，需查 `getClassMethodReturnType`）。
- **C2172×1**（原 ×2）：MainForm.c:697 `OleCreatePropertyFrame` 实参不是指针。另一条
  Common.c `vb6_CStr(vb6_VariantFromValue(UDT))` 已由 **160-C** 顺带消除（它本就走错了
  字符串 LSet 分支）。
- 3 个 Extender 方法（`vb6_Extender_Drag/ZOrder/SetFocus`）目前是 implicit-extern 警告，链接期会变成错误。

---

## 12. 2026-09-21 晚：Fix 168 / 169（demo 已进消息循环）+ 下一个 blocker 170

### Fix 168（`UserControl.Extender` 被当 IDispatch → 启动 AV；**已修**，RTL 侧）
`With UserControl.Extender: .Width/.Height/.Align`（VBFlexGrid.ctl `UserControl_Resize`）编成
`vb6_ComGetProp(&vb6_UserControl_Extender, L"Height")` —— 那是普通结构体（`vb6rtl_com.c:580`，
仅 Visible/Height 两字段），对它做 `lpVtbl->GetIDsOfNames` 即跳进 `.data` → 0xC0000005。
与 Fix 125（字体）、Fix 166 同族（**C 层"结构体指针当对象"只会告警，错误数指标不出来**）。
修法照 Fix 125 样式：`vb6_UC_IsExtender`/`vb6_UC_ExtenderField`（`uc_controls.c`，声明放
`vb6forms_controls.h` —— vb6com 单元 include 它，不需要手写 extern）+ `vb6_ComGetProp`/
`vb6_ComSetProp` 各一个守卫分支。**成员映射到 `vb6_Extender_*` 全局槽位**（Fix 162 起由
`vb6_uc_push` 按实例同步的真实存储），而不是结构体字段 —— 后者是 Fix 133u 给"直接字段访问"
用的兼容壳，读它会拿到恒 0 的假值。`GetIntProp/GetDoubleProp/GetStringProp/GetObjectProp`
都是 `vb6_ComGetProp` 的薄封装（`vb6com_wrap.c`），一处守卫全覆盖。

### Fix 169（Pattern C/D2 把 COM 的 `VARIANT*` 裸拼给具体类型 Let 形参 → 未处理 Err 380；**已修**）
Fix 167 让 demo 进了消息循环后，进程改为 `Unhandled error 380` 退出。链路：
`UserControl_ReadProperties` 的 `OLEDropMode = PBag.ReadProperty("OLEDropMode", 0)` 走
`cgen_util_comwrite.cpp` 的 **Pattern C/D2 字符串级改写**，把 `vb6_ComCall(...)` 的返回值
（**Windows `VARIANT*`，即 `void*`**）直接拼进 `prop_let_OLEDropMode(me, int32_t Value)`
→ C 只做指针→整数截断（不是错误）→ `.ctl` 的 `Select Case` 落 `Case Else` → `Err.Raise 380`
→ 该链无处理器 → `vb6_ErrRaise` 打印后 `ExitProcess(380)`。既有 156-B 分支专治此类，但它的
判定 `cExprIsVariant()` 只认**返回 vb6_VARIANT** 的前缀，`vb6_ComCall(`/`vb6_ComGetProp(`
不在表内（它们返回 `void*`），于是漏网。修法：在该分支里先把 COM 结果用既有惯用法
`vb6_VariantFromComResult(...)` 归一成 vb6_VARIANT，再走同一条 `ext156` 提取链。
demo 内 **202 处**受益；380 消失，`vb6_AnyThreadWindowVisible` 之前不再被错误打断。
`vb6_ComCallObject`/`vb6_ComGetObjectProp` **不能**进这个白名单（它们已解包成对象指针）。

### Fix 170（下一个 blocker：`B() = Text` 整数组赋值被当成 0 号元素写；**未修**）
Common.bas:1474 `StrToVar` / :1484 `VarToStr`：
```vb
Dim B() As Byte
B() = Text        ' VB6: 整数组赋值 —— 按字符串原始内存(Unicode, 2*Len 字节)建 Byte()
StrToVar = B()    ' VB6: Byte() → Variant (VT_ARRAY|VT_UI1)
```
现编成 `VB6_SA_AT(uint8_t, B, 0) = Text;`，而 `B` 是 `vb6_SafeArray1D* B = NULL;`
→ **对 NULL 数组第 0 元素赋值** → 0xC0000005（WER 故障偏移 0x21a6f6 → `Common.c:1753`，
`VarToStr` 的同一行号族）。它坐在 `UserControl_ReadProperties` → `VarToStr(ReadProperty(…))`
路径上，**启动必经**，所以这是当前唯一挡住"进消息循环"的缺陷。
需要四个方向（`arr()` 空下标 = 整数组，不是 index 0）：
① `B() = <String>`（建数组，注意是 **Unicode 原始字节**，`vb6_StringToByteArray` 是
StrConv(vbFromUnicode) 的 ANSI 语义，**不能直接用**，需新 helper 或复用 `LenB` 语义）；
② `B() = <Variant(array)>`（数组赋值/引用）；③ `<Variant> = B()`（包成 VT_ARRAY|VT_UI1）；
④ `<String> = B()`（字节按 Unicode 重解释成 BSTR）。
发射点在"空下标 ArrayAccess"的 LValue/RValue 生成处（`indices.empty()` 目前无特判，
`src/backend/` 全局搜不到 `whole array` 相关处理），正落在用户 175 系列的活跃改动区。

### 工具升级：用 PDB 行号符号化取代 .map RVA（`.temp/sym2.cs` + `.temp/sym3.ps1`）
`/MAP` 的 "Publics by Value" **不含 file-static 函数**，VBFlexGrid.obj 里生成的过程多为静态
→ 按 map 找"最近前驱公共符号"会给出 `+0xCC6E` 这种废位移（本会话因此一度把 380 错定位到
`prop_get_Clip`）。改走 dbghelp：`SymLoadModuleEx(h, NULL, exe, NULL, 0x140000000, 0, …)`
+ **经典 `SymGetSymFromAddr64`/`SymGetLineFromAddr64`**（`SYMBOL_INFO` 那条现代 API 一直
返回 win32=87，经典 API 直接给出 `Common.c:1753` 级别的精确行号）。
配套：① WER 的"故障偏移"就是 RVA，直接 `0x140000000 + RVA`；② 若要符号化 `stderr` 里打印的
**运行时**地址，基址不能假设（本会话按 0x...0000 猜错过一次 0x90000），用 trace 里已知全局的
地址反推：`base = disp(实测) − Rva+Base(map)`，例如 `vb6_UserControl_Extender` → 0x140333840。
③ `vb6_ErrRaise` 在 `C3_COM_TRACE` 下打印 `_ReturnAddress()`（未处理错误的调用点），
配合上面两条即可"错误号 → 生成的 .c 行号"。

### Fix 171（VB6 **数字行号标签**：声明 + `GoTo`/`GoSub`/`Resume`/`On Error GoTo` 引用；**已修**）
VB6 里行号就是标签（`100: x = 1`，冒号可省 → `100 x = 1`，行号 <65536），可被
`GoTo 100` / `GoSub 100` + `Return` / `Resume 100` / `On Error GoTo 100` 引用。
C3 原先只认**命名标签**：标签识别要求 `canBeName(cur_.kind)`，`parseGoToStmt` 走
`expectName`，于是语句开头的整数落到 `parseStatement` 的 `default:` →
`VB2002 unexpected token in statement: 100`。
**改动全在 parser 一层**（这点值得记住）：AST 的 `LabelStmt/GoToStmt/...` 只存
`labelName` 字符串，语义层 `declaredLabels_`/`gosubTargetLabels_` 是纯字符串比对，
后端发的是 `"vb6_label_" + cIdent(name)` —— 数字拼上前缀天然是合法 C 标签。所以：
① `parser.hpp`/`parser_helpers.cpp` 新增 `expectLabelTarget()`（整数字面量或名字都收）；
② `parser_stmt.cpp` 的 `parseStatement` 加 `case TokenKind::IntegerLiteral:` →
`parseLabelOrAssignmentOrCall()`（语句起始处只可能是行号：赋值/调用左值必须是名字）；
③ `parser_stmt_assign.cpp` 在命名标签分支后加数字标签分支（冒号可选，且沿用
`inSingleLineIf_` 守卫 —— 单行 If 内冒号是分隔符不是标签）；
④ `parser_stmt_jump.cpp` 的 GoTo/GoSub/Resume/OnError/OnGoTo/OnGoSub 目标改走
`expectLabelTarget`。
用例落在 `tests_github/t0_cases/`（按语义拆开，取代我先前那个 `tests/test_line_labels.bas`）：
`test_goto.bas` = 命名标签前向 GoTo、`200:` 带冒号行号、`100` 省略冒号 + 后向 GoTo 循环、
`On Error GoTo <命名>` + `Resume 400`、`On Error GoTo 500` + `Resume Next`（注意落点是**出错句
的下一句**）、单行 `If … Then a = a + 1: a = a + 1` 里冒号是分隔符、`GoTo` 跳出 `For` 到过程尾标签；
`test_gosub.bas` = `GoSub <命名>` + `Return`、`GoSub 300`、循环内 `GoSub`。
**注册方式变更**：这两个文件原先在 `$syntaxTests`（只跑 `--syntax-only`，退出码判定，测不出运行期
语义），现改为一边从语法队列移除、一边加 `Add-BasTest`（编译 + 运行 + 输出串比对）。
用例总数不变，仍是 **89**（新增 1 个跑用例、少 1 个纯语法用例，净增 0；我中途加的
`test_line_labels` 已删）。
**顺带发现但未修（先前就在的缺口，非本次引入）**：`On <expr> GoTo 100, 200` 的**入口分发**
不通 —— `parseOnStmt` 只在 `next_` 是 `Error`/`GoTo`/`GoSub` 时才分派，`On k GoTo …`
里 `next_` 是标识符 `k` → 直接报 "expected 'Error GoTo'…"，`parseOnGoToStmt` 实际不可达
（`parser.hpp` 有 `peek()/peek2()`，要补就是 6 行）。全仓库 `.bas` 用例零使用，
所以此前无人踩到；本次只把它的标签目标改成了 `expectLabelTarget`（等分发修好即生效）。

### 教训：harness 的 Read/grep 会给**陈旧视图**，盘上真相用 bash 复验
本会话两次踩到：① `vb6com_invoke.c` 的 Read 返回了 Fix 168 之前的版本，据此误判"用户回退了
我的改动"；② `cgen_decl_prop.cpp` 的 Read 是用户拆分成 .inc **之前**的 49KB 版本，据此做的
Edit 报"成功"但**盘上没落**（该文件仍是用户的 17632 字节版本）。凡"我的改动在不在盘上"
"用户是不是回退了"这类判断，一律 `git diff` / bash `grep` 复验；对用户活跃改动的文件
（`src/backend/decl/*`、`src/frontend/*`）不要基于工具快照下结论。

## 13. 2026-09-21 深夜：Fix 170 / 172 / 173 —— demo 第一次真正跑起来（有窗口、进消息循环）

### Fix 170：VB6 **整体数组引用 `A()`**（空括号）以前被当成 0 号元素
`Common.bas:1479` `StrToVar` 里 `Dim B() As Byte: B() = Text` —— 一维空括号走
`cgen_expr_call_prelude.inc` 的 first 分支生成 `VB6_SA_AT(uint8_t, B, 0)`：未 ReDim 的
动态数组 `data==NULL`，写 0 号元素 = 解引用 NULL → 启动 AV（就是 168/169 之后剩下的那一个
blocker）。右值同口径也是错的：`StrToVar = B()` 只搬走第一个字节、`UBound(B())` 拿到的是
字节而不是载体。改动：
① `cgen_state.inc` 加 `wholeArrayLvalue_`（只在 emit `node.target` 那一瞬为 true）；
② `cgen_assign_com_prop.inc:362` emit 目标前置位该标记 + 记局部 `tgtIsWholeArray170`；
③ prelude：`positional.empty()` 且（左值上下文 **或** 一维）→ `lastExpr_ = arrName`
（数组载体本身，左值合法）；2D+ 右值仍保留 Fix 152 的 `vb6_VariantArray` 装箱；
④ `cgen_util_type.cpp` 新增 `isWholeArrayRef()`（口径与 prelude 的 isArrayAccess 一致：
先 `knownArrays_` 再 `symTab_.lookupModule()->isArray`，所以 `GetTickCount()` 这类零参
调用不会误判）与 `wrapWholeArrayAssign()`；
⑤ `cgen_assign_value_sem.inc` 两处 emit 收口：整体数组目标右侧若非"必然新建载体"的
helper，一律 `vb6_ArrayAssign1D(dst, src)` 深拷贝；
⑥ `cgen_util_ctrl.cpp:wrapVariantValue` 在 `inferExprType` switch **之前**拦整体数组
→ `vb6_VariantArray((void*)arr)`；不拦的话 `A()` 可能被推断成**元素**类型而生成
`vb6_VariantLong(载体指针)`，C 侧只是警告、指针截断成 int32，运行期才炸；
⑦ RTL 新增 `vb6_ArrayAssign1D`（`vb6rtl_array.c`，紧挨 `vb6_SafeArrayDestroy1D`）：
深拷贝 + 先建副本再销毁 dst（`dst==src` 安全）；BSTR 元素逐槽 `SysAllocString`，
VARIANT 元素 **memset 清空后** `vb6_VariantCopy`（不能 `VariantClear`，那会把 src 仍
持有的对象释放掉）。
`freshCarrier` 白名单**不含** `vb6_VariantToByteArray(` / `vb6_VariantToSafeArray1D(`：
这俩在 Variant 持数组时 `return v.parray`（`vb6rtl_compat.c:187`、`vb6rtl.c:199`）是
**别名**而非新建，必须再拷一份。
验证：`tests/test_array.bas` 追加 4 行断言（`wa-clone=22` 深拷贝、`wa-ub=3` 整体右值、
`wa-str=65` String→Byte()、`wa-rt=65` Byte()→Variant→Byte()），并把该用例从"只跑不比对"
改成带 `Add-BasTest` 预期串。
**遗留（Fix 170b）**：`vb6_VariantArray()` 固定打 `VT_ARRAY|VT_VARIANT`，所以
`VarType(b) = vbArray + vbByte` 这类判定仍不成立（载体自己有 `elemType`，可据此推出
元素 VT 来装箱）；`Erase`/`ReDim` 走名字解析不经过本路径，未受影响。

### Fix 172（工具，不是 bug）：RTL 里的崩溃回溯钩子
`vb6rtl.c` 新增 `vb6_CrashTraceVEH`，`vb6_Init` 里按 `C3_COM_TRACE` / `C3_CRASH_TRACE`
安装：AV/非法指令/除零/栈溢出时把异常码、故障地址、**读写目标**和最多 24 帧栈按 **RVA**
打到 stderr。用法：`rva + 0x140000000` 喂 `.temp/sym3.ps1` 直接得到生成的 `.c` 行号
（x64 `/Od` 下 `CaptureStackBackTrace` 会混进栈扫描假帧 —— RVA 明显超出镜像大小的那些
丢掉即可）。这一次就是靠它把 `wcscmp` 的调用者定位到 `VBFlexGrid.c:47215`
（`GetTextDisplay`）的，之前只有 WER 的单个"故障偏移"。
配套：`.temp/gate.ps1 -Tag regressNN`（跑 `tests/run_tests.ps1 -Category all`，日志
`output/yqt_regressNN.log`，UTF-8）；`.temp/yqt_shot.ps1`（起 exe → 截主窗口 → 存 PNG，
用 Read 直接看渲染结果，是唯一能"看到"GUI 用例的手段）。

### Fix 173：未赋值的 String 是 **NULL BSTR**，比较函数不能直接喂 wcscmp
`vb6_StrCmp`（`vb6rtl.c`，`=`/`<>` 的字符串入口）原本 `wcscmp(a,b)` 裸调；UDT 里从未赋值的
`As String` 字段是 calloc 出来的 NULL → 读 0x0 → AV。踩到点：
`VBFlexGrid.ctl:20425` `If Not VBFlexGridColsInfo(iCol).Format = vbNullString Then`。
修成 `if (!a) a = L"";`（VB6 语义下 NULL BSTR ≡ `vbNullString` ≡ `""`，比较相等）。
同口径把 `vb6_StrComp`（`vb6rtl_string.c:401`）的单边 NULL 提前返回 ±1 也改成归一成
`L""` 再按长度比，否则 `StrComp("", vbNullString)` 报"不等"。

### demo 现状（Fix 170+173 之后，已达成）
`VBFlexGridDemo.exe` 启动 → 标题 `VBFlexGrid Demo` 的 940x553 窗口 → `[C3_FSM]
MessageLoop enter` 驻留，无 AV、无 unhandled Err。截图 `.temp/demo_shot.png`。
肉眼可见的**新缺陷**（下一批的输入）：
① 行头列每行都画 `149`（应为 1..N），左上角固定格也是 `149`；
② 第 1 行 A 列是 `46023`（= 2026-01-01 的日期序列号）而不是 `1.1`，B..I 列的
   `1.2 … 1.9` 正常 → `1.1` 被按日期解析了，怀疑是 `Format$`/单元格数据类型推断
   （`SortType = Generic` 那条下拉）把 `x.y` 认成日期；
③ 下方选项面板的控件标题被裁字（`CellPictureAlignme`、`oolTipTex`、`ort Des.`、
   `ow Property Page`）→ 设计期控件位置/宽度（Extender Left/Width，Fix 168 那批）
   或字体 DPI 换算不对。



## 14. 2026-09-21 深夜续：对照用户给的参考图后的定位（Fix 174/175/176）

参考图（VB6 真身）与我们的截图逐列比对后，缺陷① 的**范围收窄成"只有 col 0 与 col 1 塌了"**：
col≥2 的 `TextMatrix(i,j) = i & "." & j` 逐行全对；col 0 每行都是 `149`（= 最后一次
`TextMatrix(i,0) = i` 的值）、col 1 每行都是 `A`（= 之后 `TextMatrix(0,1) = Chr(64+1)` 的值）。
即这两列**读写都固定在第 0 行**。

已排除的：
- `MainForm.c` 三处调用点参数正确（`prop_let_TextMatrix(obj, i, j, vb6_CStrLong(i))` /
  `vb6_CStrDbl(StartDate + (i-1))` / `vb6_Chr(64+j)`），Long/Date→String 的隐式转换在
  **参数化属性 Let** 这条路上是有的；
- `prop_let_TextMatrix` → `SetCellText` 的存储式子
  `VB6_SA_AT(TCELL, VB6_SA_AT(TCOLS, Cells.Rows, iRow).Cols, iCol).Text` 正确，
  `VB6_SA_AT` 是干净的双参数宏，嵌套传参不会被预处理器切错；
- 每行的 `.Cols` 确实各自分配了（`VBFlexGridCellsInit` 里 `With Rows(i): ReDim .Cols(...)`
  的 With 绑定在循环体内，生成的 C 逐行 `ReDim1D_Udt`）；
- **嵌套 UDT 动态数组本身没问题** —— 新用例 `tests/test_nested_udt_array.bas` 用
  `Rows(i).Cols(j).Text` + `With Rows(i): ReDim .Cols(...)` + `LSet` 完整复刻了 VBFlexGrid
  的存储结构，并按 demo 的写入次序（先填 j、再填行头列、最后填列头行）跑，逐行读回全对
  （已注册进 bas 队列，预期串 `NA1=1;NB1=D0;NC1=1.2` / `HDR@ABC`）。
⇒ 剩下的唯一嫌疑是**绘制路径**（固定列/行表头那一段的画法），要重编 demo 拿
`GetTextDisplay` 调用方的循环再定（**Fix 174 未修**）。

### Fix 175（未修，写复现用例时撞上的）：Sub/Function 的 String 形参不做实参转换
`test_nested_udt_array.bas` 第一版按 demo 原样写 `SetCell i, j, StartDate + (i-1)` 与
`SetCell i, 0, i`（`ByVal v As String` 形参），生成的是
`vb6_SetCell(i, j, (StartDate + (i - 1)))` / `vb6_SetCell(i, 0, i)`：
Date → **error C2440**（响的），Long → **只有 warning C4047**，把整数当 BSTR 指针传进去
（静默错，和 Fix 166/169 同一类）。参数化属性 Let 那条路会插 `vb6_CStrLong/vb6_CStrDbl`，
普通 Sub/Function 调用点不会 → 补的口径就是"按形参类型包一层 CStr"。
（同一处把 `Date` 转成 `vb6_CStrDbl` 也是错的：它输出序列号 `46023`，而 VB6 的
`CStr(Date)` 是本地日期串 —— 参考图里 B 列正是 `2026/1/1`。这就是最初记的"缺陷②"。）

### Fix 176（已修）：`Debug.Print <用户 Function 返回 String>` 被当 int32 打印
`cgen_call.cpp` 的 Debug.Print 分派只靠 `bstrFuncs` **前缀表** + `knownBstrVars_`，
用户自定义 Function 的调用结果两个都捡不到 → 落到兜底
`vb6_DebugWriteLong((int32_t)(BSTR))` → 指针截断，打印出 `-150012728` 这种垃圾数
（**不崩，纯静默错**，会污染所有 `Debug.Print "x="; MyFunc()` 形式的用例输出）。
修法：判定里补一条按 AST 的返回类型 —— `inferExprType(*call.positional[j]) ==
Vb6Type::String`（`inferExprType` 的 `IndexOrCallExpr` 分支本来就会查
`symTab_.lookup(name)->type`，现成的）。
实测：探针 `.temp/probe_print.bas` 的 `B=` 从 `-150012728` 变成 `FN!`；
`test_nested_udt_array` 的 `HDR@ABC` 也从四个截断数变成正确的串。
同类未修的：返回 Date 的用户 Function 仍会落进 `DebugWriteLong` 兜底（该走日期串）。


---

## 15. 2026-09-22 凌晨：Fix 177 —— 单元格存的是**悬垂 BSTR**（推翻"存储已排除"）

**取证方式（可复用）**：把整个 demo 工程 `cp -r` 到 `.temp/proj_probe/`，在
`Form_Load` 填表之后插一段 `Open App.Path & "\probe.txt" For Output` + `Print #1`
回读探针，用 `.temp/yqt_probe.bat` 编副本（**不碰用户原工程**），跑完读 dump。
这比读生成 C 可靠得多 —— 一次就把问题从"绘制"翻案到"存储"。

**第 14 节的结论是错的**：`GetTextDisplay`/`DrawCell`/`DrawFixedCell` 的 `iRow`
传递**全部正确**（已逐行核对生成 C 与 `VBFlexGrid.ctl` 原文）。真正现象是
`TextMatrix` **读回**就已经错：

```
R0|149|A|B|C   R1|1|46023|1.2|1.3   R2..R12|149|A|B|C
W35=1115914680      <- 写 "ZZZ" 再读回, 拿到的是指针数值
BADROWS=148         <- 第 2..149 行无一正确
```

**根因**：`SetCellText` 里 `Cells.Rows(iRow).Cols(iCol).Text = TextIn` 发射成
**裸指针赋值** `...TCELL...).Text = (*TextIn);`。`TextIn` 是调用方
（`prop_let_TextMatrix`）的临时 BSTR，**过程返回即被释放** → 每个单元格里存的是
悬垂指针，读回的是"回收后被复用的内存"，于是整表塌到最后一个写入值。
`GetCellText` 那侧本来就是 `vb6_BSTR_Assign`（深拷贝），**读写不对称**。

**为什么 `test_nested_udt_array` 没抓到**：它写"变量里的字符串"并立刻读回，
临时量尚未被回收，别名恰好一致 —— 覆盖的是结构，不是 BSTR 所有权。

**修法（3 文件）**：
- `src/backend/cgen_util_classtype.cpp`：新增 `udtFieldIsBstrInCTarget(C 目标串)`，
  直接从**已生成的 C 串**解析 `VB6_SA_AT(vb6_type_TCELL, ...).Text` 的元素 UDT +
  成员名；`udtFieldObjCType` 对 `String` 字段新增返回 `"BSTR"`（另一调用方
  `appendUdtObjFieldMarker` 只认 `void*`/`vb6_cls_*`，不受影响）。
- `src/backend/detail/stmt/cgen_assign_value_sem.inc`：`targetIsBstr` 判定补这一路
  → 走既有 `vb6_BSTR_Assign(&target, value)`（释放旧值 + 深拷贝）。

**为什么走 C 串解析而不是 AST 链**：`inferUdtTypeOfExpr` 的 `IdentifierExpr` 分支只查
`knownUdtVars_`（仅登记参数/局部/返回/With 临时量），**类字段不在其中**；且
`Cols() As TCELL` 的 `mi.type` 带 Array 标志、`typeRefName` 为空，链在第二跳就断。
先试过改这两处，实测**均无效**，已回滚，只保留 C 串方案。

**踩到的两个字符串坑（各烧一轮构建）**：
1. `s.compare(0, 10, "vb6_type_")` 在 MSVC 的 STL 下对**明明相等的前缀**返回 1，
   改成 `rfind(prefix, 0) == 0` 才对（memory 里记过，这次又中）。
2. 解析嵌套 `VB6_SA_AT` 时，`rfind(macro, dot)` 的第二个实参是"允许的最大**起始**
   位置"而非搜索终点，取到的是**外层**宏；外层第一个逗号又落在内层实参②里
   （`VB6_SA_AT(T, VB6_SA_AT(U, a, i).Cols, j)`），于是元素类型误取成 `TCOLS`。
   最终：正向按 `cand + macro.size()` 枚举候选，取"闭合括号紧贴 `.`"的那个，
   深度初值 **1**（宏名自带的 `(` 已被 `macro.size()` 跳过）。

**修复后实测**：`MIN=A1,...`（写 A1 读回 A1）、`COL12` 第 1 行 = `C1` 正确。

**本轮收尾**：门禁 regress35 PASS=89 FAIL=0 SKIP=1 TOTAL=90；已提交 `477c90f`（只暂存 Fix 177 的 hunk + 本节§15～19；同文件里其他批次的 Fix 154/156/159/170 未提交改动保持原样）。

---

## 16. Fix 178（新发现，未修）：行 >=2 的存储仍别名到第 0 行

Fix 177 消除了悬垂指针，但**行别名**仍在，且它才是截图上"整表显示第 0 行"的直接原因：

```
写 col12 的第 1..10 行 = C1..C10, 整列读回:
  0:C10  1:C1  2:C10  3:C10 ... 12:C10
```

即 **行 1 独立且正确；行 0 与所有行 >=2 共用同一份存储**（该份存储里是最后写入的
`C10`）。`EraseFlexGridCells` 的 `Erase Rows()` 生成正确（`Destroy + NULL + Init=0`），
`InitFlexGridCells` 的 `ReDim ... 0 To PropRows-1` 也正确，`PropRows=150` 已实测确认。
⇒ 下一轮应从**运行期 `UBound(VBFlexGridCells.Rows)`** 切入（怀疑载体元素数远小于
150，行 >=2 越界踩到行 0/1 之后的内存），而不是再读生成 C。

**注意**：`Print #1, "x="; <返回 String 的用户 Function>` 仍会把字符串打成 int32
（Fix 176 只修了 `Debug.Print` 分派表）。探针里凡用 `;` 打印函数返回字符串的地方，
改成 `&` 拼接，否则读数会被误读成"还是指针"。

**下一轮探针设计（Fix 178 用，纯只读准备）**：`InitFlexGridCells` 的分配数学与
`With Rows(i)` 绑定经核对**均正确**（`count = uBound-lBound+1 = 150`，`_vb6_with_411`
在循环体内重绑），故"载体只分配了 2 个元素"的猜测不成立。当前证据
（行 1 独立正确、行 0 与行 >=2 共用一份存储）更像**读侧索引**问题。下一轮在
`proj_probe` 里做**读/写分离映射**：
1. 全新列（如 col 13）逐行写 `W0..W5`，**先整列读回**记录映射；
2. 再对该列**一次都不写**，直接读 `TextMatrix(i,14)` 全行 —— 若仍出现"多行同值"，
   即读侧别名坐实；
3. 同时读 `RowHeight(i)`（走另一个数组但同类型索引路径）作对照。

---

## 17. Fix 175 精确定位（只读分析，下一轮直接照此改）

RTL 侧**早就有** `vb6_CStrDate(double)`（`vb6rtl_conv.c:187`：打 `VT_DATE` 后走
`vb6_Format(v, NULL)`），且 `wrapToBSTR` 已按 `Vb6Type::Date` 分派到它
（`cgen_expr_binary_util.cpp:151`）。所以 `46023` **不是缺函数**，而是**类型推断层
根本不知道谁是 Date**：

- C 后端里 `As Date` 与 `As Double` 都 emit 成 `double`（`cgen_base_type.cpp:41`），
  注册表按 **C 类型串**分派（`cgen_decl.cpp:258-264`、`cgen_localdecl.cpp:214/218/425/427`），
  于是 `Dim StartDate As Date` 被登记进 `knownDoubleVars_`；
- `inferExprType(IdentifierExpr)` 只有 `knownDoubleVars_` 一路（`cgen_util_type.cpp:36`），
  没有 Date 一路；
- `BinaryExpr` 算术走 `TypeSystem::promote(lt, rt)`，`Date` 不在数值阶梯
  （`type_system.cpp:220-227` 只到 Decimal）里，故即使认得 Date 也会退化成 Double。

⇒ `StartDate + (i - 1)` 判成 Double → `vb6_CStrDbl` → 打印序列号。

**改法（4 处，一次可完成）**：
1. `cgen_state.inc` 加 `std::unordered_set<std::string> knownDateVars_;`
   （紧邻 `knownLongVars_` 第 111 行），并在各过程的 `clear()` 处一并清
   （见 `cgen_decl_func.cpp:36` 一片）。
2. 注册处：`cgen_localdecl.cpp:214/218/425/427` 与 `cgen_decl.cpp:262/264` ——
   **在 double 分支之前**先看 `resolveTypeRef(node.asType)` 是否 `Vb6Type::Date`，
   是则入 `knownDateVars_`（不能再靠 `cType=="double"` 区分）。
3. `cgen_util_type.cpp` `IdentifierExpr` 分支补
   `if (knownDateVars_.count(lower)) return Vb6Type::Date;`。
4. `cgen_util_type.cpp` `BinaryExpr` 算术分支：`+`/`-` 且一侧为 `Date` → 返回
   `Date`（VB6 语义：日期±数字仍是日期）；`*`/`/` 含 Date → `Double`
   （日期乘除无意义，按数值处理即可）。

**验证口径**：不用等 demo —— 写 `.bas` 用例
`Dim d As Date: d = DateSerial(2026,1,1): Print "x=" & (d + 1)`，
期望 `2026/1/2`；当前会打出 `46024`。这条同时覆盖 Fix 175 的
"Sub 的 ByVal String 形参收 Date 实参"（`cgen_expr_call_arg_emit.inc` 已有
`Vb6Type::Date` 分支，类型一修好就会走对）。

---

## 18. 问题 3（面板标题截断）定位：**不是几何，是字体**

逐项算过：`Command13`（Caption `ToolTipText`）设计器 `Width = 1215` 缇，
生成 C 原样传 `vb6_CreateControl(..., 2760, 240, 1215, 315, ...)`
（`MainForm.c:302`），而 `vb6_TwipToX = twips/15`（`vb6forms.c:52`，96 DPI 正确）
→ **81 px**。81 px 放下 11 个字符只可能是 **8 pt** 字体；但
`vb6_CreateControl` 给控件设的是 `GetStockObject(DEFAULT_GUI_FONT)`
（`vb6forms.c:234`），现代 Windows 上它是 **Segoe UI 9pt**，明显宽于 VB6 的
`MS Sans Serif 8.25pt` → 于是 `ToolTipText` 打成 `oolTipTex`、`Sort Desc` 打成
`ort Des.`。**几何与 DPI 数学都没错，错在字体选择**（截断在两侧控件上均匀出现，
也印证是字号而非坐标偏移）。

**改法**：`vb6forms.c` 控件默认字体改为按 VB6 口径创建
`MS Sans Serif` 8.25pt（`lfHeight = -11` @96DPI、`DEFAULT_CHARSET`），
拿不到再退回现有 `DEFAULT_GUI_FONT`；同一处 `CreateFontA` 兜底用的是
`"MS Shell Dlg"`，也应换成 `"MS Sans Serif"`。注意这是 **RTL 改动**，会重嵌入
`C3.exe`（`C3RTL_EMBEDDED_FILES` 已含该文件，正常重建即生效），且窗体/分组框
标题字体可能各自另有设置，需一并核对，否则会出现"按钮变小、标签没变"的不一致。

## 19. 问题 4（CellPicture 预览图空白）待查线索

参考图里 `Set` 按钮左侧有一个彩色图标（VB6 里是 `Image` 控件 + `.frx` 位图）。
本轮未定位。下一步先查 `MainForm.frm` 里该 Image 控件的 `Picture = ...frx:`
设计器行，以及 C3 是否解析 `.frx`（若整条路径缺失，属于"需要新子系统"级，
优先级应排在 177/178/175/字体之后）。

**已坐实（问题 4 根因）**：`FrxReader` 只在 `cgen_form_prelude.inc:188-192` 被用于
**Form.Icon** 一处；控制级 `Picture = "MainForm.frx":0000` 从未被应用到控件 ——
生成的 `MainForm.c` 里 `Picture1` 只有 `vb6_CreateControl("STATIC", "", ...)`
（第 309 行），**全文件搜不到任何 frx/LoadPicture 调用**。而 `Form_Load` 末尾
`CellPicture = vb6_GetControlPicture(vb6_hwnd_Picture1)`（第 667 行）因此取到空图。
⇒ 修法：在控件创建处识别 `FrxReference` 属性值并加载位图（STATIC 需 `SS_BITMAP`
+ `STM_SETIMAGE`，或走控件自有 image 槽），属**新增能力**而非一行修正，
建议排在 177/178/175/字体之后。

## 20. 2026-09-22 01:5x：Fix 178 **根因已钉死**（推翻 §16 的"读侧别名"猜测）

探针 `.temp/proj_probe/MainForm.frm`（`FIX178-PROBE` 块，全部用 `&` 拼接以避开
`Print ;` 的 int32 分派）实测：

```
COL13=0:W5 1:1.13 2:W5 3:W5 ... 12:W5        (往行 2..7 写 W0..W5, 整列读回)
COL14-NEVERWRITTEN=0:N 1:1.14 2:N ... 12:N   (一次都没写过的列)
W9THEN=NINE|NINE|NINE|NINE                   (写 (9,15) 后读 0/2/9/11)
RH4-THEN=285|285|765|285                     (RowHeight(4)=777 只影响行 4)
MIN=A1,A3,A3   BADROWS=148   AGAIN-COL0=149|1|149|149
```

三条结论：
1. **行 1 独立且始终正确；行 0 与所有行 ≥2 共用同一份 `.Cols`**（不是"读侧"，
   因为写一行会同时改到所有行 —— `W9THEN` 四格同值）。
2. `Rows(i)` 的**下标本身没问题**：`RowHeight(4)` 走同一个 `Rows(i)` 数组的
   `.RowInfo` 字段，改行 4 只有行 4 变 → 每行的 TCOLS 元素是各自独立的。
   ⇒ §16 怀疑的"读侧行下标别名"排除。
3. 塌的是每行的 **`Cols` 指针**。

### 根因（生成 C 直接可指）
```
VBFlexGrid.ctl:16392  LSet VBFlexGridDefaultCols = VBFlexGridCells.Rows(0)
   → VBFlexGrid.c:38122  memcpy(&me->VBFlexGridDefaultCols, &Rows(0), sizeof(TCOLS));  /* LSet UDT */
VBFlexGrid.ctl:4146/8008  LSet VBFlexGridCells.Rows(i) = VBFlexGridDefaultCols
   → VBFlexGrid.c:3375/3379/13666  VB6_SA_AT(vb6_type_TCOLS, Rows, i) = me->VBFlexGridDefaultCols;
```
即 `PropRows Let` 增长时 `ReDim Preserve Rows(0 To Value-1)` 后，用**模板**逐行
`LSet` 填新行；而模板里存的是**行 0 的 `Cols` 载体指针**。C 侧无论是 `memcpy` 还是
结构体赋值都是**浅拷贝** → 新增的行 2..149 全部别名到行 0。设计期 `Rows=2` 时
`InitFlexGridCells` 给行 0、行 1 各自 ReDim 过 `Cols`，所以**只有行 1 是独立的那一个**
（与实测完全吻合）。`_vb6_with_133->Cols = me->VBFlexGridDefaultCols.Cols`（14367 等）
是同一形态的显式数组成员赋值。

VB6 运行时对含动态数组/字符串成员的 UDT 赋值与 `LSet` 走类型描述符**深拷贝**，
所以真身不会出现这个别名。

### 修法（下一轮实现，设计已定；⚠ 本轮未动代码）
需要"**含所有权成员的 UDT 拷贝**"能力，两处调用点 + 一个 RTL 回调版克隆：

1. RTL（`vb6rtl_array.c/.h`）：
   `typedef void (*vb6_udt_elem_copy)(void* dst, const void* src);`
   `vb6_SafeArray1D* vb6_ArrayAssign1D_Cb(vb6_SafeArray1D* dst, vb6_SafeArray1D* src, vb6_udt_elem_copy cb);`
   —— Fix 170 的 `vb6_ArrayAssign1D` 加回调参数（`cb==NULL` 时保持现在的整块 memcpy；
   `cb!=NULL` 时目标槽先清零再逐元素 `cb`，因为 `TCELL` 里有 `Text As String`）。
2. 代码生成：按 `Symbol::udtMembers`（`cgen_decl.cpp:207` 的 typedef 同一份元数据）
   递归判定 `udtHasOwnedMembers(vb6_type_X)`（成员是 String / 动态数组 / 嵌套 UDT 且其
   自身含所有权成员），命中则**惰性生成**一个模块级静态函数并登记避免重复：
   ```c
   static void vb6_udtcpy_TCOLS(vb6_type_TCOLS* d, const vb6_type_TCOLS* s) {
       if (d == s) return;
       vb6_SafeArray1D* _old_Cols = d->Cols;      /* owned 成员: 先存旧值 */
       memcpy(d, s, sizeof(*d));                  /* 标量/内联嵌套按位 */
       d->Cols = vb6_ArrayAssign1D_Cb(_old_Cols, s->Cols,
                                      (vb6_udt_elem_copy)vb6_udtcpy_TCELL);
   }
   static void vb6_udtcpy_TCELL(vb6_type_TCELL* d, const vb6_type_TCELL* s) {
       if (d == s) return;
       BSTR _old_Text = d->Text;
       memcpy(d, s, sizeof(*d));
       vb6_BSTR_Free(_old_Text);
       d->Text = s->Text ? SysAllocString(s->Text) : NULL;
   }
   ```
   注意递归顺序：元素类型的拷贝函数要先有**前向声明**（`static void vb6_udtcpy_TCELL(void*, const void*);`
   形态的 cast 需要真实原型，建议统一生成 `(vb6_type_X*, const vb6_type_X*)` 原型 + 一个
   `void*` 版薄封装，避免直接 cast 触发 C4113）。
3. 调用点改两处即可（都用现成的类型信息，不必再走 Fix 177 那种"从 C 串反推"）：
   - `cgen_assign_stmt_special.inc:150` 的 `memcpy(...) /* LSet UDT */`；
   - `cgen_assign_value_sem.inc` 末端 `target = value`：当 target 是
     `VB6_SA_AT(vb6_type_X, ...)` 或已知 UDT 变量、且 value 推断为同一 `vb6_type_X` 时，
     发 `vb6_udtcpy_X(&target, &value);`。
   另外 `_vb6_with_133->Cols = ...Cols`（数组成员直接赋值）应并入 Fix 170 的
   `wrapWholeArrayAssign` 口径（那里已有 `vb6_ArrayAssign1D`，补 `cb` 版即可）。
4. 验证口径（不用等 demo）：小 `.bas` 用例 —— `Type T: A() As Long: S As String: End Type`，
   `ReDim x.A(3): x.S = "1": y = x: y.A(0) = 9: y.S = "2"` 后断言 `x.A(0)=1 / x.S="1"`
   （当前会打出 9 / "2"，即别名+共享 BSTR）。demo 侧复跑 `FIX178-PROBE` 期望
   `COL13=…2:W0 3:W1 …7:W5`、`W9THEN=NINE` 只出现在第 9 行、`MIN=A1,A2,A3`。

**风险**：这是 codegen 级新能力，触及 `cgen_assign_*` 与 LSet 主干，必须单独一轮 +
全量门禁（Charts2020/czUI/NewTab 里有大量 `LSet`/UDT 赋值）。本轮**未动任何 src 代码**，
只加了 `.temp/proj_probe` 的探针块（gitignore 区内）。

---

## 21. 2026-09-22 02:0x：本轮被并发防护跳过（只读分析）—— Fix 175 配方**补全**：`CStr(Date)` 还缺"无时间分量→只给日期"

**并发状态**：`tasklist` 无 C3/cl/link/ninja，但会话 14a01034 `runtimeState: running`
（01:58 仍在 streaming），且它已把 **Fix 178 根因钉死并写了 §20 修法**，正在动手。
⇒ 本轮不启动任何编译/测量/回归，也不改 `src/`（改 src 会污染它正在跑的构建归因）。
Fix 178 归它，本轮只做 Fix 175 的只读定位。

### 一、§17 的 4 步配方经复核**站点没漂移**（当前树行号）
- `cgen_base_type.cpp:41` 与 `:80`：`case Vb6Type::Date: → "double"`（Date/Double 在 C 层同型）。
- 登记处（按 **C 类型串** 为键，所以 Date 落进 double 集合）：
  `cgen_localdecl.cpp:214/218`、`:425/427`，模块级 `cgen_decl.cpp:262/264`。
- 推断处缺 Date 分支：`cgen_util_type.cpp:36` `if (knownDoubleVars_.count(lower)) return Vb6Type::Double;`
  （字面量 `#...#` 已在 `:53` 返回 Date，所以只有**变量**看不见）。
- `TypeSystem::promote` 在 `type_system.cpp:204`，数值阶梯里无 Date。
- 出口侧**已就绪**：`cgen_expr_binary_util.cpp:99` 与 `:151` 都有 `case Vb6Type::Date → vb6_CStrDate(...)`；
  实参侧 `cgen_assign_prop_write.inc:181`、内建函数 `cgen_expr_call_builtin_argtype.inc:93` 同。

### 二、新发现（§17 漏了，会让 Fix 175 改完仍不对参考图）
`vb6_CStrDate` (`vb6rtl_conv.c:187`) 只是 `vb6_Format(VT_DATE, NULL)`，而 `NULL` 分支
(`detail/vb6rtl_format_extract.inc:39-55`) **无条件拼 "短日期 + 空格 + 时间"**：

```c
GetDateFormatW(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &st, NULL, dateBuf, 64);
GetTimeFormatW(LOCALE_USER_DEFAULT, 0, &st, NULL, timeBuf, 64);   // ← 0:00:00 也照打
swprintf(fullBuf, 128, L"%s %s", dateBuf, timeBuf);
```

VB6 的 `CStr(Date)` 规则是：**时间分量为 0 → 只给短日期**；整数分量为 0（纯时间）→ 只给时间；
否则才"日期 时间"。demo 日期列写的是整数序列号（46023 = 2026/1/1），参考图 B 列逐行是
`2026/1/1 … 2026/1/23`（已直接目视确认参考图，无时间尾巴）。⇒ 只补类型可见性的话，
单元格会变成 `2026/1/1 0:00:00`，**仍与参考图不符**。

**最小改法（下一轮与 §17 四步同批或紧随其后）**：只动 `vb6_CStrDate`，不碰共享的
`Format(..., NULL)`（`Format` 有自己的语义与用例）。在 `vb6_CStrDate` 里先判
`frac = x - floor(x)`（容差 `1e-9`，序列号是精确整数所以安全）；`frac≈0` 时只返回
`GetDateFormatW(LOCALE_USER_DEFAULT, DATE_SHORTDATE, ...)`，否则原样委托 `vb6_Format`。
已核对：**没有任何用例钉住 `CStr(Date)` 的输出**（`tests/*.bas` 里 `CStr(` 只出现在
`test_err`/`test_variant`/`diff_smoke`，均非 Date；`test_date.bas` 故意用 `Dim d As Double`），
所以这条改动不会翻既有 golden。

### 三、与 Fix 178 批次的文件冲突预检（避免互相吞改动）
- 它（§20）预计改：`vb6rtl_array.c/.h`、`cgen_assign_stmt_special.inc:150`（LSet memcpy）、
  可能新增 codegen 辅助（声明大概率落在 `cgen_helpers.inc`）。
- Fix 175 预计改：`cgen_state.inc`、`cgen_decl.cpp`、`cgen_localdecl.cpp`、`cgen_util_type.cpp`、
  `type_system.cpp`、`vb6rtl_conv.c`。
- **唯一重叠面 = `cgen_helpers.inc`**（两边都可能加声明）。提交时一律按 hunk 暂存
  （`git apply --cached --recount`，见 §20 之后的实践记录），别整文件 add。

### 四、参考图复核（本轮直接目视）
A 列行头 = 1..23 本行行号；B 列 = `2026/1/1`…`2026/1/23`；C..M 数据格 = `行.列`（C 列是 `.3`，
即 B 被日期占用）；下方面板里 **"Partial Scr…" 在参考图里本身就是截断的** ⇒ 问题 3 的验收标准是
"与参考图一致"，不是"零截断"，别为了消除截断去改控件尺寸（几何本来就没问题）。


---

## 22. 2026-09-22 02:4x：Fix 178 **已实现并验证**（§20 设计落地 + 第 3 个别名点）

实现（与 §20 一致，另加 §20 未预见的第三个别名点）：

1. **RTL**（`vb6rtl_array.h/.c`）：新增 `typedef void (*vb6_udt_elem_copy)(void*, const void*)`
   与 `vb6_ArrayAssign1D_Cb(dst, src, cb)`；Fix 170 的 `vb6_ArrayAssign1D` 收为
   `_Cb(..., NULL)` 薄封装。`cb` 非空时对 `vb6_sa_udt` 载体逐元素回调深拷贝。
   **`dst == src` 保护**：两个名字已共用同一载体时改为"克隆且不销毁 dst"（销毁会让
   src 侧悬垂），顺带把旧别名**打破**。
2. **代码生成**（`cgen_util_classtype.cpp` 末段）：`udtHasOwnedMembers`（按符号表
   `udtMembers` 递归；String / Variant / 动态数组 / 含所有权成员的嵌套 UDT 记为有所有权，
   **对象成员维持按位**，以免给全局引入 AddRef/Release 失衡）、`requestUdtCopy`、
   `emitUdtCopyBlock`（依赖闭包 → 前向声明 → 定义）。**逐成员赋值，不 memcpy**：
   先 memcpy 再修补的写法会让嵌套成员的"旧值"其实是 src 的指针，递归释放时把源毁掉。
   生成的两个典型成员语句：
   ```c
   { BSTR _o = d->Text; d->Text = s->Text ? SysAllocString(s->Text) : NULL;
     if (_o && _o != s->Text) vb6_BSTR_Free(_o); }
   { vb6_SafeArray1D* _o = d->Cols;
     d->Cols = vb6_ArrayAssign1D_Cb(NULL, s->Cols, vb6_udtcpy_TCELL_v);
     if (_o && _o != s->Cols) vb6_SafeArrayDestroy1D(_o); }
   ```
   `_o != s->X` 的判据是"两侧本来就共享同一对象时不能释放"（BSTR 非引用计数）。
3. **落地时机**：请求点在 body pass，而 C 要求先定义后使用 → 记进
   `mutable std::map<std::string,bool> udtCopyRequested_`（值=是否需要 `void*` 薄封装），
   在 `cgen_base_generate_epilogue.inc` 把整块 **static** 函数插到 .c 自身
   `#include "<Base>.h"` 之后（static：每 .c 一份，跨模块同名不冲突）。
4. **三个调用点**：
   - `cgen_assign_stmt_special.inc`：LSet 标识符目标（原 `memcpy`）；
   - `cgen_assign_value_sem.inc` 末端 `else`：`Rows(i) = VBFlexGridDefaultCols`（demo 主因）；
   - **`isWholeArrayRef` 原先只认裸数组变量** → `VBFlexGrid.ctl:8409`
     `.Cols() = VBFlexGridDefaultCols.Cols()` 退化成载体指针直赋。补
     `isDynamicArrayMemberCallee`，并把元素 UDT 透传给 `wrapWholeArrayAssign`
     → `vb6_ArrayAssign1D_Cb(target, rhs, vb6_udtcpy_TCELL_v)`。
   ⚠ 坑：`inferUdtTypeOfExpr` 对动态数组成员返回的是**元素** UDT 类型，所以
   `udtDeepCopyAssign` 必须先排除 `isWholeArrayRef` 两端，否则会把载体指针当结构体拷。

**验证**
- 新用例 `tests/test_udt_assign.bas`（第 91 例）：`y = x`、`LSet z = x`、
  `arr(1) = arr(0)`、嵌套 `h2 = h1`（`THost{Items() As TOwned}`）四种形态均
  "改副本不影响原件"。标记 `A1=1;S1=hello;N1=42` / `A2=99;S2=world;N2=7` /
  `H1=11;HS1=alpha` / `E1=5;ES1=five` / `L1=2;LS1=hello` / `UDT-ASSIGN-DONE`。
- demo 探针（同一 `FIX178-PROBE` 块，与 §20 逐条对照，括号为 §20 的旧值）：
  ```
  COL13=0:M 1:1.13 2:W0 3:W1 4:W2 5:W3 6:W4 7:W5 8:8.13 …   (旧: 2..12 全 W5)
  COL14-NEVERWRITTEN=0:N 1:1.14 2:2.14 3:3.14 …              (旧: 全 N)
  W9THEN=O|2.15|NINE|11.15                                    (旧: 四格同 NINE)
  MIN=A1,A2,A3   CHAIN=A2x   SAMECOL=B5,B6   CELLPROP=A2      (旧: A1,A3,A3)
  COL12=0:L 1:C1 … 10:C10 11:11.12                            (旧: 0:C10 1:C1 2:C10…)
  BADROWS=1                                                    (旧: 148；1=第 149 行本身)
  ```
- 生成 C：`Fix 178` 标注 74 处；`Rows, i) = me->VBFlexGridDefaultCols;` 与
  `_with_N->Cols = me->VBFlexGridDefaultCols.Cols;` 两种裸别名赋值**归零**。

**新发现（未修，记作 Fix 180）**：`ReDim <UDT 动态数组成员>` 不带 `As` 时生成
`h1.Items = vb6_SafeArrayReDim1D(vb6_sa_variant, 0, 1)` —— 元素类型按 Variant 定，
而读写侧用 `VB6_SA_AT(vb6_type_TOwned, …)`，**步长不符** → 越界踩内存（现象：整棵
子树别名+字符串为空）。带 `As TOwned` 才走 `vb6_SafeArrayReDim1D_Udt(sizeof(...))`。
VBFlexGrid.ctl 全部带 `As`，故 demo 不受影响；测试用例已按带 `As` 的写法固化。
入口：ReDim 生成处选 `_Udt` 版的条件（`arrayUdtElemTypes_` 只登记裸数组变量，
UDT 成员数组没进去）。

---

## 23. 2026-09-22 03:0x：本轮再次被并发防护跳过（只读）—— 问题 3（字体）定位到**可照抄的改法** + 一个 GDI 生命周期陷阱

**并发状态**：`tasklist` 有 6 个存活构建进程（`C3.exe` 11772 + 5×`cl.exe`），
`.build/C3.exe` 02:57:10 刚重建，`output/yqt_regress37.log` 03:00:51 正在写（另一会话的
门禁，目前全 PASS）。⇒ 本轮不编译、不测量、不起 regress38、不改 `src/`。
**Fix 178 已由另一会话实现并验证（§22）**，本会话不再碰；`BADROWS 148→1`、`COL13/COL14` 逐格归位。

### 一、§21 的 Fix 175 站点复核：行号**一字未漂**（Fix 178 没碰这些文件）
`cgen_util_type.cpp:36` / `cgen_localdecl.cpp:214,218` / `cgen_decl.cpp:262,264` /
`type_system.cpp:204` 逐行 `sed -n` 复验一致。下一轮可直接照 §17+§21 动手。

### 二、问题 3（面板标题截断）根因**再确认**并给出改法
`src/rtl/core/vb6forms/vb6forms.c:232-242`（`vb6_CreateControl` 末尾，所有控件类的唯一漏斗）：

```c
// 设置默认字体 (VB6使用MS Sans Serif 8.25pt)      ← 注释写对了
HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);   // ← 代码给的是 Segoe UI 9pt
if (!hFont) { ... CreateFontA(-11, ..., "MS Shell Dlg") }  // ← 兜底永不执行
```

即：**代码与自己的注释不符**。Segoe UI 9pt 比 MS Sans Serif 8.25pt 既高又宽，
按钮/标签文字装不进原几何（几何本身是对的：`Command13` 宽 1215 twips → 81px）。

**改法（待定编号，建议 Fix 179）**：把那段换成"每控件建一份"的 8.25pt MS Sans Serif：

```c
HDC  hDC   = GetDC(hwnd);
int  dpiY  = GetDeviceCaps(hDC, LOGPIXELSY);
ReleaseDC(hwnd, hDC);
LOGFONTA lf; memset(&lf, 0, sizeof lf);
lf.lfHeight = -MulDiv(825, dpiY, 7200);      /* 8.25pt @96dpi = -11 */
lf.lfWeight = FW_NORMAL;
lf.lfCharSet = DEFAULT_CHARSET;
lf.lfQuality = NONANTIALIASED_QUALITY;       /* VB6 位图字体观感，别开 ClearType */
strcpy(lf.lfFaceName, "MS Sans Serif");
HFONT hFont = CreateFontIndirectA(&lf);
if (!hFont) hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
```

**⚠ 必须"每控件一份"，不要做模块级缓存**（这是本轮只读分析的主要产出）：
`vb6forms_ctrl.c:169-184` 的 `vb6_SetControlFontFromLogFont` 在替换字体后会
`DeleteObject(hOldFont)`，其注释说"stock 字体 DeleteObject 是 no-op 所以安全" ——
对今天的 `DEFAULT_GUI_FONT` 成立，但**对自建 HFONT 不成立**。若缓存一份共享，
第一个改 `Font` 的控件就会把别的控件在用的字体销毁掉（GDI 句号复用 → 别处
随机换字形/句柄失效），且这种破坏是**跨控件、非确定性**的，极难归因。
每控件一份则与既有 delete 语义天然自洽（控件数几十个，GDI 开销可忽略）。

**配套核对点（改时顺手看一眼，不必动）**：
- `vb6forms_ctrlarr.c:104` 控件数组从原型控件 `WM_GETFONT` 复制字体 → 与新默认兼容；
- `vb6forms_ctrl.c:202/259/281/303/325` 的 `lfHeight = -13`（"Default ~10pt"）是
  **Font 属性 getter** 的兜底，与默认渲染无关，但说明"读回 Size"路径也硬编码，
  若 demo 里有 `Font.Size` 读取会另行偏大；
- `uc_host.c:334`（UserControl 内嵌 edit）同样用 `DEFAULT_GUI_FONT`，属同类问题，
  本 demo 不经过，留作后续；
- 显式设过 `Font` 的控件走 `vb6forms_ctrl.c` 的 setter，不受默认值影响。

**验收标准（重要，别做过头）**：参考图里 **"Partial Scr…" 本身就是截断的**，
`Printscreen to Clipboard` 是**两行**显示。所以目标是"与参考图一致"，
不是"零截断" —— 不要为此去改控件宽高（§18 已证几何/DPI 全对）。

### 三、下一轮次序建议（避免与另一会话撞车）
1. 先只看 `output/yqt_regress37.log` 的 `Results:` 行确认 Fix 178 批次的门禁结论
   （若它已提交，本会话直接在其之上做，不要重复提交同批 hunk）；
2. 树空闲后：**先重编 demo + 截图**，看 §22 之后与参考图还剩几项差异（很可能行头/数据格已好，
   只剩日期列与字体）；
3. 一次只改一件事：优先 **Fix 175（Date 可见性 + §21 的 `CStr(Date)` 只给日期）**，
   字体（本节改法）紧随其后 —— 字体是 RTL 单点、验证成本低，适合当"稳一手"的间隔项。


---

## 24. 2026-09-22 03:4x：Fix 175 **已落地**（Date 可见性 + CStr(Date) 口径 + String 形参转换）

§17 的 4 步配方 + §21 补的 CStrDate 一条，实现时又冒出**第三半**（§17 只把它当"顺带"）：

### A. Date 在类型推断层可见
C 后端 `As Date` 与 `As Double` 同为 `double`，注册表按 **C 类型串**分派 → Date 被
`knownDoubleVars_` 吞掉。新增 `knownDateVars_`，与 double **并存**登记（照 Fix 117c 的
Single 双登记口径：既有 double 消费者一个都不能少），消费点一律**先判 Date**：
- 登记：`cgen_localdecl.cpp`（Dim）、`cgen_decl_var.cpp`（模块级变量）、
  `cgen_decl_func/proc/prop.cpp` 三处形参链（把 `if/else if` 的单语句体改成 `{}` 体，
  内加 `if (paramType == Vb6Type::Date) knownDateVars_.insert(pLower)`）；
  判据用 `resolveArrayElemType(asType.get()) == Vb6Type::Date`（codegen 侧没有
  `resolveTypeRef`，那是 SemanticAnalyzer 的成员）。
- 清理：三处过程入口 `knownSingleVars_.clear()` 之后。
- 推断：`cgen_util_type.cpp` `IdentifierExpr` 分支在 Single 之后、Double 之前插入 Date。
- 算术：`BinaryExpr` 算术分支显式处理 —— `Date ± 数值 → Date`、`Date - Date → Double`、
  其余含 Date → Double。必须单独写，因为 `TypeSystem::isNumeric(Date) == false`
  （`type_system.cpp:148`），交给 `promote` 会掉出数值阶梯；`rank()` 里的
  `Date: 5` 那一行因此从来没生效过。
- 出口：`vb6_CStr` 家族适配（`cgen_expr_call_conv_cstr.inc`）标识符与非标识符两条链
  各加 Date → `vb6_CStrDate`；`Debug.Print` 分派（`cgen_call.cpp`）在 `isDoubleExpr`
  **之前**加 Date 分支（否则 Date 变量被 `knownDoubleVars_` 抢先命中打成序列号）。

### B. `vb6_CStrDate` 不再带时间尾巴（§21 那条）
`vb6rtl_conv.c:187`：`frac = x - floor(x)`（容差 1e-9）时直接
`GetDateFormatW(LOCALE_USER_DEFAULT, DATE_SHORTDATE, ...)`，否则原样委托 `vb6_Format`。
**没碰共享的 `Format(v, NULL)`** —— 它自己有无条件拼"日期+时间"的语义与用例。

### C. 新发现：用户过程的 **ByVal String 形参**不做实参转换
`ShowStr(d)`（`Private Function ShowStr(ByVal s As String)`，实参 Date）生成
`vb6_ShowStr(d)` → **C2440 double→BSTR**；`ShowStr(n)`（Long）更阴：MSVC 只报
C4047 警告，运行期把整数当指针解引用。补在 `cgen_expr_call_arg_emit.inc` 的
`argVal` 落地处（ANSI-Declare 块之后、ByRef 块之前）：形参是 `ByVal String`
且实参非 Variant、AST 类型属于数值/日期族 → `wrapToBSTR(argVal, *node.positional[i])`
（`wrapToBSTR` 的 Date/Long/Double 分支在 `cgen_expr_binary_util.cpp:143-151` 早就齐了）。
⇒ 这一条不是 Fix 175 的附属品：它是"静默把整数当字符串指针"这一**整类**缺陷的收口点。

### 验证
- 新用例 `tests/test_date_display.bas`（门禁第 92 例）10 条断言全 Y：
  `D1-noserial / D2-year / D2b-notime / D3-nextday / D4-nextyear / D5-diff0 /
  D6-param / D7-longparam / D8-cstr / D9-format`。短日期串形态随系统 locale 变，
  所以断言只问"有没有裸序列号 / 有没有 `:` / 有没有 2026"，不钉死分隔符。
- demo 截图 `.temp/demo_after175.png`：B 列 `2026/1/1 … 2026/1/13`，与参考图一致
  （无 `0:00:00` 尾巴）；A 列行头、C..N 的 `行.列` 与参考图一致（Fix 178 的效果保留）。
- demo 重新编译 0 error 0 LNK。

### 已知缺口（本轮**故意**不做，避免把 12 处登记一次铺开）
1. **类模块的 Date 字段**：`classDoubleMembers_` 由 `cgen_base_generate_state_scan.inc:44`
   填充，要支持 `Private d As Date` 需平行加一个 `classDateMembers_` + 三处
   `knownDateVars_.insert(...)` 恢复。demo 无此写法。
2. 局部 `Const X As Date`（`cgen_localdecl.cpp:425` 一带的 cType 链）未登记。
3. **纯时间** Date（`x < 1`，如 `CStr(0.5)`）仍走 `Format` → "1899/12/30 12:00:00"；
   VB6 应只给 "12:00:00"。B 项只处理了"时间分量为 0"这一侧。


---

## 25. 2026-09-22 04:1x：Fix 181 控件默认字体（面板标题截断）**已落地**；顺带纠正 §18 的一处误判

### 改了什么
`vb6forms.c`：新增进程内缓存的 `vb6_Vb6DefaultGuiFont()` ——
`LOGFONTA{ lfHeight = -MulDiv(825, dpiY, 7200), "MS Sans Serif", DEFAULT_CHARSET }`
（8.25pt 按实际 DPI 换算，不是硬编码 -11），拿不到再退回 `DEFAULT_GUI_FONT`；
`vb6_CreateControl` 的 `GetStockObject(DEFAULT_GUI_FONT)` 换成它，同处
`CreateFontA` 兜底的字体名 `"MS Shell Dlg"` → `"MS Sans Serif"`。
RTL 改动 ⇒ 必须重建 `C3.exe`（`C3RTL_EMBEDDED_FILES` 已含该文件）。
`LOGFONTA lf = {0}` 而非 `memset`：`vb6forms.c` 没有 include `<string.h>`。

### 效果（`.temp/demo_after181.png`）
`oolTipTex→ToolTipText`、`ort Des.→Sort Desc`、`sureVisib→EnsureVisible`、
`learConten→ClearContent`、`ow Property Page→Show Property Pages`、
`tscreen to Clipb→Printscreen to Clipboard`、`en UserEditing De→Open UserEditing Demo`。
与 §18 的判断一致：**几何与 DPI 数学本来没错**（`Command13` 设计期 1215 缇 = 81px，
`vb6_TwipToX = twips/15` 在 96dpi 正确），错的是字体宽度。
按 §21 的验收口径：参考图里 `"Partial Scr…"` 本身就截断，所以目标是"与参考图一致"，
**没有**动任何控件尺寸。

### 纠正一处旧结论（重要，会误导后续轮次）
本会话早前记的"**窗体零子窗口，所有控件自绘**"是**错的**。实测
`.temp/yqt_children.ps1` 枚举主窗口的子 HWND = **35 个**（`Button`/`Static`/`ComboBox`/
`VB6_UserControlHost`/`VBFlexGridWndClass`…）。当时大概是查错了句柄。
⇒ 控件是真窗口，`WM_SETFONT` 有效（这也是本条改动能生效的原因）；
交互仍应走 `PostMessage`，但**可以**按子窗口自己的 rect 定位，不必全靠自绘坐标推算。

### 新发现：第 5 项视觉差异（用户提示过"可能还有"）
demo 网格只画到 **第 13 行**，参考图到 23 行。已核对：
- 设计期 `MainForm.frm:286` `VBFlexGrid1 = 120,120, 13575 x 5655` 缇 = **905 x 377 px**；
- 生成 C 的参数正确：`.temp/gen/MainForm.c:244`
  `vb6_UC_HostCreate("VBFlexGrid", 120, 120, 13575, 5655, ...)`；
- RTL 也按缇换算：`uc_host_create.inc:57` `vb6_TwipToX(width), vb6_TwipToY(height)`；
- 但运行时量到 `VB6_UserControlHost` / `VBFlexGridWndClass` = **583 x 233**，
  而截图上网格右缘约 737px、下缘约 287px —— **两个数不自洽**，说明量法或时机有问题
  （宿主与内层窗口同 rect，可能是 UC 内部又按 `_ExtentX/_ExtentY` 重算过一次尺寸）。
- 窗体本身尺寸是对的（客户区 922x506 = 设计 13830x7590 缇）。
⇒ 登记为任务 #28。下一步先在同一进程里同时打印宿主 `GetWindowRect`、宿主 `GetClientRect`
与 UC 自绘用的 `r->scaleWidth/scaleHeight`，再判断是宿主尺寸错还是行数/行高错。

### 门禁
`regress39` 覆盖 Fix 181（Fix 175 已由 `regress38` = PASS=91 FAIL=0 SKIP=1 TOTAL=92 覆盖）。

---

## 26. 2026-09-22 04:1x：本轮又被并发防护跳过（只读）—— 问题 4（CellPicture 预览空白）**根因改判**：容器子控件走的是第二条精简发射路径

**并发状态**：`tasklist` 无存活构建进程，但 `.build/C3.exe` 03:58:58 刚重建、`output/yqt_full1.log`
03:59:59 刚写、会话 14a01034 `runtimeState: running`（正在 "rebuilding to pick up the font fix"）。
⇒ 本轮不编译、不测量、不起门禁、不改 `src/`。
**编号说明**：台账里现在有**两个 §23**（本会话的字体分析在 1549 行，另一会话的 Fix 175 落地记录在
1620 行）。本会话后续只追加、不重排；引用时用"§23(字体)"/"§23(Fix175)"区分。
另一会话进度：Fix 178 → regress36/37 `PASS=90 FAIL=0 SKIP=1 TOTAL=91`；Fix 175 → regress38
`PASS=91 FAIL=0 SKIP=1 TOTAL=92`（新增 `test_date_display`），且 B 列已实测 `2026/1/1…2026/1/13`。

### 一、§19 的旧结论**作废**（本轮读码推翻）
§19 说"`FrxReader` 只接在 Form.Icon 上，控件级 `Picture` 从未应用" —— **不对**。
控件级 frx 早就有完整实现：`cgen_form_ctrl_style_apply.inc:252-296`（P24）覆盖
CommandButton / OptionButton / CheckBox / **PictureBox** / Image，命中后
`FrxReader::readPicture(offset,false)` → `bytesToHexArray` 进 .h →
`vb6_LoadPictureFromMemory` + `vb6_SetControlPicture`（图形按钮走 `BM_SETIMAGE`）。
RTL 侧也齐：`vb6forms_picture_prop.c` 会置 `SS_BITMAP|SS_CENTERIMAGE` 并 `STM_SETIMAGE`。

### 二、真实根因（纯静态即可钉死，不需要构建）
`Picture1` 在 .frm 里是 **Frame1 的子控件**，生成结果 `MainForm.c:309` 的调用形态是
`vb6_CreateControl("STATIC", "", <style>L, 0L, 120,240,495,375, 230, vb6_hwnd_Frame1, hInstance)`
—— 10 参、第 4 参 `0L`、控件 ID=230（≥200）。这正是
`cgen_form_frame_menu.inc:12-60` 的 `emitChildControls` lambda 的产物（容器子控件 ID 从 200 起）。

而 **P24 的 frx 块不在那个 lambda 里**：容器子控件路径只做了
`vb6_CreateControl` + `emitDesignerStyleProps`（BorderStyle/Alignment）+ 控件名注册，
主控件路径（`create_controls.inc` → `style_apply.inc`）独有的东西**全部缺失**：

| 主路径有、容器子控件路径没有 | 位置 |
|---|---|
| P24 `.frx` Picture / TextBox.Text / ListBox.List+ItemData | `style_apply.inc:252-320` |
| P20-12 `CausesValidation` | `:187` |
| P20-35 Shape/Line 属性初始化 | `:214` |
| P20-37 三个文件列表控件初始填充 | `:241` |
| Fix 145/147 ComboBox 下拉高度 | `create_controls.inc` 内 |

⇒ **问题 4 的修复点不是"给 PictureBox 加 frx 支持"，而是"容器子控件不该有第二条路径"。**
受影响面也远不止图片：任何放在 Frame/PictureBox 里的控件都拿不到这些设计期属性。

### 三、下一轮的最小改法（按代价从小到大，二选一）
1. **保守**：把 `style_apply.inc:252-320` 的 P24 块抽成一个成员函数
   `emitControlFrxProps(const FrmControl& ctrl, const std::string& hwndVar)`，
   主路径与 `emitChildControls` 各调一次（`emitChildControls` 已有 `child` 与
   `"vb6_hwnd_"+cIdent(child.controlName)`，直接传即可）。改动局限在 2 个 .inc + 1 个声明，
   风险最低，正好解掉参考图的 CellPicture 项。
2. **彻底**：让 `emitChildControls` 复用主循环的"创建后应用"整段（含上表全部），
   消除双路径漂移。代价是要核对主循环里对 `ctrlId`/`hwnd` 变量名的依赖，回归面大，
   建议单独一轮做，且必须先有覆盖"Frame 内 ComboBox/图形按钮"的用例。

### 四、顺手记的两个事实
- **`FrxReader::load` 失败是静默的**：`lastError_` 在 `src/` 里除 frx_reader 自身外**无任何消费者**
  （已 grep 确认），所以"frx 没生效"这类问题将来仍会零线索。改法①落地时建议顺带在
  `load` 失败处发一条 warning（不影响门禁输出，因为门禁比对的是运行输出）。
- **MainForm.frx 的两个条目已手工解码验证**（864B）：`0x0000` = `[4B total=832][4B 'lt'][4B img=824]["BM"...]`
  → `readPicture(0,false)` 的 `headerSize=12 / imgSize=base+8` 口径**完全正确**，能拿到 824 字节 BMP；
  `0x0344`（VBFlexGrid 的 `WallPaper`）是另一种布局 `[4B size][16B GUID(StdPicture)][…]`，
  用 `headerSize=12` 会读出垃圾 `imgSize` → 越界保护会返回空（不会崩，但也不会显示）。
  参考图里网格无壁纸，故 WallPaper 这条**不影响验收**，但 `FormatString = "UserEditingForm.frx":0000`
  之类"字符串存 frx"的形态目前只有 `readDocString` 路径支持，遇到 GUID 头仍需另案。

---

## 27. 2026-09-22 04:2x：⚠ Fix 181 落地版有一个**悬垂 HFONT**隐患（只读复核，未动代码）

另一会话的 §25(Fix181) 把默认字体做成了**进程内缓存**：

```c
/* vb6forms.c:53-75 */  static HFONT vb6_Vb6DefaultGuiFont(void) {
    static HFONT s_hFont = NULL; static int s_tried = 0; ... s_hFont = CreateFontIndirectA(&lf); ... }
/* vb6forms.c:265 */    HFONT hFont = vb6_Vb6DefaultGuiFont();  →  WM_SETFONT 给**每一个**控件
```

而换字体的路径仍然无条件删旧字体（**本轮复核确认它没被同步改**）：

```c
/* vb6forms_ctrl.c:171-185 */  vb6_SetControlFontFromLogFont():
    HFONT hOldFont = SendMessageW(hwnd, WM_GETFONT, ...);
    ... WM_SETFONT(new) ...
    if (hOldFont && GetObjectType(hOldFont) == OBJ_FONT) DeleteObject(hOldFont);
```

那段注释的前提是"旧字体是 stock 对象，`DeleteObject` 是 no-op"。Fix 181 之后旧字体是
**我们自建的共享对象** → 前提失效：任何一个控件第一次在运行期写 `Font.Name/Size/Bold/
Italic/Underline/Strikethrough`（`vb6forms_ctrl.c:214/245/267/289/311/333` 六个 setter 全部
走这条），共享默认字体就被销毁，而**其余所有控件仍把它选在 DC 上** → GDI 句柄可被复用，
表现为别处随机换字形/绘制异常。这类破坏是跨控件、非确定性、且与"哪个控件先改字体"
有关，事后极难归因。（§23(字体) 里"必须每控件一份"的理由就是这一条。）

**demo 现状**：`MainForm.frm` 只有 `PropCellFont.Name/.Size`（CommonDialog 的 Font 对象，
行 373-374）与一处 Font 比较（行 545），**没有** `Label1.Font.X = ...` 这类控件字体写入
⇒ 本 demo 大概率不会触发，所以这**不是**回归阻塞项；但它是整个工程集（以及任何后续
用 VB6 字体对话框给控件设字体的代码）都会踩的雷。

**两种收口，任选其一即可（都很小）**：
1. 尊重现有删除语义：默认字体**每控件建一份**（去掉 `static`，`vb6_CreateControl` 内
   `CreateFontIndirectA`），控件数几十个，GDI 开销可忽略；
2. 保留缓存但加判据：`vb6forms_ctrl.c:180` 改成
   `if (hOldFont && hOldFont != vb6_Vb6DefaultGuiFont() && GetObjectType(...) == OBJ_FONT)`
   —— 需要把该 helper 从 `static` 暴露给 `vb6forms_ctrl.c`（或加一个
   `int vb6_IsVb6DefaultGuiFont(HFONT)`）。顺带 `NONANTIALIASED_QUALITY` 若想要 VB6 位图观感
   也在这两处一起定（当前用的是 `DEFAULT_QUALITY`，实测观感他们已验收，不强求）。

另：`uc_host.c:334`（UserControl 内嵌 edit）仍用 `DEFAULT_GUI_FONT`，与本条无关，留档。


---

## 28. 2026-09-22 04:4x：§27 的悬垂 HFONT **已按方案 1 收口**（Fix 181 更正）+ 台账编号重排

### 改了什么（`vb6forms.c:48-77`）
`vb6_Vb6DefaultGuiFont()` 去掉 `static HFONT` 进程内缓存，改为**每次调用新建一份**
（`vb6_CreateControl` 每控件一份），并采纳 §23 的 `NONANTIALIASED_QUALITY`
（VB6 的 MS Sans Serif 是点阵字体，从不抗锯齿）。失败仍退回 `GetStockObject(DEFAULT_GUI_FONT)`
—— stock 对象被 `DeleteObject` 是 no-op，与 `vb6forms_ctrl.c:171-185` 的删除语义自洽。

### 为什么必须每控件一份（写死在这里，免得后人再"优化"成缓存）
`vb6_SetControlFontFromLogFont` 换字体后**无条件** `DeleteObject(hOldFont)`，只用
`GetObjectType()==OBJ_FONT` 判"可删"。共享自建字体也是 `OBJ_FONT`，所以任何一个控件
第一次在运行期写 `Font.Name/Size/Bold/...`（那六个 setter 全走这条路）就会把**别的控件
正在用的**字体销毁；GDI 句号还会被复用 → 表现为跨控件随机换字形，事后无法归因。
代价：几十个 GDI 对象，可忽略。

### 验收
`.temp/demo_after181b.png` 与缓存版 `.temp/demo_after181.png` 目视一致（面板标题全部
完整、B 列 `2026/1/1…2026/1/13`）⇒ 语义修正没有改变观感。
门禁：`regress40`（本会话第 4 次；基线 regress35 PASS=89 → 现在 92 例）。

### 台账编号重排（本条之前有重复号）
按文件出现顺序统一为 20…27，并把 §27 内文对"另一会话 §24(Fix181)"的引用改指 §25：
| 号 | 内容 | 作者侧 |
|---|---|---|
| §21 | Fix 175 配方补全（CStr(Date) 无时间分量） | 只读 |
| §22 | Fix 178 已实现并验证 | 实现 |
| §23 | 字体改法 + GDI 生命周期陷阱 | 只读 |
| §24 | Fix 175 已实现并验证 | 实现 |
| §25 | Fix 181 已实现并验证（原双号 §24 之一） | 实现 |
| §26 | 问题 4 根因改判：容器子控件第二条发射路径 | 只读 |
| §27 | 悬垂 HFONT 隐患（原 §25） | 只读 |
| §28 | 本条：隐患收口 | 实现 |

### 与参考图还剩的差异（更新后的清单）
1. **第 5 项（新）**：网格只画到第 13 行，参考图 23 行 —— 任务 #28，根因已收窄到
   `Form_Resize` 的单位口径（期望 903x382，实测量到 583x233，且两轴比例不一致、
   与截图目测也不自洽 ⇒ 先复测量法）。
2. 问题 4：CellPicture 预览空白 —— 任务 #29（Fix 182，按 §26 的最小改法）。
3. Fix 179：点击网格即 AV —— 任务 #25（需按当前构建重新取栈）。
4. Fix 180：`ReDim <UDT 成员数组>` 不带 `As` 时载体步长错 —— 任务 #26（demo 不经过）。


---

## 29. 2026-09-22 05:2x：`regress40` 绿（§28 的字体生命周期收口）+ #28 的**决定性证据**

### 门禁
`output/yqt_regress40.log` → `Results: PASS=91 FAIL=0 SKIP=1 TOTAL=92`、`GATE-EXIT=0`、
`[FAIL]` 计数 0、无存活构建进程。⇒ §28 的"每控件一份字体 + NONANTIALIASED_QUALITY"
在无回归前提下落地。本会话四次门禁：regress36/37（Fix 178，90/91 例）、
regress38（Fix 175，92 例）、regress40（Fix 181 + 更正，92 例）。基线 regress35 = 90 例。

### #28（网格只画到 13 行）根因**从假设升级为可指认**
容器侧读控件几何**本来就有正确通道**：`cgen_util_ctrl.cpp:23-24`
（`width → vb6_GetControlWidth`、`height → vb6_GetControlHeight`），
同一条 `Form_Resize` 里 `PicturePanel.Height` 就是走它（`.temp/gen/MainForm.c:631`），
单位自洽。**但 UserControl 实例被更早的"类成员"分支截走了** ——
`VBFlexGrid1.Left` 解析成 `vb6_VBFlexGrid_prop_get_Left(vb6_UC_InstanceOf(hwnd))`，
其函数体是 `vb6_ret_Left = vb6_Extender_Left;`（`VBFlexGrid.c:1638-1643`），
而 `vb6_Extender_Left` 是**进程级单变量**（`vb6rtl_com.c:553`），只在
`vb6_uc_push/pop`（`uc_host.c:154/175/208`）期间才代表"当前实例"。
从窗体侧直调不 push ⇒ 读到的是别的 UC 留下的残值 → 减掉一个大脏数 → 网格被缩。

### 修法（下一轮，二选一；都需要构建）
1. **贴近 VB6 语义**：容器侧 `ucInst.Left/Top/Width/Height` 一律走 Extender 访问器
   （`vb6_GetControlLeft/Top/Width/Height`，RTL 已有），即让"控件 Extender 属性"的判定
   **先于**"类实例成员"的判定命中 UserControl 宿主。
2. 或者让 `vb6_UC_InstanceOf` 直调路径也 push/pop 宿主状态 —— 改动更小，但把
   "全局当实例状态"的既有隐患保留下来了。
先做①，并顺手确认 `ScaleX/ScaleY` 的 COM 调用有没有实现（`vb6forms*` 里 grep 不到，
可能恒返回 0/垃圾；它只贡献 ~160 缇，不是主因，但同一处要一起看）。

---

## 30. 2026-09-22 05:0x：本轮第三次被并发防护跳过（只读）—— Fix 180（`ReDim <UDT 成员数组>` 不带 `As`）根因收窄到**一行判定**，且现成解析器可直接复用

**并发状态**：`tasklist` 无存活构建进程，但会话 14a01034 `runtimeState: running`
（04:59:42 收到我的小结后正在流式执行），`.build/C3.exe` 04:28:35、`yqt_regress40.log` 04:51:13。
⇒ 不编译、不测量、不起门禁、不改 `src/`、**也不往 `tests/` 落文件**（对方门禁会枚举用例目录，
中途加 .bas 会改 TOTAL）。Fix 180 归本会话（§29 清单里它是任务 #26，另一会话未动）。

### 一、根因（纯静态，已指到行）
§22 记的是现象（`h1.Items = vb6_SafeArrayReDim1D(vb6_sa_variant, 0, 1)` → 元素按 16B VARIANT 分配，
读写侧却用 `VB6_SA_AT(vb6_type_TOwned, …)` → 步长不符越界），并猜"入口是 `arrayUdtElemTypes_`
只登记裸数组变量"。**这个猜测方向对，但真正的分叉点在更早的地方**：

```cpp
// cgen_redim.cpp:12-18  visit(ReDimStmt&)
if (node.targetExpr) { emitReDimComplexTarget(node); return; }   // ← Fix 100 的复杂目标通道
...
std::string cName = resolveArrayTargetIdent(node.varName);        // "h1.Items" → h1.Items
std::string lowerVar = node.varName;  → "h1.items"
// :74-79  不带 As 时唯一的回退
auto it = arrayUdtElemTypes_.find(lowerVar);                      // 键是**裸变量名**，永远捡不到 "h1.items"
```

⇒ `ReDim h1.Items(0 To 1)`（owner **不带下标**的简单点号链）根本没进 `targetExpr` 通道
（Fix 100 的 `targetExpr` 只在链上带下标时才有，如 `m_Serie(i).PT(n)`），落到裸名回退 →
`arrayUdtElemTypes_` 键不匹配 → `isUdtArray=false` → 发 Variant 版。
多维分支（`:282`）共用同一个 `udtCType`，所以**修一处两分支都好**。

### 二、最小改法（复用现成件，不新造解析器）
`resolveReDimComplexElemType`（`cgen_redim.cpp:200-231`）已经做了完全正确的事：
`inferUdtTypeOfExpr(owner)` → 取 `vb6_type_` 前缀 → `symTab_` 查 UDT → 遍历 `udtMembers` 命中成员 →
`mi.type == UserDefinedType && !mi.typeRefName.empty()` → `vb6_type_<typeRefName>`。
它只是开头被 `if (!node.targetExpr) return;` 挡死了。

改法（约 6~8 行，单文件）：在裸名回退之后补一段"点号链 owner+member"解析 ——
把 `node.varName` 按最后一个 `.` 切成 `owner`/`member`，`owner` 查 `knownUdtVars_`
（`Dim h1 As THost` 在 `cgen_decl_var.cpp:180` 就登记了 `vb6_type_THost`），
再调**已有**的 `udtFieldObjCType(ownerCType, member)`（它对 `mi.type == UserDefinedType`
的字段直接返回 `vb6_type_<typeRefName>`，**不看 `isArrayDynamic`**，正好是我们要的
`Items() As TOwned` → `vb6_type_TOwned`），命中即赋 `udtCType`。
建议顺手把这段抽成 `resolveUdtMemberArrayCType(const std::string& varName)`，
`visit(ReDimStmt&)` 与 `resolveReDimComplexElemType` 的尾段共用，避免第三份实现。

**边界（本轮只读能确定的）**：
- owner 是类字段（`me->Foo.Items`）或 With 成员（`.Items`）时 `knownUdtVars_` 里没有 owner，
  需要另走 `classMemberVars_` / `withObjectInfoStack_`；demo 与 §22 的用例都是**局部 UDT 变量**，
  先不扩。
- 若 owner 解析不到，**保持现状回落 Variant**（现有注释说明 Variant 尺寸最大 → 过分配不越界），
  不要改成猜。

### 三、顺带纠正一条会被继续误用的结论
`Items() As TOwned` 这类成员，**`typeRefName` 是会记录的**：
`parser_decl.cpp:186-226` 把数组性放在 `arraySize`/`isArrayDynamic` 上，`As` 后面是
**普通 `SimpleTypeRef`**（不是 `ArrayTypeRef`）；`resolveTypeRef` 只在
`case ASTNodeKind::ArrayTypeRef`（`semantic_analyzer_typeref.cpp:134-140`）才 OR 上 `Vb6Type::Array`；
于是 `semantic_analyzer_decl_type.cpp:31` 的 `mi.type == Vb6Type::UserDefinedType` 成立 →
`mi.typeRefName = "TOwned"` 被写入。
⇒ Fix 177 当时"AST 路线走不通"的原因**不是**"成员类型带 Array 标志导致 typeRefName 为空"
（§22 沿用了这句，建议按此更正），而是**owner 链不可解析**：`VBFlexGridCells` 是类字段，
不在 `knownUdtVars_` 里。下一轮若还有"UDT 成员数组"类问题，先按这条口径判，别再去改语义层。
（此结论是读码得出的，落地前用 `--dump-symbols` 复验一次 `TCOLS.Cols` 的 `typeRefName` 非空即可钉死。）

### 四、下一轮的验证设计（**别在对方门禁期间建文件**）
用例形态（可并进 §22 的 `test_udt_assign.bas`，避免新增第 93 例改变 TOTAL 口径）：
```vb
Dim h1 As THost            ' THost{ Items() As TOwned }
ReDim h1.Items(0 To 3)     ' 不带 As —— 正是当前会发 Variant 步长的形态
h1.Items(2).A = 5
h1.Items(2).S = "five"
' 读回全部元素并打印，期望 A=5 / S=five 且无 AV；再加一段带 As TOwned 的对照
```
生成侧断言：`h1.Items = vb6_SafeArrayReDim1D_Udt((int32_t)sizeof(vb6_type_TOwned), 0, 3)`。

---

## 31. 2026-09-22 05:3x：树空闲后做了 Fix 183（Extender 几何读取）——**验证失败并已回退**，但拿到一条推翻"hover 才崩"的证据

### 一、开工与独立复核（本轮前半是空闲的，`tasklist`=0）
按 §29 的线索独立复核，**根因成立**：
- 生成侧 `.temp/gen/MainForm.c:546`：`Width = ScaleWidth - vb6_VBFlexGrid_prop_get_Left((vb6_cls_VBFlexGrid*)vb6_UC_InstanceOf(vb6_hwnd_VBFlexGrid1)) - ScaleX(...)`；
- 该 getter 函数体（`.temp/gen/VBFlexGrid.c:1761`）就是 `vb6_ret_Left = vb6_Extender_Left;`；
- `vb6_Extender_Left` 是**进程级单变量**（`vb6rtl_com.c:553`），只有 `vb6_uc_push/pop`
  （`uc_host.c:154/175/208`）在 UC 自身回调期间才代表"当前实例"；
- 单位自洽性已核：`vb6_GetControlWidth` 返回 `px*15`（`vb6forms_ctrl.c:124`）= 缇，与
  `vb6_GetScaleWidth`/`vb6_SetControlWidth` 同源，且同一条 `Form_Resize` 里
  `PicturePanel.Height` 本来就走它。

### 二、改了什么（已回退，源码现处 HEAD 状态）
`cgen_expr_member_form_builtin.inc` 的 UC 分支里，在 `resolveClassMemberCall` **之前**加
"Extender 几何属性读取 → `vb6_GetControl{Left,Top,Width,Height}(宿主 HWND)`"（约 26 行，
`!asCallCallee_` 才拦，写入侧不动）。ninja 重建 `C3.exe` 成功，重编 demo 后生成 C 确认变成
`vb6_GetControlLeft(vb6_hwnd_VBFlexGrid1)  /* Fix 183 */`。
改动块**已存盘**：`.temp/fix183_block.bin`（下一轮直接贴回原位，别重写）。

### 三、验证失败：demo 变成 0xC0000005 退出
`shot37`（改前）能出图；改后 `yqt_shot.ps1` 报 `EXITED code=-1073741819`。
**⚠ 推翻一条既有认知：这不是 hover 专属。** 用新写的 `.temp/yqt_crashprobe.ps1`
（只 `PostMessage`，不动鼠标）跑两组：
- **A) 纯启动、零输入** → stderr 里**同样有 `[C3_CRASH]` 栈**；
- B) 启动后向网格发 `WM_MOUSEMOVE` → 没有新增崩溃。
⇒ 用户感觉的"hover 才崩"更可能是"崩在启动阶段、但进程当时还活着，鼠标一动窗口没了"。
另注意：**这个 AV 不一定杀死进程** —— 多次出现"trace 已写、`HasExited=False`、窗口还在"，
所以"能截图"≠"没崩"（`"0 errors" ≠ healthy` 的老坑，这次是运行期版本）。

### 四、栈的读法（配套 `-g` PDB，别再用错的那份）
`.temp/yqt_demo_g.bat` = 加 `-g` 的 demo 构建（产出配套 `VBFlexGridDemo.pdb`）。
`.temp/sym3.ps1` 传的是**绝对地址**，其内部基址写死 `0x140000000` ⇒ 传 `0x140000000 + rva`。
结果（`base=0x7FF6D95C0000` 那次）：
```
#0  rtl/vb6rtl.c:89      ← VEH 处理器自身(CaptureStackBackTrace)，不是故障点
#9  VBFlexGrid.c:5277    ← ShowScrollTips Let 尾部: Create/DestroyScrollTip → vb6_UserControl_PropertyChanged
#10 MainForm.c:257       ← 设计期属性应用段(prop_let_ShowScrollTips 调用点附近)
#21 rtl/vb6forms.c:218   ← CreateWindowExA(主窗体) → WM_CREATE 一路进来
#22 MainForm.c:324
#23 Startup.c:37
```
⇒ 崩溃点在**表单创建期的 `ShowScrollTips` 设计期赋值**路径，而不是 `Form_Resize`。
`#1..#8`、`#13..#20` 是 `0xf7…/0xf9…` 的系统模块低 32 位（处理器按 `ptr - hSelf` 打印），
栈走得不干净，**归因前必须先做 A/B**（见下）。

### 五、量到的运行时事实（补 §29 的"两个数不自洽"）
`.temp/yqt_children.ps1`：主窗体 35 个子窗口，其中
`VB6_UserControlHost | 583x233 @ 65,88` 与 `VBFlexGridWndClass | 583x233 @ 65,88`
—— 宿主与内层同 rect，设计期应是 **905x377**（13575x5655 缇）。
⇒ 网格确实没被撑开；§29 怀疑的"量法/时机"问题排除，尺寸错是真的。
同一次枚举还确认字体项已生效：`Printscreen to Clipboard`/`Open UserEditing Demo`/
`Partial Search`/`EnsureVisible` 的**窗口文本**都完整（标题截断项在文本层已解决）。

### 六、本轮为什么停在"已回退"
A/B 做到一半，树又被占了：`C3.exe` PID 8400 于 **05:25:07** 起来（563MB，不是我启的），
`ninja` 链接报 `LNK1168 无法打开 C3.exe 进行写入`。按规矩不抢跑、不杀对方的 C3.exe，
于是把改动块摘回（`git diff` 该文件现在只剩别的批次的 Fix 158k/164y 两个 hunk），
**没有提交任何东西**。
⚠ 遗留状态提醒：`.build/C3.exe`（05:15:58）里**含** Fix 183，而源码已不含 ——
下次 `ninja` 会按 mtime 自动重编，无需手工处理，但在它被重建之前，用这个 exe 编出来的
demo 仍带 Fix 183（会崩）。

### 七、下一轮（顺序写死，免得再烧一轮）
1. `tasklist` 空闲后**先做 A/B**：`C3.exe` 现状（含 183）编一次 demo → 记 `[C3_CRASH]` 是否出现；
   再用 `.temp/fix183_block.bin` 摘除后重建 `C3.exe` 编一次 → 对比。
   只有"摘掉就不崩"才算我引入；若两边都崩，则 AV 是**既有缺陷**（且很可能就是 §29/任务 #25
   说的"点网格即 AV"的同一族），Fix 183 只是把死掉的 `vb6_SetControlWidth` 路径激活了。
2. 若确认既有 AV：先修 AV（栈已指到 `CreateScrollTip`/`DestroyScrollTip` + `PropertyChanged`），
   再回到 Fix 183，一次一件事。
3. 工具留档：`.temp/c3_ninja.bat`（bash 里没有 `ninja`，要用 VS 自带那份 + vcvarsall）、
   `.temp/yqt_demo_g.bat`、`.temp/yqt_crashprobe.ps1`。

## 32. 2026-09-22 05:3x：**Fix 184 已落地** —— #28（网格只画 13 行）根因是 RTL 里**两套缇/像素口径混用**，不是 Extender 残值

### 一、和 §31 的编号/时间对齐（先看这条，避免误读）
§31 那轮把后端方案（`VBFlexGrid1.Left` → `vb6_GetControlLeft`）编成了 **Fix 183** 并**已回退**。
本轮改的是 RTL 单位口径，与它无关，为免同号歧义**改叫 Fix 184**（源码注释、台账一致）。
时间线重叠说明：§31 记的"05:25:07 有个不是我启的 C3.exe PID 8400"就是**本轮** 05:24:43 的
`dev.ps1` 重建；本轮 05:24/05:29 两次重建都发生在 §31 摘回改动之后，所以本轮用的
`C3.exe` **不含** §31 的后端 Fix 183（已用 `git diff` 复核 `cgen_expr_member_form_builtin.inc`
只剩 158k/164y 两个 hunk）。

### 二、根因（可复算，不是猜）
`Form_Resize` 生成式：`Width = ScaleWidth - VBFlexGrid1.Left - ScaleX(8,...)`，
再 `vb6_SetControlWidth(VBFlexGrid1, Width)`。三段的单位口径不一致：

| 环节 | 实现 | 换算因子 |
|---|---|---|
| `vb6_GetScaleWidth`（vb6forms_widget.c:154） | `client_px * (1440/LOGPIXELSX)` | **真实 DPI** |
| `vb6_TwipToX`（vb6forms.c:82） | `twips / 15` | **写死 96** |
| `vb6_GetControlWidth/Height`（vb6forms_ctrl.c） | `px * 15` | 写死 96 |

本机系统 DPI=120（demo 进程 `GetAwarenessFromDpiAwarenessContext=1` 即 System-Aware；
`dpiAware=true` 来自工程自带的 `Resources/Resources.res` 里的 RT_MANIFEST，**原 VB6 exe 同样带**），
于是 ScaleWidth = 922*12 = **11064 缇**，减 Left=120 后交给 `/15` 落像素 →
网格宿主只有 **729x291 px**（设计应为 906x385），缩 20%。
行高由控件自身字体（`-MulDiv(825, 120, 7200)` = 120DPI 口径）得出 ≈21.5px，**本来就是 DPI 正确的**，
所以可见行 = 291/21.5 ≈ **13**，而参考图 = 483/21 ≈ **23**。⇒ 差异全在"窗口被缩小"，不在行高。

同类第二处：`uc_hostmodel_getprop.inc:61` 把 `ScaleWidth` 从缇换像素时用 `val / 15`，
而它上游 `vb6_ho_clientTwips` 已是真实 DPI ⇒ 内层 `VBFlexGridWndClass` 只有宿主的 0.8
（实测 914 vs 731）。`uc_hostmodel_setprop.inc:30` 的 `l/15` 同病。
第三处：`vb6rtl_com.c` 的 `vb6_ucScaleToPixels` 整张单位表按 96 写死。

### 三、改了什么（全部在 `src/rtl/`，无后端改动）
1. `vb6forms.c`：新增 `vb6_DpiX()/vb6_DpiY()`（进程内缓存 `GetDeviceCaps(LOGPIXELSX/Y)`）
   与反向 `vb6_XToTwipX()/vb6_YToTwipY()`；`vb6_TwipToX/Y` 改 `MulDiv(t, dpi, 1440)`。
2. `vb6forms_window.h`：导出上述 4 个入口（注释写明"禁止再写死 15"）。
3. 像素→缇的裸 `*15` 全部换成 `vb6_XToTwipX/vb6_YToTwipY`：
   `vb6forms_ctrl.c`(4)、`vb6forms_widget.c`(2)、`uc_hostmodel.c`(6)、
   `vb6forms_axcontainer.c`(4)、`vb6forms_axsite.c`(1)、`uc_hostmodel_getprop.inc`(2)。
4. 缇→像素的裸 `/15` 换成 `vb6_TwipToX/Y`：`uc_hostmodel_getprop.inc:61`（按 ScaleWidth/Height
   分别走 X/Y）、`uc_hostmodel_setprop.inc:30`。
5. `vb6rtl_com.c`：`vb6_ucScaleToPixels` 的 96 常量表改成读真实 DPI（Twips=dpi/1440、
   Points=dpi/72、Inch=dpi、mm=dpi/25.4、cm=dpi/2.54）。
   `vb6rtl_system.c` 的 `Screen.Width/Height/TwipsPerPixel*` 本来就是 DPI 口径 ⇒ 现在全栈统一。

**为什么不会波及控制台用例**：dpi=96 时 `MulDiv(t,96,1440) == t/15`，逐位等价；
只有 DPI-aware 且系统 DPI≠96 的 GUI 工程才改变行为（正是我们要的）。

### 四、实测（`.temp/yqt_children.ps1` / `yqt_formrect.ps1` / `yqt_shot.ps1`）
| 项 | 改前 | 改后 | 设计/参考 |
|---|---|---|---|
| 窗体 client | 737x404 | **922x506** | 13830x7590 缇 |
| 宿主 `VB6_UserControlHost` | 583x233 | **914x394** | — |
| 内层 `VBFlexGridWndClass` | 583x233 | **914x394**（与宿主一致） | — |
| 截图窗口尺寸 | 940x553 | **1171x680** | 参考图 **1163x671** |
| 可见数据行 | 13 | **23（第 24 行露头）** | 23 |

`.temp/now183.png` 目视：行头 1..23、B 列 `2026/1/1…2026/1/23`、C..N `行.列`、
列 A..N 全出、`Printscreen to Clipboard`/`Open UserEditing Demo`/`Partial Search`/
`EnsureVisible`/`DragRowCol` 标题完整 ⇒ 与参考图仅剩 **CellPicture 预览空白**（= #29/Fix 182）。

### 五、§31 遗留的 AV：本轮做了反向 A/B
本轮构建（不含 §31 后端改动）下 `.temp/t_run184.ps1`：进程 6 秒后 `HasExited=False`、
stderr **0 行、无 `[C3_CRASH]`** ⇒ 网格撑到正确尺寸本身不会触发 AV；
§31 的崩溃应归因于"把 `VBFlexGrid1.Left` 改走 `vb6_GetControlLeft`"这条后端改法
（它在 WM_CREATE 期间就取宿主 HWND 几何）。⇒ 任务 #25 的 AV **不必**由 #28 引出，
下一轮按 §31 的栈（`ShowScrollTips` 设计期赋值 → `Create/DestroyScrollTip` → `PropertyChanged`）另查。

### 六、门禁
`regress41`（05:31 起、05:54 结束）：`Results: PASS=91 FAIL=0 SKIP=1 TOTAL=92`、`GATE-EXIT=0`、收尾 `tasklist` 无存活 C3/cl/link/ninja ⇒ 与 regress40 同分，本轮改动零回归。基线 TOTAL=92 未变（未新增用例）。

### 七、下一轮
1. #29 / Fix 182：容器子控件第二条发射路径缺设计期属性（CellPicture 预览）——
   只动 `cgen_form_ctrl_style_apply.inc` + `cgen_form_frame_menu.inc`，与本轮文件不重叠。
2. #25 / Fix 179：点击/启动期 AV，按 §31 第五节的栈定位。
3. 提交：本轮改动全在 `src/rtl/**`（10 个文件），**必须按 hunk 暂存**；
   工作树仍混着 154/156/159/170/177 等他人未提交批次。

## 33. 2026-09-22 06:0x：**Fix 182 已落地并提交（b953147）** —— 容器子控件补上 .frx 设计期属性，CellPicture 预览不再留白

### 一、并发
开工检查：`tasklist` 无 C3/cl/link/ninja；会话 14a01034 `runtimeState: ready`、
`activeTurnId=null`、`queuedTurnCount=0`；`src/` 近 20 分钟零写入；`regress41` 已有
`Results:` 行（05:52:13，PASS=91 FAIL=0 SKIP=1 TOTAL=92）⇒ 判定树空闲，按流程开工。
本轮全程未与他人的构建抢跑（门禁期间自己也没有再起 demo 编译/探针）。

### 二、改了什么（一次一件事）
§26 的根因判定成立：容器子控件（Frame/PictureBox 的 children）由
`cgen_form_frame_menu.inc` 的 `emitChildControls` 另起一条精简发射路径，而 .frx 设计期
属性整段（`Picture` / `TextBox.Text` / `ListBox.List`+`ItemData`）内联在主控件循环里
（`cgen_form_ctrl_style_apply.inc` 的 P24 块，12 空格缩进），所以
`Picture1.Picture = "MainForm.frx":0000` 从来没被加载。

改法即 §26 的最小方案：把 P24 块整体抽成
`auto emitControlFrxProps = [&](const FrmControl& ctrl) -> void { if (!frxLoaded) return; ... }`，
定义点放在 `cgen_form_create_controls.inc` 主循环之前的**片段最外层作用域**（八个 .inc 都在
`CCodeGen::emitFormFramework` 同一个函数体内被 #include，故后面的 `cgen_form_frame_menu.inc`
能捕获它）；主循环处换成 `emitControlFrxProps(ctrl);`，`emitChildControls` 里紧跟
`emitDesignerStyleProps(child, ...)` 之后加 `emitControlFrxProps(child);`。
对顶层控件是**逐行等价 + 纯移动**（git diff：create_controls +85/-0、style_apply +3/-79、
frame_menu +1/-0）。`bytesToHexArray` 是 `cgen_form.cpp` 的文件级 static，lambda 里直接可用。

### 三、验证
- `ninja` 重建 `C3.exe` 成功（只有 `cgen_form.cpp.obj` 重编）。
- demo 重编：`C3_EXIT=0`、无 `_c3_msvc_out.txt` ⇒ 0 个 MSVC error。
- 生成 C（已快照 `.temp/gen/`）：`MainForm.c:310-311`
  `{ void* vb6_pic = vb6_LoadPictureFromMemory(vb6_frx_pic_Picture1, vb6_frx_pic_Picture1_size);
     if (vb6_pic) vb6_SetControlPicture((void*)vb6_hwnd_Picture1, vb6_pic); }`，
  `MainForm.h:69/123` 数组 824 字节 —— 与 §26 手工解码 frx 头得到的 imgSize=824 一致。
- 截图 `.temp/demo_shot39.png`：左下角 CellPicture 预览框出图，与参考图一致；
  窗口 1171x680、行头 1..23、B 列 `2026/1/1…`、C..N `行.列`、面板标题完整。
- 影响面复核：全工程 10 个生成 .c/.h 里只新增了 `vb6_frx_pic_Picture1` 一个数组 ⇒ 除预期那一处外无副作用。

### 四、门禁与提交
`regress42`（06:0x 起、06:27:35 结束）：`Results: PASS=91 FAIL=0 SKIP=1 TOTAL=92`、
`GATE-EXIT=0`、`[FAIL]` 标记 0 个、收尾 `tasklist` 无存活进程 ⇒ 与 regress41 同分，零回归。
提交 **b953147**（分支 fan/dev，未 push）。`cgen_form_create_controls.inc` 里有**两个** hunk，
第二个（旧行 521 起，别批次的 Fix 143/148/160）用 `git apply --cached --recount` 过滤掉，只暂存
自己的那一个；另两个文件各只有一个 hunk 且全是本轮改动，整文件 `git add`。

### 五、与参考图只剩的一处差异（已定位，留到下一轮 = Fix 185，任务 #11）
右下角 "Drag/drop me" 那块参考图是蓝底白字，我们这边空白。它**不是** frx、也不是按钮，
而是 `Picture2_Paint()`（`MainForm.frm:658-660`：`Picture2.Cls` + `Picture2.Print "Drag/drop me"`）。
两条独立缺口叠在一起：
1. **控件级 `_Paint` 事件从不派发**。`cgen_form_wndproc_subclass.inc` 的 `SubclassInfo`
   根本没有 `hasPaint` 字段，两处 push 门（顶层循环 ~:100、容器子控件循环 ~:146）与
   子类化 WndProc 的 `if (msg == WM_...)` 序列（`WM_DESTROY` 发射在 ~:331）都没有 WM_PAINT
   分支 ⇒ 生成的 `vb6_MainForm_Picture2_Paint()`（`MainForm.c:856`）是死代码。
   已有先例可抄：Form 级 Paint 在 `cgen_form_wndproc_create.inc:22-25`、UserControl 级在
   `cgen_form.cpp:189/199/241`。
2. **`PictureBox.Print/Cls` 落到 COM no-op**。生成的是
   `vb6_ComCall(vb6_hwnd_Picture2, L"Print", ...)`（`MainForm.c:857-858`），而 STATIC 窗口没有
   IDispatch ⇒ 静默什么都不画。全仓复核：`grep '"Print"' src/backend/` 0 命中、
   `src/rtl/core/vb6forms/` 无 `TextOutA` 打印路径 ⇒ `Print/Cls/Refresh` 这一族控件方法**从来没实现过**，
   不只是 PictureBox。
最小改法（下一轮）：子类化里加 WM_PAINT → `BeginPaint` 把 HDC 存进线程局部“当前打印 DC” →
调用户 `_Paint` → `EndPaint`；同时在控件成员调用解析处（`cgen_util_ctrl.cpp`，与
`getControlPropReadFn` 同族）把 PictureBox/Label 的 `Print/Cls` 路由到新的
`vb6_ControlPrint/vb6_ControlCls`。只做派发不做 Print 仍是空白，两件必须同批或先做 Print 侧。

### 六、下一轮
1. Fix 185（上面第五节）—— 若成功则**与参考图完全一致**，可宣告目标达成。
2. Fix 180（任务 #9，§30 配方）—— demo 不经过，不阻塞验收，排在 185 之后。
3. 台账/编号：184 已被另一会话用于 RTL 缇/像素口径（§32），183 已回退且**不要再贴回**
   `.temp/fix183_block.bin`（#28 的真实根因是口径混用，不是 Extender 残值）；本轮用 182，下一轮用 185。

## 34. 2026-09-22 06:5x：本轮被并发防护跳过（只读）—— Fix 185 的**完整可照抄配方**（三个改动点全部定位到行）

### 一、并发
`tasklist`：C3.exe PID 10584 + 3×cl.exe 存活，`output/yqt_regress43.log` 06:51:44 正在写
⇒ 另一会话（14a01034）刚起门禁。按硬性规矩本轮**没编译、没截图、没跑回归、没改 `src/`**，
只做只读定位，把结论留在这里。

### 二、背景（为什么只剩这一处）
`b953147`（Fix 182，容器子控件补 .frx）之后，与参考图仅剩右下 “Drag/drop me” 一块空白。
它是 `MainForm.frm:658-660` 的 `Picture2_Paint()`（`Picture2.Cls` + `Picture2.Print "Drag/drop me"`）。
全工程复核：`grep '_Paint()' *.frm *.ctl` **只有这一个** Paint 处理器，`grep '\.Print ' ` 也只有这一处
控件 Print ⇒ 改动的爆炸半径就是这一块，别的项目不受影响。

### 三、三条改动点（下一轮照抄即可）

**1) 后端：控件级 WM_PAINT 派发** —— `src/backend/detail/module/cgen_form_wndproc_subclass.inc`
- `struct SubclassInfo`（:28-45）加 `bool hasPaint = false;`
- **两处**扫描循环都要设值：顶层循环（push 门在 :100-102）与容器子控件循环（:123-146，
  `info.hasValidate` 那一串之后）。取值口径要窄，按 VB6 语义只有 PictureBox 有 Paint：
  `info.hasPaint = (child.controlType == FrmControlType::PictureBox) && symTab_.lookup(child.controlName + "_Paint") != nullptr;`
  （Label/Frame/Image 不要放行 —— VB6 根本没有这些 Paint，放行只会把它们的自绘标题擦掉）
- 两处 push 门各加 `|| info.hasPaint`
- 发射点：在 `WM_DESTROY` 那行（:331）**之前**插一段，命名用现成的
  `cProcName(info.ctrlName + "_Paint", AccessLevel::Private)`（与 :162 的 GotFocus 完全同族，
  它生成的就是 `vb6_MainForm_Picture2_Paint`）：
  `if (msg == WM_PAINT) { PAINTSTRUCT ps; HDC hdc = BeginPaint(hwnd,&ps);`
  `SetPropW(hwnd, L"VB6_PaintDC", (HANDLE)hdc);` `extern void FN(); FN();`
  `RemoveProp(hwnd, L"VB6_PaintDC"); EndPaint(hwnd,&ps); return 0; }`
  **必须 `return 0`**：STATIC 默认 WM_PAINT 会画自己的（空）标题并把我们的字擦掉。

**2) 后端：`Print`/`Cls` 不再走 COM 后期绑定** —— `src/backend/detail/expr/cgen_expr_call_com_bind.inc`
插入点就在 `std::string memberName = std::move(comMemberName_);`（:37）之后，
**先例是同文件 :40-60 的 Fix 057**（`Me.Controls.Add` → `vb6_Form_ControlsAdd`），照它的写法做：
`memberName` 小写 ∈ {`print`,`cls`} 且 `objExpr` 以 `vb6_hwnd_` 开头（含 `(void*)vb6_hwnd_` 变体）
→ `lastExpr_ = "vb6_ControlPrint(" + hwnd + ", " + arg + ")"` / `"vb6_ControlCls(" + hwnd + ")"`，
`isComMarker_ = false; return;`。
**语句包装已确认安全**：`src/backend/stmt/cgen_call.cpp:427` 只在
`callExpr.find("vb6_ComCall(") == 0` 时才套 `vb6_ComVarFree((void*)...)`，
所以换成普通 RTL 调用后会自动走 `else` 分支直接发射 ⇒ RTL 侧可以是 `void` 返回。
（顺带：`vb6_ComVarFree` 本身 NULL 安全，`vb6com_pack.c:181-185` 有 `if (!variant) return;`）
参数侧：`Picture2.Print "x"` 只有一个 positional 实参，`emitExpr` 后取 `lastExpr_`（BSTR = `wchar_t*`）；
带 `;`/`,`/多实参的完整 VB6 Print 语义本轮不做（工程里零使用）。

**3) RTL：新增 `vb6_ControlPrint` / `vb6_ControlCls`** —— 放**已在** `C3RTL_EMBEDDED_FILES` 里的
`src/rtl/core/vb6forms/vb6forms_ctrl.c`（新文件要同时改 `CMakeLists.txt` 与 `c3rtl.rc`，别踩这个坑），
原型放 `vb6forms_prop_pic.h`（`vb6_SetControlPicture` 就在这里声明，:35）。实现要点：
- DC：`GetPropW(hwnd, L"VB6_PaintDC")`，取不到再 `GetDC(hwnd)`（运行期直接 Print 也能画）。
- **NULL BSTR ≡ `""`**：`text == NULL` 必须先归一（memory 里这一族专门坑过）。
- 字体：`SendMessageW(hwnd, WM_GETFONT, 0, 0)` —— Fix 181 之后每控件一份 HFONT，正好取得到；
  `SelectObject` 后记得还原。
- 颜色：`VB6_ForeColor` 窗口属性（`vb6forms_ctrl.c:344/357` 已有 getter/setter），
  没有就用 `GetSysColor(COLOR_WINDOWTEXT)`；`SetBkMode(TRANSPARENT)`。
  注意：**全仓没有 WM_CTLCOLORSTATIC 处理**（`grep WM_CTLCOLOR src/rtl/core/vb6forms/*.c` 0 命中），
  所以别指望向父窗发 WM_CTLCOLORSTATIC 拿颜色，直接读属性即可。
- 光标：`VB6_PrintX/VB6_PrintY` 两个窗口属性（缺省给一点边距），`DrawTextW` 带
  `DT_LEFT|DT_NOPREFIX|DT_SINGLELINE`，打印后按字体行高推进 Y（VB6：不带 `;` 的 Print 结束一行）。
- `Cls`：复位 X/Y；若不在 paint 窗口内（没有 `VB6_PaintDC`）再补 `InvalidateRect`。
  在“每次 WM_PAINT 重画”的模型里 Cls 天然是 no-op，**不需要** AutoRedraw 位图。

### 四、为什么不做成“运行时在 vb6_ComCall 里认 HWND”
`vb6com` 的 invoke 路径确实有按身份识别的先例（`vb6_UC_IsFont`/`vb6_Host_IsHostObject`），
但把 Print/Cls 塞进去等于给一个纯 GDI 操作套一层 IDispatch 语义，且每次调用都走
`GetIDsOfNames`；memory 里那条“宁可窄的语义正确改法，别放宽身份守卫”就是为这类情况写的。
放在 `cgen_expr_call_com_bind.inc` 的 Fix 057 旁边，改动小、可测、不影响任何真实 COM 对象。

### 五、下一轮
1. 树空闲 → 按第三节三步实现 Fix 185（一次一批：后端两文件 + RTL 一文件，同属一个功能，
   只做派发不做 Print 仍是空白，所以必须同批）。
2. 验证口径：截图里 “Drag/drop me” 出现且颜色/位置与参考图一致；`grep vb6_ControlPrint .temp/gen/MainForm.c`。
3. 门禁 `regress44`（43 已被另一会话占用）；绿了按 hunk 提交。
4. 之后：Fix 180（任务 #9，§30 配方，demo 不经过）。

## 35. 2026-09-22 07:0x：又被并发防护跳过（你 regress43 在跑）—— Fix 185 配方**改判到更好的落点**；另记一条我自己踩的 `--dump-symbols` 违规

### 零、先占号：§34 已用（我的 Fix 185 配方），本节是 §35
你在 06:5x 的回信里说“门禁绿了我再补台账 §34”——**§34 已经被我在 06:55:25 写了**（见文件位置 2195 行）。
你的 Fix 179 那一节请写 **§36**（`grep -o "^## [0-9]*\." | sort | uniq -d` 现在只剩历史遗留的 `## 2.` / `## 10.`）。

### 一、⚠ 我违反了一次只读约束（如实记录，别学）
`--dump-symbols` 我以为是纯前端 dump（本轮简报里把它列为允许的只读手段），**实测不是**：
`.build/C3.exe <vbp> --dump-symbols` 打印完符号表后**继续走 codegen 并调用了 MSVC** ——
证据：退出码 1、`%TEMP%\C3C80847909961200\` 里有 `.c/.h/.obj` 和一个 **605 KB 的 `_c3_msvc_out.txt`**
（该文件只在有 error 时才落盘）。⇒ 在你门禁运行期间我白跑了一次全量编译（抢 CPU）。
损害范围已核，**零污染**：`output/c3-error.log` 仍是昨天 15:47、`output/yqt_full1.log` 仍是我 06:06 那次、
仓库根 `VBFlexGridDemo.exe` 仍是你 06:43:38 那次、VB 工程目录没有 07:0x 的新文件（`bisect*.vbp` 是 9 月 21 日 17:4x 的历史文件）、
你的会话目录 `180982088027600` 与我的并存未被删。
**结论/规矩修正**：dump 类开关只在**单个 .bas** 上用（配 `--output-dir .temp/scratch`），
或在树空闲时用；对 `.vbp` 一律当成"会编译"。已写进项目 memory 的陷阱层。

### 二、`--dump-symbols` 回答不了 §30 的那个问题（顺手钉死）
§30 第 3 条建议"落地前用 `--dump-symbols` 复验 `TCOLS.Cols.typeRefName` 非空"。实测该开关
**只列模块级 Type/Enum/Declare/Property 条目**（本次只输出 `Type TCOLS : UserDefinedType [Private] (unused)`，
行 3165），**不打印 UDT 成员字段**，所以它证明不了 `typeRefName` 是否为空。
⇒ Fix 180 的那步验证改成：实现时直接在 `resolveUdtMemberArrayCType` 里临时打印 `udtFieldObjCType` 的返回值，
或者干脆以"生成的 `ReDim` 行是否变成 `vb6_type_TOwned` 版"为唯一判据（生成 C 快照在 `.temp/gen/`，一次编译就能看）。

### 三、Fix 185 改判：不要动 `cgen_expr_call_com_bind.inc`，走**既有的"控件族方法派发"两文件对**
§34 第 2 条说在通用 COM 后期绑定片段里按 `objExpr` 前缀认 `vb6_hwnd_`。**有更合规的落点**，
因为它就是为这件事建的：控件族方法是"**成员片段设标记 + 调用片段发射**"成对实现的，
已有两族先例，且都按 `knownFormControls_` 的**类型**分派，不需要字符串猜前缀：
- 成员侧 `src/backend/detail/expr/cgen_expr_member_form_builtin.inc`：
  WebBrowser 分支 :242-253、ListBox/ComboBox 分支 :256-266 —— 两者都是
  `comObjExpr_ = objLower; comMemberName_ = memLower; isComMarker_ = true; lastExpr_ = objLower; return;`
  （注意存的是**小写控件名**，不是 HWND 表达式）。
- 调用侧 `src/backend/detail/expr/cgen_expr_call_callee_withm.inc`：
  WebBrowser :240-262、ListBox/ComboBox :264-310 —— `knownFormControls_.find(comObjExpr_)` 命中类型后
  `c_.emitLine("vb6_AddItem((void*)vb6_hwnd_" + ctrlName + ", " + itemArg + ");  /* ListBox.AddItem */"); lastExpr_ = "0";`

**为什么 `lastExpr_ = "0"` 不会漏出一条裸 `0;`**：`src/backend/stmt/cgen_call.cpp:202` 有显式的
`else if (callExpr == "0")` 分支（"表达式访问器内部已经把语句发出去了"协议）。
实测当前生成 C 里裸 `0;` 语句 **0 处**（`grep -rn "^\s*0;$" .temp/gen/*.c`）⇒ 协议成立。
这也说明 §34 里"RTL 可以是 void"的判断对，但**理由要换成这条**：走 `emitLine + "0"` 协议时根本不经过
`cgen_call.cpp:427` 的 ComVarFree 包装，比"前缀不是 vb6_ComCall("更直接。

**改判后的三步（替换 §34 第三节第 2 步）**：
1. `cgen_expr_member_form_builtin.inc`：在 :266 之后、:294 的 Fix 023e/089d 兜底**之前**插 PictureBox 分支
   （`memLower ∈ {print, cls}` → 设 `comObjExpr_=objLower`/`comMemberName_=memLower`/`isComMarker_=true`）。
   位置关键：兜底那段会把 `comObjExpr_` 设成 **HWND 表达式**（:297 `comObjExpr_ = ctrlHwnd`），
   一旦先落进兜底，调用侧的 `knownFormControls_.find(comObjExpr_)` 就再也查不到类型 —— 这正是
   今天 `Picture2.Print` 变成 `vb6_ComCall(vb6_hwnd_Picture2, L"Print", ...)` 的原因。
2. `cgen_expr_call_callee_withm.inc`：在 ListBox 块之后加第三块（`itCtrl->second == FrmControlType::PictureBox`），
   `print` 取 `node.positional[0]` 实参、`cls` 无参；**照抄 :280-296 的 Fix 143 守卫**：若实参自身又设了
   COM 标记，要先消费成 `vb6_ComGetStringProp(...)`，否则残留标记会被上层当独立语句发射。
3. RTL 侧同 §34 第 3 步（`vb6forms_ctrl.c` 里 `vb6_ControlPrint/vb6_ControlCls`，原型进 `vb6forms_prop_pic.h:35` 旁）。
   后端两文件与另一会话当前在改的 `vb6rtl*`/With 块区域**零重叠**。

### 四、你那边 Fix 179 的两条连带结论（对我下轮有用）
1. **§31 的 A/B 到此结束**：你重跑（不含 Fix 183）仍有 6 条启动 trace ⇒ AV 与我的改动无关，
   `.temp/fix183_block.bin` 永久作废，我不会再贴回。
2. 你把 #13 的"跨过程泄漏"推翻成 `With New <Form 模块>` 未识别 Form 符号 → 整块退化 `vb6_ComSetProp`，
   并预留 **Fix 186** ⇒ 我这边任务表不碰 186，编号继续用 185（我）/187+。

### 五、门禁与提交
本轮**未跑门禁**（无代码改动），**未提交**；HEAD 仍是 `b953147`（我的 Fix 182）。
你的 `regress43` 06:51:44 起、07:04:30 仍在写。编号：42=我、43=你、**44 归我下轮**。

### 六、下一轮（树空闲）
按第三节改判后的三步做 Fix 185 → 截图验收 "Drag/drop me" → `regress44` → 绿了按 hunk 提交。
若成功，与参考图**完全一致**，届时明确宣告目标达成并建议用户停用本任务；之后只剩 Fix 180（demo 不经过）。

## 36. 2026-09-22 07:5x：用户直接指派 —— **`output/Charts2020/Proyecto1.exe` 关闭时鼠标转圈，已确认是回归**（同工程 09-19/09-20 的构建关闭干净）

### 一、症状量化（探针 `.temp/gui_one_probe.ps1` / `.temp/charts_bisect.ps1` / `.temp/gui_close_triage.ps1`）
跑的是 `.temp/probe_charts/` 下的**副本**（门禁会覆盖原文件）。启动出窗 ~0.6s，`PostMessage(WM_CLOSE)` 之后：

| 构建（同一工程 Proyecto1，x86） | 关闭延迟 | 退出码 |
|---|---|---|
| `output/reg_charts_after_di/` 09-19 18:21 | 171 ms | **0 干净** |
| `output/_reg/charts2020/` 09-20 09:08 | 24 ms | **0 干净** |
| `output/yqtcharts/` 09-20 18:36 | 35 ms | **0 干净** |
| `output/Charts2020/` 09-22 07:43（当前） | **2703 ms** | **0xC000041D** |

⇒ **用户记忆正确，这是回归**，引入窗口 = 2026-09-20 18:36 → 2026-09-22 07:43（即 09-21 起的整个未提交系列 168→184 + 179/182）。
“转圈”就是这 2.7 秒：UI 线程卡在回调里不泵消息，Windows 显示等待光标，然后进程带异常码退出。

### 二、故障形态（不是猜，有 trace）
1. WER（Application 日志 1000/1001）：`异常代码 0xc0000005`、**故障模块 unknown、偏移 0x00000000**、
   两条分别对应 `output\Charts2020\`（07:37:55，门禁自己那次）与 `.temp\probe_charts\`（07:42:11，我这次）。
   ⇒ 访问地址 0，且**门禁自己跑的时候也崩了**，只是它看不见（见第四节）。
2. RTL 自带 VEH（`C3_CRASH_TRACE=1`，探针 `.temp/charts_trace_probe.ps1`）：
   `[C3_CRASH] code=0xc0000005 at rva=0xff020000 base=00FE0000 frames=4` /
   `av write=8 target=0x0` ⇒ **往地址 0 写 8 字节**（x86 进程里的 8 字节存储 = `double`/`__int64`/结构体前 8 字节，
   典型是“往一个 NULL 出参写值”）。
   栈：`#0 rva=0x5e556`（本 exe 内，绝对地址 = `0x00FE0000 + 0x5e556`），`#1..#3 = 0x76b5xxxx/0x76b4xxxx`
   （user32/kernelbase）⇒ **故障函数是被 user32 回调进来的**（WndProc / 枚举回调），不是我们自己主动调的。
3. 崩溃前最后一段 trace 是成串的 `[C3_COM] GetProp entry: disp=00709298 isFont=1 isHost=1 name=Name/Bold/Italic/
   Underline/Strikethrough/Size` 循环（同一个 font 宿主对象被反复读六项，跨多个控件），
   停在其中一轮 `name=Size` 之后 ⇒ 现场在“遍历控件读 Font”的关闭/卸载路径上。

### 三、爆炸半径（差分，全部是今天同一 C3.exe 产出的 GUI exe）
| 用例 | 关闭结果 |
|---|---|
| `czUI/czFormDemo.exe` 07:43（**同为 x86 + 工程内 UserControl**） | 110 ms / 0 干净 |
| `empty_form.exe` 07:12 | 103 ms / 0 干净 |
| `BalloonTooltips.exe` 07:42 | 98 ms / 0 干净 |
| `VbQRCodegen/Project1.exe` 07:42 | 110 ms / 0 干净 |
| `Charts2020/Proyecto1.exe` 07:43 | **2703 ms / 0xC000041D** |

⇒ 不是通用 teardown 回归（否则 czUI/empty_form 一起倒），是 **Charts2020 特有路径**。它比 czUI 多的是
`Proyecto1.LabelPlus`（带 frx 位图 Caption）、`ucTreeMaps`、`ucProgressCircular` 重复实例等。

### 四、⚠ 门禁看不见这一类故障（结构性盲区，值得单独修）
`tests/run_tests.ps1:724` 是 `Test-GuiVbp "Charts2020" ... -Arch "x86" -AutoExitSec 3`，而
`Test-GuiVbp` 在 `AutoExitSec>0` 分支里是 **`$proc.Kill()`**（:301）—— 强杀之后再不看退出码；
只有 `AutoExitSec=0` 那一支才 `WaitForExit(2000)` 并在 :306 检查“关不掉就 throw”。
⇒ 任何“关窗时 AV / 慢退出 / 关不掉”的用例，在带 `-AutoExitSec` 时一律报 PASS。
这解释了为什么 regress41/42/43/45 全绿而它是坏的。**建议**（改 `tests/` 会动别人的门禁枚举，先只记录不改）：
`-AutoExitSec` 分支在 `Kill()` 之前先 `WaitForExit(小超时)`，若已自行退出就断言 `ExitCode -eq 0`。

### 五、附带发现（另一个、更早就存在的退出缺陷）
`VBFlexGridDemo.exe`（x64，今天 06:43 由另一会话产出）：`WM_CLOSE` 后**窗口确实被销毁**
（`IsWindow=False`）但**进程 20s 仍存活**（6 线程）⇒ 最后一个窗体关闭后没人 PostQuitMessage /
`vb6_AnyThreadWindowVisible()` 驻留判据没退出。这个同样被 `-AutoExitSec` 掩盖。与本轮的 8 字节写 0 是**两个问题**，
别混在一起修。

### 六、排除项：不是我上轮的 Fix 182
`tests/Charts 2020/Form2.frm` 里 3 处 frx 引用全是 **`Caption = "Form2.frx":00xx`**，宿主是
`Proyecto1.LabelPlus`（工程内 UserControl 实例），且**都是窗体直接子控件**（缩进 3 空格，无 Frame/PictureBox 嵌套）。
Fix 182 只改 `emitChildControls`（容器子控件第二条路径）的 frx 发射 ⇒ 这个工程没有容器子控件走 frx，
新代码路径根本不触发。（本轮全程未编译，结论来自 .frm 结构静态核对。）

### 七、下一步（需要编译，等另一会话门禁结束）
1. `--arch x86` + `-g` 重编该工程拿 PDB，符号化 `#0`：绝对地址 = `base(0x00FE0000) + rva(0x5e556)`；
   `.temp/sym3.ps1` 是照 x64 的 `0x140000000` 写的，x86 要传这个 base+RVA（dbghelp 用同一 base 加载 PDB）。
2. 定位到函数后，按“往 NULL 出参写 8 字节”的形态找缺空指针检查的那一处（优先看关闭/卸载路径里
   遍历控件读 Font 的循环，以及 UC 宿主 `isFont=1 isHost=1` 那条 GetProp 分支）。
3. 编号：185 归我（Paint/Print）、186 归另一会话（`With New <Form>`）、**本问题记为 Fix 187**。
4. 门禁：`regress44` 归我（43/45 是另一会话的）。

## 37. 2026-09-22 07:1x：**Fix 179 已落地** —— 启动期 AV 的根因是 `vb6_App_hInstance()` 把 64 位镜像基址截成 int32_t（故障地址逐位吻合），与 §31 猜的 ShowScrollTips/Form_Resize 都无关

### 〇、先更正 §32 里我自己写错的一条（重要，别再拿它当"没崩"的证据）
§32 第六节那条反向 A/B（"纯启动 6s、`HasExited=False`、stderr 0 行、无 `[C3_CRASH]` ⇒ AV 归 Fix 183 那条路线"）**无效**：
我用的是自己手写的 `.temp/t_run184.ps1`，它**没有设 `C3_CRASH_TRACE`**，而 VEH 处理器是环境变量门控的
（`vb6rtl.c:114` / `vb6forms.c:532`）⇒ "stderr 空"只证明我没开开关，不证明没崩。
用对方的 `.temp/yqt_crashprobe.ps1`（它设了）重跑**当时盘上的 exe**（b953147 + Fix 184，**不含** Fix 183）：
启动即 **5~6 条 `[C3_CRASH]`**。⇒ 顺带把 §31 悬着的 A/B 做完了，结论与对方相反但方向一致：
**这个 AV 与 Fix 183 无关，是既有缺陷**（183 只是多踩了一次同一条 `CreateWindowEx` 路径）。
教训（已进 memory 那一层）：门控 trace 的空输出永远不能当阴性结果。

### 一、新栈（`-g` 06:33 构建，PDB 与 exe 同分钟配套，中间目录 `C3C\179034726549300`）
`.temp/sym3.ps1`（传 `0x140000000 + rva`）：
```
#9  VBFlexGridBase.c:194   FlexClassAtom = RegisterClassEx((void*)&(WCEX));
#10 VBFlexGrid.c:951       vb6_VBFlexGridBase_FlexWndRegisterClass();   ← UserControl_Initialize
#11 VBFlexGrid.c:552       vb6_VBFlexGrid_ucHostInit: UserControl_Initialize(me)
#12 rtl/uc_host_create.inc:95
#13 MainForm.c:244   #14 MainForm.c:118
#23 rtl/vb6forms.c:249     CreateWindowEx(主窗体) → WM_CREATE
```
⇒ 崩在 **UC 的 `Initialize` 里注册窗口类**，不是 `ShowScrollTips`、不是 `Form_Resize`。
§31 那条 `VBFlexGrid.c:5277`（Create/DestroyScrollTip）只是**同一根因的另一个调用点** ——
`VBFlexGrid.c:5957/5996/6451/6553/6580/6750` 的 `CreateWindowEx(..., vb6_App_hInstance(), ...)` 全都传同一个被截断的句柄。

### 二、根因（算术级确证，不需要猜）
```c
int32_t vb6_App_hInstance(void) { return (int32_t)(intptr_t)GetModuleHandleW(NULL); }
```
x64 下 `GetModuleHandleW(NULL)` = 镜像基址 `0x7FF7A6580000` → 截成 `0xA6580000`（负 int32）→
赋给 `intptr_t WCEX.hInstance` 时**符号扩展**成 `0xFFFFFFFFA6580000`。
trace 第二行 `av write=0 target=0xffffffffa6580000` 与它**逐位相同**，且 `base=00007FF7A6580000` 的低 32 位就是 `0xA6580000`。
user32 在 `RegisterClassExW` 里要拿 `hInstance` 去查模块（`RtlImageNtHeader` 一类）→ 裸读该地址 → `0xC0000005` 读故障。
`#1..#8` 落在 `exe基址+0x2c1…`／`+0x2ad…` 两个系统模块（ASLR 下每次同偏移，跨两次构建一致）⇒ 与"user32→kernelbase 内部炸"吻合。
VB6 里 `App.hInstance As Long` 只是因为 VB6 只有 32 位，不是"这值只有 32 位有效"。

### 三、改了什么（Fix 179，2 行，全在 `src/rtl/core/vb6rtl/`）
1. `vb6rtl_builtin.h:210` 原型 `int32_t` → `intptr_t`；
2. `vb6rtl_system.c:226` 实现 `return (intptr_t)(uintptr_t)GetModuleHandleW(NULL);` + 把截断/符号扩展的推导写进注释。

同类面已穷举复核（免得下一轮再扫）：
- `grep "(int32_t)(intptr_t)" src/rtl/` 只有 8 处，其中**手写内建里唯一把指针当 int32 返回**的就是 `vb6_App_hInstance`；
- DI 桩生成器产出的 handle 形参/返回**已经全是 `intptr_t`**（`vb6_di_CreateWindowExW/FindWindowExA/GetWindowLongW/LoadLibraryW/GetProcAddress/…`）⇒ 这一族已被生成器覆盖；
- backend 只有 `cgen_expr_member_form_builtin.inc:66-67` 两处发射 `vb6_App_hInstance()`，生成的 `.c/.h` 里**没有**第二处 `int32_t vb6_App_hInstance` 重声明 ⇒ 无 C2371/隐式 int 截断风险。
- 遗留（本次**没动**，已评估）：`vb6_UserControl_ContainerHwnd` 仍是 `int32_t`（`uc_host.c:169` 写入，生成侧 `VBFlexGrid.c:13691/34659` 拿它当 `MapWindowPoints`/`GetWindowLong` 的 HWND 实参）。Win64 保证 HWND 落在 32 位内 ⇒ 只有低 32 位最高位置 1 时才会符号扩展出错，属潜在项而非本次故障。

### 四、验证
- `ninja` 重建 `C3.exe`（06:42:12）→ demo `-g` 重编 `C3_EXIT=0`、无 `_c3_msvc_out.txt` ⇒ 0 MSVC error。
- `.temp/yqt_crashprobe.ps1 -Hover`（PostMessage-only）连跑 **3 次**：启动 + hover 全部 **0 条 trace、stderr 0 行**，进程 `HasExited=False`。
  改前同一脚本同一 exe 路径 = **6 条 trace**（快照 `.temp/trace179_before.txt`，改后 = 空文件 `.temp/trace179_after.txt`）⇒ 干净 A/B。
- 截图 `.temp/now179.png`：窗口 `1171x680`、行头 1..23、B 列 `2026/1/1…`、C..N `行.列`、CellPicture 出图、面板标题完整 ⇒ 与 §33 之后一致，**纯稳定性修复，零视觉变化**。
- 门禁 `regress43`（06:51:44→07:07）：`Results: PASS=89 FAIL=2 SKIP=1 TOTAL=92`、`GATE-EXIT=1` —— 两条 FAIL 是
  `test_com_default_prop` / `test_bstr_concat_scalar`，真实报错 **`fatal error C1060 编译器的堆空间不足`**（cl.exe 在
  `/Od /W3 /Gy /MP` 一次带 ~55 个 RTL TU 时 OOM），**散在互不相关的 `rtl/*.c`** ⇒ 不是代码错误。旁证：两个用例的
  `.exe/.obj` 仍停在 06:18/06:19（= regress42 全绿那轮的产物），说明它们在编译阶段就没产出；单编两个用例
  `exit=0` 通过；`grep -l hInstance tests/*.bas` = 0 命中 ⇒ 我的改动碰不到它们。
  诱因已查明（对方 §35 第一节自记）：07:03-07:05 对方把 `--dump-symbols` 当只读手段用在 `.vbp` 上，它实际会跑完整
  codegen + 调 MSVC ⇒ 在我门禁的 /MP 窗口里多了一份并发编译。**重跑 `regress45`（07:20→07:49:09）：
  `Results: PASS=91 FAIL=0 SKIP=1 TOTAL=92`、`GATE-EXIT=0`、`[FAIL]` 标记 0 个、收尾 C3/cl 进程数 0** ⇒ 与 regress41/42
  同分，**Fix 179 零回归**，且反证 regress43 那 2 条是环境性 OOM。

### 五、与参考图仍只剩的那一处
右下 "Drag/drop me"（`Picture2_Paint` + `Print/Cls` 族缺失）—— 即 §33 第五节，**Fix 185 归对方**，本轮没碰那两半行。

### 六、状态与下一轮
- 未提交。工作树里现在是我这边的 178/175/181(+收口)/184/**179**，加上其他批次的 154/156/159/170/177 hunks；HEAD 仍 `b953147`。
- 编号：179 已被本项用掉（任务 #25 标题里就是它），180/185 归对方，**我这轮之后下一个空号是 186**。
- 任务 #25 关闭；#29（Fix 182 = b953147）已由对方落地，我顺手把状态改成 completed，避免下一轮有人重做。
- 下一轮我这边排队：**#13 已改判**（见本节末尾附注，真根因是 `With New <Form 模块>` 未识别 Form 符号 → 整块退化成 `vb6_ComSetProp`，InputForm 对话框静默失效），修号取 **186**；或 #12（Extender 访问器剩余小簇）。若对方 185 成功、验收达成，就转做 #26/#9 那类"demo 不经过但语义错"的存量项。
- 编号/台账：对方案 §34（Fix 185 配方）+ §35（改判 + 自记 `--dump-symbols` 违规）都已落盘，我这一节是 **§36**。
  `regressNN`：42=对方、43=我（被 C1060 污染，见下）、**44=对方下轮**、45=我的重跑。
- ⚠ 给对方的 hunk 提醒：Fix 185 第 3 步要把 `vb6_ControlPrint/vb6_ControlCls` 加进 **`vb6forms_ctrl.c`**，
  而这个文件里有我 Fix 184 的**未提交** hunks（`vb6_GetControlLeft/Top/Width/Height`，:124 附近）。
  新函数追加在文件尾 ⇒ 是独立 hunk，但**整文件 `git add` 会把我的口径改动一起吞进你的提交**，请按位置筛 hunk。

## 38. 2026-09-22 08:2x：Fix 187 取证续 —— **拿到真实调用链**：`DestroyWindow` 内部 user32 调到地址 0（execute-at-NULL），不是写越界

### 一、把 §36 的两处判读纠正过来（重要，后面别再按旧说法走）
1. `av write=8 target=0x0` 里的 **8 不是"写 8 字节"**：`ExceptionInformation[0]==8` 是 **DEP/执行违例**，
   `target=0x0` 是**被执行的地址** ⇒ 故障形态是 **call/jump 到 NULL**（`vb6rtl.c:95-100` 的注释也是这么写的，
   我 §36 里读成"写 8 字节"是错的）。
2. §36 里"栈 #0 是应用函数"也不对：`#0 rva=0x8f806` 用 `.temp/charts_g2/Proyecto1.map` 解出来是
   **`_vb6_CrashTraceVEH@4 + 0x66`**，`c3_crash.txt` 的 `#00 Proyecto1.exe+0xB6FB9` 解出来是
   **`_vb6_crashFilter@4 + 0x199`** —— 两个都是崩溃上报器自己的帧，`CaptureStackBackTrace` 从 handler 里走，
   应用帧全丢。**真正的线索来自新加的 ESP 线性扫描（`st+N`）**。

### 二、真实调用链（ESP 扫描 + map 符号化，构建 = `.temp/charts_g2/`，用 08:03:37 的 C3.exe 即含 st+ 扫描的那版 RTL）
```
[C3_CRASH] code=0xc0000005  EXECUTE(DEP) target=0x0
  st+144 rva=0xb10ed  -> _vb6_di_DestroyWindow@4      (+0xD)
  st+147 rva=0x8f3ec  -> _vb6_form_wndproc_Form2@16   (+0x15C)   ← WM_CLOSE 分支里的 DestroyWindow(hwnd)
  st+166 rva=0x52fc0  -> _vb6_ucChartBar_ucTimerThunk_tmrMOUSEOVER (+0x0)  ← 只是栈上的数据指针，不是返回地址
  st+280 rva=0xb65f6  -> _vb6_MessageLoop             (+0x76)
  st+290 rva=0x88568  -> _WinMain@16                  (+0x58)
  st+293 rva=0xd1edf  -> __scrt_common_main_seh       (+0xF8)
```
⇒ **`消息循环 → user32 → Form2 的 WndProc(WM_CLOSE) → DestroyWindow → user32 内部把控制权交给了地址 0`**。
`st+0..st+143` 之间没有任何镜像内的值 ⇒ 直接调 NULL 的那一帧**在系统模块里**（user32/ntdll），
不是我们某个函数里 `call NULL`。也就是说：**某个窗口在这一刻的 WndProc 已经变成/本来就是 NULL**。
（RegisterClass 时 lpfnWndProc=NULL 会直接失败，所以只能是**运行期被 SetWindowLongPtr 写进去的 NULL**，
或者 comctl32 子类链在条目已失效后被 `DefSubclassProc` 派发。）

### 三、RTL 里所有写 GWLP_WNDPROC 的点（逐个核过，明面上都有 `if (origProc)` 保护）
- `vb6forms_widget.c:33/49`（`vb6_InstallControlSubclass` / `vb6_RemoveControlSubclass`）
- `vb6forms_picture_prop.c:293/310`（Image/PictureBox 子类，**与上面共用同一个属性名 `VB6_OrigProc`**）
- `vb6forms_shape.c:437/452`（Graphical button 子类，属性名 `VB6_GfxBtn_OrigProc`）
⇒ 单看每一处都不会写 NULL。**剩下的可疑组合是"同一窗口被两套子系统叠装/互拆"**：
生成码的 `WM_DESTROY` 里先 `vb6_RemoveControlSubclass(hwnd)`（把属性删掉、proc 还原），
随后同一窗口若再被另一条路径按 `GetPropW(VB6_OrigProc)` 取值还原，就会拿到 **NULL 并写进 GWLP_WNDPROC**
—— 这条链在 `vb6forms_picture_prop.c` 与 `vb6forms_widget.c` 共用 `VB6_OrigProc` 时是**能成立**的
（两边都"只装一次"、谁先删属性另一边就读到 0）。**下一轮先验这条**：给这两处加"取到 NULL 就不写"的
显式断言/日志（或把两处属性名分开），一次只动一处，重编 charts_g2 复测。

### 四、为什么只有 Charts2020 中招（差分线索，未证）
`Form2.frm` 里 `ucProgressCircular1` **同名出现 3 次**（:322/:357/:391）⇒ 这是**工程内 UserControl 的控件数组**，
再叠加 `LabelPlus`（3 个、带 frx 位图 Caption）与 `ucTreeMaps`。今天同 C3.exe 产出的
czUI/empty_form/BalloonTooltips/VbQRCodegen 都没有这个组合，且它们关闭全部 98–110ms 干净。
若第三节那条验不过，下一步就按**工程侧二分**：把这三类控件分别注掉重编（每次 ~40s），看哪一个消失后不崩。

### 五、本轮的并发纪律
开工时树空闲（regress45 已绿：PASS=91 FAIL=0 SKIP=1 TOTAL=92）。中途发现另一会话 08:00:58 改了
`src/rtl/core/vb6rtl/vb6rtl.c`（+76 行，就是 `st+` ESP 扫描）并于 08:03:37 重建 C3.exe ⇒ 我没有再动
`.build/C3.exe`，而是 **`copy` 了一份 `.temp/C3_snap.exe` 用副本编译**，产物全部落在
`.temp/charts_g/`、`.temp/charts_g2/`，`--output-dir` 独立，没碰 `output/Charts2020/`（那是它门禁的产物）。
探针：`.temp/gui_one_probe.ps1`（单 exe 关闭延迟+退出码）、`.temp/charts_bisect.ps1`（四构建对照）、
`.temp/trace_probe.ps1`（抓 `[C3_CRASH]`）、`.temp/wndproc_enum*.ps1`（**跨进程读 GWLP_WNDPROC 不可信，
64 位与 32 位 PowerShell 都拿不到真值，别再走这条路**）。

## 39. 2026-09-22 08:2x：**Fix 187 已落地** —— Charts2020 关闭崩溃根因是 Declare 的 `ByVal As String` 只在**函数名以 A 结尾**时才做 ANSI 编组，`GetProcAddress` 拿到裸 BSTR 返回 0 → 工程自造子类化 thunk 里 `call 0`

### 〇、先更正 §36 第二节的故障形态读法（对方 §38 独立得出同一结论：execute-at-NULL）（这条决定去找什么）
`[C3_CRASH] av write=8 target=0x0` 里的 **`8` 不是"写 8 字节"**：`EXCEPTION_RECORD.ExceptionInformation[0]` 的取值是
`0=读 / 1=写 / **8=执行(DEP)**`。再看 `at rva=0xff020000 base=00FE0000` —— x86 是 32 位指针，
`0 - 0x00FE0000` 回绕正好等于 `0xFF020000` ⇒ **`ExceptionAddress = 0`**。
两者合起来 = **CPU 跳到地址 0 去执行**，不是"往 NULL 出参写值"。所以 §36 第二步"按缺空指针检查的出参去找"
的方向不成立；要找的是**一个为 0 的函数指针被调用**。
（已把这条读法固化进 RTL：`vb6rtl.c` 的 VEH 现在打 `av EXECUTE(DEP)` / `av read` / `av write`。）

### 一、根因（一句话）
`cgen_decl_api.cpp` 只在 Declare 的**函数名/Alias 以 'A' 结尾**时才把它登记进 `knownDeclareAnsi_`，
而调用点的 BSTR→ANSI 编组（`cgen_expr_call_arg_emit.inc:96-110`）只对登记过的函数生效。
于是：

| Declare | 名字 | 实参怎么发出去 | 结果 |
|---|---|---|---|
| `GetModuleHandleA(sDLL As String)` | 以 A 结尾 | `vb6_BSTR_ToANSI` → `char*` | 正常（hmod 非 0） |
| `GetProcAddress(h, sProc As String)` | **不以 A 结尾** | 裸 BSTR（UTF-16）当 LPCSTR | **返回 0** |

VB6 的真实语义是：`As String` 过 Declare 边界**永远**按 ANSI 编组（VB6 没有宽字符编组，跟 API 叫什么名字无关）。
所以这是编组条件写错了，不是 `GetProcAddress` 特殊。

### 二、它怎么变成"关窗转圈 2.7 秒 + 退出码 0xC000041D"
`tests/Charts 2020/ucChartArea/ucChartArea.ctl`（以及 LabelPlus / ucChartBar / ucPieChart /
ucProgressCircular / ucTreeMaps 共 6 个 .ctl 全同）里是 VB6 圈经典的"自造机器码子类化"写法：
1. `:2617 zFnAddr(sDLL, sProc)` = `GetProcAddress(GetModuleHandleA(sDLL), sProc)` ← **就是这里返回 0**；
2. `:912 z_ScMem = VirtualAlloc(0, MEM_LEN, MEM_COMMIT, PAGE_RWX)` 申请可执行内存；
3. `:915 CreateWindowExA(0, "Static", "GDI+Safe Patch", WS_CHILD, ...)` 造一个隐藏 Static 窗；
4. 把 `z_Code()` 里那堆手拼机器码 `RtlMoveMemory` 进 `z_ScMem`，其中
   `z_Code(2) = zFnAddr("user32", "CallWindowProcA")` —— **在 C3 下是 0**；
5. `SetWindowLong(hwndGDIsafe, GWL_WNDPROC, z_ScMem + WNDPROC_OFF)` 把这个 Static 的窗口过程指向堆里的 thunk。

Static 建好后到关窗前几乎收不到消息；`DestroyWindow` 时才收到 `WM_DESTROY`/`WM_NCDESTROY` →
进 thunk → thunk 执行 `call [z_Code+8]` = **`call 0`** → 执行违例 → 进程带异常码退出。
这也解释了为什么"崩在关闭"、以及为什么栈里紧邻 Esp 的调用者**不在 exe 镜像内**（它在 VirtualAlloc 的堆里）。

### 三、怎么定位到的（x86 上原来的 trace 完全不够用）
1. `--arch x86 -g --keep-for-debug --output-dir .temp/charts187` 拿配套 PDB（`.temp/charts187_probe.ps1` 复现）。
2. 原 VEH 在 x86 只返回 4 帧，且 `#0` 是 `vb6rtl.c:89`（`CaptureStackBackTrace` 自己），`#1..#3` 全在
   ntdll/异常派发链 ⇒ **应用侧调用者一个都没有**（§31 那句"#1..#8 是栈扫描噪声"在 x86 上其实是"全丢"）。
3. 于是给 VEH 加了一段 `#ifdef _M_IX86` 的 **Esp 线性扫描**：从 `ContextRecord->Esp` 往下 512 个 DWORD，
   把落在本模块 `[hSelf, hSelf+SizeOfImage)` 内的值按 `st+N rva=…` 打出来（`__try` 包住读，最多 24 个）。
   配套 `.temp/sym_x86.ps1`（`sym3.ps1` 写死 x64 的 `0x140000000`，x86 要传真实 base）。
4. 扫出来符号化：`vb6_di_DestroyWindow`(`vb6_di_user32_stubs.c:287`) ← `Form2.c:53`(`WM_CLOSE→DestroyWindow`)
   ← `vb6forms.c:433`(`DispatchMessage`) ← `Form2.c:1120`(main)。紧邻 Esp 的第一帧不在镜像内 ⇒ 堆里的 thunk。
5. 10 行最小复现 `.temp/scratch187/t1.bas`：改前 `procaddr=0`，改后 `procaddr=1978643600`。
   生成侧对照一眼看穿：`t1.c:14` 有 `vb6_BSTR_ToANSI`，`t1.c:17` 直接把 BSTR 塞给 `GetProcAddress`。

### 四、改了什么
1. `src/backend/decl/cgen_decl_api.cpp`：删掉 A 后缀启发式（22 行），**每个 Declare 都登记**进
   `knownDeclareAnsi_`；调用点条件不变（`ByVal` + 形参 `As String`），本来就与 API 名字无关。
2. `src/rtl/core/vb6rtl/vb6rtl.c`（诊断，非行为修复）：`av write=%d` → `read`/`write`/**`EXECUTE(DEP)`**；
   新增 x86 Esp 扫描块。都只在 `C3_CRASH_TRACE`/`C3_COM_TRACE` 门控的崩溃路径上，正常运行零影响。

**风险（本轮唯一一条宽面改动）**：这会让**所有**非 A 后缀 Declare 的 `As String` 实参改成传 ANSI，
包含 W 版 API（`FindWindowW` 等）。按 VB6 语义这是"更忠实"（VB6 也传 ANSI），但如果某个测试工程原本
靠"C3 意外传了宽字符"才能跑，就会在这里翻转 —— 交给 `regress46` 判定。

### 五、验证
- 最小复现翻转：`procaddr=0` → `procaddr=1978643600`。
- Charts2020（x86 `-g`）连跑 3 次 `WM_CLOSE`：**`[C3_CRASH]` 0 条**；关闭延迟 ≈**64~106 ms**
  （§36 表里 09-19/09-20 的干净构建是 24~171 ms，改前那版是 2703 ms）。
  独立旁证 = WER：`Get-WinEvent Application id=1000` 里 `Proyecto1` 最后一条是 **07:46:13**（改前），
  改后的三次运行**没有新增任何事件**（`.temp/wer_recent.ps1`）。
- demo（VBFlexGridDemo，x64）重编 `C3_EXIT=0`、窗口 `1171x680`、截图 `.temp/now187.png` 与 §33/§37 一致、
  `stderr` 0 行 ⇒ 这条宽面规则没碰坏验收过的画面。
- 门禁 **`regress46`**（08:2x→09:0x）：`Results: PASS=91 FAIL=0 SKIP=1 TOTAL=92`、`GATE-EXIT=0`、`[FAIL]` 标记 0 个、
  收尾 `C3.exe` 进程数 0 ⇒ 与 regress41/42/45 **同分**，这条宽面编组规则零回归。
- 关闭路径横扫（`WM_CLOSE`，同一套探针，含 `-AutoExitSec` 看不见的退出码）：
  `Proyecto1.exe` **close=71 ms / exit=0 (clean)**（§36 表里改前是 2703 ms / 0xC000041D）、
  `czFormDemo.exe` 85 ms、`BalloonTooltips.exe` 63 ms、`VbQRCodegen/Project1.exe` 69 ms、
  `NewTab/Test.exe` 67 ms —— 五个 x86 GUI 用例 **crash trace 全 0**。

### 六、遗留（本轮没做，别混进 187）
1. **§36 第五节仍未修**：`VBFlexGridDemo.exe` `WM_CLOSE` 后窗口销毁但进程不退出（6 线程存活）
   ⇒ 最后一个窗体关闭后没人 `PostQuitMessage`／`vb6_AnyThreadWindowVisible()` 判据不退出。与 187 是两回事。
2. **门禁看不见这类故障**（§36 第四节，结构性盲区）：`Test-GuiVbp -AutoExitSec` 分支是 `$proc.Kill()`
   后再不看退出码 ⇒ "关不掉/关时 AV"一律 PASS。改动要动 `tests/run_tests.ps1`，会碰别人的门禁枚举，
   建议单独一轮做，并明确它会让原本"假绿"的用例变红。
3. 树里有个 **06:49:33 起的 `VBFlexGridDemo.exe` 僵尸进程**占着 exe（我 demo 重编时撞到 `LNK1168`）。
   我**没有杀它**（可能是另一会话在查上面第 1 条），改测 `.temp/demo187/` 里的独立产物绕开。
4. **提交时的 hunk 位置图**（本轮 4 个文件全是混着别人改动的大文件，整文件 `git add` 会互吞）：
   `src/backend/decl/cgen_decl_api.cpp` 两个 hunk，**只有 `@@ -47,27 +47,15 @@` 是 187**（另一个 `@@ -94,6 +82,11 @@` 是别批次）；
   `src/rtl/core/vb6rtl/vb6rtl.c` 四个 hunk，**只有 `@@ -72,9 +73,73 @@` 是 187 的诊断**；
   `vb6rtl_builtin.h` / `vb6rtl_system.c` 是 179+184+别批次的混合，同样按位置筛。
5. 编号：187=本项（沿用 §36 第三节的分配）；185/180 归另一会话；186=我的 `With New <Form 模块>`。
   门禁号：43/45/46 我，44 另一会话。

## 40. 2026-09-22 08:5x：**Fix 187 拿到极小复现**（单个 UC 演示工程就崩），并推翻 §38 的两条判读；另发现一个更严重的独立缺陷

### 一、极小复现（这是本轮最大产出）
`tests/Charts 2020/` 下面每个 UserControl 自带一个演示工程。用 `.temp/C3_snap.exe`（= 08:03:37 的 C3.exe 副本）
`--arch x86 --keep-for-debug --output-dir .temp\sub_<UC>` 分别构建：

| 子工程 | 构建 | 关闭行为 |
|---|---|---|
| `ucChartArea/Proyecto1.vbp` | OK | **WM_CLOSE 后 155–167 ms → 0xC000041D，`EXECUTE(DEP) target=0x0`** ⇒ 与大图工程同一故障 |
| `ucPieChart/Proyecto1.vbp` | OK | **启动 ~578 ms 就崩**（`EXITED_EARLY 0xC000041D`，根本没窗口），同一故障形态 |
| `ucChartBar` / `ucProgressCircular` / `ucTreeMaps` | C3 退出码 1（VB4001 之后编译失败） | 未产出 exe；属**既有**编译问题，与本故障无关 |

⇒ 复现面从"Form2 五千行"缩到**一个窗体 + 一个 UC**。以后回归这个 bug 只要 40 秒构建 + 15 秒探针。

### 二、⚠ 推翻 §38 的两条判读（别照 §38 动手）
1. **`vb6_di_DestroyWindow+0xD` 不是"动态导入解析失败"**。`src/rtl/core/di/vb6_di_user32_stubs.c:285` 是
   手写转发桩：`return ((intptr_t (WINAPI *)(intptr_t))DestroyWindow)(hWnd);` —— 调的就是链接进来的真 API。
   那个返回地址只说明"正在 user32!DestroyWindow 里面"。
2. **"两套子系统共用 `VB6_OrigProc` 属性名"这条对本工程不成立**：`grep Subclass / GWL_WNDPROC .temp/gen_charts/*.c`
   里**一次都没有**（只有没用到的 `#define GWL_WNDPROC (-4)`）⇒ 这工程根本不装控件子类化，谈不上互拆属性。
   （该假设对*有*子类化的工程仍是隐患，但**不是 Fix 187 的因**，别在这里花时间。）

### 三、重新确认的调用链（极小复现 + map 符号化，`.temp/subg_ucChartArea/Proyecto1.map`）
```
[C3_CRASH] code=0xc0000005  av EXECUTE(DEP) target=0x0
  st+145 rva=0x1cd8d -> _vb6_form_wndproc_Form1@16 (+0x15D)   ← WM_CLOSE 分支的 DestroyWindow(hwnd)
  st+152 rva=0xe5728 -> (CRT/异常表区，非函数)
  st+164 rva=0x1c090 -> _vb6_ucChartArea_ucTimerThunk_tmrMOUSEOVER (+0x0)  ← 数据指针，不是返回地址
  #0     rva=0x1d1e4 -> _vb6_CrashTraceVEH@4 (+0xA4)          ← 上报器自身，忽略
```
大图工程那条链的 `vb6_form_wndproc_Form2+0x15C` 与这里的 `Form1+0x15D` 是**同一个发射点**（WM_CLOSE→DestroyWindow），
⇒ 链是真的，不是栈残渣。**结论不变**：`DestroyWindow` 在 user32 内部把控制权交给了地址 0，
而 `st+0..st+143` 没有任何镜像内的值 ⇒ 直接调 NULL 的那一帧在系统模块里。
本工程既没有子类化、`RegisterClass` 也不可能带 NULL proc，所以嫌疑收窄到
**user32 回调应用侧的其它入口**：UC 的 `IOleInPlaceSiteWindowless` 站点（`vb6forms_axsite.c:855` 有 3 条
C4113 **签名与槽位不符**的警告，说明该 vtable 的初始化顺序与接口顺序对不上——不是 NULL 槽，但会调错函数）、
IME/MSCTFIME、以及 UC 设计期定时器窗口（`uc_host.c:413/425`，`SetTimer(h,1,50,NULL)`）。

### 四、下一轮怎么收尾（需要一次真调试，别再靠栈扫描猜）
`D:\ProgramData\snapshot_2026-05-27_12-11
elease2\headless.exe` 支持 `headless.exe {OPTIONS} [filename] -- [args]`
与 `-c <command>`（已实测 `-h` 会打印这份帮助）。计划：`-c` 里下异常断点 + `run`，命中后读 **ESP 起第一个返回地址**
和 user32 帧里的 HWND/参数，直接点名"哪个窗口/哪个回调是 NULL"。若 `-c` 的脚本能力不够，就退到在 RTL 里加一段
env-gated 诊断（`RegisterClassEx`/`CreateWindowEx` 打 class+proc 指针），但那只动 `src/rtl/core/vb6forms/`，
要先和另一会话错开（它 08:00 起在动 `vb6rtl.c`）。
工具沉淀：`.temp/mapsym.py <map文件> <rva...>`（x86 用 base 0x400000 + RVA 直接查 .map，比 dbghelp 省事）、
`.temp/gui_one_probe.ps1`（单 exe：出窗时刻/关闭延迟/退出码）、`.temp/trace_probe.ps1`（抓 `[C3_CRASH]`）、
`.temp/charts_sub_build.bat <UC>`（40 秒构建极小复现）。

### 五、并发与门禁
本轮全程只用 `.temp/C3_snap.exe` 副本编译，产物只在 `.temp/sub_*`、`.temp/subg_*`、`output/sub_*.log`；
没动 `.build/C3.exe`、`src/`、`output/Charts2020/`。**没跑门禁、没提交**（HEAD 仍 b953147）。
另一会话的 regress46 已绿（08:39:57，PASS=91 FAIL=0 SKIP=1 TOTAL=92）。
⚠ 顺带记一条覆盖面问题：这 5 个子工程**完全不在门禁里**（`run_tests.ps1` 只注册了主 vbp 的 `Charts2020`），
所以 `ucPieChart` 演示工程"启动即崩"这种级别的问题没有任何地方能发现。

## 41. 2026-09-22 08:5x：**独立复核 §39（Fix 187）= 已修好**，并补两条对方没覆盖的数据（极小复现 + ucPieChart 启动即崩同源）

### 一、复核方法（不共享构建产物，零污染）
用**当前** `.build/C3.exe`（08:13:10，含 08:12:17 的 `cgen_decl_api.cpp` 修复）把三个工程编到
我自己的目录 `.temp/verif_area|verif_pie|verif_full/`，再用 `.temp/close_check.ps1` 量 `WM_CLOSE` 延迟与退出码。
本轮我**没有**再动 `.build/C3.exe`、`src/`、`output/`（日志写 `.temp/verif_*.log`）。

### 二、结果：三例全部干净（改前基线取自 §36/§40）
| 目标 | 改前 | **改后（本轮实测）** |
|---|---|---|
| `tests/Charts 2020/ucChartArea`（极小复现） | 155–478 ms / 0xC000041D | **73 ms / exit 0** |
| `tests/Charts 2020/ucPieChart` | **启动 ~578 ms 即崩**（无窗口）0xC000041D | **75 ms / exit 0** |
| `tests/Charts 2020`（用户报的那个大图工程） | 2703 ms / 0xC000041D | **73 ms / exit 0** |

⇒ **§39 的根因判定成立，用户报的"关闭转圈"已消除**，且回到 09-19/09-20 干净构建的量级（24–171 ms）。

### 三、两条新增信息（对方 §39 里没有）
1. **`ucPieChart` 演示工程"启动即崩"与关闭崩溃是同一个根因**，不是第二个 bug —— 修复后它 75 ms 干净退出。
   所以 §40 第三节那条"更严重的独立缺陷"自动关闭，别再单开一项。
2. **极小复现值得沉淀成回归用例**：`tests/Charts 2020/ucChartArea/Proyecto1.vbp`（一个窗体 + 一个 UC）
   就能复现，构建 ~40 秒。另外这 5 个子工程**一个都不在门禁里**（`run_tests.ps1` 只注册主 vbp 的 `Charts2020`），
   所以"ucPieChart 启动即崩"这种级别的问题没有任何地方能发现 —— 与 §36 第四节的 `-AutoExitSec` 盲区是两回事，
   这是**覆盖面**盲区。顺带记：`ucChartBar`/`ucProgressCircular`/`ucTreeMaps` 三个子工程当前**编译失败**
   （C3 退出码 1，VB4001 之后），是既有问题、与 187 无关，纳入门禁前要先修。

### 四、状态
- 本轮**未改任何源码**（修复是另一会话的 §39），**未跑门禁、未提交**；HEAD 仍 `b953147`（我的 Fix 182）。
  §39 第四节的 hunk 位置图由对方自己按它提交，我不代提。
- 任务 #12（Fix 187）置为 completed（含本表复核数据）。
- 另一会话提到的 06:49 `VBFlexGridDemo.exe` 僵尸进程：本轮查 `tasklist` 已不在（我自己也没有遗留进程）。

## 42. 2026-09-22 09:1x：**Fix 188 已落地** —— `Startup = Sub Main` 的工程关窗后进程不退出：没人投递 WM_QUIT，因为 Fix 144 的 PostQuitMessage 只挂在"启动窗体"上，而这类工程**没有启动窗体**

### 一、症状与复现（§36 第五节那条，独立于 187）
`.temp/demo_close_probe.ps1`（PostMessage-only）对 `.temp/demo188/VBFlexGridDemo.exe`：
`WM_CLOSE` 后 **`IsWindow=False`（窗口真销毁了）但进程 20s 仍存活、6 个线程**，stderr 零 trace。
⇒ 不是崩，是**挂死**；副作用很实际：exe 被占住，下一轮 demo 重编直接 `LNK1168`（本轮就撞了一次）。

### 二、根因（三行代码闭环）
1. 入口：`Startup.c:63` 是 `if (vb6_AnyThreadWindowVisible()) vb6_MessageLoop();`（`cgen_base_generate_entry.inc:116/165`）。
2. 循环：`vb6forms.c:431` 是 `while (GetMessage(&msg, NULL, 0, 0))` —— **只有 WM_QUIT 能结束它**。
3. 谁投 WM_QUIT：后端 `cgen_form_wndproc_dispatch.inc:142-156` 的 Fix 144 规定
   **只有"启动窗体"的 WM_DESTROY 才发 `PostQuitMessage(0)`**。而 `Startup = Sub Main` 的工程
   **没有启动窗体** ⇒ 生成的 `MainForm.c:206-207` 的 WM_DESTROY 里只有 `vb6_Forms_Unregister(hwnd)`，
   没有任何投递 ⇒ `GetMessage` 永久阻塞。
（对照：`Startup = 窗体` 的工程正常，因为那条规则命中了。所以 czUI/Charts2020/empty_form 全都干净退出，
只有 VBFlexGridDemo 这一类驻留 —— 与 §36 的差分一致。）

### 三、改法（VB6 语义 = "最后一个窗体卸载 ⇒ 程序结束"，判据本来就在 Forms 注册表里）
`vb6rtl_system.c` 的 Forms 集合已经有 `g_formCount`（`VB6_MAX_FORMS` 那张表），所以：
1. `vb6_Forms_Unregister`：摘除后 **`g_formCount == 0` 且消息循环真在跑** ⇒ `PostQuitMessage(0)`。
2. 新增 `void vb6_Forms_LoopDepth(int delta)`（`vb6rtl_builtin.h` 声明），`vb6_MessageLoop` 进出各调一次
   （**计数**而非布尔，模态嵌套循环才不会把外层误关掉）。
3. 为什么必须带"循环在跑"这个限定：`Sub Main` 里先 `Load Form: Unload Form` 再 `Show` 另一个的工程，
   如果无条件投递，那条提前进队列的 WM_QUIT 会让它**永远进不了消息循环**（Show 完立刻退出）。
4. 为什么不去放宽后端 Fix 144 那条"仅启动窗体"：Sub Main 工程根本没有可判定的"启动窗体"对象；
   而 Forms 注册表就是 VB6 自己的判据，天然覆盖多窗体 / MDI / 模态。

改动文件：`src/rtl/core/vb6rtl/vb6rtl_system.c`、`src/rtl/core/vb6rtl/vb6rtl_builtin.h`、
`src/rtl/core/vb6forms/vb6forms.c` —— 与另一会话在做的 Fix 185 三文件
（`cgen_expr_member_form_builtin.inc` / `cgen_expr_call_callee_withm.inc` / `vb6forms_ctrl.c`）**零重叠**。

### 四、验证
- demo 关窗：改前 `20s 存活 / 6 线程 / IsWindow=False` → 改后 **`close_ms=81~157` 且进程真退出**；
  `-g` 构建连跑 12 次 + 非 `-g` 再连跑 6 次，**crash trace 全 0**。
- 启动窗体路径没被搞坏：`tests/Charts 2020`（Form2 是启动窗体，现在会双投递 WM_QUIT）
  用当前 exe 重编后 **`close=84ms / exit=0 (clean)`**（`.temp/gui_one_probe.ps1`，与 188 前同量级）。
- demo 编译 `C3_EXIT=0`，画面未复看（本轮改动不碰绘制路径）。
- 门禁 **`regress47`**：`Results: PASS=91 FAIL=0 SKIP=1 TOTAL=92`、`GATE-EXIT=0`、`[FAIL]` 标记 0 个、收尾无存活进程
  ⇒ 与 regress41/42/45/46 **同分**，零回归。
- 关闭行为横扫（全部用 regress47 现建的 exe，即**已含 188**，`.temp/gui_one_probe.ps1` 会读真实退出码）：
  `Charts2020/Proyecto1` 87ms、`czUI/czFormDemo` 63ms、`BalloonTooltips` 85ms、`VbQRCodegen/Project1` 87ms、
  `NewTab/Test` 75ms、`empty_form` 70ms、**`form_mdi_parent` 69ms** —— **7/7 全部 `exit=0 (clean)`**。
  （MDI 与"启动窗体双投递 WM_QUIT"这两类是 188 最可能踩坏的，实测都没坏。）

### 五、残余（188 **没有引入**，只是它让进程第一次真能走到退出路径才暴露）
非 `-g` 构建 16 次关闭里 **1 次**出现退出路径上的"往地址 0 写"AV：
`code=0xc0000005 at rva=0x15a271 base=00007FF6281B0000`、`av write target=0x0`、
`#4 0x15a271  #5 0xa9df0  #6 0x5ed33  #7 0x138643`（一次 26 条 trace 的级联）。
**同一形态在 188 之前就抓到过**：06:29 那次 release exe 的最后一条 trace 是
`rva=0x15a201 / write=1 / target=0x0` —— 当时进程挂死在消息循环里，所以没人把它当问题。
复现与抓取方法、以及"为什么要配同一次构建的 PDB"都记在 `.temp/teardown_av_note.md`；
这条另开任务，不并进 188。

### 六、编号与台账
- Fix **188** = 本项（对方 §41 说 188+ 空闲，取之）。台账本节 = **§42**（§40/§41 是对方 187 的续）。
- 门禁号：43/45/46/47 我，44 对方。
- 未提交。`vb6rtl_system.c` / `vb6rtl_builtin.h` 现在是 **179 + 184 + 188 三批混合**（还混着别人的 hunk），
  提交时按位置筛；`vb6forms.c` 是 184 + 188 + 别批次。

---

## 43. 2026-09-22 12:2x：**Fix 189 已落地（仅测试侧）** —— 门禁 `Test-GuiVbp -AutoExitSec` 的盲区：窗口出来了、随后自己崩退，照样记 PASS

**触发**：`Charts2020`/`czUI`/`NewTab`/`ExtShow` 四个用例走 `-AutoExitSec 3`。旧实现在"等窗口 → 轮询到超时 → `Kill()`"之后**无条件 `$script:pass++`，从不读退出码**；而轮询循环里的 `if ($proc.HasExited) { break }` 让"它提前自退"也走进同一条 pass 路径。于是窗口出现后的运行期崩溃只要落在预算秒数内，门禁就是绿的——实测标本 `flexgrid_compile2\VBFlexGridDemo.exe`：约 18s 抛 `0xC0000005`，`rva=0x15c5b1 av write target=0x0`。"编译期 0 错误 ≠ 健康"这条口径在运行期同样成立。

**改法**（`tests/run_tests.ps1` 的 `Test-GuiVbp`，只加断言、不动其它分支）：区分"我们杀的"与"它自己退的"。超时兜底那条仍免检 PASS；`$proc.HasExited` 为真则读 `ExitCode`，非 0 就 `throw ("exited on its own at ~{0}s with code 0x{1:X8}" -f ...)` 交给既有 catch 记 FAIL，退码 0 记 `PASS (compile, window, self-exit at ~Ns, code 0)`。`$script:pass++` 移到分支末尾，throw 路径自然跳过。PowerShell 的 `throw "…" -f $x` 会被当成 `throw` 的参数，必须加括号。

**验证**：① `.temp/fix189_proof.ps1 -Exe <那个 11:21 的包> -BudgetSec 25`：同一次运行下**新代码 FAIL、旧代码 PASS**，洞确认闭合；② `.temp/gate_vbp.ps1 -Tag vbp189`（只跑 `-Category vbp`，255 秒 vs 全门禁 20–25 分钟）→ `PASS=12 FAIL=0 SKIP=1 TOTAL=13`，四个 `AutoExitSec` 用例仍报 `auto-exit after 3s`，加严没把现成用例打成红。

**同日查到、且不在代码里的两条，一并记录**：
1. 嵌入清单是**三方**约束：`src/driver/c3rtl.rc` 的 `<id> RCDATA "路径"`、`src/driver/rtl_embedded.hpp` 的 `RTL_NAME = <id>`、`src/driver/rtl_embedded.cpp` 的 `{ RTL_NAME, "basename" }`——`.cpp` 只按 **basename** 索引，所以 id 错位是"抽出另一个文件"而非报错。合并提交 `c9b7986` 解这一族时取了一侧：`RTL_VB6_DI_UNKNOWN_STUBS_C`(205) 在枚举和表里、`.rc` 却没有 205，于是 `vb6_di_unknown_stubs.c` 根本没嵌进 C3.exe；同族 DI 桩从 **489 掉到 421，162 个手写桩丢失**。工作树里那份未提交的重生**是真并集**（583 = 270 两侧一致 + 182 取我们 + 131 取上游；名字与桩体均无丢失、无杜撰），但 HEAD 仍未提交，从 HEAD 干净构建拿到的还是坏编译器。核对工具：`.temp/yqt_embed_xcheck.sh <rev|WORKTREE>`、`.temp/di_body_audit.py`（比**桩体**，不只比名字——名字是全并集也可能逐符号取了对方桩体而静默回退手工修复）、`.temp/di_sig_divergence.py`（揪指针宽度/元数变化）。
2. `ByVal <x> As Currency` 传 Win32 `POINT` 按值，x64 下**只有打包成单个 64 位整数**（x 在低 32、y 在高 32）才对：`.temp/abi_probe.c` 以真原型为基准实测，我们的 `(double)` 与上游的 `(intptr_t,intptr_t)` **都返回错误的 HWND**（真值=可见探针窗口）。且 DI 桩只按 API 名索引，同一 API 的两种真实声明形状无法共存（工作树已自相矛盾：`WindowFromPoint` 用上游 2 参、`ChildWindowFromPoint` 用我们 1 参，而 VBFlexGrid 里两者都是 `As Currency`）。要修必须让桩名按声明形状区分（codegen + `gen_di_stubs.ps1` 一起改），不是选边。
---

## 44. 2026-09-22 14:3x：**Fix 190 + Fix 191 已落地** —— demo 的 hover 崩溃与左键点击崩溃，一条是代码生成的左值判定，一条是 `As LongPtr` 数组按 Variant 载体分配

**现象**（用户实测，两次都靠人鼠标操作才暴露）：
1. 鼠标**悬停**到网格上 → 进程自动退出。
2. 悬停修好后，鼠标**左键点击**单元格 → 退出；**右键不崩**（右键不抢焦点，这条差异直接把根因钉到 `WM_SETFOCUS`）。

**工具链事实（先记，后面还会反复用到）**：要符号化崩溃轨迹**必须**用 `-g` 编译，
`.temp/sym_x86.ps1 -Exe <exe> -Base 0x140000000 -Addrs <rva,...>` 依赖同目录的 `.pdb`；
`.temp/yqt_demo.bat` 只带 `--keep-for-debug`（**无 PDB**），那种 exe 的 RVA 一个函数名都出不来。
本轮全部改用 `.temp/case190g.ps1`（`tests\VBFlexGridDemo\VBFlexGridDemo.vbp -g --keep-for-debug
--output-dir .temp\case190g` 顺手启动带 `C3_CRASH_TRACE`+`C3_COM_TRACE` 的实例给人点）。
另：C3 不带 `--output-dir` 时 exe 落在**当前工作目录**（脚本里是仓库根），不在 vbp 目录也不在
`output/` —— 找产物时别只翻 `output/`。

**Fix 190（悬停崩）= 代码生成把带 `[]` 下标的链判成非左值。**
`VBFlexGrid.ctl:28348` `CopyMemory .szText(0), ByVal StrPtr(Text), LenB(Text)`（`szText(0 To 159) As Byte`，
`With NMTTDI`）展开成 `_vb6_with_776->szText[0]`；`cgen_expr_call_arg_emit.inc` 的 `isUdtFieldChain`
字符循环只放行 标识符/`.`/`->`，方括号一出现就判非左值 → 落入 `As Any ByRef` 的非左值兜底
`(void*)(intptr_t)(expr)`，于是把**首字节的值**当 memcpy 的目的地址；`NMTTDI` 已清零 → 写 0x0。
生成码现在是 `(void*)&(_vb6_with_776->szText[0])`。
改成按方括号深度判定：括号外仍只允许 标识符/`.`/`->`，括号内是下标表达式（右值）整体放行，
这样 `buf(i - 1)` 这类带算术的下标也不会再退化（x64 下 `a[i-1]` 此前同样会走强转兜底）。
全工程度量：`grep -oE "\(void\*\)\(intptr_t\)\([A-Za-z_][A-Za-z0-9_.>-]*\[[0-9]"` 在生成码里
**只有这一处**，所以这次修改的爆炸半径是一个点。
新增回归用例 `tests/test_asany_subscript.bas`（`Add-BasTest` 断言
`WITH-SUB=Y / EXPR-SUB=Y / SCALAR=Y / CHAIN=Y / ASANY-DONE`）。
写用例时踩到两个坑，都是"看起来是编译器坏了"：① `StrPtr` 给的是 **UTF-16** 字节，
拷进 `Byte` 数组后奇数字节是 0，断言要按实际字节写；② `Byte` 数组元素**直接**进 `If` 比较会走
Variant 通道（`vb6_VariantFromValue` + `vb6_VarCmpLongEq`）并且**不成立**，中间转一次 `Long` 就对
（那是另一处缺陷，与本用例无关，已在用例注释里点名，未动）。

**Fix 191（左键点击崩）= `As LongPtr` 数组按 Variant 载体分配，把伪 vtable 打成一堆 VARIANT 头。**
链：`WM_SETFOCUS` → `vb6_VBFlexGrid_WindowProcControl` → `vb6_VTableHandle_ActivateIPAO(me)` →
`VTableHandle.c` 的 COM 调用 → `vb6_getDispid` → `pDisp->lpVtbl->GetIDsOfNames`（`vb6com_invoke.c`）。
`Private VTableIPAO(0 To 9) As LongPtr` 是**手写伪 COM vtable**：`ProcPtr(AddressOf IOleIPAO_*)` 逐个
填进数组，再把 `VarPtr(VTableIPAO(0))` 当 vtable 指针装进 `VTableIPAOData.VTable`，于是
`&VTableIPAOData` 就是一个合法 COM 对象。`cgen_expr_array.cpp` 的 `mapSaElemType`/`mapSaElemCType`
**没有 `Vb6Type::LongPtr` 分支**（`resolveTypeName` 是对的，`builtinTypes_` 里 `longptr` 早在 Fix 081e
就有），落到 `default` → `vb6_sa_variant` / `VB6_SA_AT(vb6_VARIANT, …)`。步长从 8 变 16，槽内容从
指针变成 `vb6_VARIANT` 头，于是"vtable 第 5 槽"读出来是 `vt = VT_I4 = 3` → `3 + 0x28` →
**AV 读 0x2b**（与轨迹里的 `av read target=0x2b` 精确对上）。已改为 `vb6_sa_ptr` + `intptr_t`。

**同一条链上还有第二、三处，都在运行时补齐了口径**（三处是依次暴露的，不是一次看全的）：
1. `obj.AddRef` / `obj.Release` 现在**直发 vtable 槽 1**。IUnknown 三法在**所有** COM 接口的 vtable
   里固定在槽 0/1/2，自定义接口（`IOleInplaceActiveObject` 这类没有 IDispatch 的）同样成立；
   而按名后期绑定要先 `GetIDsOfNames`（IDispatch 槽 5），对伪 vtable 就等于把
   `IOleIPAO_TranslateAccelerator` 当 `GetIDsOfNames` 调，参数全错位（实测 `av read target=0xffffffffffffffff`）。
2. `vb6_getDispid` 前加 `vb6_ComIsDispatchable(disp)` 守卫：接收者不是真 COM 对象时返回
   `DISPID_UNKNOWN`，走已有的"method not found → 返回 NULL"quiet 路径。这类 Sub 在 VB6 里全程
   `On Error GoTo CATCH_EXCEPTION`，所以静默失败**就是**VB6 语义，而一次野 `lpVtbl` 解引用是杀进程。
   判据踩过一次坑：先写成"vtable 必须落在已加载模块镜像里"（`GetModuleHandleExW`
   `FROM_ADDRESS`），**错的** —— 伪 vtable 的载体是堆上的 `LongPtr` 数组，那条规则把合法对象一起
   拒了。改成：对象可读 → 首槽可读且容得下前 7 槽 → 槽 0/1/2/5/6 指向**可执行内存**
   （`VirtualQuery` 的 `PAGE_EXECUTE*`）。
3. `vb6_ReleaseObject`（每条 `Set X = Nothing` 都走它）用同一个守卫；不 dispatchable 就只清指针。
   触发点是 `DeActivateIPAO` 结尾的 `Set VTableIPAOData.OriginalIOleIPAO = Nothing` —— 该字段被
   `Set .OriginalIOleIPAO = This` 存成了类实例 `me`，而 `me` 的首字段 `__comObj` 为 NULL →
   读 `NULL + 0x10`（Release 槽 2）。**遗留**：`Set <接口字段> = Me` 应存 COM 身份
   （`me->__comObj`）而不是 `me`，这是代码生成侧的独立缺陷，本轮只在运行时兜住。

**门禁**：`.temp/gate.ps1 -Tag regress50` → **`PASS=93 FAIL=0 SKIP=1 TOTAL=94`**（基线 92/0/1/93，
+1 即新增的 `test_asany_subscript`；SKIP 仍是 `test_vbman`——`VBMANLIB.cVBMAN` 未注册），
`output/yqt_regress50.log`，GATE-EXIT=0，无 `fatal error C1060`，跑完无残留 C3/cl/link/ninja。
编号让位：任务 #33 原挂 "Fix 191"（`As Currency` 传 `POINT` 的 x64 编组）尚未落地，已改标 **Fix 192**，
191 归本轮的 LongPtr 载体 + COM 可派发守卫（代码注释与本节一致）。

**验收**：用户实测 hover 不崩、左键点击不崩（"可以了，没有崩溃了"）。

## 以下为合并自 origin/main 的并轨文档 (fan/dev 主文档之上追加保留)


# C3 编译器错误修复 — 任务交接文档

> 用途：新会话恢复上下文用。新开会话后直接说「读取 C3_FIX_HANDOFF.md 并继续修复」。
> 更新日期：2026-09-11（091a-092z：无窗体 125 模块 bisect 基线 65 → **7**、含窗体全量 129 条目 → **0**；提交 aeaa95f/05793ac/ae72bf2/d8f11aa/e97a90d/02205f5/ad62a09/96d123a/7e58595/ac78b3c/36d83a3/1c8daf7/db139c9/d5806d8/821e0ea/02e49a3/8a8eac2/866e814/288c4ea/a29240f/504e86b/aa98df1/5c5bf65/53da196/7757dee/d6a4dcf/8217f5f/ff133f4/f799e55/b6ff125/7559ad2/50de9b7/bad7b27/ac69fc0/e0df780/ec7fa10/4a03e23/97eb89c。**剩余 7 全部为窗体假阳性**（cLogs/cLayer 引用的 `.frm` 不在 bisect 编译集）→ 无窗体面已收敛。**092z 起新增「含窗体」窗口**：`-Forms` 开关 + 排除陈旧对象后，**含窗体全量（129 条目，含全部 4 个 `.frm`）error C = 0** —— 编译期全绿。**窗体 Release 崩溃「阻塞项」已解除**：根因并非未初始化/UB，而是 `.build` 的**陈旧对象**（`cmake --build .build --clean-first` 全量重建后该崩溃即消失）→ 不必再上 cdb 抓栈；下一步转入**链接期**（LNK2005 重定义 + 由源码 `Declare ... Lib "msvbvm60"` 生成的 `msvbvm60.lib` 引用；后者 x64 无库可补，须改道 RTL））

### 最近修复摘要（EXE 工程类 IDispatch 桥接：VBMAN_DEMO 启动 0xC0000005 → 通过；提交 3dd3689/17f349a/df42abc/3b797f9，2026-09-21）

- **现象与根因**：`VBMAN_DEMO.exe` 启动即崩 `0xC0000005` —— demo 侧 `New bHello`（EXE 工程类, MultiUse）产出**裸 C 结构体指针**塞进 `VT_DISPATCH` VARIANT，传给 VBMAN.dll 后 `Set Controllers(...) = Controller`（`cHttpServerRouter.cls:63`）走 AddRef，把结构体首字段当 vtable → `callq *0x8(%rax)` → AV。**本质**：C3 的 IDispatch 生成原被 8 处 `isDll_` 守卫锁在 ActiveX DLL 侧，EXE 工程类实例无 `__comObj` 首字段、无 coclass 表、`vb6comserver*.c` 不链入。
- **阶段 1（3dd3689/17f349a）解锁生成 + 链入**：`__comObj` 无条件生成、`_New()` 初始化 `__comObj = NULL`、类字段访问器放开 EXE、`generateDllEntry` 增第三参 `includeDllExports`（导出函数段含 DllMain 包进守卫、`g_vb6_coclassCount` 留 if 外）、EXE 平行生成 `com_entry.c`（progId 链 projectBaseName_ → options.dllProgId → "VB6EXE"）、vb6comserver 5 个 .c 无条件链入（com_entry.c 仅 `!msvcOpts.isDll`）。
- **阶段 3（df42abc）打包点接入 —— 真正止血**：`comPackExpr` 命中本工程类 `New <类>` 实参时改走按类函数 `vb6_ComPack_<类>` → RTL `vb6_ComPackVB6InstanceRaw`（FindCoClassDesc → `ComObject_FromInstance` → `VT_DISPATCH` VARIANT）；`FromInstance` 给出的 1 个引用直接转移给 VARIANT（不额外 AddRef，否则包装器与实例永不释放）；类未进 coclass 表（Private / 非 MultiUse|SingleUse）返回 NULL（等效 Nothing）。**有意不动**：`visit(NewExpr)` 输出（裸 `vb6_cls_X_New()` 留给本工程内部早绑定）、`inferExprType` 的 NewExpr case、FireEvent 广播 —— 收窄原则，泛用实参包装改动曾致回归 30→81。
- **阶段 4（3b797f9）回归用例 + CI 目录统一**：新增 T2 执行断言用例 `tests_github/t2_cases/test_exe_com_bridge/`（EXE 工程 + MultiUse 工程类 + `Scripting.Dictionary` 当对端，不依赖 VBMAN.dll；断言 `PING-OK` / `EXEB:2/2` / `EXE-COM-BRIDGE PASSED`），`run_t2.ps1` 清单 7 → 8（含 `-ne 8` 门禁）；`ci_vbman.yml` 20 处 `tests/` → `tests_github/`（vbman 与 vbman-demo 保持平级，demo 的 `..\..\..\vbman\dist\DLL` 相对引用不受影响）。
- **A/B 证据**：修复版编译运行 exit=0 且 8 行输出全中；把 `comPackExpr` 的 NewExpr 分支临时置 `false` 重编 C3 → **同一用例在 `d.Add "x", New bHello` 处段错误**（exit=139 ≈ `0xC0000005`），与 VBMAN_DEMO 现场一致；还原后重编复跑全绿。全量 `--emit-c` 回归 **151 个 .bas 全通过**（唯二失败 `Charts 2020/ucTreeMaps/Proyecto1.vbp` 是 `.pag` 不被 parser 支持的既有问题）。
- **已知限制（未覆盖）**：`Set h = New bHello`（赋给 `As Object` 变量）后再传**变量**仍 AV —— `comPackExpr` 只识别字面 `New` 表达式，变量路径走通用 `vb6_ComPackObject`（对裸实例直接 AddRef）；VBMAN_DEMO 用的是字面 `New`，不影响止血目标。继续收口需在「Set 变量 = New 工程类」赋值点也接 `FromInstance`。
- **风险备注**：`__comObj` 成为 EXE 类结构体首字段属**全量布局变更**，静态论证零影响 + 全量回归确认；EXE 不跑 TypeLib 构建，CLSID/IID 走确定性哈希 fallback。

### 最近修复摘要（092r-z：无窗体 bisect 基线 16 → **7**、含窗体全量 → **0**；提交 f799e55/b6ff125/7559ad2/50de9b7/bad7b27/ac69fc0/e0df780/ec7fa10/4a03e23/97eb89c，2026-09-11）

- **092r（16→15）Debug.Assert 条件的顶层 `=` 不再被当赋值**。`Debug.Assert (a And &HFF&) = (&H201 And &HFF&)`：语句级 `parseExpression(9)` 已吃掉 `Debug.Assert (<cond>)`，随后的 `=` 被当赋值 → `vb6_DebugAssert(<cond>) = <rhs>` C2186（ToolsTlsThunks 4733）。修法：在赋值检测前识别 `IndexOrCallExpr(callee = Debug.Assert, 1 个实参)` + 当前 token 是 `=`，把 `=` 右侧并回断言条件（`BinaryExpr(Eq)` 作为唯一实参）。
- **092s（15→14）UDT 数组成员元素类型 + VarPtr 左值宏白名单**。① `mapSaElemCType(UserDefinedType)` 退化 `vb6_VARIANT` → `uOutput.Buffer(i)`（`Buffer() As CERT_RDN_ATTR`）元素大小/取址全错（cTlsSocket 4086/4093，其中 4086 是**静默语义错**）；改用 `vb6_type_<UDT>`（`mi.typeRefName`），覆盖 MemberAccess 链与 With 两条生成点。② `VarPtr(uOutput.Buffer(0))` 生成 `(intptr_t)&(void*){VB6_SA_AT(...)}` C2440 — `VB6_SA_AT`/`VB6_SA_ND_ATn` 展开是**左值**，加入白名单走 `&(...)`（`vb6_VariantArrayGet` 按值返回，不入白名单）。
- **092t（14→12）TypeName(Me) 折叠为类名字面量**。`Me` 在 C 侧是 `vb6_cls_X*`，`vb6_TypeName` 形参是 `vb6_VARIANT` → C2440（pvSubClass 543/544）。类名编译期已知（vbp 的 `Class=` 名 == `moduleName_`）→ 生成 `vb6_BSTR_FromStr(L"pvSubClass")`；这也是语义上唯一正确形态（经 Variant 包装对象只会得到 `"Object"`）。
- **092u（12→11）类内参数化 Property Let 赋值**。`Extend(Array(A, C)) = Split(...)`（cToolsArray.cls 97，隐式 me）此前落到通用路径：`emitExpr(target)` 生成裸名 + pad 的 value 占位 `Extend(&_arr_1, &(vb6_VARIANT){0})`，再拼 ` = <RHS>` → C2106。修法：AssignmentStmt 中识别「target 是 `Ident(args...)`、该名是**当前类**的写方向属性、形参数 == 实参数+1」→ 生成 `vb6_<Cls>_prop_let_<Name>((void*)me, &Vars, &Value)`（ByRef Variant 落栈变量取址；非左值 ByRef 也落栈变量以免 C2102）。**收窄条件**：含 Optional 形参时不走此路径（Optional 在 C 侧另有存在标志参数，cAsyncSocket 2611 曾因此 C2198）。
- **092v（无回归）模块限定调用前缀以限定模块名为准**。`Mod.Member(...)` 的 `sourceMod` 原取 `memSym->sourceModule`；当限定名是模块而全局存在同名过程时会取错模块（命中 `cVBMAN.Version` 而非 `Common.Version`）。改为优先用「限定名对应的 external 符号所属模块规范名」。注意 cHttpServerResponse 452 的根因是 `Common`（外部 ActiveX 引用的模块）**不在任何 vbp 内**，符号表无该模块 → 属配置/缺口（见下）。
- **092w（11→9）链式默认属性访问 `X(a)(b)` 走后期绑定**。Demo_Database 506 `TestDB.Rows(1)("score")`：外层 `("score")` 的实参被并入内层 `prop_get_Item` 调用（3 实参 vs 签名 2）→ C2197 + C2440。修法（cgen_expr.cpp `IndexOrCallExpr` 拆链 Fix 086 分支）：① 内层 `_prop_get_` 调用已自带实参（`classMethodObjArg` 顶层段数 ≥ 2，即 this + ≥1 实参）时判定内层自足，外层索引改走返回对象的默认成员 `Item` 后期绑定；② 内层函数在 `variantReturnFuncs_` 中（返回 `vb6_VARIANT`，如 `cCollection.prop_get_Item`）时先经 `vb6_VariantToObjectVal` 提取对象指针再传 `vb6_ComCall`（否则 Variant 直传 void* C2440）；③ `wrapToBSTR` 增 `vb6_VariantFromComResult(` 前缀识别（须在 `vb6_BSTR` 子串检查之前，实参可能含 `vb6_BSTR_FromStr`）。
- **092x（9→8）COM 接口字段链式成员访问走后期绑定**。cHttpServer 821 `Request.Header.Exists("Cookie")`：`Header As Dictionary` 是 COM 接口字段，生成 `Request->Header->Exists(...)` 结构体成员访问 → C2037（`vb6_ComIface_IDictionary` 无结构定义）。根因：内层 MAE 值上下文字段访问仅查 `classVoidFieldMap_` 决定是否加 `voidptr` 标记，而 driver 预扫描把 COM 接口字段记入 `classTypedFieldMap_`（值前缀 `COM:<Interface>`）且**不入** `classVoidFieldMap_` → 无标记 → 外层 MAE Fix 088b `inferClassTypeOfExpr` 遇 `COM:` 前缀返回空串 → 落入 `obj->member`。修法：内层 MAE 追加 `voidptr` 标记时同时查 `classTypedFieldMap_` 的 `COM:` 前缀条目 → 外层 Fix 023 识别 → 生成 `vb6_ComCall(Request->Header, L"Exists", ...)`，与 `cSSE.c` `RootItem.Exists` 既有形态一致（注：`vb6_ComCall(...) == (-1)` 比较指针属**既有**架构形态，非本修复引入）。
- **092y（8→7）模块限定调用前缀兜底取工程模块规范名**。cHttpServerResponse 440 `Common.Version()` 生成 `vb6_cVBMAN_Version(&(void*){0}, 0)`（缺 `me`）→ C2198。**根因（修正 092v 的判读）**：`Common` **不是**外部 ActiveX 引用，它就是 `VBMAN.vbp` 第 65 行的标准模块（`Module=Common; Common\Common4DLL.bas`，`Public Function Version(Optional HostApp As Object)`）。真实根因是 `storageKey(Function) == lowerName` → `Common.Version`/`Common.Path` 与 `cVBMAN.Version`/`cVBMAN.Path` **四个成员两两同键**，`defineExternal` 先到先得 → Common 的成员**全部**被丢弃 → 消费模块作用域内不存在任何 `sourceModule=="Common"` 的 external 符号 → 092v 的 external 扫描落空 → `sourceMod` 退回 `memSym->sourceModule`("cVBMAN")。修法（cgen_expr.cpp 模块限定调用前缀判定）：092v 扫描失败时**兜底查 `externalModules_`**（工程内除本模块外的全部模块名、规范大小写）→ 命中且 `symTab_.lookup(extMod)` 非 `SymbolKind::Class` 时采用其规范名（与既有 3 处 `toLower(extMod)` 比对惯例一致）。仅对非类模块生效，不改 `类名.成员` / `窗体名.成员` 语义。结果 `vb6_Common_Version(&(void*){0}, 0)` 与签名 `BSTR(void**, int)` 匹配。
- **092z（含窗体全量 1→0）内置全局对象成员不再走同名项目成员形参兜底**。`FLogs.frm` 175 `Clipboard.SetText Text1.Text` 生成 `vb6_Clipboard_SetText(vb6_VariantFromValue(vb6_GetControlText(vb6_hwnd_Text1)))` → C2440（`vb6_VARIANT`→`BSTR`）。**根因**：`cQRcode.cls:34` 定义 `Public Function SetText(ByVal Content As Variant)`，IndexOrCallExpr 的 **class-unaware 兜底** `lookupModule("SetText")` 命中该 external 符号 → `calleeParams` 被填成 `[ByVal Variant]` → Fix 086 / Fix 024P2 把实参包装成 Variant；但 callee 早已由 `Clipboard` 硬编码分支解析为 RTL 函数 `vb6_Clipboard_SetText(BSTR)`，**根本不存在 VB 侧形参表**。修法：对象为 VB6 保留全局对象（`clipboard/screen/printer/forms/debug/err/app`）时跳过该兜底，`calleeParams` 保持为空。**教训：与 092y 同族 —— 「名字撞车」是本地最高频根因**（092y = `storageKey(Function)==lowerName` 使 `Common.Version` 与 `cVBMAN.Version` 撞键；092z = 跨类同名 `SetText` 撞车）。
- **窗体 Release 崩溃「阻塞项」解除（重大纠正）**：此前 092v 记录的「release 独崩 0xC0000005、最小批次 38 = FLayer.frm、疑似未初始化/UB、建议上 cdb」**结论有误**。实测：`.build` 增量构建的二进制崩，而 ① `cmake --build .build --clean-first` 全量重建后**含窗体全量直接跑到 0 错**；② RelWithDebInfo（`/Ob1`）与 Release+PDB（`/O2 /Ob2 /Zi`）都不崩 —— 即崩因是**陈旧对象**（本文件「注意事项」第 344/385 行早有同类记录：增量构建产生陈旧对象 → 运行期 0xC0000409，clean-first 解决）。**因此不必再上 cdb**，含窗体回归可直接作为常规验证面。另：`--dump-frm` 单测 `FLayer.frm` 正常（exit 0）→ 窗体**解析**阶段无问题，崩点在其后。

### 历史：092m-q（无窗体 bisect 基线 23 → **16**；提交 53da196/7757dee/d6a4dcf/8217f5f/ff133f4，2026-09-10）

- **092m（23→22）With 类字段写消费 COM 标记 + 类字段类型表**。`With CI` 内 `.IP = m_oServer.RemoteHostIP` 走「数据字段写」路径但**不消费** `isComMarker_` → `lastExpr_` 只含对象（`.IP = me->m_oServer`，静默丢属性）+ 标记泄漏到下一条语句（`.ConnectAt = ... L"RemotePort"` C2440，cHttpServer 372/373/374）。修法：字段写路径调 `resolveComValue(hint)`，hint 取自新增 `Symbol::memberFieldTypes`（语义分析登记类字段 VB 类型 + driver 跨模块拷贝）→ String→`ComGetStringProp` / Long→`ComGetIntProp` / Date→`ComGetDoubleProp`。
- **092n（22→21）For 子表达式独立消费 COM 标记**。`For i = .Count To 1 Step -1`：start 只得到对象本身，且**残留标记被 Step 表达式里 UnaryExpr 的 `if (isComMarker_) resolveComValue()` 消费** → `i_step = (-vb6_ComGetStringProp(_vb6_with_2, L"Count"))` C2171（pvSubClass 708/709）。改为每个子表达式生成前清标记、生成后按 Long 消费。
- **092o（21→20）With 目标表达式在外层上下文中生成**。`With .ReturnJson()`（嵌套 With）：`withObjectInfoStack_.push_back(withInfo)` 发生在 `emitExpr(*node.object)` **之前**，生成目标表达式时栈顶已是**内层** info（className=cJson）→ `.ReturnJson`（实为 cHttpClient 成员）解析失败退化为数据字段（cAliyunCaptcha 222 C2039 + 实参丢失）。改为目标表达式生成后再入栈（BuiltinObject 分支单独入栈）。
- **092p（20→18）类字段名规范化回声明名**。VB6 大小写不敏感：源码 `Client.socket.SendData`（小写）直接 `cIdent` → `me->Client->socket`，而 C 结构体成员名按声明是 `Socket` → C2039 + C2198（cHttpServerResponse 508）。新增 `Symbol::memberFieldNames`（lower → 声明原名）+ `canonicalClassFieldName()`，用于类字段访问与 With 字段写生成点。
- **092q（18→16）For Each 遍历 ParamArray 用 PA 专用 API**。ParamArray 形参 C 类型是 `SAFEARRAY*`，而 `vb6_LBound/vb6_UBound/VB6_SA_AT` 都要求 `vb6_SafeArray1D*`（`(arr)->data` / `->lBound`）→ C2039 ×2 且上下界取值语义错（ToolsTlsThunks 5468，`mdTlsThunks.bas` 的 `For Each vElem In a`）。改用 `vb6_PA_LBound`/`vb6_PA_UBound`/`vb6_PA_GetVariant`；Windows `VARIANT` 需经 `vb6_VariantFromComResult(&tmp)` 转 `vb6_VARIANT`（首版直接赋值触发 C2440）。

### 历史：092f-l（无窗体 bisect 基线 32 → **23**；提交 866e814/288c4ea/a29240f/504e86b/aa98df1/5c5bf65，2026-09-10）

- **092f（32→31）返回变量赋值按返回 C 类型提取 Variant**。`Function ReturnBody() As Byte()` 内 `ReturnBody = Inst.ResponseBody` → `vb6_ret_ReturnBody = vb6_VariantFromComResult(...)` C2440（cHttpClient 399）。`inferUdtFieldVb6Type` 对 `vb6_ret_X` 返回 Unknown、返回变量不落入任何目标类型判定 → 在赋值提取路径补「目标是当前返回变量 → 按 `currentReturnCType_`（SafeArray1D*/BSTR/void*/double/整型）提取」。
- **092g（31→30）字面量数组实参跳过 Variant 数组提取**。`Join(Array(a,b,c), " ")` 的 `Array(...)` 生成为匿名 `_arr_0`（C 侧已是 `vb6_SafeArray1D*`），却被按「Variant 数组形参」提取 → `vb6_Join(vb6_VariantToSafeArray1D(_arr_0))` C2440（cPLI 56）。形参为数组且实参前缀 `_arr_` 时跳过提取。
- **092h（尝试，已回退）TypeName/VarType 实参包装**。`TypeName(Me)`（pvSubClass 543/544）需 `vb6_VariantFromValue(me)`，但内建调用不经过该实参分支，改动无效；且**泛用** `getRuntimeParamCType(...) == "vb6_VARIANT"` 会把 `CStr(Index)` 提前包成 `vb6_CStrLong(vb6_VariantFromValue(Index))`（bisect 30→**81**）。**教训：该表的 "vb6_VARIANT" 语义是「该函数可接受 Variant 实参」，不能反用为「实参需要包装成 Variant」。** 已 `git checkout` 回退。
- **092i（30→28）String + BSTR 返回调用视为拼接**。源码 `GB_UrlDecode + Chr$(d)`（`GB_UrlDecode As String`）—— `inferExprType(Add)` 因右侧是内建调用而推不出 String → 退化为算术 `+` → `(GB_UrlDecode + vb6_Chr(d))` 触发 C2110 指针相加 + C2198（cToolsHttp 280）。左操作数为 String 且右侧是 BSTR 返回调用（`vb6_Chr(`/`vb6_Mid(`/`vb6_BSTR_` 等白名单）时按拼接处理。
- **092j（28→27）With 对象引用为 Variant 值**。`With me->LangInfo.Item("LangList").Item(Idx+1)` —— `inferClassTypeOfExpr` 非空（按声明推断为项目类）走 `(void*)` 硬转兜底，但 C 级表达式是 `vb6_VariantFromComResult(...)` → C2440（cLang 144）。兜底分支补 `cExprIsVariant(lastExpr_)` 判定 → 改用 `vb6_VariantToObjectVal(...)`。
- **092k（27→24）With 多级成员路径逐段展开**。`Erase .MessBuffer.Data` 经 `cIdent("MessBuffer.Data")` 把 `.` 变 `_` → `_vb6_with_N->MessBuffer_Data` C2039（ToolsTlsThunks 1780，同时 C2198）。`resolveArrayTargetIdent` 的 With 分支改为逐段展开（首段 `->`，后续段保留 `.`）。
- **092l（24→23）NULL 实参改用复合字面量取址**。`FireOnCertificate(me, NULL)` 的 ByRef 形参取址生成 `&NULL` → C2101（cTlsSocket 440）。`NULL`/`nullptr` 与 `isConstIdent` 同等处理，按形参 C 类型生成 `(&(<cType>){0})`。

### 历史：092a-e（无窗体 bisect 基线 36 → **32**；提交 d5806d8/821e0ea/02e49a3/8a8eac2，2026-09-10）

- **092a（36→35）BSTR 形参提取跳过条件：子串 → 顶层**。`paramBase == String` 分支原用 `argVal.find("vb6_BSTR")` 子串判定"已是 BSTR"→ `StrConv(LoadResData("AES.CBC","JSCRIPT"), 64, 0)` 的实参内部含 `vb6_BSTR_FromStr(` → 整体跳过提取 → C2440（cAesCBC 25）。改为 4 项**顶层前缀**白名单（`vb6_BSTR_` / `VB6_SA_AT(BSTR,` / `vb6_VariantToString(` / `vb6_BSTR_FromStr(`），与 5080 处同源。
- **092b（35→34）Object/COM 变量的属性写不再命中他类 Property Let**。`Rs As Object` 的 `Rs.Filter = <COM 属性>` 被全局 `lookupModuleByKind("Filter", PropertyLet)` 命中 cDialog.Filter，生成 `vb6_cDialog_prop_let_Filter(Rs, ...)`（C2440 + 丢 COM 语义）。在 cgen_stmt 的类属性写路径加 `objClass.empty()` 时的 Object/COM 变量判定（`knownObjectVars_` / `knownTypedComVars_`）→ 跳过，交回 COM 写路径（收窄 Fix 084g-2 的"isExternal 即生成"）。
- **092c（34→33）RHS 为 Variant 返回变量**。`Function ShowOpen(...) As Variant` 内 `mData.mstrFileName = ShowOpen` 的 RHS 生成 `vb6_ret_ShowOpen`（裸返回变量名）—— `cExprIsVariant` 只认函数前缀、`knownVariantVars_` 只认 VB 名 → 未识别为 Variant → 不做提取（cDialog 393 C2440）。在赋值通用提取路径加"`value == currentReturnVar_ && currentReturnCType_ == "vb6_VARIANT"`"判定。
- **092e（33→32）ByRef 数组形参的复合字面量类型**。`Decode(ByRef Utf() As Byte)` 的 C 形参是 `vb6_SafeArray1D**`（Fix 078 rev2），但调用点 `mapType(Byte|Array)` 给出 `uint8_t*` → `(&(uint8_t*){Variant})` C2440（cHttpClient 418）。ByRef 数组形参统一按 `vb6_SafeArray1D**` 生成复合字面量，并在 Variant 提取映射中补 `vb6_SafeArray1D** → vb6_VariantToSafeArray1D`。
- **重要判读：cLogs/cLayer 的 7 个 C2065/C2198 是无窗体 bisect 假阳性**（`FLogs`/`FLayer` 是 `.frm` 窗体：`vb6_FLogs_Visible`、`vb6_cCsv_ShowTo(&Content, ...)` 漏 me 等），窗体模块本就不在 bisect 编译集，不应作为修复目标。
- **两个窗口的残留**：① **无窗体 125 = 7**（C2065×5 + C2198×2，092y 后已无真实代码缺陷）—— 7 个**全部**是 cLogs/cLayer 的窗体假阳性：cLogs×5（`vb6_FLogs_Visible` 未声明 ×3、`FLogs` 未声明 ×1、`vb6_cTimeUse_Show` 参数少 ×1）、cLayer×2（`FLayer` 未声明、`vb6_cCsv_ShowTo` 参数少）。二者引用的 `FLogs`/`FLayer` 都是 `.frm` 窗体，**窗体模块不在无窗体 bisect 编译集内** → 无窗体面已收敛，这 7 错**不必再修**（纳入窗体后自动消失）。② **含窗体全量 129 = 0 error C**（2026-09-11，092z 后）→ **编译期全绿**；继续推进应转向**链接期**：
  1) ~~**Demo_Database 506（C2440 + C2197）**：`TestDB.Rows(1)("score")` 连续索引（默认属性链）~~ → **已修（092w）**：外层索引不再并入内层 `prop_get_Item`，改为对返回 Variant 结果做 `vb6_ComCall(vb6_VariantToObjectVal(<inner>), L"Item", ...)` 后期绑定；
  2) ~~**cHttpServer 821（C2037）**：`Request->Header->Exists(...)` — `Header` 是 COM 接口字段，成员方法调用应走后期绑定~~ → **已修（092x）**：内层 MAE 追加 `voidptr` 标记时同时识别 `classTypedFieldMap_` 的 `COM:<Interface>` 条目 → 外层走 COM dispatch；
  3) ~~**cHttpServerResponse 452（C2198，原判为「配置缺口」）**：`Common.Version()`~~ → **已修（092y）**，且**原判读有误**：`Common` 并非外部 ActiveX 引用，而是 `VBMAN.vbp` 第 65 行的标准模块 `Module=Common; Common\Common4DLL.bas`；真实根因是 `storageKey(Function)==lowerName` 造成 `Common.{Version,Path}` 与 `cVBMAN.{Version,Path}` 全部同键、`defineExternal` 先到先得丢弃 Common 成员 → 092v 扫描落空。故**无需** Reference/TypeLib 级命名空间符号（`SymbolKind::ComModule`/`ComGlobalNs` 导入仍可作为独立可选项，但已不再是本错误的必要条件）；
  4) **方法学提醒**：① 内建调用实参路径 ≠ 用户函数实参路径（092h 未生效即此因），先在 `typeNameMap` 等**内建识别分支**定位；② VB6 大小写不敏感 — 所有 `cIdent(成员名)` 都需按声明规范化，092p 只覆盖了 3 个生成点，其余 6 处 `->" + cIdent(node.memberName)` 仍是同类隐患；③ `isComMarker_` 是全局状态，语句/子表达式边界必须显式消费或清除（092m/n 已覆盖 With 字段写与 For，If/While/Select/Call 实参等边界待查）；④ 泛用的实参包装改动极易引发大面积回归（092h：30→81），务必先用小集合验证并用 `ParameterInfo` 上下文收窄；⑤ 改生成器前先加**一次性诊断打印**（`fprintf(stderr, "[DBG...]")` + bisect + grep `run.log`）—— 092o/092u/092v 都靠它一次定位路径，比反复读代码快得多；⑥ **判断某生成形态是否「既有可接受」时，先在生成产物目录（temp 的 `C3C\<pid>\*.c`，非仓库内、不受 .gitignore 影响）用 `search_content` 搜同类形态**——092w/092x 都靠搜到 `cSSE.c` 现有 `RootItem.Exists(...)`/`vb6_ComCall(...) == (-1)` 快速确认与既有行为一致、无需再扩大改动面。注意 `run.log`（C3.exe 输出）与 `c3-error.log`（cl.exe 错误）分属两个文件；⑦ **交接文档里的「根因」结论也要复核** —— 092v 把 cHttpServerResponse 452 记为「`Common` 不在任何 vbp 内的配置缺口」，实际它就写在 `VBMAN.vbp` 第 65 行；误判让该错误被搁置了整整一轮。凡「外部模块 / 配置缺口」类结论，归档前先用 `search_content` 在 `.vbp` 中核实条目，并优先怀疑「同名符号 storageKey 冲突 / 跨类同名成员」这类更常见的内部原因；⑧ **任何「崩溃」结论落档前必须先排除陈旧对象** —— 本次「窗体 Release 崩溃（0xC0000005）」经查只是 `.build` 增量构建的陈旧对象，`--clean-first` 全量重建后含窗体回归直接跑到 0 错；092v 却把它当成「未初始化/UB」并计划上 cdb。规则：**见到 release-only 崩溃，先 `--clean-first` 全量重建再复现**，仍崩才考虑 UB。

### 历史：091p-s 摘要（无窗体 bisect 基线 41 → **36**；提交 ac78b3c/36d83a3/1c8daf7/db139c9，2026-09-10）

- **091p（42→41）类字段 Variant 判定**：`Set m_vUserData = Value`（cWinsock Property Set，字段 `Private m_vUserData As Variant`）被误判为 typed 目标 → 生成 `vb6_VariantToObjectVal(Value)` C2440。根因同 091m：`knownVariantVars_` 每过程 `clear()`，类字段不在其中；但字段访问带明确 `me->` 前缀，故新增 `classVariantFields_`（类模块字段声明时登记，**不回灌裸名集合**），Set 判定中按 `target.rfind("me->",0)==0` 单独查询。
- **091q（41→40）ForEach COM 集合源**：`For Each x In me.LangInfo.Item("LangList")` 生成 `vb6_ForEach_Init(vb6_VariantToObjectVal(vb6_ComCall(...)))` C2440（void*→vb6_VARIANT）—— `vb6_ComCall/vb6_ComCallObject` 已返回对象指针，`isDefinitelyVariantExpr` 的 fallback 不该再提取。加 `collIsObjPtr091q` 前缀排除（两个分支都加）。
- **091r（40→38）Debug.Print Variant 兜底**：`Debug.Print x`（x 是 Variant 循环变量）落入整数分支生成 `vb6_DebugWriteLong((int32_t)(x))` → C2440 + C2198。Debug 参数判定原来只查 `cExprIsVariant`（C 表达式级），新增 `isVariantVal091r` 兜底 `knownVariantVars_` / `classVariantFields_`（含 `me->`/`(*p)` 剥离与 `/* */` 注释后缀处理）。
- **091s（38→36）ByVal Variant 收 Object 实参**：`ToolsJsonVba.ConvertToJson(Json)`（`Json As Object` → void*，形参 `ByVal JsonValue As Variant`）未打包 → C2440。Fix 084d 的跳过条件（`inferClassTypeOfExpr` 非空即跳过）过宽：① 收窄为**项目类**（`symTab_.lookup(类名)->kind == SymbolKind::Class`）；② 实参是 `knownObjectVars_/knownTypedComVars_` 的 Object/COM 变量时**强制打包**（`inferClassTypeOfExpr` 对 Object 变量可能返回内建 "Object" 类名）。
- **残留（36 = C2440×12 + C2039×6 + C2198×6 + C2065×5 + 其它 7）**，下一步候选（按族）：
  1) **少 me 的跨模块方法调用**（C2198×3）：`vb6_cLayer` 的 `vb6_cCsv_ShowTo(&Content, &(int32_t){800}, 0)`、`vb6_cTimeUse_Show(&(BSTR){...}, 0)`、`vb6_cVBMAN_Version(&(void*){0}, 0)` —— 调用点漏传对象指针 me（VB 里像 `cCsv.ShowTo ...` / `Me.Version` 这类限定写法），同时 cHttpServerResponse 508 把对象表达式错传为 `(void*)me->Client->socket`；
  2) **窗体/全局对象引用**（C2065×5）：cLogs 的 `FLogs` / `vb6_FLogs_Visible`、cLayer 的 `FLayer` —— 窗体变量名未解析（应走全局单例或 `me->` 字段）；
  3) **Variant → 具体类型提取仍缺**（C2440×6）：cDialog 393（`me->mData.mstrFileName = vb6_ret_ShowOpen`）、cAesCBC 25（`vb6_StrConv(vb6_LoadResData(...), 64, 0)`）、cToolsList 25（属性 Let 的 BSTR 形参，091n 未命中）、cHttpClient 399/418（函数返回 SafeArray1D*/uint8_t* ← `VariantFromComResult`）、Demo_Database 506（`cCollection_prop_get_Item` 参数个数）；
  4) **BSTR 拼接误用指针算术**（C2110+C2198）：cToolsHttp 280 `vb6_BSTR_Assign(&GB_UrlDecode, (GB_UrlDecode + vb6_Chr(d)))` → 应 `vb6_BSTR_Concat`；
  5) **RTL 签名不匹配**：ToolsTlsThunks 1780 `vb6_SafeArrayDestroy1D(_vb6_with_30->MessBuffer_Data)`（参数太少）＋ 1780/5468 的 C2039（`MessBuffer_Data`/`data`/`lBound` 成员不存在）；cTlsSocket 4093、cLang 144、cHttpServer 374、cHttpClient 418 的 void*/double 桥接。

### 历史：091k-o 摘要（无窗体 bisect 基线 52 → **42**；提交 e97a90d/02205f5/ad62a09/96d123a，2026-09-10）

- **091k（52→48）ParamArray/Variant 参数边界三处**：
  - 通用实参转换中实参是当前过程的 **ParamArray 参数**时不再按 Variant 数组提取（此前 `UBound(OutVars)` → `vb6_PA_UBound(vb6_VariantToSafeArray1D(OutVars))` C2440，cToolsArray DeArray）；
  - **ByRef Variant 参数索引免取址**：`v(i)` 曾生成 `vb6_VariantArrayGet(&v, i)`，而 ByRef Variant 形参的 C 类型已是 `vb6_VARIANT*` → 改传裸名（cToolsArray prop_let_Extend/DeArray）；
  - **IsMissing 语义/表项修正**：RTL `int32_t vb6_IsMissing(SAFEARRAY*)` 是 ParamArray 专用（判断是否未传实参），cgen 此前把任意 Variant 实参传给它 → 现：ParamArray 名 → `vb6_IsMissing(pa)`；其它复合表达式（数组/ParamArray 元素）→ 常量 `(0)`（VB6 IsMissing 仅对 Optional Variant 可能为真）。运行时表项 `vb6_VARIANT*` → `SAFEARRAY*`。
- **091l/091m（48→46）Variant 目标赋值包装 + 模块级 Variant 回灌**：赋值 `Variant ← 具体类型值`（`Array(...)` 物化的 `_arr_N` SafeArray1D* 等）补 `vb6_VariantFromValue`；判定依赖 `knownVariantVars_`，而该集合在每个过程开始时被 `clear()` → 模块级 `Dim x As Variant` 在过程体内不可见 → 新增 `moduleVariantVars_`（模块级声明时登记，过程开始回灌，**必须排除类模块/窗体成员**——否则裸名生成 C2065，曾 +7 回归）。
- **091n（46→44）属性 Let 值参按写方向末参类型适配**：`obj.Prop = value` 的 prop_let_ 调用此前完全不打包 → 用 091a 的 `findClassMemberWriteParams(类, 成员, isSet, out)` 取末参：Variant 形参 + **数组载体**值 → `packLetValueArg`；BSTR 形参 + Variant 值 → `wrapToBSTR`。**不能用裸 `propLetSym->params`**（`lookupModuleByKind` 全局同名命中他类属性 → 误打包 +36 C2440）；也不要 `Unknown` 类型兜底（同样 +36）；值形态未确认是数组载体时不打包（否则 COM 属性 `Dictionary.CompareMode = 1` 被误打包 → 大回归）。清 cWinsock 1949/1965。
- **091o（44→42）命名实参 ByVal 适配**：`Name:=expr` 路径此前只做 `applyByRef`（ByRef 处理），漏了 ByVal 形参类型适配 → `oListenSocket.Accept(m_oSocket, UseTls:=bUseTls)`（第 4 参 `Optional ByVal UseTls As Variant`）C2440 int16_t→vb6_VARIANT。类对象实参保持指针直传（同 Fix 084d）。
- **残留（42 = C2440×17 + C2198×7 + C2039×6 + C2065×5 + 其它 7）**，下一步候选：
  1) ForEach COM 源 `vb6_VariantToObjectVal(vb6_ComCall(...))` → 应为 `vb6_VariantFromComResult`（cLang 26）；
  2) `vb6_Join(vb6_VariantToSafeArray1D(_arr_N))` — 091g 物化的 `_arr_N` 临时未登记 knownArrays_（cPLI 56）；
  3) 类 Variant 字段 `me->f = <void* COM 结果>`（cWinsock 172、Demo 857/859）；
  4) COM 对象属性 Let 的 BSTR 提取（cToolsList 25，091n 未命中：写方向表对 COM 类的处理需确认）；
  5) `vb6_cDialog_prop_let_Filter(rs, vb6_VariantFromComResult(...))` 类归属（cDialog 目标 vs rs 实际类）；
  6) Debug 强转调用参数量（pvSubClass 543/544，C2198 族）。

### 历史：091e-i 摘要（58 → 52，2026-09-10）

### 最近修复摘要（091e-i：无窗体 bisect 基线 58 → **52**；提交 d8f11aa 等，2026-09-10）

- **091e（未观测到变化，保留）**：`rtParamTypeUsable = i >= calleeParams.size() || calleeParams[i].type 是 Variant/Empty`（此前仅参数表为空才查运行时参数类型表）+ 参数表补 `{"vb6_StrConv", {"BSTR","int32_t","int32_t"}}`。逻辑正确但单独验证 58→58。
- **091f/091g（58→55→55）ForEach 数组源**：`For Each` 源为返回 `vb6_SafeArray1D*` 的内置调用时走**数组迭代路径**而非 COM 路径（此前生成 `vb6_ForEach_Init(vb6_VariantToObjectVal(vb6_Split(...)))` → C2440）。白名单：`Split/Filter`（元素 BSTR，`Vb6Type::String`）、`Array(...)`（vb6_ArrayCreate 建 **Variant 数组**，元素 `vb6_VARIANT`）。表达式集合先物化到 `_fe_arrN` 临时变量，避免 LBound/UBound/`VB6_SA_AT` 处重复求值（重新分配数组）。清 ToolsTlsThunks 754/2913/3178。
- **091h（55→54）BSTR 判定前缀修正**：`bstrTopPrefixes` 移除 `"vb6_VariantFromComResult("` — 该函数返回 **vb6_VARIANT**（vb6rtl.h:918）而非 BSTR，旧前缀把 COM 属性结果误判为"已 BSTR"而跳过提取。清 cTlsSocket.c 200（`vb6_LenB(vb6_VariantFromComResult(ComGetProp(...)))`）。
- **091i（54→52）wrapToBSTR 识别项目 Variant 返回函数**：`wrapToBSTR()` 的"其他 vb6_ 函数假定为 BSTR"直通分支前，增加 `variantReturnFuncs_` 检查（driver 预扫描，含 `vb6_<cls>_prop_get_<name>`）→ 命中则 `vb6_VariantToString(...)`。清 Demo_Database 310/678（`vb6_BSTR_Concat(L"...", vb6_cDataBase_LastInsertId(...))`）。
- **残留（52 = C2440×26 + C2198×7 + C2039×6 + C2065×5 + 其它 8）**：
  1) **形参为按值 Variant 的运行时函数**（表项类型 `"vb6_VARIANT"`，如 `vb6_VariantToLong/ObjectVal/CStr/CBool/CByte/CDate/CCur`、`vb6_IIfVariant`、`vb6_CLngV/CIntV`）实参为具体类型时未包装：`vb6_VariantToObjectVal(vb6_ComCall(...))`（cLang 26 void*）、`vb6_PA_UBound(vb6_VariantToSafeArray1D(...))`（cToolsArray 147 SAFEARRAY*，注意 PA_UBound 若要数组变体需 `vb6_VariantFromValue` 无法产生数组变体语义）、`vb6_Join(vb6_VariantToSafeArray1D(...))`（cPLI 56）、`vb6_cTlsSocket_Accept(..., bUseTls, ...)`（cWinsock 1604 int16_t）、`cWinsock 1949/1965`（baBuffer Byte 数组 → ByVal Variant）、`pvSubClass 543/544`（cls*）、`cDelay 74`（SAFEARRAY*）。注意 `vb6_CLngV/CIntV` 有 5993-6010 的反向剥离后处理，包装改动需与其对齐。
  2) **赋值路径目标类型识别**：`EnumLevelNames = _arr_0`（ToolsLogs 44，Variant 目标 ← SafeArray1D*，疑 knownVariantVars_ 未注册）、`me->m_vUserData = vb6_VariantToObjectVal(Value)`（cWinsock 172）、cHttpClient 399/418、cLang 144、Demo 671、cTlsSocket 4093（Variant → 具体类型目标）。
  3) **UDT 嵌套字段**：`me->mData.mstrFileName = vb6_ret_ShowOpen`（cDialog 393，嵌套 UDT 成员类型推断未覆盖）。
  4) `Demo_Database 506`：`vb6_BSTR_Concat(L"...", vb6_cCollection_prop_get_Item(...))` — prop_get 返回 Variant 未被 091i 命中（需确认 cCollection.Item 的 returnType 与集合名是否一致）。
  5) cAesCBC 25 / cToolsList 25（`vb6_StrConv(LoadResData(...))`、`prop_let_Filter(rs, ComGetProp(...))`）、cToolsArray 98/107（`vb6_IsMissing(Variant 值)`，RTL 形参是 SAFEARRAY*）。


## 1. 项目与目标

- **C3**：VB6→C 转换器（工作区 `c:/Users/vi/Desktop/c3.vb6.pro`）
- **目标**：减少 `vbman/dist/c3-error.log` 中 MSVC 编译错误数
- **进度**：777 → 588 → 552 → 216 → 204 → 176 → 165 → 155 → 142 → 132 → 109 → 107* → 103* → **100***（090ad 清 Dictionary 2、090ae/090af 清 cCsv 4、090ag 清 ToolsJsonVba 3；`*`=全量回归被 C3 自身崩溃阻塞，100 为推断值，未全量跑通）
- **当前状态**：**cZipArchive 簇 error C = 0、Demo With/COM 链清零（090s/090t）**；090u/v/w/x/y/aa/ab/ac 降 23（132→109）；**090ad 修 Dictionary.key(OldKey)=NewKey 只写属性 LHS 多传被 pad 的 value 实参（C2197 x2，单模块 0 错，附带激活 090w Variant 末参打包死代码——substr(5)→substr(4) bug）**。残余错误分布极散：C2440 各类方向 30+ 处（每类 1-7 处）+「参数太少/太多」调用形态 16 处 + 成员不存在/SAFEARRAY 成员 10 处（含 cCsv `d(0).Root` C2039 + VariantToObjectVal 实参、cAliyunCaptcha With .ReturnJson() 字段函数化、cHttpServerResponse VBMAN.Version/->socket 等）+ cCollection prop_get_Item x1。下轮候选：见「残留错误族明细」节。
- **阻塞项（C3 自身崩溃）**：release `.build\C3.exe` 与 ASAN `.build-asan\C3.exe` 在全量 vbman 收尾/后段均崩溃（release 崩于 cVBMAN.c 生成前收尾、ASAN 崩点漂移 Dictionary 模块后 ↔ Form Picture1.Align/FToastDrawer 处理 → 内存损坏特征，无 ASAN report）。**2026-09-07 验证：ASAN clean-first 全量重建后仍在同位置崩（179733 行日志、Picture1.Align VB4001 后）→ 排除「陈旧对象」假说**（旧注意事项 2026-09-05 结论对此崩溃不适用，勿再浪费 clean 重建）；FToastDrawer 单 frm 裁剪不崩 → 需多模块组合才能复现，建议用「逐步追加模块 bisect」定位（从 VBMAN.vbp 头部 Class 依 vbp 顺序追加到 Form）。全量回归依赖修复该 bug（HANDOFF 090z TODO：With void* ClassInstance 0xC0000409；0x100EE/0xE2A72 偏移 map 解析命中 std::string 移动赋值，疑似未初始化读取/越界写）。修复前只能用 fix_tool 单模块/裁剪簇验证（注意 c3-error.log 现被 ASAN 崩溃前 31 文件快照污染，需 `git checkout -- vbman/dist/c3-error.log` 恢复 109 基线）。
- **快速验证**：修复 C3 后不要直接全量编 vbman（2-3 分钟），先跑 `scripts/fix_tool.ps1` 裁剪出含错误模块的最小 vbp 工程（约 5-7 秒/簇）复现/验证，最后才全量回归。
- **cluster 注意**：小工程缺依赖模块时部分跨模块代码分支不执行，可能零错但全量仍报（cCollection/cToolsStr 案例），须以全量回归为准。

## 2. 构建 / 验证工作流

1. `scripts/build.bat` — 重编 C3.exe
2. **快速迭代（推荐）**：`powershell -File scripts/fix_tool.ps1 -Modules <相对 vbman/src 路径>[,<更多>...]`
   - 从 VBMAN.vbp 裁剪只含指定模块的最小工程（产物 `vbman/src/FIX_<Name>.vbp` + `vbman/src/_fix/<Name>/out/c3-error.log`）
   - 注意：多模块参数需写成 wrapper .ps1（`& fix_tool.ps1 -Modules @(...)`）或单模块直传；错误复现数与全量一致（已验证 ToolsJsonVba 7/7、cZipArchive 4 模块簇 23/23）
3. 全量回归：`vbman/build_vbman.bat`（产物 `vbman/dist/c3-error.log`）
4. `scripts/_tmp_count.ps1` — 统计错误总数
5. **注意**：PowerShell 内联 `$_` 会被转义，复杂逻辑必须写进 .ps1 脚本文件再执行

## 2. 构建 / 验证工作流

1. `scripts/build.bat` — 重编 C3.exe
2. `vbman/build_vbman.bat` — 用新 C3.exe 重新生成 VBMAN 的 C 代码并触发 MSVC 编译
   - C3 生成成功时打印新临时目录（如 `113776838989600`），exit 1 是警告所致，属正常
   - C3 失败时自动写 `vbman/dist/c3-error.log`
3. `scripts/_tmp_count.ps1` — 统计错误总数
4. 辅助脚本（按需重写）：`scripts/_tmp_grep.ps1`（模式统计）、`scripts/_tmp_peek.ps1`（看生成代码）
5. **含窗体回归（092z 起）**：`powershell -File scripts/_tmp_bisect.ps1 -N 999 -Forms` — 纳入 `VBMAN.vbp` 的 `Form=` 条目（默认不纳入，行为不变），产物 `vbman/src/FIX_bisectfrm.vbp` + `_fix/bisect/outfrm/c3-error.log`。**当前含窗体全量基线 = 0 error C**
6. **改过 cgen/头文件后务必全量重建**：`cmake --build .build --clean-first`。增量构建会产生**陈旧对象** → release 二进制运行期崩溃（0xC0000005 / 0xC0000409），极易误判为 UB（本次「窗体 Release 崩溃」即此）
7. **崩溃栈定位**：`set C3_CRASH_TRACE=1` + 带 PDB 的构建（`.build-rwdi` / Debug）→ 崩溃时 stderr 输出 `[C3-CRASH]` 异常码/地址 + 符号化调用栈（dbghelp；`c3` 目标已链接 dbghelp）
8. **注意**：PowerShell 内联 `$_` 会被转义，复杂逻辑必须写进 .ps1 脚本文件再执行

## 3. 错误演进史

777 → 774 → 748 → 723（VBA 子集常量）→ 700（VBA 全量 + 枚举）→ 638（ReDim/Erase + 枚举）→ 594（C2102 常量取址修复）→ 589 → 588（C2099 静态初始化清零）→ 216（088e-089h）→ 204（089i/j/k）→ 176（090a/c/d/e + P25b）→ 165（090f/g）→ 155（090h-n）→ 142（090o-p）→ 132（090s/090t）→ 109（090u/v/w/x/y/aa/ab/ac）→ 107*（090ad）→ 103*（090ae/090af）→ **100***（090ag）

### 最近修复摘要（091a/091c/091d：无窗体 bisect 基线 65 → **58**；提交 aeaa95f / 05793ac / ae72bf2，2026-09-10）

- **091a（属性写方向签名表，65→63）**：`Symbol` 新增 `memberLetParams`/`memberSetParams`（Let/Set 各自唯一，无条件填充于 Pass1/Pass2；driver.cpp 跨模块拷贝）。新函数 `findClassMemberWriteParams(className, member, isSet, out)`（Phase A 模块作用域 Let/Set 符号 → Phase B Class 符号写方向表）。090w 的 C/D2 打包改用它 —— 修复 `memberParams` 读优先（Get > Func > Sub > Let > Set）对「Get 有参 + Let 末参才是 value」属性的遮蔽：`item$pl` 等跨模块 storageKey 冲突时 Phase A 找不到本类符号，旧逻辑回退 Get 的 `[key]` → 不打包 → C2440（Demo.c 766/786 `prop_let_Item(json, key, double)`，766/767/768 现均字段式打包 ✓）。
- **091c（ParamArray 元素赋值，63→61）**：`OutVars(i) = value` 生成 `vb6_PA_GetLong(OutVars, i) = ...` → 取值函数作左值 C2106 + Variant 值传具体类型 Set 形参 C2440。cgen_stmt 赋值路径新增 `vb6_PA_Get*` 目标识别：改写为 `vb6_PA_SetLong/Double/BSTR(args, <按类型提取的 value>)`（Variant 值 → `vb6_VariantToLong/ToDouble/ToString`）。清 cToolsArray.c 158/167。
- **091d（常量类型推断/整型折叠，61→58）**：
  (a) semantic_analyzer `LocalDeclStmt` 的 ConstDecl 分支此前被"简化"为 Variant，现直接复用 `registerConstant`（完整字面量/一元负号推导，define 到局部作用域）；
  (b) cgen_stmt 局部 ConstDecl 无 `As 类型` 时按字面量推断 C 类型（Integer/Long→int32_t、Single/Double→double、String→BSTR、Boolean→VBABOOL）—— 此前一律 `const vb6_VARIANT SW_SHOWNORMAL = 1;` → C2440 初始化（cToolsSystem.c 11/13）；
  (c) cgen_decl 模块级 ConstDecl 生成前先 `tryEvalConstInt` 折叠整型表达式 → `#define BIF_USENEWUI (80)`，避免 Variant 语义把字面量操作数包装成 `vb6_VariantToLong(64)` → C2440（cDialog.c 36，报错在使用处 532）。
- **未采用（已回退）091b**：对象属性赋值路径（cgen_stmt.cpp 578 `/* Property Let */`）值参打包。首版按 `propLetSym->params.back()` 无条件打包 → +36 C2440（`lookupModuleByKind` 全局同名属性命中错类）；改用 `findClassMemberWriteParams(objClass, ...)` 后回归消失，但 ByVal Variant 场景（cWinsock 1949/1965 `UserData Let(ByVal Value As Variant) ← baBuffer`）仍不生效：`inferExprType(baBuffer)` 与 `symTab_.lookup("baBuffer")` 均拿不到数组位（局部数组类型信息缺失）。且枚举类型（`Dictionary.CompareMode As CompareMethod`）在符号表被解析为 Variant 而 cgen 生成 int32_t 形参 → 打包即 C2440（Dictionary.c 9/27）。结论：该路径需要"实参 C 类型"与"符号表类型"一致性的前置条件，待后续。
- **残留错误族（58 = C2440×32 + C2198×7 + C2039×6 + C2065×5 + 其它 8）**：
  1) **X → Variant 实参/赋值包装缺失（约 14 处）**：`vb6_ForEach_Init(vb6_VariantToObjectVal(vb6_Split(...)))`（ToolsTlsThunks 754/2913/3182）、`vb6_ComCall` 结果直接传 VariantToObjectVal（cLang 26）、跨模块方法实参（cWinsock 1604 int16_t→Variant）、`EnumLevelNames = _arr_0`（ToolsLogs 44，SafeArray→Variant 目标，疑未注册 knownVariantVars_）、Set 属性对象→Variant 字段（cWinsock 172）、`vb6_PA_UBound(vb6_VariantToSafeArray1D(...))`（cToolsArray 147）、pvSubClass 543/544（cls*→Variant）。
  2) **Variant → 具体类型提取缺失（约 7 处）**：内置/运行时函数实参（cTlsSocket 200、cAesCBC 25、cToolsList 25、Demo_Database 310/506/678 均为 `Variant → BSTR`；cDialog 393 UDT 嵌套字段）、cHttpClient 399/418、cLang 144、cToolsArray 98/107、Demo 671。
  3) cToolsArray 130 `Extend(&_arr_1, &(vb6_VARIANT){0}) = vb6_Split(...)`（过程调用作赋值目标，C2106）、cHttpServer 374（void*→double）。

### 最近修复摘要（090w 修正+090h/090x：无窗体 bisect 基线 68 → **65**；提交 18c58eb / 087e203，2026-09-10）

- **090w 修正（cgen_util.cpp，ByRef Variant 值参字段式打包）**：旧实现 ByRef 分支生成 `(&(vb6_VARIANT){vb6_VariantFromValue(x)})` —— 结构体复合字面量不能用另一个结构值初始化，命中即 C2440「vb6_VARIANT→vb6_vartype」。激活后（substr(4) 修正使 classLower 匹配成功）cJson.Items 等案例爆 8 条此类新错。改为**字段式**（.vt=VT_BSTR/.bstrVal、VT_I4/.lVal、VT_UI1/.bVal、VT_R8/.dblVal、VT_BOOL/.boolVal，default 对齐 M22：变体表达式 vb6_VariantToString、其余按 BSTR 兜底），依据 `inferExprType(valueExpr)` 选字段。
- **090x（cgen.hpp/cgen_util.cpp helper `packLetValueArg`）**：把 090w 打包逻辑抽成共享 helper，供两处使用——(a) cgen_util Pattern C/D2（prop_get_ LHS rewrite 追加 value 实参）；(b) cgen_stmt.cpp With 块 prop_let_ 调用（090x-With：With 循环记录 PropertyLet 符号，末参 Variant 时对 lastExpr_ 打包 → 清 cHttpServer.c 913 `.Expires = DateAdd(...)` C2440 double→vb6_VARIANT*）。
- **090h（cgen_stmt.cpp，With 块类属性写识别）**：跨模块 storageKey 冲突（如 CookieAttr.Value 的 value$pl 被先注入的其它类同名属性覆盖）使 moduleScope 扫描漏 PropertyLet/Set → With 内 `.Value =` 误走数据字段写（`_vb6_with->Value`）→ C2039（CookieAttr 无 Value 字段）。补 findClassMemberCallParams（Class 符号 memberParams 精确到类）二次确认属性存在 → 生成 `vb6_cHttpServerCookieAttr_prop_let_Value(...)`。清 cHttpServer.c 908 C2039。
- **净效果**：C2440 41→39（清 Demo 592、cHttpServer 913，修 cJson.Items 系列回归为合法字段式）、C2039 7→6，无新增回归。总 68 → 65。
- **残留同类**：Demo 766/786（cJson.Item）与 cWinsock 1949 等——消费模块 moduleScope 中该类 PropertyLet 符号被跨模块同名($pl)冲突覆盖/缺失，Get 方向参数表（memberParams，Get>Func>Sub>Let>Set 单一 read 优先）无法给出 Let 末参签名 → 090w/090x 不打包仍 C2440。需语义层为属性写方向（Let/Set）提供完整签名（或跨模块符号按 sourceModule 多保留）——下一轮候选。
- **注意**：090x 命名与 9/7 handoff「090x Debug.Print」重复，本会话 090x = Let 值参打包 helper，勿混淆。

### 最近修复摘要（090bx/by：无窗体 125 模块基线 84 → **79**；提交 f3ed79c）

- **ToolsJs 源修复（vbman）**：`NewObj` 函数体内误写 `Set NewArr = MSSC.Eval("{};")`（复制粘贴笔误，应 `Set NewObj = ...`）→ 清 ToolsJs.c(41) C2106+C2198。VB6 中跨函数名作赋值左侧非法，属源码 bug 非 cgen 问题。
- **090bx（cgen_stmt.cpp）**：With 块 `void*` 目标恢复 COMObject 分发（090y 设计；090z 为定位 0xC0000409 崩溃临时还原 ClassInstance，现崩溃已定位到窗体主题）→ 清 Demo.c(509) `.Item("abc") = 123` C2106（此前 classInstance 撞名 cTimers.Item）。With 推断两处 `kind == Unknown && tempType == "void*"` → COMObject。
- **090by（cgen_expr.cpp + cgen_base.cpp）**：`resolveArrayElemType` 对 VB6 内置对象类型（Collection/Forms/ErrObject/App/Screen/Printer/Clipboard，含 VBA. 前缀）返回 Variant 但 C 字段是 void* → 误注册 classVariantMembers_ → 类内 `m_Col(i)` 生成 `vb6_VariantArrayGet(&me->m_Col, i)`（C2172）。修复：cgen_expr 6447 SimpleTypeRef 分支加 vb6BuiltinObjTypes 集合（与 semantic_analyzer.cpp:587 一致）→ Object；cgen_base.cpp 177 分支加防御性排除（非内置名且符号表无条目不注册）。清 C2172 x2（cHttpServer.c 62 / cHttpServerSvr.c 160 的 `m_DefaultDocumentAdditions(i)` Collection Item 调用）。
- **已知运行期缺陷（编译通过但语义错，未修）**：cHttpServer.c AddDefaultDocument 的 `For i = 1 To m_DefaultDocumentAdditions.count` 上界生成 `int32_t i_end = me->m_DefaultDocumentAdditions;`（.count 属性读被忽略）；正向 for 分支条件误用 ComCall("count") 包 Item 结果（d2 反向分支正确）→ 需 Collection `.count` 属性在整数上下文读（vb6_ComGetIntProp）与正向分支修正。

### HttpServer 簇（cHttpServer/cHttpServerResponse/cHttpServerCookies，约 20 错，多独立深机制）

当前 79 错分布：C2440 51 / C2198 8 / C2039 7 / C2065 5 / 单发 8 个码。HttpServer 簇含 5+ 独立机制，逐一深挖：
1. **cHttpServer.c 804 State403 C2198（090ak 曾声称清过但语句调用上下文仍漏）**：`Response.State403` 是 Call 语句（非值上下文），Fix 083d 只覆盖值上下文无括号类方法引用；语句调用 Optional Say 未补参（C 签名 BSTR* Say + int _has_Say）。同族：cHttpServerResponse.c 508 TlsReMaster.SendData C2198、cHttpServer.c 374/821/913、cookies 65/168 的 Variant/COM 值转换。
2. **cHttpServer.c 906 `Cookies.Cookie(x)` COM dispatch**：Cookies 是局部 `Dim As cHttpServerCookies`（项目类，早绑定应 vb6_cHttpServerCookies_prop_get_Cookie），生成却 `vb6_ComCall(Cookies, L"Cookie", ...)`——默认成员（VB_UserMemId=0）调用被 COM 化。致 908 `.Value =` C2039（With 对象 ClassInstance 但 cHttpServerCookieAttr.Value 是 Property 非字段，字段直写失败）。
3. **Variant prop 值转换缺口**：cHttpServerCookieAttr.Expires As Variant——(a) cHttpServerCookies.c 168 `FormatHttpDate(CK.Expires)` ByRef double 参数需 `&(double){vb6_VariantToDouble(prop_get_Expires(CK))}`（当前 vb6_VARIANT 直接塞 double 复合字面量 C2440）；(b) cHttpServer.c 913 `.Expires = DateAdd(...)` prop_let Variant 参数需 vb6_VariantFromValue 打包。同族：cHttpServer.c 374 void*→double、cookies 65、response 152/153。
4. **cHttpServer.c 821 `Request.Header.Exists(...)` C2037**：Header 被解析成 vb6_ComIface_IDictionary* 且 `->Exists` 结构体成员访问（应 vb6_ComCall 或类方法早绑定）。
5. **cHttpServerResponse.c 452 VBMAN.Version C2198**：ComGlobalNs promoted 函数链生成 `vb6_cVBMAN_Version(&(void*){0}, 0)`（当静态模块函数）；应 CreateObject(sGlobal) + ComCall VBMAN + 晚绑定 .Version（cgen_expr 424 分支未命中）。
6. **cHttpServer.c 372-374 With CI 内 `m_oServer.RemoteHostIP/RemotePort` 链式读错乱**：RHS 退化为 me->m_oServer（属性名丢失）、`.ConnectAt = Now()` 错配 COM RemotePort。With ClassInstance 字段写 RHS 含类字段链式属性读的路径缺陷。
7. **cHttpServer.c 382 FireEvent 事件参数打包**：OnAccept(CI, IsDiss) 的 CI 生成 `vb6_BSTR_FromStr(CI)`（对象被当字符串打包，运行时错；编译无错故未计）。

候选低垂：cHttpServerCookies.c 65/168（Variant↔对象/Date 转换，机制同 090ag 089c2/IsEmpty 修复，可先做）。

- **验证基线变更**：用 `scripts/_tmp_bisect.ps1 -N 125`（FIX_bisect.vbp，Class/Module 125 个、无窗体——VBMAN.vbp 的 `Form=Layer\FLayer.frm` 行无分号，bisect 第 14 行正则 `^(Class|Module|Form)=[^;]+;.+$` 要求分号 → 窗体被排除）做快速全量验证，错误日志 `vbman/src/_fix/bisect/out/c3-error.log`。注意此基线与 HANDOFF 上方记录的不同窗口期（当前 84 错 = 无窗体全量）。
- **090aj（cgen_base.cpp）**：Date 类型类字段归入浮点成员集 classDoubleMembers_（此前 UDT/字段映射遗漏 → 相关 C2440）。
- **090ak+090al+090am（cgen_expr.cpp）**：值上下文无括号类方法引用补默认参数（C2198 参数太少）——清 cHttpServerSession 5 错（`If Db.Sql(s).Param(p).QueryParam Then` 链）+ cHttpServer.c(804) State403 + cHttpServerResponse.c(452) VBMAN.Version + cLayer cCsv_ShowTo 等。生效点是 **Fix 083d 分支**（cgen_expr 2189：`node.object->kind != IdentifierExpr && !asCallCallee_` 只发 this 单参）——按 findClassMemberCallParams 形参表补默认值（_prop_ 属性跳过）。090al（1899 段 knownClassVars_ 未命中 → inferClassTypeOfExpr emplace）与 090ak（Fix 088b typed 字段链补参）为同族防御性补丁。
- **090an（36cd62c）**：数组下标为 Variant 时转 Long——`baBuffer(maxLen)`/`ReDim` 边界等生成 `VB6_SA_AT(type, arr, (*maxLen))`，宏内 `[(idx)-lBound]` 对 vb6_VARIANT 做减法 → C2088。修 cTlsReMaster.c(372) C2088+C2198（visit 一维/二维数组元素访问 + toLongIfVariant）。注意 ByRef Variant 参数以 `(*name)` 形态作下标时 toLongIfVariant 依赖 knownVariantVars_/cExprIsVariant 识别，未见漏网。
- **窗体 Release 崩溃验证（既有阻塞项确认）**：VBMAN.vbp 有 4 窗体（FLayer/FLogs/FToastCenter/FToastDrawer），bisect 因正则从未含窗体 → 崩溃从未在回归面。修正正则含窗体后 release `.build\C3.exe` 崩溃（0xC0000005）最小批次 38 = **FLayer.frm**（VBMAN.vbp 顺序 index37）；**.build-asan 版同批次正常完成**（FLayer.c 只报 2 个截断依赖错 ToolsWindow）→ release 独崩 = 未初始化/UB。印证 HANDOFF 上方「ASAN 崩点漂移 ↔ release 崩 cVBMAN 收尾」内存损坏特征。**窗体方法/默认实例跨模块调用（cLayer FLayer.ShowTo 分发到 cCsv_ShowTo、cLogs FLogs.Visible 未声明）为窗体模块缺失的副作用，修复窗体崩溃后才可见真实错误面**。窗体崩溃定位建议：Debug 构建 + cdb 抓 FLayer 崩溃栈（ASAN 不崩无法用 report）。

### 最近修复摘要（090ag ToolsJsonVba 3 错，103* → 100*）

- **C2039/C2037 x2（ParseObject 内 `json_ParseObject.Item(json_Key) = v`）**：`As Dictionary`（项目外 COM 类）函数返回的 C 类型是 `vb6_ComIface_IDictionary*`；MAE visit 的 089c2 分支只匹配 `currentReturnCType_ == "void*"`（As Collection/As Object），字典类型落入 UDT fallback → `vb6_ret_X.Item(...)` C2037（未定义结构 vb6_ComIface_IDictionary）。修复：089c2 条件加 `find("vb6_ComIface_") != npos`，comObjExpr_ 统一 `(void*)currentReturnVar_` → COM dispatch，与函数返回 COM 属性的既有形态（如 CompareMode 的 FnRetObj COM SetProp）对齐。
- **C2083（ConvertToJson `If JsonValue Then`）**：ByVal As Variant 参数作 If 条件 → `if (JsonValue)`（vb6_VARIANT 结构不能直接 bool）。修复：visit(IfStmt) 在条件 emit 后若为 Variant 值（cExprIsVariant 或 knownVariantVars_ 标识符）包 `vb6_VariantToBool(x)`（VB6 真值判定语义）；已含前缀则跳过防重包。
- **验证**：fix_tool 单模块 ToolsJsonVba.bas 3→0；cCsv 回归 0 error；cZipArchive/cJson 单编的错均裁剪缺依赖假错（vb6_type_ZipVfsType/FILETIME 类型定义与 ToolsList 模块缺失），非改动引入（全量基线 0 错）。提交 368d0ee。

### 090ah 候选分析（cToolsArray 6 错，暂缓——多机制点 + 草稿代码语义存疑）

cToolsArray.cls 的 6 错集中在三个草稿/边缘函数（Extend/DeArray/test，「未完待续」注释），涉及独立机制点（各需一处 cgen 改动 + rtl 语义决策），且 VB 源对 IsMissing 数组元素的用法在 VB6 是否合法存疑（非主线收益）：

1. **C98/C107（Extend 内 `IsMissing(Vars(i)) = False Then Vars(i) = Value(i)`）**：vb6_IsMissing 形参是 SAFEARRAY*（ParamArray 用）；对 As Variant 数组元素（vb6_VariantArrayGet 返回 vb6_VARIANT）调用 → C2440。需 Variant 版 missing 判定（VB6 IsMissing 语义 ≈ 参数是否 Empty/未传）——rtl 无 Variant 版（vb6_IsMissing 只做 psa==NULL），要新增或内联 vt==VT_EMPTY（Optional 缺省 C3 表示另查）。
2. **C130（test 内 `Extend(Array(A, C)) = Split(...)`）**：本类 Public Property Let（Vars, Value 双参）经 IdentifierExpr callee 的 LHS 属性赋值——生成 `Extend(&_arr_1, &(vb6_VARIANT){0}) = RHS`（C2106）。与 090ad 场景同族但对象是本类属性（非 COM），prop_let rewrite 未覆盖「callee 为裸标识符属性 Let」的 LHS。
3. **C147（DeArray 内 `All = UBound(OutVars)`）**：ParamArray 形参（SAFEARRAY*）被当作 Variant 数组包了 vb6_VariantToSafeArray1D(OutVars) → C2440。ParamArray 参数体内使用时应直接 SAFEARRAY*（UBound(OutVars) → vb6_PA_UBound(OutVars)）。
4. **C158/C167（DeArray 内 `IsMissing(OutVars(i)) / OutVars(i) = Arr(i)`）**：PA 元素被生成为 vb6_PA_GetLong(...) 作 LHS（C2106，rtl 无 PA_SetLong 形式的写——实际应 PA_SetVariant 或元素是 VARIANT 槽）；IsMissing(PA 元素) 语义存疑。
5. 连带：vb6_PA_SetLong(_pa_0, 0, A) 传 BSTR（test 里 A/C 是 String）可能另报（错误数 6 外）。

**建议**：此簇让位给更高收益错误（cAliyunCaptcha/cHttpServerResponse/依赖重模块多文件 fix）。若做，从 #3（UBound(ParamArray) 最小改动）切入。

### 最近修复摘要（090ae/090af cCsv COM 链写 + Variant 数组元素成员，107* → 103*）

- **现象**：cCsv.cls 生成 C 4 错集中于两形态——`Set Data(LineNumber)(ColumnNumber) = Dat`（prop_set_Value 内，C2197/C2440）与 `If TypeOf d(0) Is cJson Then Set d(0) = d(0).Root`（NewLine，C2039/C2198/C2440）。
- **090ae（链式 COM 写缺失于 SetStmt）**：P25b（链式 COM 默认属性索引赋值）原只存在于 AssignmentStmt，SetStmt 无分支 → LHS 被 emitExpr 按**链式读**生成 `vb6_VariantFromComResult(vb6_ComCall(...)) = value`（非左值）→ C2440。修复：P25b 提取为 `CCodeGen::tryEmitChainedComWrite`（cgen_util.cpp），AssignmentStmt/SetStmt 共用；root 判定扩展「当前类 void* COM 字段」（classVoidFieldMap_ 按 moduleName_+字段名查，`Data As New Dictionary` → me->Data）。生成 `vb6_ComSetPropArg(vb6_ComCallObject(me->Data, L"Item", {Line}, 1), L"Item", {Col}, 1, vb6_ComPackValue(Dat))`。
- **090af（Variant 数组元素对象成员）**：(a) MemberAccessExpr 对 `VB6_SA_AT(vb6_VARIANT, ...)` 前缀表达式此前走通用 fallback → `.Root` 结构体字段访问 C2039；(b) SetStmt 038b-6 对 Variant RHS 一律 ToObjectVal 提取、051 缺「Variant 数组元素 LHS」判定 → void* 赋 vb6_VARIANT 元素 C2440。修复：MemberAccessExpr 对 `VB6_SA_AT(vb6_VARIANT,` 设后期绑定 marker（同 knownVariantVars_ P24-04）：`vb6_ComGetProp(vb6_VariantToObject(&(elem)), L"Root")` → Variant 值；SetStmt 把 targetIsVariant 判定上移共用（含 VB6_SA_AT(vb6_VARIANT, LHS），038b-6 对 Variant 容器跳过（RHS 直接拷贝/FromValue Identity）、051 统一包装。
- **验证**：fix_tool 单模块 cCsv.cls 0 error；回归 Dictionary.cls（090ad 路径）与 cIni.cls（090e 默认成员链写）均 0 error 无破坏。提交 54e2258。

### 最近修复摘要（090ad 只写属性 LHS 参数多传，109 → 107*）

- **现象**：Dictionary.c(110/162) C2197 `vb6_Dictionary_prop_let_key(me->m_Dict, OldKey, vb6_BSTR_Empty(), NewKey)` —— VB `m_Dict.key(OldKey) = NewKey` 中 key 只有 `Property Let`（无 Get）。
- **根因**：只写属性 LHS emit 时（resolveClassMemberCall 读上下文 fallback 到 Let），IndexOrCallExpr 参数补齐逻辑按 prop_let_key 形参 (OldKey, NewKey) 对缺失的 value 形参也 pad 默认值 `vb6_BSTR_Empty()`；随后 Pattern C/D2 rewrite（tryRewriteCOMLvalue）把 target 当 prop_get 形式**追加** RHS value → 4 实参 vs 3 形参。
- **修复**（cgen_util.cpp Pattern C/D2）：matchedVerb != prop_get_ 时用 findClassMemberCallParams(cls, afterPg) 取业务形参表；若 target 括号实参顶层段数 == 形参数+1（对象+全形参=被 pad 完整），丢弃末段由真实 value 取代 → `prop_let_key(me->m_Dict, OldKey, NewKey)`。配套新增 splitTopLevelArgs 顶层逗号分割 lambda。
- **附带修正**：090w/090ad 中 `prefix.substr(5)` → `substr(4)`（"vb6_" 仅 4 字符）。原 bug 使类名被截首字符（`vb6_Dictionary_`→`ictionary_`），findClassMemberCallParams 永远失败 → **090w 的 Variant 末参打包此前是死代码**。修正后激活：值实参走 `vb6_VariantFromValue`（_Generic：标量→Long/Double、BSTR→String、**vb6_VARIANT→Identity 不透传不二次包**），安全。
- **验证**：fix_tool 单模块 Dictionary.cls → 0 error（生成 `prop_let_key(me->m_Dict, OldKey, NewKey)` 正确 3 参）。cCsv.cls 裁剪复现 4 错与全量基线同形（`d(0).Root` 等，属下一轮，非本修复引入）。**全量回归被 C3 崩溃阻塞**（见上「阻塞项」）。

### 最近修复摘要（090u/v/w/x/y/aa/ab/ac，132 → 109）

- **验证**：全量 132 → 109（C2440 73→46）；错误分布由「cZipArchive/Demo 大簇」转为「64 组零散 1-7 个」（根因分散，逐点反查 VB 源）。
- **090ac**（cgen_expr + cgen_stmt + vb6rtl.c/h）：`App.HelpFile` 此前无 App 属性分支 → 生成 `(void*)0.HelpFile` 非法语法（C2059/C2198）。新增 rtl `vb6_App_HelpFile()`（返回空串 BSTR），MemberAccessExpr App 分支、wrapToBSTR 及 CallStmt/Print/Write/Debug 的 BSTR 前缀表全部登记，防二次装箱。
- **090v**（cgen_decl.cpp + cgen.hpp）：模块/类级 `Dim As New` 变量持久注册 `moduleNewVars_`——`knownNewVars_` 在 Sub/Function/Property 入口清空，仅从 moduleNewVars_ 恢复，避免局部同名变量（如 cColl 内 `Dim A As New cCollection` vs JsonStr 内 `Const A`）残留导致 const 变量被注入 auto-instantiate（C2166）。
- **090ab**（cgen_util inferExprType）：UDT 已确认但字段符号不可解析（跨模块 Public Type 符号不可达）时返回 `Unknown` 而非 `Variant`——否则 `COMSTAT.fBitFields` 等 int32 字段被 `isDefinitelyVariantExpr` 误判 Variant → 实参套 `vb6_VariantToLong(int32)` → C2440。
- **090aa**（cgen_stmt ForEachStmt）：For Each 元素是项目类变量（`For Each oClient In colTimedOut`）时 cast `(vb6_cls_X*)vb6_ComUnpackObject(&feVar)`；此前漏查 knownClassVars_ → 走 Variant 分支 `oClient = vb6_VARIANT` → C2440。
- **090u**（cgen_expr MsgBox/BSTR 形参）：实参已是 `vb6_VariantToString(...)`（参数打包段对 Variant 实参按 BSTR 形参已转一次，如 `MsgBox .FindFirst(...) As Variant`）时不再二次包裹 → 消除 `VariantToString(BSTR)` C2440。
- **090x**（cgen_stmt Debug.Print）：Variant 表达式/变量 → `vb6_DebugWriteBSTR(vb6_VariantToString(x))`，此前落入 `DebugWriteLong((int32_t)(x))` → C2440。
- **090w**（cgen_util tryRewriteCOMLvalue）：Property Let 末参 `As Variant`（cJson.Item Dat / cCsv.Value）经 Pattern C/D2 改写为 prop_let_ 时，标量值实参（double/int/BSTR）打包成 vb6_VARIANT（ByVal → `vb6_VariantFromValue`；ByRef → 复合字面量取址）。
- **连带**：090y 注释厘清 With void* 兜底语义（COMObject 后期绑定更贴合 COM，TODO(090z) 还原 ClassInstance 定位崩溃）；Demo `Users.Decode .Rs` 加括号消除解析歧义。

### 最近修复摘要（090s/090t With 块成员链解析，142 → 132）

- **验证**：全量 142 → 132（C2440 79→73）；Demo 簇 21 → 15，修复 5 类 With/COM 链错误（99/104/142/144/502 行级全部清零）。
- **090s**（cgen_util inferClassTypeOfExpr + cgen_stmt WithStmt + cgen_expr WithMemberExpr asCallCallee）三处协同：
  1. WithMemberExpr 推断：`.X` 是 With 目标类的 typed 项目类字段时返回**字段类**（`.Router.Reg` → .Router As cHttpServerRouter），否则外层 findClassMemberCallParams 形参解析失败、实参全丢（C2198 `.Router.Reg "Test"`/`.CBC.Encode("x")`）。
  2. WithStmt MAE 目标（`With Http.RequestDataQuery`）按宿主类字段表分类：RequestDataQuery As New Dictionary = COM void* 字段 → WithObjKind::COMObject；此前 memSym 全局查找捡 Dictionary 符号当 vb6_cls_Dictionary* 类实例 → `.Item(k)=v` 结构体字段调用 C2039（顺带激活 COM 参数化写路径 → vb6_ComSetPropArg）。memSym 兜底限 kind==Unknown。
  3. WithMemberExpr asCallCallee_ 改为 pendingChainObj_ 协议（同 MAE Fix 015/088b）→ CallStmt 补 WithMemberExpr 形参解析 + Optional padding（`.Start` 4 个 Optional 参 → `vb6_cHttpServer_Start((void*)w, &(int32_t){80}, ...)`）。此前生成 funcName(this) 完整调用使 CallStmt bare-call 分支跳过补齐 → C2198。
- **连带修复**：`.Root("data")("coatingWeight")` 嵌套 COM 链读（Fix 086 嵌套对象参数）因推断类修正自动正确。
- **Demo residual 15**（Ini/Reg/Json 等函数，下轮专项候选）：
  1. `Demo.c(513)` C2106：`With ini.Section("App")` 嵌套 With（Section 返回类实例）→ `.Item(k)=v` 误生成 vb6_cTimers_Item(...)=（void* With 目标 + 全局符号撞名 cTimers.Item）
  2. `(534)(550)(600)(601)` C2440：MsgBox 类方法 String 返回被双包 VariantToString（cRegedit.FindFirst As String → BSTR 当 VARIANT）；UDT 数组元素字段 BSTR 写等
  3. `(681)` C2440/C2198：Debug.Print 链
  4. `(697)` C2166：左值指定 const（写只读？）
  5. `(721)` C2198+C2039：`.Rs` 实参（cDataBase COM 字段传参）Decode 参数太少
  6. `(778)(798)(869)(871)` C2440：double/int→VARIANT*（参数打包字面量、ToolsJsonVba.ConvertToJson 跨模块）

### 最近修复摘要（090o-p cZipArchive 簇清零，155 → 142）

- **验证**：全量 155 → 142（C2440 85→79）；cZipArchive 簇 6 → 0（全量 `cZipArchive error C: 0`）。
- **090o**（cgen_stmt.cpp For/GoTo）：VB6 `For i = lo To hi` 方向拆分为两副本时，`GoTo 标签` 的后缀 `_dN` 仅在「目标标签定义于被拆分 For body 内」（两份副本各定义一次）时附加；目标在 body 外（函数级出口标签如 QH/EH，GoTo 跳出循环到过程尾）只定义一份 → 加后缀生成 `goto vb6_label_QH_d2` 指向不存在标签 → C2094（cZipArchive 事件取消出口 683）。新增 `forSplitLabelStack_`/`collectForBodyLabels` 收集 body 内标签集判定。
- **090p**（cgen_stmt.cpp Erase）：Erase 对 UDT 的 `As Variant` 数组字段（`(*uFile).BufferArray`）生成裸 `vb6_SafeArrayDestroy1D((*uFile).BufferArray)` 把 VARIANT 当 SafeArray* → C2440；改 `vb6_VariantClear(&...);`（释放数组并置 VT_EMPTY）。把 090m 的 UDT 字段判定提炼为 `isVariantArrayTarget()`（ReDim 084a/090m 复用同判定，Erase 覆盖）。
- **090r**（cgen_expr.cpp 参数提取）：Variant→double 参数提取分支补 `Vb6Type::Date`（OLE date = double）。`pvToFileTime(me, vb6_VariantArrayGet(&..SourceFileInfo, 6))`（形参 ByVal Date）此前不走 Currency/Double/Single 提取（缺 Date）→ VARIANT 裸传 double 形参 C2440（2439；同行 2437 Currency 有包装形成差异对照）。
- **090q**（cgen_expr.cpp As Any 打包 + cgen_base/hpp pending 行）：Declare 的 ByVal/ByRef As Any 参数实参是「返回 UDT 的函数调用」（`SetFileTime ..., pvToFileTime(...)`，FILETIME）时：`(void*)(intptr_t)(struct 值)` → C2440；MSVC C 也不支持复合字面量 `(T){fnRetUdt()}` 内联初始化（报 C2440 "初始化…无法从 T 转换"）。修复：`generate()` 入口预扫模块 declarations 注册 `funcUdtRetCType_`（小写函数名→C 返回类型，类方法无 VB 顺序约束故不能等 emitFunctionDecl 逐函数注册）；As Any 打包命中时生成 `vb6_type_X _vb6_anytmpN = <call>;` 挂 `CodeEmitter::addPending`（新 pending 行机制），`emitLine`/`emitBlank` 前经 `flushPending` 先落地声明再输出引用行（表达式拼接期无法在行中插语句，声明恒先于引用）。

### 最近修复摘要（090h-n UDT 字段类型推断链，165 → 155）

- **根因链**：cZipArchive 的 VFS 系列函数（`pvVfsOpen`/`pvVfsCreate`/`pvVfsSetEof` 等，返回 UDT `ZipVfsType` 或 ByRef UDT 参数）里字段访问的类型推断失效：① 返回 UDT 的函数名/返回值变量（`vb6_ret_X`）未注册 `knownUdtVars_`；② 推断层 `inferUdtTypeOfExpr` 不认函数名对象；③ UDT 的 `As Variant` 字段（如 `BufferArray`/`SourceFileInfo`）不被识别为 Variant 表达式 → 被当 SafeArray/String/函数处理 → C2440/C2198/C2064 连锁（簇 23 错）。
- **090i**（cgen_decl.cpp）：Function 返回类型 `vb6_type_*` 时注册 `knownUdtVars_[funcRetLower]=retType`（与 Fix 089c 的 `void*` 类对象注册并列）→ 函数体内 `vb6_ret_X.Field` 字段类型可推断。
- **090j**（cgen_util.cpp `inferUdtTypeOfExpr`）：IdentifierExpr 分支增加「函数名 == 当前过程名且本函数返回 UDT」→ 返回 `currentReturnCType_`（与 090i 对称；发射层 084z-4/088d 已有同名机制）。
- **090h**（cgen_expr.cpp VarPtr）：`lastExpr_` 以 `(*` 开头（ByRef 解引用）是左值 → `(intptr_t)&(expr)`，不走 `(int32_t){}` 常量复合字面量（`VarPtr(File)`/`VarPtr((*uFile).BufferArray)` → C2440）。
- **090k**（cgen_expr.cpp As Any 打包）：`(*uFile).BufferArray` 这类「ByRef UDT 解引用 + 字段链」识别为左值取址，而非 `(void*)(intptr_t)(expr)` 值强转（Variant 字段值强转 → C2440）。
- **090l**（cgen_util.cpp `isDefinitelyVariantExpr`）：MemberAccessExpr 分支，对象是 UDT 且字段声明 As Variant → 明确 Variant（`inferUdtTypeOfExpr` 命中 + `inferExprType==Variant`）。
- **090n**（cgen_expr.cpp UBound/LBound）：首参是 Variant 数组表达式时包 `vb6_VariantToSafeArray1D(&arg)`（如 `UBound(vb6_ret_pvVfsCreate.BufferArray, 1)` → C2440）。
- **090m**（cgen_stmt.cpp ReDim）：`isVariantArrayVar` 增加 UDT 字段目标判定——ByRef UDT 参数/返回 UDT 变量的 `As Variant` 字段（`(*uFile).BufferArray`）按 Variant 数组处理（`vb6_VariantToSafeArray1D` + `vb6_VariantFromValue` 回包）。注：`knownUdtVars_` 值为 `vb6_type_` + `cIdent()`（私有 UDT 名前导 `_`，如 `vb6_type__ZipVfsType`），`lookupModule` 前须 strip 前导 `_`。
- **验证**：全量 165 → 155（C2440 91→85）；cZipArchive 簇 23 → 6。
- **cZipArchive 残留 6 错**（090o-p 专项已全部清零，见上节）：
  1. ~~`cZipArchive.c(683)` C2094：事件回调内 `goto vb6_label_QH_d2` 标签未定义~~ → 090o（后缀仅当标签在 For 拆分 body 内时附加）
  2. ~~`(2439)` C2440：Variant 数组取元素未解包 double~~ → 090r（参数提取补 Vb6Type::Date）
  3. ~~`(2658)` C2440 x2：Erase uFile.BufferArray~~ → 090p（Erase 走 isVariantArrayTarget → vb6_VariantClear）
  4. ~~`(2672)` C2440+C2198：Declare FILETIME ByVal UDT 参数强转 struct 值~~ → 090q（As Any 打包 UDT 调用实参 → pending 临时变量取址）

### 最近修复摘要（090f/g 跨模块同名冲突误生成，176 → 165）

- **090f（Function 返回赋值被误拦截为同名跨类 PropertyLet）**：cCollection.Items() 内 `Items = Array()` → 误生成 `vb6_cJson_prop_let_Items((void*)me, _arr_1)`（C2198 参数太少，Items() 无参却被按 2 参带参属性拼调用）。根因：cgen_stmt.cpp P6.11 裸标识符 PropertyLet/Set 分支的 `isAssigningReturnValue` 只覆盖 PropertyGet/Let/Set 过程内的同名赋值，漏普通 Function —— cJson.Items Property Let/Set（带参属性）与 cCollection.Items() 函数同名，`lookupModuleByKind` 全局命中即拦截。修复：返回赋值判定扩展到 `SymbolKind::Function`（VB6 函数体内 `FuncName = expr` 唯一语义即返回赋值，同参照 Keys() 正常路径）
- **090g（函数名对象方法调用形参解析错类）**：cToolsStr.SplitLinesToCollection() As cCollection 内 `SplitLinesToCollection.Add Mid(...)` → 5 实参 `(this, (&(BSTR){Mid}), &(BSTR){empty}, &(int32_t){0}, 0)`（C2197 参数太多 + C2440）。根因：类感知形参解析（Fix 033/084z-3，cgen_expr.cpp ~4170）把函数名对象当模块名 → `findClassMemberCallParams` 失败 → 回退 `lookupModule(storageKey "add")` 命中跨类同名方法抢占的错误类，形参表错（Item 按 ByRef String 打包、多出 Optional int32）。修复：对象名 == 当前函数名时，经 Fix 088c 注册的 `knownClassVars_["vb6_ret_<fn>"]` 解析为返回类（cCollection），`findClassMemberCallParams` 命中正确形参表
- **验证**：全量 176 → 165（C2197 x5→3、C2198 x36→31、C2440 x95→91）；cToolsStr/cCollection 清零。生成对照：`vb6_cCollection_Add((void*)vb6_ret_SplitLinesToCollection, vb6_VariantFromValue(vb6_Mid(...)), vb6_BSTR_FromStr(L""), 0)`、`vb6_ret_Items = _arr_1`（与 Keys() 对称）

### 最近修复摘要（090a/c/d/e + P25b 链式写，204 → 176）

- **P25b（链式 COM 默认属性写）**：`Dic(Section)(Key) = v`（嵌套 Dictionary/默认项链）LHS → `vb6_ComSetPropArg(vb6_ComCallObject(...inner...), L"Item", outer-args, value)`。链层从最深（`chain[last]`）向 `chain[1]` 逆序取默认成员对象，`chain[0]` 为最外层带索引 Put；根须是 COM 变量或（**090e**）模块默认成员 PropertyGet（VB_UserMemId=0，如 cIni.Root 返回 Dictionary）→ emitExpr(Root) 得 `vb6_cIni_prop_get_Root(me)` 作为链起点
- **090a（As New 类字段 cast）**：`Dim FileStream As New cToolsStream` → C 结构体 `void*` 字段；knownClassVars_ fallback 中 trailingLower 命中 knownNewVars_ 且为工程类（kind==Class）→ 成员访问前 cast `((vb6_cls_X*)obj)->member`（外部 COM 仍走 dispatch 不 cast）；driver.cpp classTypedFieldMap 纳入 As New 工程类字段使 inferClassTypeOfExpr 可推断字段类
- **090c（无实参类方法调用的 Optional padding）**：`Trim(FileStream.ReadLine())` 空括号无参调用 → MAE 把 this 拼成完整调用文本 `vb6_cToolsStream_ReadLine(me->FileStream)`，ICE 3984 处"callee 完整调用且无实参"直接 return 跳过 P14.1.4 补齐 → C2198。修复：positional 空时若 callee 是 MemberAccessExpr 且 `findClassMemberCallParams` 非空（方法有参数），置 needsSplitForMissingArgs 落入拆分流程 → calleeParams 解析 → 补默认值 + `_has_` 尾参 `(me->FileStream, 0, 0)`
- **090d（CByte/CSng/CBool 转换适配）**：P8.4 只覆盖 CInt/CLng/CDbl。`CByte("&H" & Mid(...))` 生成 `vb6_CByte(BSTR)` → C2440。修复：P8.4 扩展 CByte/CSng/CBool + V 后缀（新增 inline `vb6_CByteV/vb6_CSngV/vb6_CBoolV`）；BSTR 实参解析从 `vb6_Val` 升级为新 RTL `vb6_NumVal`（VB6 类型转换语义：支持 `&H`/`&O` 前缀 + 十进制前缀数字，Val 不识别 &H）；**改 RTL 后须重跑 `.temp/build_rtl_libs.bat` 并重编 C3.exe（lib 内嵌）**
- **验证**：`scripts/_fix_ini_cluster.ps1`（cIni+cToolsStream+cCollection+cToolsStr 四模块簇）编译错误 204 阶段 6 个 → 0；全量 191 → 176（C2440 110→95）。cluster 残留仅 LNK2019（缺 ToolsFso/ToolsMath 模块，非编译缺陷）

### 最近修复摘要（089i-089k，216 → 204）

- **089i**：用户函数调用返回 Variant 作实参时按形参类型解包（正常符号路径 + 运行时函数超参路径 089i 判定 argIsVariant）
- **089j**：VB6 无 `As` 类型声明成员默认 Variant（打包/解包/初始化）
- **089k**：4622（运行时函数超参 BSTR 分支）把 049b 的 `find("vb6_BSTR")` 子串包含判断收紧为「剥外层括号后的顶层前缀表」（`vb6_BSTR_`/`VB6_SA_AT(BSTR,`/`vb6_VariantToString(`/`vb6_VariantFromComResult(`，前缀长度用 `strlen` 避免硬编码错误）——修复 `json_ParseErrorMessage(...)` 这类内部实参含 `vb6_BSTR_FromStr` 的用户函数调用被误判「已 BSTR」而漏包 `vb6_VariantToString`（ToolsJsonVba ErrRaise 6 个 C2440）；4575 正常形参路径保留子串检查避免 MsgBox 专用逻辑已转换的表达式二次包装
- **遗留**：Demo 104/534 的 COM 链（`cJson.Root.Item("data").Item(...)` 动态 dispatch 参数形态 C2172/BSTR→Variant）与 C2039 `Item` 成员为**既有深层问题**，专项修 Demo 模块时处理

## 4. 已完成的修复（Fix 084 系列）

### cgen_expr.cpp
- **Fix 084x**（~1600 行）：优先级 3 `isVarName` 判定补充 cgen 集合检查（knownUdtVars_/knownClassVars_/knownNewVars_/knownObjectVars_/knownTypedComVars_/knownIfaceVars_），修复 `oCallback` 局部类变量被误判为模块名
- **Fix 084z-2**（IdentifierExpr）：`inferClassTypeOfExpr` 支持 PropertyGet 符号返回类实例（如 `pvSocket() As cTlsSocket`）
- **Fix 084z-3**：Fix 033 分支 className 回退优先用 `inferClassTypeOfExpr`
- **Fix 084o-5**（LHS）：函数名引用返回对象的成员访问 → `vb6_ret_Func->Field`
- **Fix 084z-4**（RHS）：对称读取侧（`NewItem.RootItem` → `vb6_ret_NewItem->RootItem`）
- **Fix 084y-6/084y-7**：VBA 模块成员——常量表内联数值（vbCr/vbLf/vbTab/vbCrLf/vbNullString/vbTrue/vbFalse/VarType 数值），函数生成裸 `vb6_<name>`（去 `$` 后缀）；Fix 033 回退补 `symTab_.lookup(fnName)` 全表查询拿 Optional 参数表
- **Fix 084y-8**（~1760 行）：M22 fallback 前加 EnumType 分支——枚举类型.成员有常量值则内联数值，否则 `vb6_enum_<Enum>_<member>`
- **Fix 084aa 系列**：
  - 位置实参 / 具名实参 ByRef：常量（`isConstIdent` 命中 #define 宏）不可取址 → 按形参类型用 `wrapConstArgForByRef` 生成 `(&(TYPE){CONST})` 复合字面量（Variant 按常量自身类型选字段：VT_BSTR/VT_R8/VT_BOOL/VT_UI1/VT_I4）
  - `VarPtr(函数调用)`：调用结果非左值 → `(intptr_t)&(void*){call}` 复合字面量包装（如 `VarPtr(.Glob(0))` 的 COM 属性调用）
  - VarCmp 比较路径（~1104-1186 行）：`vb6_VarCmpLong<fn>(&left, right)` 的左右操作数左值判断排除常量宏；Variant vs Variant 的 `isLvalue` lambda 排除常量

### cgen_stmt.cpp
- **Fix 084y**（~495 行）：P6.7 Property Let/Set 前加 `objClassMatchesProp` 校验（对象类与属性 sourceModule 不一致时跳过属性路径）
- **Fix 084y-2**（~1164 行）：P6.8 同类匹配校验
- **Fix 084o-5**：`vb6_ret_Func->Field`（类返回值成员访问）
- **Fix 084y-5**：ReDimStmt/EraseStmt 改用 `resolveArrayTargetIdent`（With 块成员 → `_vb6_with_N->Field`；ByRef UDT/数组/Variant 参数 → `(*obj).Field`；UDT 变量 → `obj.Field`）
- **Fix 084aa**：静态局部变量初始化必须是编译期常量——`vb6_VariantEmpty()` → `{0}`，`vb6_BSTR_Empty()` → `NULL`

### cgen_base.cpp / cgen.hpp
- `resolveArrayTargetIdent`：ReDim/Erase 目标解析（cIdent 之后）
- `lookupConstSym` / `isStringConstIdent` / `isConstIdent` / `constIdentType` / `wrapConstArgForByRef`（ByRef 常量包装）
- `inferClassTypeOfExpr` MemberAccessExpr 分支字段表未命中时回退 `getClassMethodReturnType`（属性 Get 返回类）

### cgen_decl.cpp
- **Fix 084aa**：模块级标量 Variant 初始化 `vb6_VariantEmpty()` → `{0}`（既有 BSTR → NULL 处理之外）

## 5. 已完成的修复：m_uData 模块级 UDT 变量（Fix 010n 扩展）

### 症状（vbman/dist/ToolsTlsThunks.c，修复前）
1. **C2064 x24**：`m_uData.Pfn(2)` — UDT 固定数组字段被当函数调用（VB6 `Pfn(2)` 应为 C `Pfn[2]`）
2. **C2224 x37**：`_vb6_with_15->LocalCertificates.Item(lIdx)`、`(*uInput).Stack.Remove(1)` — `.Item`/`.Add`/`.Remove` 左侧非结构体（**独立簇，见第 9 节**）
3. **C2440 x13**：`vb6_SafeArray1D* → vb6_VARIANT`、`(void*)m_uData` UDT 值强转

### 根因（已实证，非推测）
`With m_uData`（模块级 UDT 变量）被误分类为 **COM 对象** → 生成 `(void*)m_uData` 强转 + 所有字段访问变成 COM 调用 → C2440/C2224/C2064 连锁错误。
**实证链**（临时调试输出，C3_DEBUG_UDT 环境变量）：
- `[UDTREG] var=m_uData type=UcsCryptoData sym=0x... kind=8 cType=vb6_type_UcsCryptoData` → **注册代码确实执行**，`knownUdtVars_["m_udata"]` 正常写入
- `[WITHUDT] obj=m_udata inKnown=0 tempType=void*` → **但 With 分类时查不到**！

**真正原因**：`knownUdtVars_` 在**每个过程开头被 `clear()`**（M22-fix，cgen_decl.cpp 三处：visit(FunctionDecl)/visit(SubDecl)/visit(PropertyDecl)，位于 67/293/1252 行附近），clear 后仅从 `classUdtMembers_` 恢复——而 `classUdtMembers_` **只对类模块填充**（cgen_base.cpp generate() 开头 `if (isClassModule_)` 分支）。**普通 .bas 模块的模块级 UDT 变量（如 m_uData）在进入过程后永久丢失**，导致过程内 `With m_uData` 分类失败。

### 修复（对称扩展 classUdtMembers_ 机制）
1. **cgen.hpp**：新增成员 `std::unordered_map<std::string, std::string> moduleUdtMembers_;`（小写 var 名 → UDT C 标识符）
2. **cgen_base.cpp** generate() 开头：
   - `moduleUdtMembers_.clear()`
   - `if (!isClassModule_)` 分支：扫描 `module.declarations` 的 VariableDecl（`asType` 为 `SimpleTypeRef` 且 `lookupDotted` 命中 `UserDefinedType`），写入 `moduleUdtMembers_[lower] = "vb6_type_" + cIdent(name)`
3. **cgen_decl.cpp** 三处过程开头恢复处（classUdtMembers_ 恢复行后）各加：
   `knownUdtVars_.insert(moduleUdtMembers_.begin(), moduleUdtMembers_.end());`

### 验证结果（修复前后对比）
- ToolsTlsThunks.c：`vb6_type_UcsCryptoData* _vb6_with_125 = &(m_uData)`（修复前 `void* _vb6_with_125 = (void*)m_uData`）
- `.hRandomProv` → `_vb6_with_125->hRandomProv`（修复前 `vb6_ComGetIntProp(...)`）
- `m_uData.Pfn[2]`（修复前 `m_uData.Pfn(2)` 函数调用）
- **错误数：588 → 552**；ToolsTlsThunks：74 → 51（C2064 x24 清零、C2440 13→3 均为其他根因）

### 注意事项（踩坑记录）
- **增量构建可能产生陈旧对象 → 运行时 0xC0000409 崩溃**（症状：任何模式跑 VBMAN.vbp 都崩溃，`--dump-symbols` 也在 `=== End Symbol Table ===` 后崩）。本次通过 `cmake --build .build --clean-first` 全量重建解决。**今后改 cgen.hpp 后若运行崩溃，优先怀疑陈旧对象，先 clean 构建**

## 5b. 已完成的修复（Fix 088e/089 系列，2026-09-05，552 → 216）

演进：552 → 502 → 489 → 486 → 301 → 278 → 267 → 260 → 251 → 243 → **216**
- **088d/088e**：类返回实例链上成员访问 (ReturnJson.Decode)、getClassMethodReturnType Phase-B 兜底（278→267）
- **089**：跨模块 COM/Collection 变量分发（267→260）
- **089c/089d**：COM 返回函数变量、控件未知成员分发（260→251）
- **089e**：Friend 成员被错误编译为 static → extern（251→243）
- **089f**：类模块内无括号引用本类 Function（`pvSessionID = GenerateSessionID`，Fix 086 barecall）缺 `me` → 补 `((void*)me)`（与 PropertyGet/CallStmt 对齐）
- **089g**（cgen_util.cpp resolveClassMemberCall）：跨模块同名成员 storageKey 抢占 —— 多个类有同名 Property Get（如 cWebSocketClient/cWebSocketServerClient 都有 State）时 driver 的 globalPublicSyms 按 `$pg` 键只保留先分析模块符号，目标类 Get 缺失而 Let 正常注入 → 读上下文误发 `prop_let_State`。修复：Get 缺失而读到 Let/Set 时查 Class 符号 memberProcKinds（semantic 按读上下文 Get>Function>Sub>Let>Set 写入），按读形式重定向 `prop_get_`
- **089h**（cgen_expr.cpp ×3 处值上下文）：无括号裸方法引用只发 this → C2198 参数太少。三处分支（Fix 015 链式值上下文 / memSym 类变量分支 / class var 分支）补默认参数（Optional/必选 + `_has_` 尾参）。**属性（`_prop_` 前缀函数）不 pad** —— findClassMemberCallParams 对 Get+Let 并存属性返回 Let 参数，pad 会 C2197（经 stash 基线对比确认无回归，C2197 x5 为 pre-existing：cToolsStr Add x2 / Dictionary prop_let_key x2 / Demo_Database Item x1，属默认属性多级链与 COM 写 rewrite 簇）
- 已验证零新增回归（git stash 基线 243 vs 当前 216，C2197/C2440 同桶）

## 6. 剩余错误分布（216，最新确认 2026-09-05）

按错误码（总数 216）：
- C2440 x118（COM/Variant 打包解包簇：`vb6_VARIANT ↔ void*/double/int32_t`、`类实例 → VARIANT`、函数指针调用参数类型、SafeArray）、C2198 x45、C2039 x11、C2172 x5、C2223 x5、C2197 x5（pre-existing：cToolsStr Add / Dictionary prop_let_key / Demo_Database 默认属性链）、C2106 x5、C2094 x4、C2064 x3、C2037 x3、C2083 x2、C2059 x2、C2171/C2063/C2110/C2101/C2186/C2088/C2143/C2166 各 1
- 已清零：C2102、C2099、C2129、C2065、C2045、C2296、C2224（C2224 已全部转译/消除）
- 待办簇（按体量）：① C2440 x118 —— cTlsSocket.c 4093 等 Variant↔具体类型、cHttpServerRouter.c 29 类实例→VARIANT 参数、cHttpServer.c 374/892 double↔void*/VARIANT；② C2039 x11 —— 成员访问到错误类（Dictionary.Item/Value/Rs/ReturnJson 簇）；③ C2198 x45 中 DataBase 链式值上下文已修、余 COM 打包与默认属性链簇

## 7. 关键文件地图

| 文件 | 职责 |
|---|---|
| `src/backend/expr/cgen_expr.cpp` | 表达式生成：IndexOrCallExpr、M22 fallback、ByRef 实参、VarCmp、VarPtr、枚举分支（同目录另有 `_array`/`_binary`/`_call`/`_ident`/`_member`/`_with`） |
| `src/backend/stmt/cgen_stmt.cpp` | 语句生成：With 分类、属性 Let/Set、ReDim/Erase、静态局部（同目录另有 `_assign`/`_call`/`_control`/`_file_io`/`_jumps`/`_loop`/`_redim`/`_select`/`_setlet`/`_with`） |
| `src/backend/decl/cgen_decl.cpp` | 声明生成：模块级变量 + knownUdtVars_ 注册（`decl/cgen_decl_var.cpp` 的 `visit(VariableDecl&)`，152-160 行；同目录另有 `_api`/`_proc`/`_prop`、`cgen_localdecl.cpp`） |
| `src/backend/cgen_base.cpp` | 辅助：lookupConstSym/wrapConstArgForByRef/resolveArrayTargetIdent/inferClassTypeOfExpr |
| `src/backend/cgen_util.cpp` | inferClassTypeOfExpr、inferUdtTypeOfExpr（已按职责拆为 8 文件：主文件 + `_type`/`_scan`/`_com`/`_comwrite`/`_classcall`/`_classtype`/`_ctrl`） |
| `src/backend/cgen.hpp` | 声明 |
| `src/driver/driver.cpp` | 跨模块注入（1094-1128，仅 Public） |
| `src/semantics/symbol_table.cpp` | lookup/lookupModule 作用域链 |
| `vbman/src/Socket/mdTlsThunks.bas` | 当前错误源文件（VB6 侧） |
| `vbman/dist/c3-error.log` | MSVC 错误日志（统计源） |

## 8. 会话纪律

- 每次修复后必须走完整工作流（build.bat → build_vbman.bat → _tmp_count.ps1）验证错误数变化
- 不用内联 PowerShell `$_`，写 .ps1 脚本
- 修复按错误簇批量处理，每轮结束后更新本文档第 5/6 节
- **改 cgen.hpp 后运行崩溃（0xC0000409）先怀疑陈旧对象 → `cmake --build .build --clean-first` 全量重建**
- **改了 RTL 源码/库后必须确认 `c3rtl.rc.res` 已重编**：否则 C3.exe 仍嵌旧库，症状是「桩明明补好了，链接期仍报未解析 `vb6_di_*`」——极具误导性（根因与修法见第 10 节 2.2）；改 RTL 后固定流程是 `scripts/build_rtl_libs.bat` → 重建 C3.exe。

## 10. 链接期（编译期全绿后的新前沿）

`-Forms` 全量 129 条目已达 **error C = 0**。链接阶段原有两类问题，其中**第 2 项已于 2026-09-11 修复**（提交 `03de92f`、`460e621`，实施与验证见 2.1/2.2，历程见 `ai/开发历程/87-*`）。

**当前链接期基线（2026-09-11，含窗体 125 模块 `--dll` x64 链接，日志 `vbman/src/_fix/bisect/outfrm/c3-error.log`）**：`LNK1104 = 0` / `LNK2005 = 27 条` / **未解析外部符号 443 条**（绝大多数是其它模块待补的 `vb6_di_*` 转发桩）—— 这两项即下一步入口。

1. **LNK2005 重定义（疑似真实缺陷，值得修）** —— 同一符号在多个 `.obj` 中被定义：
   - `FToastDrawer.obj : Sad / m_Theme / m_State / PosVal / TopVal / m_ParentToast / m_TagName 已经在 FToastCenter.obj 中定义`
   - `mDelay.obj : Delay 已经在 Tools.obj 中定义`
   - `cModbusTransportTCP.obj / cModbusSlave.obj : vb6_evt_wrap_m_socket_Connect / _CloseEvent / _DataArrival / _Error / m_listensocket_* 已经在 cWebSocketClient.obj / cWebSocketServer.obj 中定义`
   → 指向两类生成问题：① **模块级私有变量未按模块前缀 mangle**（VB6 中各模块 `Private m_Theme` 互不冲突，C 侧必须消歧；`Delay` 同族）；② **事件包装函数 `vb6_evt_wrap_m_*` 被写进每个发送者模块**，应只在其事件源类模块定义、其它模块仅保留声明。
2. ~~**`LNK1104: 无法打开文件 msvbvm60.lib`**~~ —— **已修复（2026-09-11，提交 `03de92f` + `460e621`）**：不是配置缺口，而是生成器缺「无导入库 `Lib` 特判」（2026-09-11 核实，前一轮判读有误）。以下 2.0 为完整追根记录，最终实现与验证见 2.1/2.2。
   - **来源**：`cgen_decl.cpp:1219` 对**每条** VB6 `Declare ... Lib "X"` 都生成 `#pragma comment(lib, "X.lib")`；而 VBMAN 源码确有 **10 个模块 14 处** `Declare ... Lib "msvbvm60"`（`mdTlsThunks.bas:105/106`、`cAsyncSocket.cls:193/203`、`cTlsSocket.cls:160/161`、`mdTlsSodium.bas:77/79`、`mdTlsNative.bas:121/127`、`ToolsJsonUcs.bas:54`、`cHttpRequest.cls:120`、`cHttpDownload.cls:51`、`cToolsArray.cls:18`）→ 生成物中出现 `#pragma comment(lib, "msvbvm60.lib")`（`cAsyncSocket.c:45/55`、`cTlsSocket.c:101/102`、`ToolsTlsThunks.c:252/253`、`cToolsArray.c:5`），链接器遂去找该文件。
   - **只涉 3 类符号**：`Alias "VarPtr"`（源码名 `ArrPtr`，取数组/变量地址）、`Alias "__vbaObjSetAddref"`（对象赋值时的运行时 AddRef）、`Alias "#644"`（**按序号**导入的运行时内部函数，源码名 `SplitLongToBytes`）。
   - **x64 路线上无法「补」这个库**：本机 `C:\Windows\SysWOW64\msvbvm60.dll` 存在（32 位），但 `System32` 下**无** 64 位版本；Windows SDK / VS 安装目录里**不存在任何 `msvbvm60.lib`**。VB6 运行时只有 32 位 → `--arch x64` 没有可链接的导入库，且与 `ai/011`「静态链接 msvbvm60 ❌ 不采用、不支持 x64」及网站「零依赖部署」的定位冲突。
   - **该库无法提供、且提供也无用**（2026-09-11 二次核实）：工程内 / `C:\Windows` / `Program Files*` / 用户目录**均无** `msvbvm60.lib`；VB6 未安装（无 `VB98`）；Windows SDK / VS 均不携带 —— 该文件只随 VB6/VS6 发行，无法凭空取得。
   - **决定性事实（Fix 076）**：Declare 早已不走导入库路线 —— 生成物里 `__declspec(dllimport)` 出现 **0 次**，改为 `extern <ret> __stdcall vb6_di_<name>(...)` + `#define <VB名> vb6_di_<name>`，由 `src/rtl/core/di/vb6_di_stubs.c` 提供转发桩（`vb6_di_ord_12` 即先例，走 `GetProcAddress(shlwapi, 12)`）。因此 `#pragma comment(lib, "msvbvm60.lib")`（`cgen_decl.cpp:1218-1219` 无条件生成）是**纯残留**：唯一后果是制造 fatal `LNK1104`；**即使补上真库也只消 LNK1104、随即变 `LNK2019`**，因为需要的符号是 C3 内部名，MSVBVM60.DLL 导出表里根本没有。
   - **实测待补 RTL 桩 3 个**（`vb6_di_stubs.c` 87 行内目前 0 个 msvbvm60 桩）：`vb6_di_VarPtr`（源码 `ArrPtr`，`cAsyncSocket.h:460` `extern intptr_t __stdcall vb6_di_VarPtr(vb6_SafeArray1D** Ptr)`）、`vb6_di_vb6___vbaObjSetAddref`（源码 `vbaObjSetAddref`，用法 `vbaObjSetAddref((void*)&(oCallback), _vb6_with_60->ClientCertCallback)`）、`vb6_di_ord_644`（源码 `SplitLongToBytes`，`cToolsArray.h:215`，返回 `vb6_type_longByteType`）。
   - **可行方向**：① `cgen_decl.cpp:1219` 特判 `libName == "msvbvm60"`（不分大小写）→ 跳过 pragma；② `vb6_di_stubs.c` 补这 3 个桩，语义按 VBMAN 调用点实现（`ArrPtr` 取 SAFEARRAY 描述符、`__vbaObjSetAddref` = 写对象变量并 AddRef、`#644` = Long→4 字节 UDT）；**序号桩不能照抄 `ord_12` 的 `GetProcAddress`**，x64 下无 32 位 msvbvm60.dll 可加载。
   - `LNK1104` 是 **fatal**，它会挡在真正的「未解析外部符号」清单之前 → **必须先处理它才能看清链接期全貌**。

#### 2.1 已实施（2026-09-11，提交 `03de92f`）

- **生成器**（`cgen_decl.cpp`）：对 VB6/VBA 运行时库（`msvbvm60`/`msvbvm50`/`vbe7`/`vbe6`/`vba7`/`vba6`）与 `cryptdlg` **不再生成** `#pragma comment(lib, ...)`；其余库照旧（实测其余 19 个库在 SDK 中均存在）。
- **RTL**（`src/rtl/core/di/vb6_di_stubs.c`）新增 **4 个原生桩**（除 `cryptdlg` 外均不 LoadLibrary 转发）：
  - `vb6_di_VarPtr`（`ArrPtr`）：`return (intptr_t)Ptr;` —— 数组 ByRef 传递时 C3 传入的实参**已经是**数组变量地址（生成声明 `vb6_SafeArray1D**`），即 VB6 的 `&baBuffer`，故恒等。调用点 `CopyMemory(ArrPtr(a), ArrPtr(b), 4)` 是交换两个 `SAFEARRAY*`，语义核对通过。
  - `vb6_di_vb6___vbaObjSetAddref`：`if (src) src->AddRef(); *(void**)dst = src;` —— **不** Release 旧值，对齐原版 VB6 语义（避免误 Release 非持有引用）。
  - `vb6_di_ord_644`（`SplitLongToBytes`）：**序号 644 经 `dumpbin /exports SysWOW64\msvbvm60.dll` 反查确认就是 `VarPtr`**（350 = `__vbaObjSetAddref`）。x86 下 VarPtr 把入参放回 EAX，而 VB6 对 4 字节 UDT 返回值同样取 EAX，遂成「Long → 4 字节小端位重解释」技巧 → 此处按小端拆字节返回 4 字节结构。
  - `vb6_di_CertSelectCertificateW`：`cryptdlg.dll` **没有导入库**（SDK `10.0.26100.0\um\x64` 实测无 `cryptdlg.lib`），故走 `GetModuleHandleW`/`LoadLibraryW` + `GetProcAddress`（导出序号 15）动态加载。
- **`scripts/build_rtl_libs.bat` 入库**：原脚本只在被 `.gitignore` 忽略的 `.temp\` 里，而 C3 链接依赖这些 `.lib`（经 `c3rtl.rc` 嵌入 C3.exe），必须可复现构建。**该 `.bat` 必须保持纯 ASCII** —— 中文注释在 cp936 控制台下会把命令行切碎（实测报 `'vb6rtl.obj' is not recognized ...`）。
- **验证（含窗体 125 模块，`--dll` x64）**：`error C = 0`；`LNK1104 = 0`（msvbvm60 / cryptdlg 均不再出现）；**本次新增 4 符号 0 条未解析**；未解析外部符号总数 **451 → 443**。

#### 2.2 陷阱：重建 RTL 库后 C3 仍嵌旧库（提交 `460e621` 修复）

`c3rtl.rc` 以 RCDATA 嵌入 10 个文件（4 头 + 3×x64 库 + 3×x86 库），但 CMake 只把 `.rc` 自身当依赖 → **重建 `src/rtl/lib/*.lib` 后 `.res` 不重编**，C3.exe 继续嵌旧库。本次实测中招：`.res` 停留在 12:17 而新库是 19:45；C3 会话目录提取出的 `vb6rtl.lib` = 467468 字节（正好等于 HEAD 版大小，新库 470076），于是链接期仍报 `vb6_di_VarPtr` 等未解析 —— **看起来像桩没生效**，极易误判。
修法：`CMakeLists.txt` 把这 10 个文件显式声明为 `c3rtl.rc` 的 `OBJECT_DEPENDS`（CMake 3.31 + Ninja 实测有效：重建库后 `.res` 时间戳刷新，C3.exe 体积随新库变化 3354624 → 3359744）。

**建议顺序（2026-09-11 更新）**：第 2 项**已完成**（见 2.1/2.2），链接期已能看到全貌。下一步：① 对着新基线批量补 `vb6_di_*` 桩 —— 清单可从 `c3-error.log` 用 `Select-String -Pattern 'LNK(2019|2001)'` + 正则 `vb6_[A-Za-z0-9_]+` 去重导出（当前 443 条）；② 确认第 1 项两簇 LNK2005 在 `vbman/build_vbman.bat` 全量链接下是否重现（bisect 是裁剪工程，需排除「裁剪导致的重复」），然后分别修。

## 9. （历史，2026-09-05 遗留，已非当前下一步）ToolsTlsThunks C2224 x37（COM 集合簇）+ SafeArray 簇

### 症状（生成代码）
1. **C2224 x37**：`_vb6_with_15->LocalCertificates.Item(lIdx)`、`(*uInput).Stack.Remove(1)` — `.Item`/`.Remove` 左侧非结构体
2. **C2039 x4**：`"MessBuffer_Data": 不是 "vb6_type_UcsTlsContext" 的成员`（1786 行）
3. **C2198 x3 / C2440 x3**：`vb6_SafeArrayDestroy1D(...)` 参数太少、`vb6_SafeArray1D* → vb6_VARIANT`（758 行）

### 已确认的根因（C2224）
- VB6 侧：`UcsTlsContext` UDT 内字段 `LocalCertificates As Collection`、`Stack As Collection`（**UDT 内嵌 COM 对象字段**）
- C 侧：UDT 结构体中该字段生成 `void*`；访问 `.Item(...)` 时被当**结构体字段+函数调用**，而非 **COM 方法调用**
- C3 已有 COM 调用生成（`vb6_ComGetIntProp`/`vb6_ComCall*`，用于局部 COM 变量与 With 块 COM 对象），但 **UDT 内嵌 void* 字段上的方法调用未走 COM 路径**

### 建议方向（未实施）
- 在 MemberAccessExpr/IndexOrCallExpr 生成时，若接收者表达式推断为 `void*`（COM 指针，如 UDT 内嵌 Collection/类字段），成员访问走 COM 调用路径（`vb6_ComGetIntProp(obj, L"Item")` 等）
- 需先梳理 cgen_expr.cpp 中 COM 调用判定条件（knownObjectVars_/knownTypedComVars_ 等），确定「UDT 字段类型为 void*」的识别点
- C2039/C2198/C2440（SafeArray 簇）独立，涉及 `MessBuffer_Data` 字段缺失与 SafeArray 参数包装，可另起一轮

