#include "backend/cgen.hpp"
#include <algorithm>
#include <cctype>
#include <iostream>
#include <cstdlib>
#include <functional>
#include <cstdio>

namespace vb6c3 {

// --- cgen_expr_member.cpp: MemberAccessExpr 求值（类成员 / COM / UDT 字段访问） ---

void CCodeGen::visit(MemberAccessExpr& node) {
    // Fix 086: AddressOf 模块限定函数 (AddressOf ToolsTlsThunks.pvCallCollectionItem)
    // — 解析器将 `ToolsTlsThunks.pvCallCollectionItem` 交给 AddressOfExpr(仅取
    // object名) + 外层MemberAccessExpr(成员名). 此前生成非法的
    // (void*)vb6_<mod>_<mod>.<member> (C2065). 在此合成为真正的函数指针.
    if (node.object && node.object->kind == ASTNodeKind::AddressOfExpr) {
        auto& ao86 = static_cast<AddressOfExpr&>(*node.object);
        (void)ao86;
        std::string fnName86 = node.memberName;
        Symbol* fnSym86 = symTab_.lookupModule(fnName86);
        if (!fnSym86) fnSym86 = symTab_.lookup(fnName86);
        if (fnSym86 && (fnSym86->kind == SymbolKind::Sub || fnSym86->kind == SymbolKind::Function)) {
            lastExpr_ = "(void*)" + cProcName(fnName86, fnSym86->access,
                fnSym86->isExternal ? fnSym86->sourceModule : "");
        } else {
            lastExpr_ = "(void*)" + cProcName(fnName86, AccessLevel::Private);
        }
        return;
    }
    // Fix 010k: Err/builtin object member access — check FIRST before anything else
    // This must be at the very top to avoid any other code path consuming the node
    if (node.object && node.object->kind == ASTNodeKind::IdentifierExpr) {
        auto& _objId = static_cast<IdentifierExpr&>(*node.object);
        std::string _objLower = _objId.name;
        std::transform(_objLower.begin(), _objLower.end(), _objLower.begin(), ::tolower);
        std::string _memLower = node.memberName;
        std::transform(_memLower.begin(), _memLower.end(), _memLower.begin(), ::tolower);

        if (_objLower == "err" || _objLower == "lasterror") {
            // Fix 056b: LastError 属性返回 VBA.ErrObject (全局单例),
            // .Number/.Description/.Source 等价于 Err.Number 等.
            if (_memLower == "number")      { lastExpr_ = "vb6_ErrNumber()";      return; }
            if (_memLower == "description") { lastExpr_ = "vb6_ErrDescription()"; return; }
            if (_memLower == "source")      { lastExpr_ = "vb6_ErrSource()";      return; }
            if (_memLower == "lastdllerror") { lastExpr_ = "GetLastError()";       return; }
            if (_memLower == "helpfile")    { lastExpr_ = "(BSTR)0";              return; }
            if (_memLower == "helpcontext") { lastExpr_ = "0";                    return; }
            if (_memLower == "clear")       { lastExpr_ = "vb6_ErrClear";         return; }
            if (_memLower == "raise")       { lastExpr_ = "vb6_ErrRaise";         return; }
        }

        // Fix 084y-6: VBA 模块成员 (VBA.vbCr / VBA.Replace 等). 编译器不注册
        // "VBA" 模块符号 → M22 fallback 生成 vb6_VBA_<member> (rtl 无此名 →
        // C2065). 常量内联字面量; 函数生成裸 rtl 名 vb6_<member> (去 $ 后缀,
        // 如 VBA.Mid$ → vb6_Mid), 由外层 IndexOrCallExpr 的 Fix 033 回退
        // (lookupModule + lookup 全表) 解析参数表并填充 Optional 默认值.
        if (_objLower == "vba" || _objLower == "vba5") {
            static const std::unordered_map<std::string, std::string> vbaConsts = {
                {"vbcr", "13"}, {"vblf", "10"}, {"vbtab", "9"},
                {"vbnullchar", "0"}, {"vbnullstring", "vb6_BSTR_FromStr(L\"\")"},
                {"vbcrif", "vb6_BSTR_FromStr(L\"\\r\\n\")"},
                {"vbnewline", "vb6_BSTR_FromStr(L\"\\r\\n\")"},
                {"vbempty", "0"}, {"vbnull", "1"}, {"vbinteger", "2"}, {"vblong", "3"},
                {"vbsingle", "4"}, {"vbdouble", "5"}, {"vbcurrency", "6"}, {"vbdate", "7"},
                {"vbstring", "8"}, {"vbobject", "9"}, {"vberror", "10"}, {"vbboolean", "11"},
                {"vbvariant", "12"}, {"vbdecimal", "14"}, {"vbbyte", "17"}, {"vblonglong", "20"},
                {"vbuserdefinedtype", "36"}, {"vbarray", "8192"},
                {"vbtrue", "-1"}, {"vbfalse", "0"},
            };
            auto itC = vbaConsts.find(_memLower);
            if (itC != vbaConsts.end()) { lastExpr_ = itC->second; return; }
            std::string fnName = node.memberName;
            if (!fnName.empty() && fnName.back() == '$') fnName.pop_back();
            Symbol* vbaFn = symTab_.lookup(fnName);
            if (vbaFn && (vbaFn->kind == SymbolKind::Function || vbaFn->kind == SymbolKind::Sub
                          || vbaFn->kind == SymbolKind::PropertyGet
                          || vbaFn->kind == SymbolKind::DeclareSub
                          || vbaFn->kind == SymbolKind::DeclareFunc)) {
                // Fix 086: VBA.Now() 等无参内置函数作为裸值引用 (非callee上下文,
                // 如 pvToFileTime(VBA.Now)) — 需带调用括号, 否则函数设计符裸名 → C2440
                static const std::unordered_set<std::string> vbaZeroArgFns = {
                    "now", "date", "time", "timer", "freefile", "command",
                    "curdir", "erl", "doevents"
                };
                // Fix 093a: 用符号表里的声明拼写 (VBA.varType → vb6_VarType). 此前用
                // 源码拼写 → vb6_varType, 与 RTL/intrinsic 符号名不一致 → LNK2019.
                std::string fnCanon = vbaFn->name.empty() ? fnName : vbaFn->name;
                std::string fnLower86 = Symbol::toLower(fnCanon);
                if (fnLower86 == "rnd") {
                    lastExpr_ = "vb6_Rnd(0)";
                } else if (vbaZeroArgFns.count(fnLower86)) {
                    lastExpr_ = "vb6_" + cIdent(fnCanon) + "()";
                } else {
                    lastExpr_ = "vb6_" + cIdent(fnCanon);
                }
                return;
            }
        }
    }

    // Fix 110n: CallByName(...).成员 — RTL 的 vb6_CallByName 返回 vb6_VARIANT, 其值
    // 可能是对象 (VbGet 取到 Font 等对象属性). 对 VARIANT 结构体取成员在 C 里非法:
    //   CallByName(oCtrl, PropFont, VbGet).Size
    //     → vb6_CallByName(...).Size  C2039 ("Size" 不是 "vb6_VARIANT" 的成员)
    // 赋值时更会退化成 "<struct> = <double>" C2088. 必须走 COM 晚绑定: 先从 VARIANT
    // 取出对象 (vb6_VariantToObjectVal) 再 vb6_ComGetXxxProp / ComSetProp.
    // 读上下文由外层 (BinaryExpr/赋值) 的 resolveComValue 消费 marker;
    // 写上下文由 AssignmentStmt 的 COM SetProp 分支消费.
    // 实测: Charts 2020 ClsResizer.cls 107/147.
    if (node.object && node.object->kind == ASTNodeKind::IndexOrCallExpr) {
        auto& cbCall = static_cast<IndexOrCallExpr&>(*node.object);
        if (cbCall.callee && cbCall.callee->kind == ASTNodeKind::IdentifierExpr) {
            auto& cbId = static_cast<IdentifierExpr&>(*cbCall.callee);
            if (Symbol::toLower(cbId.name) == "callbyname") {
                emitExpr(*node.object);
                std::string cbObjExpr = std::move(lastExpr_);
                comObjExpr_ = "vb6_VariantToObjectVal(" + cbObjExpr + ")";
                comMemberName_ = node.memberName;
                isComMarker_ = true;
                isEarlyBoundCom_ = false;
                earlyBoundSym_ = nullptr;
                lastExpr_ = comObjExpr_;
                return;
            }
        }
    }

    // P7.6: 控件数组属性读取 ctrlArr(idx).Property
    // 必须在IdentifierExpr分支前检查, 因为cmdBtn(0)的object是IndexOrCallExpr
    if (node.object && node.object->kind == ASTNodeKind::IndexOrCallExpr) {
        auto& idxExpr = static_cast<IndexOrCallExpr&>(*node.object);
        if (idxExpr.callee && idxExpr.callee->kind == ASTNodeKind::IdentifierExpr) {
            auto& arrIdent = static_cast<IdentifierExpr&>(*idxExpr.callee);
            std::string arrNameLower = arrIdent.name;
            std::transform(arrNameLower.begin(), arrNameLower.end(), arrNameLower.begin(), ::tolower);
            if (knownControlArrays_.count(arrNameLower)) {
                auto itCtrl = knownFormControls_.find(arrNameLower);
                if (itCtrl != knownFormControls_.end()) {
                    std::string readFn = getControlPropReadFn(itCtrl->second, node.memberName);
                    if (!readFn.empty()) {
                        std::string idxArg;
                        if (!idxExpr.positional.empty()) {
                            emitExpr(*idxExpr.positional[0]);
                            idxArg = std::move(lastExpr_);
                        } else {
                            idxArg = "0";
                        }
                        lastExpr_ = readFn + "(vb6_CtrlArr_GetAt(&vb6_arr_" + cIdent(arrIdent.name) + ", " + idxArg + "))";
                        return;
                    }
                }
            }
        }
    }
    // 内置对象方法: Debug.Print → vb6_DebugPrint
    // 检查 object 是否是 IdentifierExpr

    // Fix 023e: Form-module Me.member access — handle Form-specific properties
    // (Height/Width/hwnd/Left/Top/Caption/Visible/Enabled/...) via vb6_GetControlXxx
    // helpers and route unknown members (ScaleHeight/ScaleWidth/Move/Refresh/Show/Cls/...)
    // through COM dispatch on the form HWND (treated as void* IDispatch*).
    // 背景: MeExpr 在 isFormModule_ 时被 visit(MeExpr) 解析为 vb6_hwnd_<FormName>,
    // 这是一个 void* 窗口句柄. Me.Height / Me.ScaleWidth / Me.Move(...) 等成员访问在
    // 下方 IdentifierExpr 分支中无法匹配 (因为 node.object->kind == MeExpr 而非
    // IdentifierExpr), 一直漏到 fallback 处直接 emit "vb6_hwnd_<FormName>.Member"
    // 形成 C2224 (void* 上的 .member 访问). 此处统一处理:
    //   1. 已知 Form 控件属性 (Height/Width/hwnd/Caption/Visible/Enabled/Font*/...) 
    //      走 getControlPropReadFn 返回的 vb6_GetControlXxx(vb6_hwnd_<FormName>).
    //   2. 未知的 Form 成员 (ScaleHeight/ScaleWidth/Move/Refresh/Show/Print/Cls/...)
    //      走 COM dispatch (vb6_ComCall/ComGet*Prop).
    // 注: form HWND 不是真正的 IDispatch*, 这是编译 stub (使 MSVC 接受代码); 真正的
    // 运行时正确性需要为 Form 实现专门的 IDispatch 或在 RTL 中添加专用 Form-属性
    // 辅助函数 (vb6_GetFormScaleWidth / vb6_MoveForm 等) — 留待后续 fix.
    if (node.object && node.object->kind == ASTNodeKind::MeExpr && isFormModule_ && !knownFormName_.empty()) {
        // 构造 form 的 HWND C 表达式: vb6_hwnd_<FormName> (保持原始大小写)
        std::string formHwnd;
        auto itOrig = knownFormControlOriginalNames_.find(knownFormName_);
        if (itOrig != knownFormControlOriginalNames_.end()) {
            formHwnd = "vb6_hwnd_" + cIdent(itOrig->second);
        } else {
            formHwnd = "vb6_hwnd_" + moduleName_;
        }
        // 优先用 Form 控件属性读函数解析已知属性
        std::string readFn = getControlPropReadFn(FrmControlType::Form, node.memberName);
        if (!readFn.empty()) {
            lastExpr_ = readFn + "(" + formHwnd + ")  /* Form." + node.memberName + " via Me */";
            return;
        }
        // 未知 Form 成员 → COM dispatch (作为编译 stub; 运行时不可靠)
        comObjExpr_ = formHwnd;
        comMemberName_ = node.memberName;
        isComMarker_ = true;
        isEarlyBoundCom_ = false;
        earlyBoundSym_ = nullptr;
        lastExpr_ = formHwnd;  // void* 表达式 (form HWND)
        return;
    }

    if (node.object && node.object->kind == ASTNodeKind::IdentifierExpr) {
        auto& objIdent = static_cast<IdentifierExpr&>(*node.object);
        std::string objLower = objIdent.name;
        std::transform(objLower.begin(), objLower.end(), objLower.begin(), ::tolower);
        std::string memLower = node.memberName;
        std::transform(memLower.begin(), memLower.end(), memLower.begin(), ::tolower);

        if (objLower == "debug" && memLower == "print") {
            lastExpr_ = "vb6_DebugPrint";
            return;
        }
        if (objLower == "debug" && memLower == "assert") {
            lastExpr_ = "vb6_DebugAssert";
            return;
        }
        // P14.3.4: App全局对象属性
        if (objLower == "app") {
            if (memLower == "path") { lastExpr_ = "vb6_App_Path()"; return; }
            if (memLower == "exename") { lastExpr_ = "vb6_App_EXEName()"; return; }
            if (memLower == "helpfile") { lastExpr_ = "vb6_App_HelpFile()"; return; }  // Fix 090ac: 此前落到 (void*)0.HelpFile → 语法错
            if (memLower == "hinstance") { lastExpr_ = "vb6_App_hInstance()"; return; }
            if (memLower == "hinstancehnd") { lastExpr_ = "vb6_App_hInstance()"; return; }  // VB6别名
            if (memLower == "title") { lastExpr_ = "vb6_App_EXEName()"; return; }  // 简化
            if (memLower == "major") { lastExpr_ = "0"; return; }
            if (memLower == "minor") { lastExpr_ = "0"; return; }
            if (memLower == "revision") { lastExpr_ = "0"; return; }
            // Fix 056b: App.ThreadID - 当前线程ID (windows.h 已包含)
            if (memLower == "threadid") { lastExpr_ = "(int32_t)GetCurrentThreadId()"; return; }
            // Fix 086: 补齐 App.LogMode / App.LogEvent (ToolsIDE/clsSubClass 引用)
            if (memLower == "logmode") { lastExpr_ = "(int32_t)1"; return; }
            if (memLower == "logevent") {
                // RTL 无 LogEvent — 转发到调试输出 (首参为消息文本, 类型参数忽略)
                lastExpr_ = "((void(*)(BSTR,int32_t))vb6_DebugWriteBSTR)";
                return;
            }
        }

        // Fix 086: 跨模块窗体默认实例属性读取 (FLogs.Visible →
        // vb6_GetControlVisible(vb6_form_hwnd_FLogs())). 窗体名在
        // knownFormModuleNames_ (driver预扫描) 中且非当前窗体时生效.
        if (knownFormModuleNames_.count(objLower)
            && (!isFormModule_ || objLower != knownFormName_)) {
            std::string extFormHwnd = "vb6_form_hwnd_" + cIdent(objIdent.name) + "()";
            // Fix 093a: 窗体默认实例方法 Show — VB6 的 `Frm.Show` 是窗体内置方法,
            // 窗体外的调用此前落到通用的类成员/全局符号解析, 误命中同名用户函数
            // (cLogs: `FLogs.Show` → vb6_FLogs_Show(<cTimeUse.Show 的默认实参>,0)
            // → LNK2019). 转发到窗体自带的 vb6_form_show_<Form>(NULL).
            if (memLower == "show") {
                lastExpr_ = "vb6_form_show_" + cIdent(objIdent.name) + "(NULL)  /* external form show */";
                return;
            }
            std::string extReadFn = getControlPropReadFn(FrmControlType::Form, node.memberName);
            if (!extReadFn.empty()) {
                lastExpr_ = extReadFn + "(" + extFormHwnd + ")  /* external form prop */";
                return;
            }
        }

        // P18-C: Clipboard 对象
        if (objLower == "clipboard") {
            if (memLower == "settext") { lastExpr_ = "vb6_Clipboard_SetText"; return; }
            if (memLower == "gettext") { lastExpr_ = "vb6_Clipboard_GetText()"; return; }
            if (memLower == "clear") { lastExpr_ = "vb6_Clipboard_Clear"; return; }
            if (memLower == "getformat") { lastExpr_ = "vb6_Clipboard_GetFormat"; return; }
            // Fix 056: SetData method for Clipboard.SetData picture
            if (memLower == "setdata") { lastExpr_ = "vb6_Clipboard_SetData"; return; }
        }

        // P18-C: Screen 对象
        if (objLower == "screen") {
            if (memLower == "width") { lastExpr_ = "vb6_Screen_Width()"; return; }
            if (memLower == "height") { lastExpr_ = "vb6_Screen_Height()"; return; }
            if (memLower == "mousex") { lastExpr_ = "vb6_Screen_MouseX()"; return; }
            if (memLower == "mousey") { lastExpr_ = "vb6_Screen_MouseY()"; return; }
            if (memLower == "twipsperpixelx") { lastExpr_ = "vb6_Screen_TwipsPerPixelX()"; return; }
            if (memLower == "twipsperpixely") { lastExpr_ = "vb6_Screen_TwipsPerPixelY()"; return; }
            if (memLower == "activecontrol") { lastExpr_ = "(int32_t)(intptr_t)vb6_Screen_ActiveControl()"; return; }
            if (memLower == "activeform") { lastExpr_ = "(int32_t)(intptr_t)vb6_Screen_ActiveForm()"; return; }
        }

        // P18-C: Printer 对象
        if (objLower == "printer") {
            if (memLower == "print") { lastExpr_ = "vb6_Printer_Print"; return; }
            if (memLower == "enddoc") { lastExpr_ = "vb6_Printer_EndDoc"; return; }
            if (memLower == "newpage") { lastExpr_ = "vb6_Printer_NewPage"; return; }
            if (memLower == "width") { lastExpr_ = "vb6_Printer_Width()"; return; }
            if (memLower == "height") { lastExpr_ = "vb6_Printer_Height()"; return; }
            if (memLower == "currentx") { lastExpr_ = "vb6_Printer_CurrentX()"; return; }
            if (memLower == "currenty") { lastExpr_ = "vb6_Printer_CurrentY()"; return; }
        }

        // P18-C: Forms 集合
        if (objLower == "forms") {
            if (memLower == "count") { lastExpr_ = "vb6_Forms_Count()"; return; }
            if (memLower == "item") { lastExpr_ = "(int32_t)(intptr_t)vb6_Forms_Item"; return; }
        }

        // P7.5+P7.6: 窗体控件属性读取
        // 情况1: ctrl.Property (非数组) → vb6_GetControlXxx(vb6_hwnd_ctrl)
        // 情况2: ctrlArr(idx).Property (数组) → vb6_GetControlXxx(vb6_CtrlArr_GetAt(&vb6_arr_ctrl, idx))
        {
            // P16: WithEvents控件属性读取 (优先于标准控件, 因为变量名可能同名)
            auto itWECtrl = knownWithEventsCtrlVars_.find(objLower);
            if (itWECtrl != knownWithEventsCtrlVars_.end()) {
                std::string readFn = getControlPropReadFn(itWECtrl->second, node.memberName);
                if (!readFn.empty()) {
                    auto itOrig = knownWithEventsCtrlOrigNames_.find(objLower);
                    std::string weVarName = (itOrig != knownWithEventsCtrlOrigNames_.end()) ? itOrig->second : objLower;
                    lastExpr_ = readFn + "(" + weVarName + ")  /* WithEvents ctrl */";
                    return;
                }
            }
            // P7.5: 非数组控件属性读取
            auto itCtrl = knownFormControls_.find(objLower);
            if (itCtrl != knownFormControls_.end()) {
                // Fix 112: 工程内 UserControl 子控件 → 类成员直接调用.
                // `ucChartBar1.AddSerie(Value)` 此前落 COM 晚绑定
                // `vb6_ComCall(vb6_hwnd_ucChartBar1, L"AddSerie", ...)`, 而 HWND 不是
                // IDispatch. 这里按 .ctl 模块解析成员, this 取宿主窗口的实例.
                auto itUC = knownUserControlCtrlVars_.find(objLower);
                if (itUC != knownUserControlCtrlVars_.end()) {
                    std::string resolvedFn = resolveClassMemberCall(itUC->second, node.memberName);
                    if (!resolvedFn.empty()) {
                        std::string hwndArg = makeCtrlHwndArg(objLower, itCtrl->second);
                        std::string thisArg =
                            "(vb6_cls_" + cIdent(itUC->second) + "*)vb6_UC_InstanceOf(" + hwndArg + ")";
                        if (asCallCallee_) {
                            pendingChainObj_ = thisArg;
                            lastExpr_ = resolvedFn;
                        } else {
                            // 值上下文无括号调用: 按形参表补 Optional 默认值 (同 Fix 089h)
                            std::vector<ParameterInfo> paramsUC;
                            bool isBuiltinUC = false;
                            if (resolvedFn.find("_prop_") == std::string::npos
                                && findClassMemberCallParams(itUC->second, node.memberName,
                                                             paramsUC, isBuiltinUC)
                                && !paramsUC.empty() && !isBuiltinUC) {
                                std::string argListUC = thisArg;
                                for (size_t i = 0; i < paramsUC.size(); i++) {
                                    const auto& pm = paramsUC[i];
                                    argListUC += ", ";
                                    std::string defV = (pm.hasDefaultValue && !pm.defaultValueExpr.empty())
                                                     ? pm.defaultValueExpr : defaultValue(pm.type);
                                    if (pm.isByVal) argListUC += defV;
                                    else {
                                        std::string ct = mapType(pm.type);
                                        if (pm.type == Vb6Type::Variant || pm.type == Vb6Type::Empty
                                            || pm.type == Vb6Type::Null || pm.type == Vb6Type::Object)
                                            argListUC += "&(" + ct + "){0}";
                                        else argListUC += "&(" + ct + "){" + defV + "}";
                                    }
                                }
                                for (size_t i = 0; i < paramsUC.size(); i++) {
                                    if (paramsUC[i].isOptional && !paramsUC[i].isParamArray)
                                        argListUC += ", 0";
                                }
                                lastExpr_ = resolvedFn + "(" + argListUC + ")";
                            } else {
                                lastExpr_ = resolvedFn + "(" + thisArg + ")";
                            }
                        }
                        return;
                    }
                }
                std::string readFn = getControlPropReadFn(itCtrl->second, node.memberName);
                if (!readFn.empty()) {
                    lastExpr_ = readFn + "(" + makeCtrlHwndArg(objLower, itCtrl->second) + ")  /* ctrl prop read */";
                    return;
                }
                                // P7.9: WebBrowser method access (Navigate/GoBack/GoForward/Refresh)
                if (itCtrl->second == FrmControlType::WebBrowser) {
                    std::string memLower = node.memberName;
                    std::transform(memLower.begin(), memLower.end(), memLower.begin(), ::tolower);
                    if (memLower == "navigate" || memLower == "goback" || memLower == "goforward" || memLower == "refresh") {
                        // Set marker for IndexOrCallExpr to handle
                        comObjExpr_ = objLower;  // Store lowercase control name
                        comMemberName_ = memLower;
                        isComMarker_ = true;
                        isEarlyBoundCom_ = false;
                        earlyBoundSym_ = nullptr;
                        lastExpr_ = objLower;  // Placeholder expression
                        return;
                    }
                }
                // P13.3: ListBox/ComboBox method access (AddItem/RemoveItem/Clear/List)
                if (itCtrl->second == FrmControlType::ListBox || itCtrl->second == FrmControlType::ComboBox) {
                    std::string memLower = node.memberName;
                    std::transform(memLower.begin(), memLower.end(), memLower.begin(), ::tolower);
                    if (memLower == "additem" || memLower == "removeitem" || memLower == "clear" || memLower == "list") {
                        comObjExpr_ = objLower;
                        comMemberName_ = memLower;
                        isComMarker_ = true;
                        isEarlyBoundCom_ = false;
                        earlyBoundSym_ = nullptr;
                        lastExpr_ = objLower;
                        return;
                    }
                }
                // ActiveX控件 (ImageList等): 属性访问走COM后期绑定
                if (itCtrl->second == FrmControlType::ImageList ||
                    itCtrl->second == FrmControlType::Toolbar ||
                    itCtrl->second == FrmControlType::StatusBar ||
                    itCtrl->second == FrmControlType::CommonDialog) {
                    std::string memLower = node.memberName;
                    std::transform(memLower.begin(), memLower.end(), memLower.begin(), ::tolower);
                    std::string ctrlOrigName = knownFormControlOriginalNames_.count(objLower) ?
                        knownFormControlOriginalNames_[objLower] : objIdent.name;
                    comObjExpr_ = "vb6_com_" + cIdent(ctrlOrigName);
                    comMemberName_ = node.memberName;
                    isComMarker_ = true;
                    isEarlyBoundCom_ = false;
                    earlyBoundSym_ = nullptr;
                    lastExpr_ = comObjExpr_;  // IDispatch* expression
                    return;
                }
                // Fix 023e/089d: 控件 readFn 为空 (未知属性/方法如 Form.ScaleWidth,
                // Move/Refresh/Align等) 走 COM dispatch on control HWND, 避免落入
                // warn-and-fall-through 后由通用 fallback 先做默认属性展开再拼
                // ".Member" → C2224 (vb6_GetControlText(...) .Move / .Align).
                // Fix 089d 将 Form 分支扩展为所有非 Menu 控件 (FLogs List1.Move、
                // FLayer LContent.Move、FToastDrawer Picture1.Align 等).
                // 注: 与上面 MeExpr 分支一致, 这是编译 stub; 控件 HWND 不是真正的
                // IDispatch*, 运行期语义由真实 VB6 runtime 提供.
                if (itCtrl->second != FrmControlType::Menu) {
                    std::string ctrlHwnd = makeCtrlHwndArg(objLower, itCtrl->second);
                    comObjExpr_ = ctrlHwnd;
                    comMemberName_ = node.memberName;
                    isComMarker_ = true;
                    isEarlyBoundCom_ = false;
                    earlyBoundSym_ = nullptr;
                    lastExpr_ = ctrlHwnd;  // void* 表达式 (control HWND)
                    return;
                }
                diag_.warn(DiagnosticID::CodeGenUnsupportedFeature, SourceLocation{},
                    std::string("P7.5: Unknown control property '") + objIdent.name + "." + node.memberName +
                    "' for control type, generating struct field access (may not compile)");
            }
        }

        // 优先级0: COM前期绑定成员访问 (P6.3, Dim x As FileSystemObject)
        // 有具体类型信息的COM变量, 通过vtable直接调用而非IDispatch::Invoke
        if (knownTypedComVars_.count(objLower)) {
            emitExpr(*node.object);
            comObjExpr_ = lastExpr_;       // 保存对象表达式
            comMemberName_ = node.memberName;  // 保存成员名
            isComMarker_ = true;           // 标记为COM调用
            isEarlyBoundCom_ = true;       // P6.3: 标记为前期绑定
            earlyBoundSym_ = knownTypedComVars_[objLower];  // ComClass符号
            lastExpr_ = lastExpr_;         // 保持不变
            return;
        }

        // P6.4: 接口引用成员访问 (Dim x As IFoo) → 通过vtable调用
        // 设置接口标记, 由IndexOrCallExpr/AssignmentStmt识别
        auto itIfaceVar = knownIfaceVars_.find(objLower);
        if (itIfaceVar != knownIfaceVars_.end()) {
            emitExpr(*node.object);
            comObjExpr_ = lastExpr_;       // 接口引用变量C表达式
            comMemberName_ = node.memberName;  // 接口方法名
            isComMarker_ = true;           // 复用COM标记机制
            isEarlyBoundCom_ = false;      // 不是COM前期绑定
            // 设置接口标记
            earlyBoundSym_ = nullptr;       // P6.4接口不是ComClass
            lastExpr_ = lastExpr_;         // 保持接口引用变量名
            return;
        }

        // 优先级1: COM对象成员访问 (Object类型变量, 后期绑定)
        // COM对象的成员名不在符号表中, 需通过IDispatch::Invoke调用
        // 设置COM标记, 由IndexOrCallExpr/AssignmentStmt/SetStmt识别并处理
        // 注意: 项目类实例 (Dim b As Button / Dim WithEvents b As Button) 的 C 类型也是
        // void*, 可能被误注册进 knownObjectVars_, 但它不是 IDispatch — 类实例是纯 C
        // 结构体 (vb6_cls_Button*), 用 vb6_ComCall 会解引用 vtable 崩溃 (0xC0000005).
        // 已知是类实例时跳过 COM 晚绑定, 交给下面的"优先级2"做直接分发.
        if (knownObjectVars_.count(objLower) && !knownClassVars_.count(objLower)) {
            emitExpr(*node.object);
            comObjExpr_ = lastExpr_;       // 保存对象表达式
            comMemberName_ = node.memberName;  // 保存成员名
            isComMarker_ = true;           // 标记为COM调用
            // lastExpr_设为对象表达式(可用作值), 具体调度由上层决定
            lastExpr_ = lastExpr_;         // 保持不变 (对象C表达式)
            return;
        }

        // P24-04: Variant变量成员访问 (Variant持有COM对象, 后期绑定)
        // VB6: For Each b In col: b.Index -> vb6_ComCall(vb6_VariantToObject(&b), L"Index", ...)
        // Variant变量可能持有IDispatch指针, 成员访问需要通过COM晚绑定
        if (knownVariantVars_.count(objLower)) {
            emitExpr(*node.object);
            std::string varExpr = std::move(lastExpr_);
            comObjExpr_ = "vb6_VariantToObject(&" + varExpr + ")";
            comMemberName_ = node.memberName;
            isComMarker_ = true;
            lastExpr_ = "vb6_VariantFromComResult(vb6_ComGetProp(vb6_VariantToObject(&" + varExpr + "), L\"" + node.memberName + "\"))";  // P25: Variant COM prop Get → VARIANT
            return;
        }

        // P24-04: VB_GlobalNameSpace promoted函数的成员访问 (如 VBMAN.Version)
        // VBMAN是提升到全局的函数(sGlobal._sGlobal.VBMAN()), 返回cVBMAN COM对象
        // VBMAN.Version = VBMAN().Version = 先创建sGlobal单例, 调用VBMAN()获取cVBMAN, 再访问.Version
        if (auto* gnsSym = symTab_.lookup(objIdent.name)) {
            if (gnsSym->kind == SymbolKind::ComGlobalNs) {
                // 1. 生成sGlobal单例创建 + promoted方法调用表达式
                std::string progIdWide = "L\"" + gnsSym->comProgId + "\"";
                std::string methodName = gnsSym->comGlobalNsMethodName;
                std::string gnsCallExpr = "vb6_ComCallObject(vb6_CreateObject(" + progIdWide + "), L\"" + methodName + "\", NULL, 0)";
                // 2. 设置COM marker, .memberName将在IndexOrCallExpr中消费
                comObjExpr_ = gnsCallExpr;
                comMemberName_ = node.memberName;
                isComMarker_ = true;
                isEarlyBoundCom_ = false;
                earlyBoundSym_ = nullptr;
                lastExpr_ = gnsCallExpr + "  /* GlobalNs." + methodName + " */";
                return;
            }
        }

        // 查找成员名称的符号
        // Fix 031: UDT 变量字段访问 — 必须在 memSym 查找之前拦截.
        // 当 obj 是已知 UDT 变量 (knownUdtVars_) 时, obj.member 总是结构体字段访问
        // (obj.member 或 (*obj).member 形式, 取决于 emitExpr 对该 UDT 变量的求值),
        // 不应进入 memSym 路径把 obj 当作模块名 (或 uOutput 误当类实例).
        // 现象: Private Sub pvTlsBuildClientHello(uCtx As UcsTlsContext, uOutput As UcsBuffer)
        //       其中 UcsBuffer UDT 含 Size As Long 字段, 同时项目里又有 cByteBuffer.cls
        //       声明了 Property Get Size, memSym 查找命中该跨模块属性 → 误生成
        //       vb6_cByteBuffer_Size 函数引用 (而非 (*uOutput).Size 字段访问), 触发 C2065.
        // 已知 UDT 变量与类实例 (knownClassVars_) / Object (knownObjectVars_) 在注册阶段
        // 互斥, 此处不冲突.
        if (knownUdtVars_.count(objLower)) {
            emitExpr(*node.object);
            std::string obj = std::move(lastExpr_);
            // Fix 085: UDT 字段是对象 (项目类/Collection/COM) 时, 在生成的访问文本
            // 上追加 "/* udt objfield <CType> */" 注释标记, 由外层 MemberAccessExpr
            // 消费 → 转类方法调用 / COM dispatch (否则 obj.Field.Method 触发 C2039/
            // C2224). 例: uFile.SourceArchive (As cZipArchive) 后接 .frCopyCompressed.
            lastExpr_ = appendUdtObjFieldMarker(obj, knownUdtVars_[objLower], node.memberName);
            return;
        }

        // Fix 084z-4: 函数名引用返回值的成员访问 (RHS 读取侧, 与 LHS 的
        // Fix 084o-5 对称). VB6 函数体内可用函数名访问返回对象的成员
        // (如 cJson: RootItems.Add NewItem.RootItem, NewItem As cJson) →
        // 展开 vb6_ret_NewItem->RootItem; 而非误当模块名落入 M22 生成
        // vb6_NewItem_RootItem (C2065 未声明).
        if (node.object && node.object->kind == ASTNodeKind::IdentifierExpr) {
            auto& objIdent084z = static_cast<IdentifierExpr&>(*node.object);
            std::string objLower084z = Symbol::toLower(objIdent084z.name);
            if (currentProc_ && objLower084z == Symbol::toLower(currentProc_->name)
                && (currentProc_->kind == SymbolKind::Function || currentProc_->kind == SymbolKind::PropertyGet)
                && !currentReturnVar_.empty()) {
                bool isCls084z = currentReturnCType_.find("vb6_cls_") != std::string::npos;
                if (isCls084z) {
                    // Fix 088d: 函数返回类实例时, 函数名引用的成员可能是方法/属性
                    // (ReturnJson.Decode s) 而非仅公开数据字段 (NewItem.RootItem).
                    // 先尝试类方法/属性分发 (canonical 调用), 找不到才视为字段:
                    //   vb6_cJson_Decode((void*)vb6_ret_ReturnJson, s)
                    // 此前无条件生成 vb6_ret_X->member → C2039 (Decode 不是
                    // vb6_cls_cJson 的成员).
                    std::string retCls088d = currentReturnCType_.substr(8);
                    if (!retCls088d.empty() && retCls088d.back() == '*')
                        retCls088d.pop_back();
                    std::string fn088d =
                        resolveClassMemberCall(retCls088d, node.memberName);
                    if (!fn088d.empty()) {
                        if (asCallCallee_) {
                            pendingChainObj_ = "(void*)" + currentReturnVar_;
                            lastExpr_ = fn088d;  // 外层补参数
                        } else {
                            lastExpr_ = fn088d + "((void*)" + currentReturnVar_ + ")";
                        }
                        return;
                    }
                    // 类无此成员 → 公开数据字段 (RootItem 等)
                    lastExpr_ = currentReturnVar_ + "->" + cIdent(node.memberName);
                } else if (currentReturnCType_ == "void*"
                           || currentReturnCType_.find("vb6_ComIface_") != std::string::npos) {
                    // Fix 089c2: 当前函数返回内置 COM 对象 (As Collection / As Object →
                    // C void*) 时, 函数体内 FuncName.Add(...)/FuncName.Item(...) 走
                    // COM dispatch (vb6_ComCall) — 如 cZipArchive.pvEnumFiles As
                    // Collection → pvEnumFiles.Add path, key. 此前 isCls084z false 走
                    // appendUdtObjFieldMarker(void* 无字段) → 生成
                    // vb6_ret_X.Add(...) 结构成员调用 → C2224 (void* 上 .Add).
                    // 与 Fix 088d (类返回) / Fix 085 (UDT 返回) 对齐.
                    // Fix 090ag: As Dictionary (项目外 COM 类, C 类型 vb6_ComIface_
                    // IDictionary* 而非 void*) 的函数返回同样走 COM dispatch —
                    // ToolsJsonVba json_ParseObject As Dictionary 体内
                    // json_ParseObject.Item(Key) = v 此前生成
                    // vb6_ret_X.Item(...) → C2037 (未定义结构 vb6_ComIface_IDictionary).
                    lastExpr_ = currentReturnVar_;
                    comObjExpr_ = "(void*)" + currentReturnVar_;  // 统一 void* 对象表达式
                    comMemberName_ = node.memberName;
                    isComMarker_ = true;
                    isEarlyBoundCom_ = false;
                    earlyBoundSym_ = nullptr;
                } else {
                    // Fix 085: 当前函数返回 UDT 时, 字段为对象(Collection/COM)需标记,
                    // 使外层链式成员访问走 COM/类方法路径 (例: retUdt.Stream.VfsSetFilePointer)
                    lastExpr_ = appendUdtObjFieldMarker(currentReturnVar_, currentReturnCType_,
                                                        node.memberName);
                }
                return;
            }
        }

        // Fix 086: UDT返回属性的字段读取 (CurLang.Index — Property Get CurLang()
        // As TypeLang). 属性返回结构体右值, C 允许对函数返回值取成员 f().Field;
        // 此前误判为 Module.X → 生成未声明的 vb6_CurLang_Index (C2065).
        // 注意: 仅当对象是当前类的 Property Get 且返回UDT时走此路径.
        if (node.object && node.object->kind == ASTNodeKind::IdentifierExpr && isClassModule_) {
            Symbol* propSym = symTab_.lookupModule(objIdent.name);
            if (propSym && propSym->kind == SymbolKind::PropertyGet
                && !propSym->variableTypeName.empty()) {
                Symbol* udtSym86 = lookupDotted(propSym->variableTypeName);
                if (udtSym86 && udtSym86->kind == SymbolKind::UserDefinedType) {
                    emitExpr(*node.object);
                    std::string objExpr86 = std::move(lastExpr_);
                    lastExpr_ = objExpr86 + "." + cIdent(node.memberName)
                              + "  /* UdtProp." + node.memberName + " */";
                    return;
                }
            }
        }

        auto* memSym = symTab_.lookupModule(node.memberName);
        // Fix 110e: VB6 宿主伪对象 UserControl / Ambient / Extender / PropertyPage 之
        // 后的成员**不是**"别的模块的同名成员". 它们是宿主状态符号, 由
        // rtl/core/vb6rtl/vb6rtl_userctl.h 提供 (vb6_UserControl_* / vb6_Ambient_* /
        // vb6_Extender_* / vb6_PropertyPage_*), 生成名 = vb6_<伪对象名>_<成员>
        // (即下方 M22 分支的形式).
        // 若在此按"模块名.成员"解析, 成员会命中别处注入的同名外部符号 →
        //   UserControl.Enabled    → 命中 LabelPlus 的 Property Get Enabled
        //                            → vb6_LabelPlus_Enabled   (C2065)   ← 实测
        //   Ambient.ForeColor      → vb6_LabelPlus_ForeColor      (C2065)   ← 实测
        // (在 LabelPlus 模块内恰好回退到 sourceMod = "UserControl" 才碰巧正确,
        //  这也是同一表达式在不同模块生成不同符号名、仅部分模块报错的原因.)
        // 跳过本块, 交由 M22 分支以对象名作前缀生成正确的宿主标识符.
        const bool hostPseudoObj110e =
            objLower == "usercontrol" || objLower == "ambient" ||
            objLower == "extender" || objLower == "propertypage";
        if (!hostPseudoObj110e && memSym
            && (memSym->kind == SymbolKind::Sub || memSym->kind == SymbolKind::Function
                    || memSym->kind == SymbolKind::PropertyGet
                    || memSym->kind == SymbolKind::PropertyLet
                    || memSym->kind == SymbolKind::PropertySet)) {

            // 优先级2: 类实例成员访问: obj.Method → vb6_ClassName_MethodName(obj)
            // Fix 010r-10: 使用map查找, 可获取类名用于方法分发
            // Fix 011r-1: 当obj在knownClassVars_中时, 优先用resolveClassMemberCall
            // 精确解析该类的方法/属性, 避免memSym捡错模块的同类同名方法/属性
            // P6.4+: 类模块默认实例 (VB_PredeclaredId=True) 的裸类名 (cTT.CreateToolTip
            // / cTT.EnableTooltips(...) 在 frm 里) — 注册为"类实例变量"并按类精确
            // 解析成员 (注入 me 首参与类型化形参), 对象表达式用 vb6_cls_X_Default().
            std::string defaultInstCls = registerDefaultInstanceClass(objLower);
            auto itClassVar = knownClassVars_.find(objLower);
            if (itClassVar == knownClassVars_.end() && node.object) {
                // Fix 090al: 链式对象 (Db.Sql(s).Param(p).QueryParam) 或局部类变量
                // 未注册 knownClassVars_ → AST 推断类名再分发 (含值上下文补参),
                // 否则无括号裸调用只发 this → C2198 参数太少.
                std::string inferredCls = inferClassTypeOfExpr(*node.object);
                if (!inferredCls.empty()
                    && !resolveClassMemberCall(inferredCls, node.memberName).empty()) {
                    itClassVar = knownClassVars_.emplace(objLower, inferredCls).first;
                }
            }
            if (itClassVar != knownClassVars_.end()) {
                // 用对象的真实类名查找方法, 防止跨模块同名冲突
                std::string resolvedFn = resolveClassMemberCall(itClassVar->second, node.memberName);
                if (!resolvedFn.empty()) {
                    std::string objExpr;
                    if (!defaultInstCls.empty()) {
                        objExpr = "vb6_cls_" + cIdent(defaultInstCls) + "_Default()";
                    } else {
                        emitExpr(*node.object);
                        objExpr = std::move(lastExpr_);
                    }
                    // Fix 089h: 值上下文无括号方法引用 (Trim(FileStream.ReadLine)
                    // → vb6_cToolsStream_ReadLine(me->FileStream) 只发 this) —
                    // C2198 参数太少. 补默认参数 (Optional/必选), 与链式值上下文
                    // / With 分支 (Fix 044a) 一致. asCallCallee_ 由外层补参.
                    // 属性 (prop_get_/prop_let_/prop_set_) 不 pad — findClassMemberCallParams
                    // 对 Get+Let 并存属性返回 Let 参数, 无括号属性读只发 me 即正确
                    // (prop_get_State(oClient)), pad 反而 C2197 参数过多.
                    if (!asCallCallee_
                        && resolvedFn.find("_prop_") == std::string::npos) {
                        std::vector<ParameterInfo> params89h;
                        bool isBuiltin89h = false;
                        if (findClassMemberCallParams(itClassVar->second, node.memberName,
                                                       params89h, isBuiltin89h)
                            && !params89h.empty() && !isBuiltin89h) {
                            std::string argList89h = objExpr;
                            for (size_t i = 0; i < params89h.size(); i++) {
                                const auto& param = params89h[i];
                                argList89h += ", ";
                                std::string defVal89h;
                                if (param.hasDefaultValue && !param.defaultValueExpr.empty()) {
                                    defVal89h = param.defaultValueExpr;
                                } else {
                                    defVal89h = defaultValue(param.type);
                                }
                                if (param.isByVal) {
                                    argList89h += defVal89h;
                                } else {
                                    std::string cType89h = mapType(param.type);
                                    if (param.type == Vb6Type::Variant
                                        || param.type == Vb6Type::Empty
                                        || param.type == Vb6Type::Null
                                        || param.type == Vb6Type::Object) {
                                        argList89h += "&(" + cType89h + "){0}";
                                    } else {
                                        argList89h += "&(" + cType89h + "){" + defVal89h + "}";
                                    }
                                }
                            }
                            for (size_t i = 0; i < params89h.size(); i++) {
                                const auto& param = params89h[i];
                                if (param.isOptional && !param.isParamArray) {
                                    argList89h += ", 0";
                                }
                            }
                            lastExpr_ = resolvedFn + "(" + argList89h + ")";
                        } else {
                            lastExpr_ = resolvedFn + "(" + objExpr + ")";
                        }
                    } else {
                        lastExpr_ = resolvedFn + "(" + objExpr + ")";
                    }
                    return;
                }
                // resolveClassMemberCall返回空 → 该类中无此方法/属性 → 数据字段访问
                // 落入下方cross-module fallback分支处理 obj->member
            }

            // 优先级3: 模块名.方法名: MathUtils.Add → vb6_MathUtils_Add
            //    object名称不是已知变量, 但成员是函数 → 视为模块限定调用
            bool isVarName = false;
            auto* objSym = symTab_.lookup(objIdent.name);
            if (!objSym) objSym = symTab_.lookupModule(objIdent.name);
            if (objSym && (objSym->kind == SymbolKind::Variable || objSym->kind == SymbolKind::Parameter)) {
                isVarName = true;
            }
            // Fix 084x: 补充检查cgen层跟踪集合 (与M22分支一致), 防止过程级
            // 局部类变量/UDT变量被误判为"模块名", 落入优先级3 "模块名.方法名"
            // 用 memSym->sourceModule 生成 vb6_<他类>_<member> 函数名 (C2065).
            // 例: oCallback.Socket.Accept 中 oCallback 是 Dim As cClientCallback 局部
            // 变量, 不在模块作用域符号表 → 原判定 isVarName=false, memSym 命中
            // cTlsSocket.Socket 属性 → 生成 vb6_cTlsSocket_Socket (错误).
            if (!isVarName && knownUdtVars_.count(objLower)) isVarName = true;
            if (!isVarName && knownClassVars_.find(objLower) != knownClassVars_.end()) isVarName = true;
            if (!isVarName && knownNewVars_.count(objLower)) isVarName = true;
            if (!isVarName && knownObjectVars_.count(objLower)) isVarName = true;
            if (!isVarName && knownTypedComVars_.count(objLower)) isVarName = true;
            if (!isVarName && knownIfaceVars_.count(objLower)) isVarName = true;

            // Fix 083c: obj 是当前模块的属性(返回类对象) → pvSocket.GetLocalHost(...)
            // 不能按"模块名.方法"处理(会丢失对象实例导致 me 参数缺失 C2198),
            // 生成: vb6_<ObjClass>_<Method>((void*)vb6_<Mod>_prop_get_pvSocket(me), args...)
            if (!isVarName && objSym && objSym->kind == SymbolKind::PropertyGet &&
                !objSym->variableTypeName.empty()) {
                std::string propMod = objSym->isExternal ? objSym->sourceModule
                                    : (isClassModule_ ? moduleName_ : "");
                if (!propMod.empty()) {  // 仅类模块属性带 me
                    std::string propClass = objSym->variableTypeName;
                    std::string resolvedFn = resolveClassMemberCall(propClass, node.memberName);
                    if (!resolvedFn.empty()) {
                        std::string propFn = cProcName("prop_get_" + objIdent.name, objSym->access, propMod);
                        std::string objExpr = propFn + "((void*)me)";
                        lastExpr_ = resolvedFn + "((void*)" + objExpr + ")";
                        return;
                    }
                }
            }

            // Fix 086: 成员命中本模块符号但限定符是另一个模块名时, 优先在该模块
            // 中解析成员常量 (modSerialPortAPI.SetDTR — 本模块恰有同名 Sub SetDTR,
            // 误生成 vb6_modSerialPortAPI_SetDTR → C2065; 实际取的是模块常量 SETDTR=5).
            if (!isVarName && !memSym->isExternal) {
                std::string qModLower = Symbol::toLower(objIdent.name);
                std::string curModLower86 = Symbol::toLower(moduleName_);
                if (!qModLower.empty() && qModLower != curModLower86 && symTab_.moduleScope()) {
                    const std::string want86 = Symbol::toLower(node.memberName);
                    bool qResolved = false;
                    for (const auto& [qkey, qsym] : symTab_.moduleScope()->symbols()) {
                        if (!qsym || !qsym->isExternal) continue;
                        if (qsym->lowerName != want86 && Symbol::toLower(qsym->name) != want86) continue;
                        if (Symbol::toLower(qsym->sourceModule) != qModLower) continue;
                        if (qsym->kind == SymbolKind::Constant && qsym->hasConstValue) {
                            if (qsym->constType == Vb6Type::Long || qsym->constType == Vb6Type::Integer
                                || qsym->constType == Vb6Type::Boolean || qsym->constType == Vb6Type::Byte
                                || qsym->constType == Vb6Type::Error) {
                                lastExpr_ = std::to_string(qsym->constIntValue);
                                return;
                            }
                            if (qsym->constType == Vb6Type::Single || qsym->constType == Vb6Type::Double) {
                                lastExpr_ = std::to_string(qsym->constFloatValue);
                                return;
                            }
                        }
                        break;
                    }
                    // 注入被本地同名符号阻止时, 查 driver 预扫描的模块公共常量表
                    if (!qResolved && modulePublicConsts_) {
                        auto itMod = modulePublicConsts_->find(qModLower);
                        if (itMod != modulePublicConsts_->end()) {
                            auto itVal = itMod->second.find(want86);
                            if (itVal != itMod->second.end()) {
                                lastExpr_ = std::to_string(itVal->second);
                                return;
                            }
                        }
                    }
                }
            }

            if (!isVarName) {
                // 确定函数名的模块前缀
                std::string sourceMod;
                // Fix 092v: `Common.Version()` — 限定符是**模块名**时以限定模块为准.
                // Common4DLL.bas 与 cVBMAN.cls 都有 Version: lookupModule("Version")
                // 命中 cVBMAN.Version → 前缀取 memSym->sourceModule = "cVBMAN" →
                // vb6_cVBMAN_Version(...) 少了 me 参数 → C2198
                // (cHttpServerResponse.cls 440; 实参已按 Common 版形参正确生成
                // vb6_Common_Version(void**, int)). 这里改用限定符对应的规范模块名.
                std::string modCanon092v;
                {
                    const std::string objL092v = Symbol::toLower(objIdent.name);
                    if (symTab_.moduleScope()) {
                        for (const auto& [k092v, s092v] : symTab_.moduleScope()->symbols()) {
                            if (!s092v || !s092v->isExternal) continue;
                            if (Symbol::toLower(s092v->sourceModule) == objL092v) {
                                modCanon092v = s092v->sourceModule;
                                break;
                            }
                        }
                    }
                    // Fix 092y: 兜底 — 限定符本身就是工程内的**标准模块**名时, 直接
                    // 采用其规范名. 当该模块的公开过程与其它类的成员**全部**同键冲突时
                    // (Common 的 Version/Path 与 cVBMAN 的 Version/Path 同键,
                    // storageKey(Function)=lowerName → defineExternal 先到先得, Common
                    // 的两个成员全被丢弃), 本模块作用域内不存在任何
                    // sourceModule=="Common" 的符号 → 上面的扫描落空 → sourceMod 误取
                    // memSym->sourceModule ("cVBMAN") → vb6_cVBMAN_Version 少 me 参数 →
                    // C2198 (cHttpServerResponse.cls 440 Common.Version()).
                    // 仅对非类模块生效, 不改变 类名.成员 / 窗体名.成员 的现有语义.
                    if (modCanon092v.empty()) {
                        for (const auto& extMod092y : externalModules_) {
                            if (Symbol::toLower(extMod092y) != objL092v) continue;
                            Symbol* extSym092y = symTab_.lookup(extMod092y);
                            if (extSym092y && extSym092y->kind == SymbolKind::Class) break;
                            modCanon092v = extMod092y;
                            break;
                        }
                    }
                }
                if (!modCanon092v.empty()) {
                    sourceMod = (Symbol::toLower(modCanon092v) == Symbol::toLower(moduleName_))
                                ? "" : modCanon092v;
                } else if (memSym->isExternal) {
                    sourceMod = memSym->sourceModule;
                } else {
                    // 同模块调用也允许 Module.Method 语法
                    // 检查object名是否匹配当前模块名
                    std::string modLower = moduleName_;
                    std::transform(modLower.begin(), modLower.end(), modLower.begin(), ::tolower);
                    if (objLower == modLower) {
                        sourceMod = "";  // 同模块, 不需要前缀
                    } else {
                        sourceMod = objIdent.name;  // 假设object名就是模块名
                    }
                }
                std::string funcName = cProcName(node.memberName, memSym->access, sourceMod);
                lastExpr_ = funcName;  // 仅输出函数名, 参数由IndexOrCallExpr添加
                return;
            }
        }
    }

    // M22: 跨模块变量访问 Module1.myName → vb6_Module1_myName
    // 优先级4: object不是已知变量, 成员也不是函数 → 模块名.变量名
    if (node.object && node.object->kind == ASTNodeKind::IdentifierExpr) {
        auto& objIdent2 = static_cast<IdentifierExpr&>(*node.object);
        std::string objLower2 = objIdent2.name;
        std::transform(objLower2.begin(), objLower2.end(), objLower2.begin(), ::tolower);
        bool isVarName2 = false;
        auto* objSym2 = symTab_.lookup(objIdent2.name);
        if (!objSym2) objSym2 = symTab_.lookupModule(objIdent2.name);
        if (objSym2 && (objSym2->kind == SymbolKind::Variable || objSym2->kind == SymbolKind::Parameter)) {
            isVarName2 = true;
        }
        // M22-fix: symTab_在模块作用域, 找不到过程级局部变量
        // 补充检查cgen层的跟踪集合: UDT变量、类实例变量、Dim As New变量、COM对象变量
        if (!isVarName2 && knownUdtVars_.count(objLower2)) isVarName2 = true;
        if (!isVarName2 && knownClassVars_.find(objLower2) != knownClassVars_.end()) isVarName2 = true;
        if (!isVarName2 && knownNewVars_.count(objLower2)) isVarName2 = true;
        if (!isVarName2 && knownObjectVars_.count(objLower2)) isVarName2 = true;
        if (!isVarName2 && knownTypedComVars_.count(objLower2)) isVarName2 = true;
        if (!isVarName2 && knownIfaceVars_.count(objLower2)) isVarName2 = true;
        // Fix 056b: 属性符号也是有效对象表达式 (PropertyGet/Let/Set),
        // 不能当作"模块名限定符"处理, 否则 LastError.Number → vb6_LastError_Number (C2065)
        if (!isVarName2 && objSym2 &&
            (objSym2->kind == SymbolKind::PropertyGet ||
             objSym2->kind == SymbolKind::PropertyLet ||
             objSym2->kind == SymbolKind::PropertySet)) {
            isVarName2 = true;
        }
        // Fix 084y-8: 枚举类型名.成员 (EnumLogLevel.LvCustom) — 枚举类型不是
        // 变量也不是模块, 原逻辑落入模块名限定符 → vb6_EnumLogLevel_LvCustom
        // (C2065, 声明名是 vb6_enum_EnumLogLevel_LvCustom 带 enum_ 层). 成员
        // 有常量值则内联数值 (LvCustom=5), 否则生成声明同构名.
        if (!isVarName2 && objSym2 && objSym2->kind == SymbolKind::EnumType) {
            Symbol* enumMemSym = symTab_.lookup(node.memberName);
            if (enumMemSym && enumMemSym->kind == SymbolKind::EnumMember
                && enumMemSym->hasConstValue) {
                lastExpr_ = std::to_string(enumMemSym->constIntValue);
                return;
            }
            lastExpr_ = "vb6_enum_" + cIdent(objIdent2.name) + "_" + cIdent(node.memberName);
            return;
        }

        if (!isVarName2) {
            // object不是已知变量 → 假设是模块名限定符
            // 成员是变量: Module1.myName
            // When module is #included, use the unprefixed name directly
            std::string modName2 = objIdent2.name;
            std::string varName2 = cIdent(node.memberName);
            std::string modLower2 = modName2;
            std::transform(modLower2.begin(), modLower2.end(), modLower2.begin(), ::tolower);
            // Fix 086: 在指定模块中解析成员实际符号 (大小写无关).
            // 1) 常量: 内联数值 (跨模块常量引用 modSerialPortAPI.SetDTR —
            //    模块头文件只发 #define SETDTR, 无 vb6_<mod>_<name> 标识符,
            //    且使用处大小写可能与 #define 不同 → C2065).
            // 2) 变量: 用符号的规范大小写 (extern 声明名与使用处不同).
            Symbol* qSym = nullptr;
            if (symTab_.moduleScope()) {
                const std::string wantLower = Symbol::toLower(node.memberName);
                for (const auto& [qkey, qsym] : symTab_.moduleScope()->symbols()) {
                    if (!qsym) continue;
                    if (qsym->lowerName != wantLower && Symbol::toLower(qsym->name) != wantLower) continue;
                    if (!qsym->isExternal || Symbol::toLower(qsym->sourceModule) != modLower2) continue;
                    qSym = qsym.get();
                    break;
                }
            }
            if (qSym && qSym->kind == SymbolKind::Constant && qSym->hasConstValue) {
                if (qSym->constType == Vb6Type::Long || qSym->constType == Vb6Type::Integer
                    || qSym->constType == Vb6Type::Boolean || qSym->constType == Vb6Type::Byte
                    || qSym->constType == Vb6Type::Error) {
                    lastExpr_ = std::to_string(qSym->constIntValue);
                    return;
                }
                if (qSym->constType == Vb6Type::Single || qSym->constType == Vb6Type::Double) {
                    lastExpr_ = std::to_string(qSym->constFloatValue);
                    return;
                }
                varName2 = cIdent(qSym->name);
            } else if (qSym && qSym->kind == SymbolKind::Variable) {
                varName2 = cIdent(qSym->name);
            } else if (!qSym && modulePublicConsts_) {
                // Fix 086: 注入被本地同名符号阻止时, 查 driver 预扫描常量表
                auto itMod = modulePublicConsts_->find(modLower2);
                if (itMod != modulePublicConsts_->end()) {
                    auto itVal = itMod->second.find(Symbol::toLower(node.memberName));
                    if (itVal != itMod->second.end()) {
                        lastExpr_ = std::to_string(itVal->second);
                        return;
                    }
                }
            }
            bool isIncluded2 = false;
            for (const auto& extMod : externalModules_) {
                std::string extLower = extMod;
                std::transform(extLower.begin(), extLower.end(), extLower.begin(), ::tolower);
                if (extLower == modLower2) { isIncluded2 = true; break; }
            }
            lastExpr_ = isIncluded2 ? varName2 : ("vb6_" + cIdent(modName2) + "_" + varName2);
            return;
        }
    }

    // Fix 083d: 函数调用结果上的方法调用 — pvToSocket(.SocketPtr(lIdx)).frNotifyGetHostByName(...)
    // object 是非标识符表达式(方法调用返回类实例) → 用类型推断得到类名,
    // 生成全局方法调用 vb6_<Class>_<Method>((void*)<objExpr>, args...),
    // 避免生成 objExpr.member(args) 非法 C (C2039/结构体无此成员)
    // Fix 084y-4: 调用上下文 (asCallCallee_=true, 如 db.Sql(s).Exec(...)) 时跳过本
    // 分支 — 它生成 func((void*)objExpr) 只含 this 指针, 绕过外层 IndexOrCallExpr
    // 的 Optional 默认参数填充 → C2198 参数太少 (cDataBase.Exec 声明5参却传1参).
    // 调用上下文应交给 Fix 015 (IndexOrCallExpr 链 → pendingChainObj_ 裸函数名,
    // 由外层补齐参数). 值上下文 (bX = obj.Fn().Prop) 才走本分支.
    if (node.object && node.object->kind != ASTNodeKind::IdentifierExpr && !asCallCallee_) {
        std::string className = inferClassTypeOfExpr(*node.object);
        if (!className.empty()) {
            std::string resolvedFn = resolveClassMemberCall(className, node.memberName);
            if (!resolvedFn.empty()) {
                emitExpr(*node.object);
                std::string objExpr = std::move(lastExpr_);
                // Fix 090am: 值上下文无括号方法引用 (If Db.Sql(s).Param(p).QueryParam Then)
                // — object 是非标识符链式调用 → 本 Fix 083d 分支只发 this ((void*)obj)
                // → C2198 参数太少 (QueryParam 声明5参传1参). 与 Fix 089h/090ak 同模板:
                // 按 findClassMemberCallParams 形参表补默认值. 属性 (prop_get_/prop_let_)
                // 不 pad (无括号属性读只发 this 即正确, pad 反致 C2197).
                std::vector<ParameterInfo> params090am;
                bool isB090am = false;
                if (resolvedFn.find("_prop_") == std::string::npos
                    && findClassMemberCallParams(className, node.memberName,
                                                 params090am, isB090am)
                    && !params090am.empty() && !isB090am) {
                    std::string argList090am = "(void*)" + objExpr;
                    for (size_t i = 0; i < params090am.size(); i++) {
                        const auto& param = params090am[i];
                        argList090am += ", ";
                        std::string defVal090am;
                        if (param.hasDefaultValue && !param.defaultValueExpr.empty()) {
                            defVal090am = param.defaultValueExpr;
                        } else {
                            defVal090am = defaultValue(param.type);
                        }
                        if (param.isByVal) {
                            argList090am += defVal090am;
                        } else {
                            std::string cType090am = mapType(param.type);
                            if (param.type == Vb6Type::Variant
                                || param.type == Vb6Type::Empty
                                || param.type == Vb6Type::Null
                                || param.type == Vb6Type::Object) {
                                argList090am += "&(" + cType090am + "){0}";
                            } else {
                                argList090am += "&(" + cType090am + "){" + defVal090am + "}";
                            }
                        }
                    }
                    for (size_t i = 0; i < params090am.size(); i++) {
                        const auto& param = params090am[i];
                        if (param.isOptional && !param.isParamArray) {
                            argList090am += ", 0";
                        }
                    }
                    lastExpr_ = resolvedFn + "(" + argList090am + ")";
                } else {
                    lastExpr_ = resolvedFn + "((void*)" + objExpr + ")";
                }
                return;
            }
        }
    }

    // 通用成员访问 (结构体字段 / 链式COM访问)
    emitExpr(*node.object);

    // 链式COM调用检测: 如果object求值产生COM标记, 先resolve为对象值
    // (fso.GetFolder("x") → vb6_ComCallObject → 返回IDispatch*)
    // 然后在新对象上访问成员 → 设置新的COM标记
    if (isComMarker_) {
        // object是COM属性访问, 解析为对象值
        resolveComValue("Object");
        // 检查resolveComValue后的结果是否是对象表达式 (ComCallObject/ComGetObjectProp)
        // 如果是, 说明这是一个链式COM对象访问, 设置新的COM标记
        std::string objExpr = lastExpr_;
        // 检测是否是COM对象表达式 (由ComCallObject/ComGetObjectProp返回的void*)
        // 这些都是void*类型, 可以作为COM对象继续访问成员
        if (objExpr.find("vb6_ComCallObject(") == 0 ||
            objExpr.find("vb6_ComGetObjectProp(") == 0 ||
            objExpr.find("vb6_CreateObject(") == 0) {
            // 链式COM: 设置COM标记, objExpr是中间对象表达式
            comObjExpr_ = objExpr;
            comMemberName_ = node.memberName;
            isComMarker_ = true;
            lastExpr_ = objExpr;  // 保持对象表达式
            return;
        }
        // 非COM对象值, 按结构体字段处理
        lastExpr_ = objExpr + "." + cIdent(node.memberName);
        return;
    }

    std::string obj = std::move(lastExpr_);

    // Fix 090af: Variant 数组元素 (VB6_SA_AT(vb6_VARIANT, arr, i)) 的对象成员访问.
    // VB6: Dim d() As Variant ... If TypeOf d(0) Is cJson Then Set d(0) = d(0).Root
    // d(0) 持有对象时 .member 是后期绑定调用 (同 knownVariantVars_ 标识符路径
    // P24-04 1753). 此前落入通用 fallback 生成 VB6_SA_AT(...).member 结构体字段
    // 访问 → C2039 (vb6_VARIANT 无该成员) + 级联 C2198/C2440.
    if (obj.find("VB6_SA_AT(vb6_VARIANT,") == 0) {
        std::string varRef = "&(" + obj + ")";
        comObjExpr_ = "vb6_VariantToObject(" + varRef + ")";
        comMemberName_ = node.memberName;
        isComMarker_ = true;
        lastExpr_ = "vb6_VariantFromComResult(vb6_ComGetProp(" + comObjExpr_
                  + ", L\"" + node.memberName + "\"))";
        return;
    }

    // 链式COM检测2: object求值结果是COM对象表达式
    // (从IndexOrCallExpr产生的COM调用结果, 是void*类型的IDispatch*)
    if (obj.find("vb6_ComCallObject(") == 0 ||
        obj.find("vb6_ComGetObjectProp(") == 0 ||
        obj.find("vb6_ComCall(") == 0 ||
        obj.find("vb6_ComGetProp(") == 0) {
        // 对于vb6_ComCall/vb6_ComGetProp, 需要先解封为对象
        // 链式调用: ComCall返回VARIANT*, 需ComCallObject才能拿到IDispatch*
        std::string resolvedObj = obj;
        if (obj.find("vb6_ComCall(") == 0) {
            // vb6_ComCall → vb6_ComCallObject (同一参数, 返回void*而非VARIANT*)
            resolvedObj = "vb6_ComCallObject" + obj.substr(strlen("vb6_ComCall"));
        } else if (obj.find("vb6_ComGetProp(") == 0) {
            // vb6_ComGetProp → vb6_ComGetObjectProp
            resolvedObj = "vb6_ComGetObjectProp" + obj.substr(strlen("vb6_ComGetProp"));
        }
        comObjExpr_ = resolvedObj;
        comMemberName_ = node.memberName;
        isComMarker_ = true;
        lastExpr_ = resolvedObj;
        return;
    }

    // Fix 085: UDT 对象字段链的成员访问.
    // 内层 UDT 字段访问 (Fix 031 / Fix 084z-4 / WithMemberExpr / 通用 fallback) 在
    // 检测到字段是对象 (项目类 vb6_cls_X* / Collection·COM void*) 时, 于生成的访问
    // 文本后追加 "/* udt objfield <CType> */" 注释标记. 此处外层 MemberAccessExpr
    // 消费该标记:
    //   - vb6_cls_X* (项目类)  → 类方法/属性/数据字段: vb6_cZipArchive_frCopyCompressed
    //   - void* (Collection/COM)→ COM dispatch marker (同 Fix 023 机制)
    // 例: uFile.SourceArchive.frCopyCompressed(...)  (uFile.SourceArchive As cZipArchive)
    //     → vb6_cZipArchive_frCopyCompressed((void*)(uFile.SourceArchive), ...)
    //   withCtx.LocalCertificates.Item(lIdx)  (As Collection)
    //     → vb6_ComCall...(withCtx->LocalCertificates, L"Item", ...)
    if (obj.find("  /* udt objfield ") != std::string::npos) {
        size_t mkPos = obj.find("  /* udt objfield ");
        std::string objExpr = obj.substr(0, mkPos);
        std::string fldType = obj.substr(mkPos + 18);  // 跳过 "  /* udt objfield "
        size_t endMark = fldType.find(" */");
        if (endMark != std::string::npos) fldType = fldType.substr(0, endMark);
        if (fldType.rfind("vb6_cls_", 0) == 0) {
            // 项目类对象字段 (early bound): obj.Field.Method → vb6_Class_Method((void*)obj)
            // 注意: "vb6_cls_" 为 8 字符, 类名紧随其后 (vb6_cls_cZipArchive* → cZipArchive).
            std::string clsName = fldType.substr(8);
            if (!clsName.empty() && clsName.back() == '*') clsName.pop_back();
            std::string resolvedFn = resolveClassMemberCall(clsName, node.memberName);
            if (!resolvedFn.empty()) {
                std::string thisArg = "(void*)" + objExpr;
                if (asCallCallee_) {
                    // 由外层 IndexOrCallExpr 把 thisArg 前置到参数首 (同 Fix 015)
                    pendingChainObj_ = thisArg;
                    lastExpr_ = resolvedFn;
                } else {
                    lastExpr_ = resolvedFn + "(" + thisArg + ")";
                }
                return;
            }
            // 类中无此方法/属性 → 类数据字段 (Public Field): objExpr->field
            lastExpr_ = objExpr + "->" + cIdent(node.memberName);
            return;
        }
        if (fldType == "void*") {
            // Collection/COM 对象字段 → 下游 IndexOrCallExpr/BinaryExpr 走 COM dispatch
            comObjExpr_ = objExpr;
            comMemberName_ = node.memberName;
            isComMarker_ = true;
            lastExpr_ = objExpr;
            return;
        }
        // 兜底 (嵌套 UDT 等非对象标记不应出现) → 普通字段
        lastExpr_ = objExpr + "." + cIdent(node.memberName);
        return;
    }

    // Fix 023: void* struct 字段访问结果作为 COM 对象使用.
    // 内层 MemberAccessExpr 的 knownClassVars_ fallback 路径 (见本函数尾部) 在检测到
    // 当前访问的类字段是 void* 时, 在 emit 的表达式中加入 "voidptr" 注释标记:
    //   例: Db.Rs  →  "Db->Rs  /* class var .Rs field voidptr */"
    // 此处 outer MemberAccessExpr 检测该标记, 将其视为 void* IDispatch* 指针,
    // 设置 COM marker 让下游 (IndexOrCallExpr / BinaryExpr / resolveComValue) 通过
    // vb6_ComCall/vb6_ComGet*Prop 走 COM dispatch 通道:
    //   Db.Rs.EOF         → vb6_ComGetXXXProp(Db->Rs..., L"EOF")
    //   Db.Rs.FileExists()→ vb6_ComCallXXX(Db->Rs..., L"FileExists", args, argc)
    // 注: class_voidptr 的注释会被 MSVC 视为空白, 不影响 C 代码语义.
    if (obj.find("/* class var .") != std::string::npos) {
        if (obj.find(" voidptr */") != std::string::npos) {
            comObjExpr_ = obj;
            comMemberName_ = node.memberName;
            isComMarker_ = true;
            lastExpr_ = obj;  // 保持 void* 对象表达式供外层使用
            return;
        }
        // Fix 088b: typed 类对象字段链 — obj 是 "objExpr  /* class var .X field */"
        // (无 voidptr 注释), 字段是项目类实例 (vb6_cls_Y*). 外层 .Method/Property 应
        // 解析为 canonical 调用, 与 udt objfield 的 vb6_cls_ 分支同机制. 此前只有
        // void* (COM) 字段被消费, typed 字段 (如 oCallback.Socket As cTlsReMaster)
        // 落入主 fallback 生成 obj'.'member → C2039 (Accept 不是 vb6_cls_cTlsReMaster
        // 的成员). 例: oCallback.Socket.Accept requestId
        //   → vb6_cTlsReMaster_Accept((void*)oCallback->Socket, &requestId)
        std::string objExpr = obj;
        size_t mkPos = obj.find("  /* class var .");
        if (mkPos != std::string::npos) objExpr = obj.substr(0, mkPos);
        std::string fieldCls =
            node.object ? inferClassTypeOfExpr(*node.object) : "";
        if (!fieldCls.empty()) {
            std::string resolvedFn =
                resolveClassMemberCall(fieldCls, node.memberName);
            if (!resolvedFn.empty()) {
                std::string thisArg = "(void*)" + objExpr;
                if (asCallCallee_) {
                    // 由外层 IndexOrCallExpr/CallStmt 把 thisArg 前置到参数首 (同 Fix 015)
                    pendingChainObj_ = thisArg;
                    lastExpr_ = resolvedFn;
                } else {
                    // Fix 090ak: 值上下文无括号裸调用 (If Db.Sql(s).Param(p).QueryParam Then) —
                    // 对象链尾段方法经 Fix 088b typed 字段链分支 (此分支先于 Fix 015/089h
                    // 命中) 到达此处, 只发 this 会 C2198 参数太少 (QueryParam 声明 5 参传 1 参).
                    // 按形参表补 Optional 默认值 (同 Fix 089h 2404 分支模板).
                    std::vector<ParameterInfo> params090;
                    bool isB090 = false;
                    if (resolvedFn.find("_prop_") == std::string::npos
                        && findClassMemberCallParams(fieldCls, node.memberName,
                                                     params090, isB090)
                        && !params090.empty() && !isB090) {
                        std::string argList090 = thisArg;
                        for (size_t i = 0; i < params090.size(); i++) {
                            const auto& param = params090[i];
                            argList090 += ", ";
                            std::string defVal090;
                            if (param.hasDefaultValue && !param.defaultValueExpr.empty()) {
                                defVal090 = param.defaultValueExpr;
                            } else {
                                defVal090 = defaultValue(param.type);
                            }
                            if (param.isByVal) {
                                argList090 += defVal090;
                            } else {
                                std::string cType090 = mapType(param.type);
                                if (param.type == Vb6Type::Variant
                                    || param.type == Vb6Type::Empty
                                    || param.type == Vb6Type::Null
                                    || param.type == Vb6Type::Object) {
                                    argList090 += "&(" + cType090 + "){0}";
                                } else {
                                    argList090 += "&(" + cType090 + "){" + defVal090 + "}";
                                }
                            }
                        }
                        for (size_t i = 0; i < params090.size(); i++) {
                            const auto& param = params090[i];
                            if (param.isOptional && !param.isParamArray) {
                                argList090 += ", 0";
                            }
                        }
                        lastExpr_ = resolvedFn + "(" + argList090 + ")";
                    } else {
                        lastExpr_ = resolvedFn + "(" + thisArg + ")";
                    }
                }
                return;
            }
            // 类中无此方法/属性 → 类数据字段 (Public Field): objExpr->field
            lastExpr_ = objExpr + "->" + cIdent(node.memberName);
            return;
        }
        // 字段类型推断失败 → 按结构体数据字段访问
        lastExpr_ = objExpr + "->" + cIdent(node.memberName);
        return;
    }

    // Fix 015: 类方法链式调用 — db.Sql(s).Exec(...) 模式
    // node.object 是 IndexOrCallExpr, 其 callee 是 MemberAccessExpr.
    // 通过 inferClassTypeOfExpr 递归推断 node.object 求值后的类类型:
    //   - 从最内层 IdentifierExpr 起在 knownClassVars_ 中拿到 base 类名
    //   - 逐层用 getClassMethodReturnType 查方法的返回类型名 (Fix 015 在
    //     semantic_analyzer 给 Function/PropertyGet 补了 variableTypeName)
    // 若 node.object 确实返回类实例, 用 resolveClassMemberCall 分发外层成员.
    // 类方法返回的是 vb6_cls_X* 指针, 链上中间结果可直接作为下一段的 this
    // 指针透传 (嵌套函数调用), 无需复合字面量:
    //   db.Sql(s).Exec(args) → vb6_cDataBase_Exec((void*)vb6_cDataBase_Sql(db, s), args)
    // 注: 链上的每一段 (Sql→Param→Exec 等) 都会经此分支处理, 嵌套调用本身合法.
    if (node.object && node.object->kind == ASTNodeKind::IndexOrCallExpr) {
        std::string retClassName = inferClassTypeOfExpr(*node.object);
        if (!retClassName.empty()) {
            // 内层调用返回类实例 (vb6_cls_X* 指针) → 直接作为 this 参数透传.
            std::string wrappedObj = obj;

            std::string resolvedFn =
                resolveClassMemberCall(retClassName, node.memberName);

            // 关键决策点: 外层是否要把本节点当作 callee 调用?
            // - asCallCallee_=true: visit(IndexOrCallExpr)/visit(CallStmt) 会随后附加
            //   用户参数 + Optional 默认值填充. 此时只 emit 裸函数名, wrappedObj 走
            //   pendingChainObj_ 通道在 IndexOrCallExpr / CallStmt 里前置. 这样可
            //   正确生成 vb6_cDataBase_Exec(wrappedObj, def1, def2, ...) (5个参数).
            // - asCallCallee_=false: 上下文是值引用 (bX = obj.Sql(s).Prop), 没有外层
            //   call 来填补默认值. 此时只能退化为 func(wrappedObj) 形式 (只有 this 指针,
            //   默认参数缺失). 与非链式 obj.Method (无括号) 的 Priority 2 路径行为一致,
            //   同样需要后续统一改进.
            if (asCallCallee_) {
                if (!resolvedFn.empty()) {
                    pendingChainObj_ = wrappedObj;
                    lastExpr_ = resolvedFn;  // 裸函数名, 由外层补全
                } else {
                    // 数据成员但被当作 callee 调用 — 罕见, 保持 wrappedObj->member 形式
                    pendingChainObj_.clear();
                    lastExpr_ = wrappedObj + "->" + cIdent(node.memberName);
                }
            } else {
                if (!resolvedFn.empty()) {
                    // 值上下文 (If Db.Sql(s).Param(p).QueryParam Then 等无括号裸引用):
                    // emit 完整 func(wrappedObj) 形式 — 但要补默认参数. 仅发 this 会
                    // C2198 参数太少 (QueryParam 声明5参传1参). 参数表经
                    // findClassMemberCallParams 精确取得 (Fix 033 优先级与
                    // resolveClassMemberCall 一致); 无参属性 Get 读 (params 空) 不受影响.
                    // Fix 089h: Optional/必选参数按默认值补全 (同 With 类 Fix 044a),
                    // 必选参数在 VB 无括号引用时非法, 但补默认值可保持可编译.
                    std::vector<ParameterInfo> params89h;
                    bool isBuiltin89h = false;
                    if (resolvedFn.find("_prop_") == std::string::npos
                        && findClassMemberCallParams(retClassName, node.memberName,
                                                   params89h, isBuiltin89h)
                        && !params89h.empty() && !isBuiltin89h) {
                        std::string argList89h = wrappedObj;
                        for (size_t i = 0; i < params89h.size(); i++) {
                            const auto& param = params89h[i];
                            argList89h += ", ";
                            std::string defVal89h;
                            if (param.hasDefaultValue && !param.defaultValueExpr.empty()) {
                                defVal89h = param.defaultValueExpr;
                            } else {
                                defVal89h = defaultValue(param.type);
                            }
                            if (param.isByVal) {
                                argList89h += defVal89h;
                            } else {
                                std::string cType89h = mapType(param.type);
                                if (param.type == Vb6Type::Variant
                                    || param.type == Vb6Type::Empty
                                    || param.type == Vb6Type::Null
                                    || param.type == Vb6Type::Object) {
                                    argList89h += "&(" + cType89h + "){0}";
                                } else {
                                    argList89h += "&(" + cType89h + "){" + defVal89h + "}";
                                }
                            }
                        }
                        for (size_t i = 0; i < params89h.size(); i++) {
                            const auto& param = params89h[i];
                            if (param.isOptional && !param.isParamArray) {
                                argList89h += ", 0";
                            }
                        }
                        lastExpr_ = resolvedFn + "(" + argList89h + ")";
                    } else {
                        lastExpr_ = resolvedFn + "(" + wrappedObj + ")";
                    }
                } else {
                    lastExpr_ = wrappedObj + "->" + cIdent(node.memberName);
                }
            }
            return;
        }
    }

    // Fix 010r-10: 类实例变量的成员访问 fallback
    // 需要区分: 数据字段(obj->member) vs 方法/属性(vb6_ClassName_MethodName(obj))
    // Fix 011r-1: 用 resolveClassMemberCall 精确解析 (避免原 vb6_cls_ 前缀bug
    // 同时处理Pattern D1 — 跨模块类Public数据字段访问)
    // Fix 014: 链式类成员访问 — 如 me.m_oSocket.Create(...)
    // emitExpr(node.object) 后 obj 可能是 "me->m_oSocket" 这样的 C 表达式,
    // 而 knownClassVars_ 中的 key 注册的只是简单名 "m_osocket" (不带 me-> 前缀),
    // 因此完整 objLower 在 map 中找不到. 此时提取 obj 尾部标识符
    // (即最后一个 "->" 或 "." 之后的标识符) 再做一次 fallback 查找,
    // 即可解析出 obj 的真实类类型, 让方法分发正确工作.
    {
        // Fix 026: 剥掉 (*name) 解引用外层 (ByRef class/UDT 参数 emit 形式为 (*name)).
        // ByRef class 在 C 中是 cls**, (*name) 得 cls*, 应走类成员路径 (obj->member).
        // ByRef UDT 在 C 中是 struct_t*, (*name) 得 struct_t, struct.field 仍正确.
        // 仅剥完整 (*X) 外层; 其他形式 (me->X / a.b / 直接 X) 不动.
        std::string objBase = obj;
        if (objBase.size() > 4 && objBase[0] == '(' && objBase[1] == '*'
            && objBase.back() == ')') {
            objBase = objBase.substr(2, objBase.size() - 3);
        }
        std::string objLower = objBase;
        std::transform(objLower.begin(), objLower.end(), objLower.begin(), ::tolower);
        auto itClassVar = knownClassVars_.find(objLower);

        // Fix 014: 链式成员访问 — 若完整 objLower 找不到, 尝试提取尾部标识符
        // 例: obj = "me->m_oSocket" → trailingLower = "m_osocket"
        //     obj = "vb6_x()->y"     → trailingLower = "y"
        //     obj = "(*p).field"     → trailingLower = "field"
        std::string trailingLower;
        if (itClassVar == knownClassVars_.end()) {
            size_t lastArrow = objLower.rfind("->");
            size_t lastDot = objLower.rfind('.');
            size_t start = std::string::npos;
            if (lastArrow != std::string::npos && lastDot != std::string::npos) {
                // 取后出现的边界: 对于 "->", '-' 在 pos, 跳过2字符到标识符; 对于 ".", '.' 在 pos, 跳过1字符
                start = (lastDot > lastArrow) ? lastDot + 1 : lastArrow + 2;
            } else if (lastArrow != std::string::npos) {
                start = lastArrow + 2;
            } else if (lastDot != std::string::npos) {
                start = lastDot + 1;
            }
            if (start != std::string::npos && start < objLower.size()) {
                // 跳过非标识符字符 ('>', '*', '(', ')', 空白 等)
                while (start < objLower.size()
                       && !isalnum((unsigned char)objLower[start])
                       && objLower[start] != '_') {
                    start++;
                }
                size_t endPos = start;
                while (endPos < objLower.size()
                       && (isalnum((unsigned char)objLower[endPos])
                           || objLower[endPos] == '_')) {
                    endPos++;
                }
                trailingLower = objLower.substr(start, endPos - start);
            }
            if (!trailingLower.empty()) {
                auto itTrailing = knownClassVars_.find(trailingLower);
                if (itTrailing != knownClassVars_.end()) {
                    itClassVar = itTrailing;
                }
            }
        }

        if (itClassVar != knownClassVars_.end()) {
            // Fix 090a: obj 若是 As New 自动实例化项目类变量 (knownNewVars_, C 存储 void*),
            // 继续访问其成员前需 cast 回具体类指针 ((vb6_cls_X*)obj), 否则 void* 上
            // 直接 -> 触发 C2223 / COM 误 dispatch.
            // 例: cIni 内 Dim FileStream As New cToolsStream → me->FileStream->UseLine
            //     (C2223). 仅当 As New 目标是工程内 Class (kind==Class) 时 cast;
            //     As New 外部 COM (ADODB.Stream, kind==ComClass) 走 COM dispatch 不 cast.
            std::string obj090 = obj;
            if (!trailingLower.empty()) {
                auto itNew090 = knownNewVars_.find(trailingLower);
                if (itNew090 != knownNewVars_.end()) {
                    auto* newClsSym090 = symTab_.lookupModule(itNew090->second);
                    if (newClsSym090 && newClsSym090->kind == SymbolKind::Class) {
                        obj090 = "((vb6_cls_" + cIdent(itNew090->second) + "*)" + obj + ")";
                    }
                }
            }
            // 对象是类实例变量 — 用resolveClassMemberCall查证成员身份
            std::string resolvedFn = resolveClassMemberCall(itClassVar->second, node.memberName);
            if (!resolvedFn.empty()) {
                // 方法/属性调用: vb6_<className>_<prefix><memberName>(obj)
                // Fix 089h: 值上下文无括号方法引用 (x = obj.ReadLine 或参数内引用)
                // 只发 this → C2198 参数太少 (ReadLine 声明3参). 补默认参数
                // (同 Fix 015 链式值上下文 / With 类 Fix 044a). asCallCallee_
                // (带括号调用) 由外层 IndexOrCallExpr 经 split 补参, 此处不 pad.
                if (!asCallCallee_
                    && resolvedFn.find("_prop_") == std::string::npos) {
                    std::vector<ParameterInfo> params89h;
                    bool isBuiltin89h = false;
                    if (findClassMemberCallParams(itClassVar->second, node.memberName,
                                                   params89h, isBuiltin89h)
                        && !params89h.empty() && !isBuiltin89h) {
                        std::string argList89h = obj090;
                        for (size_t i = 0; i < params89h.size(); i++) {
                            const auto& param = params89h[i];
                            argList89h += ", ";
                            std::string defVal89h;
                            if (param.hasDefaultValue && !param.defaultValueExpr.empty()) {
                                defVal89h = param.defaultValueExpr;
                            } else {
                                defVal89h = defaultValue(param.type);
                            }
                            if (param.isByVal) {
                                argList89h += defVal89h;
                            } else {
                                std::string cType89h = mapType(param.type);
                                if (param.type == Vb6Type::Variant
                                    || param.type == Vb6Type::Empty
                                    || param.type == Vb6Type::Null
                                    || param.type == Vb6Type::Object) {
                                    argList89h += "&(" + cType89h + "){0}";
                                } else {
                                    argList89h += "&(" + cType89h + "){" + defVal89h + "}";
                                }
                            }
                        }
                        for (size_t i = 0; i < params89h.size(); i++) {
                            const auto& param = params89h[i];
                            if (param.isOptional && !param.isParamArray) {
                                argList89h += ", 0";
                            }
                        }
                        lastExpr_ = resolvedFn + "(" + argList89h + ")";
                    } else {
                        lastExpr_ = resolvedFn + "(" + obj090 + ")";
                    }
                } else {
                    lastExpr_ = resolvedFn + "(" + obj090 + ")";
                }
            } else {
                // 非方法/属性 → 数据字段访问: obj->member
                // (此时obj类型为 vb6_cls_<className>*, ->访问可正确编译)
                // Fix 023: 检查该字段是否是 void* (外部 COM 指针). 若是, 在注释中
                // 加入 "voidptr" 标记, 由外层 MemberAccessExpr 的链式 COM 检测2
                // (~line 1477 之后) 识别并切换为 COM dispatch.
                // 例: Db.Rs (Rs As New ADODB.Recordset → C 结构体中 void* 字段)
                //     内层 emit: "Db->Rs  /* class var .Rs field voidptr */"
                //     外层 .EOF 识别 voidptr → vb6_ComGetIntProp(Db->Rs..., L"EOF")
                bool isVoidPtr = false;
                if (classVoidFieldMap_) {
                    auto itV = classVoidFieldMap_->find(itClassVar->second);
                    if (itV != classVoidFieldMap_->end()) {
                        std::string memLower = node.memberName;
                        std::transform(memLower.begin(), memLower.end(),
                                       memLower.begin(),
                                       [](unsigned char c) { return (char)std::tolower(c); });
                        if (itV->second.count(memLower) ||
                            itV->second.count("m_" + memLower)) {
                            isVoidPtr = true;
                        }
                    }
                }
                // Fix 092x: COM 接口字段 (如 cHttpServerRequest.Header As Dictionary)
                // 由 driver 预扫描记入 classTypedFieldMap_, 值为 "COM:<Interface>" 前缀,
                // 而 classVoidFieldMap_ 不含 (isVoidFieldType 为真时被 continue 跳过,
                // 但内置 COM 类走 COM: 前缀分支). 此处一并视为 COM 指针字段并追加
                // "voidptr" 标记, 供外层 MemberAccessExpr (Fix 023, line 2437) 切换为
                // COM dispatch. 例: Request.Header.Exists("Cookie") — 此前无标记 →
                // 外层 Fix 088b inferClassTypeOfExpr 遇 COM: 前缀返回空串 → 落入
                // obj->member 结构体访问 → C2037 (cHttpServer.c 821).
                if (!isVoidPtr && classTypedFieldMap_) {
                    auto itT = classTypedFieldMap_->find(itClassVar->second);
                    if (itT != classTypedFieldMap_->end()) {
                        std::string memLower = node.memberName;
                        std::transform(memLower.begin(), memLower.end(),
                                       memLower.begin(),
                                       [](unsigned char c) { return (char)std::tolower(c); });
                        auto itF = itT->second.find(memLower);
                        if (itF == itT->second.end()) itF = itT->second.find("m_" + memLower);
                        if (itF != itT->second.end()
                            && itF->second.compare(0, 4, "COM:") == 0) {
                            isVoidPtr = true;
                        }
                    }
                }
                // Fix 092p: 字段名规范化回声明名 — VB6 大小写不敏感, 源码 `.socket`
                // 与声明 `Socket` 是同一字段, 直接用源码拼写会生成 `...->socket` (C2039).
                const std::string fldC092p =
                    canonicalClassFieldName(itClassVar->second, node.memberName);
                if (isVoidPtr) {
                    lastExpr_ = obj090 + "->" + cIdent(fldC092p)
                              + "  /* class var ." + node.memberName + " field voidptr */";
                } else {
                    lastExpr_ = obj090 + "->" + cIdent(fldC092p)
                              + "  /* class var ." + node.memberName + " field */";
                }
            }
        // Fix 089b: trailing 变量名匹配在 obj 含 '.' 时跳过 — obj 是 UDT/属性字段链
        // (如 uFile.Data) 时, 尾段 "data" 是字段名而非变量名. 若外部 COM 变量恰好
        // 与 UDT 字段同名 (如 cSSE/cCsv 的 Public Data As Dictionary), trailing 匹配
        // 会误把 UDT 值字段当 COM 指针 → obj->member (C2232: uFile.Data->X).
        // 含 '.' 的链交由 Fix 088c / Fix 085(inferUdtTypeOfExpr) 兜底, 生成正确 . 访问.
        // 形如 me->field / 裸变量名的 trailing (Fix 014 场景) 不受影响.
        } else if (knownTypedComVars_.count(objLower)
                   || (obj.find('.') == std::string::npos && !trailingLower.empty()
                       && knownTypedComVars_.count(trailingLower))) {
            lastExpr_ = obj + "->" + cIdent(node.memberName);
        } else if (node.object) {
            // Fix 088c: AST 层兜底推断 — obj 是 knownClassVars_ 未覆盖的类实例
            // 表达式文本 (prop_get 调用文本 vb6_cY_prop_get_Z(a)、类方法链返回、
            // WithMemberExpr 等). inferClassTypeOfExpr 走 AST 递归
            // (knownClassVars_/classTypedFieldMap_/方法返回类型表), 成功后按类成员
            // 分发, 避免生成 obj'.'member/obj'->'member
            // → C2039 (SendData 不是 vb6_cls_cWinsock 的成员等).
            std::string clsFromAst = inferClassTypeOfExpr(*node.object);
            if (!clsFromAst.empty()) {
                std::string resolvedFn =
                    resolveClassMemberCall(clsFromAst, node.memberName);
                if (!resolvedFn.empty()) {
                    if (asCallCallee_) {
                        // 外层 IndexOrCallExpr/CallStmt 前置 this 并补参数
                        pendingChainObj_ = "(void*)" + obj;
                        lastExpr_ = resolvedFn;
                    } else {
                        lastExpr_ = resolvedFn + "((void*)" + obj + ")";
                    }
                    return;
                }
                // 类中无此成员 → 数据字段
                lastExpr_ = obj + "->" + cIdent(node.memberName);
                return;
            }
            // Fix 085: 更深嵌套的 UDT 链末端字段访问 (Fix 031 只覆盖直接 UDT 变量).
            // object 仍是 UDT 表达式时, 若 member 为对象字段则追加标记,
            // 由外层 MemberAccessExpr (Fix 085 消费点) 转类方法/COM 路径.
            std::string udtChainType = inferUdtTypeOfExpr(*node.object);
            if (!udtChainType.empty()) {
                lastExpr_ = appendUdtObjFieldMarker(obj, udtChainType, node.memberName);
            } else {
                lastExpr_ = obj + "." + cIdent(node.memberName);
            }
        } else {
            lastExpr_ = obj + "." + cIdent(node.memberName);
        }
    }
}

} // namespace vb6c3
