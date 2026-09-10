# C3 编译器错误修复 — 任务交接文档

> 用途：新会话恢复上下文用。新开会话后直接说「读取 C3_FIX_HANDOFF.md 并继续修复」。
> 更新日期：2026-09-10（090w 修正+090h/090x：无窗体 125 模块 bisect 基线 68 → **65**；提交 18c58eb/087e203。窗体 Release 崩溃仍为既有阻塞项，bisect 正则不含窗体）

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
5. **注意**：PowerShell 内联 `$_` 会被转义，复杂逻辑必须写进 .ps1 脚本文件再执行

## 3. 错误演进史

777 → 774 → 748 → 723（VBA 子集常量）→ 700（VBA 全量 + 枚举）→ 638（ReDim/Erase + 枚举）→ 594（C2102 常量取址修复）→ 589 → 588（C2099 静态初始化清零）→ 216（088e-089h）→ 204（089i/j/k）→ 176（090a/c/d/e + P25b）→ 165（090f/g）→ 155（090h-n）→ 142（090o-p）→ 132（090s/090t）→ 109（090u/v/w/x/y/aa/ab/ac）→ 107*（090ad）→ 103*（090ae/090af）→ **100***（090ag）

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
| `src/backend/cgen_expr.cpp` | 表达式生成：IndexOrCallExpr、M22 fallback、ByRef 实参、VarCmp、VarPtr、枚举分支 |
| `src/backend/cgen_stmt.cpp` | 语句生成：With 分类、属性 Let/Set、ReDim/Erase、静态局部 |
| `src/backend/cgen_decl.cpp` | 声明生成：模块级变量 + knownUdtVars_ 注册（903-912） |
| `src/backend/cgen_base.cpp` | 辅助：lookupConstSym/wrapConstArgForByRef/resolveArrayTargetIdent/inferClassTypeOfExpr |
| `src/backend/cgen_util.cpp` | inferClassTypeOfExpr、inferUdtTypeOfExpr（2076+） |
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

## 9. 下一步调查：ToolsTlsThunks C2224 x37（COM 集合簇）+ SafeArray 簇

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
