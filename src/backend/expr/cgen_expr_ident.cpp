#include "backend/cgen.hpp"
#include <algorithm>
#include <cctype>
#include <iostream>
#include <functional>
#include <cstdio>

namespace vb6c3 {

// --- cgen_expr_ident.cpp: IdentifierExpr 求值（内置函数映射、常量折叠、COM/数组/对象分派） ---

void CCodeGen::visit(IdentifierExpr& node) {
    // 查找符号确定类型
    std::string cName = cIdent(node.name);

    // 内置对象特殊处理
    std::string lower = node.name;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    if (lower == "me") {
        // Same logic as MeExpr: form→hwnd, class→me, else→error
        if (isClassModule_) {
            lastExpr_ = "me";
        } else if (isFormModule_ && !knownFormName_.empty()) {
            auto it = knownFormControlOriginalNames_.find(knownFormName_);
            if (it != knownFormControlOriginalNames_.end()) {
                lastExpr_ = "vb6_hwnd_" + cIdent(it->second);
            } else {
                lastExpr_ = "vb6_hwnd_" + moduleName_;
            }
        } else {
            lastExpr_ = "vb6_Me";  // should not happen (semantic error caught)
        }
        return;
    }

    // Fix 010c: VB6全局Err对象 — 不能用me->ERR访问
    // Err是全局内置对象, 不是类成员
    if (lower == "err") {
        lastExpr_ = "(void*)0";  // Err对象本身作为值无意义, 属性访问由MemberAccessExpr处理
        return;
    }

    // Fix 086: VB6全局App对象作为值 (Common4DLL: Set HostApp = App).
    // 编译产物中没有App COM对象, 用NULL对象引用; 属性/方法访问由
    // MemberAccessExpr的App分支 (vb6_App_Path/GetCurrentThreadId等) 拦截.
    if (lower == "app") {
        lastExpr_ = "(void*)0";
        return;
    }

    // M22: 检查是否是当前函数名 — VB6语义歧义
    // - 作为IndexOrCallExpr的callee(函数调用) → 返回函数过程名
    // - 作为普通表达式(返回值引用) → 返回返回值变量
    // P6.7: Property Get也使用返回值赋值语义
    if (currentProc_ && lower == Symbol::toLower(currentProc_->name)
        && (currentProc_->kind == SymbolKind::Function || currentProc_->kind == SymbolKind::PropertyGet)) {
        if (asCallCallee_) {
            // 在IndexOrCallExpr的callee上下文中, 返回函数名供调用
            lastExpr_ = cProcName(currentProc_->name, currentProc_->access, currentProc_->sourceModule);
        } else {
            // 在普通表达式上下文中, 返回返回值变量(VB6: 引用函数名=引用返回值)
            lastExpr_ = currentReturnVar_;
        }
        return;
    }

    // 内置常量/枚举映射
    if (lower == "vbokonly")       { lastExpr_ = "0"; return; }
    if (lower == "vbcancel")       { lastExpr_ = "2"; return; }
    if (lower == "vbabortretryignore") { lastExpr_ = "2"; return; }
    if (lower == "vbyesnocancel")  { lastExpr_ = "3"; return; }
    if (lower == "vbyesno")        { lastExpr_ = "4"; return; }
    if (lower == "vbretrycancel")  { lastExpr_ = "5"; return; }
    if (lower == "vbcritical")     { lastExpr_ = "16"; return; }
    if (lower == "vbquestion")     { lastExpr_ = "32"; return; }
    if (lower == "vbexclamation")  { lastExpr_ = "48"; return; }
    if (lower == "vbinformation")  { lastExpr_ = "64"; return; }
    if (lower == "vbcrlf")         { lastExpr_ = "vb6_BSTR_FromStr(L\"\\r\\n\")"; return; }
    if (lower == "vblf")           { lastExpr_ = "vb6_BSTR_FromStr(L\"\\n\")"; return; }
    if (lower == "vbcr")           { lastExpr_ = "vb6_BSTR_FromStr(L\"\\r\")"; return; }
    if (lower == "vbtab")          { lastExpr_ = "vb6_BSTR_FromStr(L\"\\t\")"; return; }
    // Fix 086: 补齐缺失的VB6内建常量 (ToolsJsonVba/cShadow/pvSubClass 等引用)
    if (lower == "vbback")         { lastExpr_ = "vb6_BSTR_FromStr(L\"\\b\")"; return; }
    if (lower == "vbformfeed")     { lastExpr_ = "vb6_BSTR_FromStr(L\"\\f\")"; return; }
    if (lower == "vbverticaltab")  { lastExpr_ = "vb6_BSTR_FromStr(L\"\\v\")"; return; }
    if (lower == "vbsrccopy")      { lastExpr_ = "(13369376)"; return; }  // &HCC0020
    if (lower == "vbsrcand")       { lastExpr_ = "(8913094)"; return; }   // &H8800C6
    if (lower == "vbsrcpaint")     { lastExpr_ = "(15597702)"; return; }  // &HEE0086
    if (lower == "vbsrcinvert")    { lastExpr_ = "(5588696)"; return; }   // &H555009
    if (lower == "vbblackness")    { lastExpr_ = "(66)"; return; }        // &H42
    if (lower == "vbwhiteness")    { lastExpr_ = "(16711782)"; return; }  // &HFF155A
    if (lower == "vblogeventtypeerror")     { lastExpr_ = "4"; return; }
    if (lower == "vblogeventtypewarning")   { lastExpr_ = "3"; return; }
    if (lower == "vblogeventtypeinformation") { lastExpr_ = "2"; return; }
    if (lower == "vblogeventtypeconstant")  { lastExpr_ = "1"; return; }
    if (lower == "vbnewline")      { lastExpr_ = "vb6_BSTR_FromStr(L\"\\r\\n\")"; return; }
    if (lower == "vbnullstring")   { lastExpr_ = "vb6_BSTR_Empty()"; return; }
    // vbempty is VarType constant 0, NOT vb6_VariantEmpty() — fixed in 010q
    if (lower == "vbnothing")      { lastExpr_ = "NULL"; return; }
    if (lower == "vbtrue")         { lastExpr_ = "(-1)"; return; }
    if (lower == "vbfalse")        { lastExpr_ = "0"; return; }
    // Fix 056b: vbUseDefault (-2) - 常用于字体/光标等可选参数
    if (lower == "vbusedefault")   { lastExpr_ = "(-2)"; return; }

    // Fix 010q: VarType constants
    if (lower == "vbempty")        { lastExpr_ = "0"; return; }
    if (lower == "vbnull")         { lastExpr_ = "1"; return; }
    if (lower == "vbinteger")      { lastExpr_ = "2"; return; }
    if (lower == "vblong")         { lastExpr_ = "3"; return; }
    if (lower == "vbsingle")       { lastExpr_ = "4"; return; }
    if (lower == "vbdouble")       { lastExpr_ = "5"; return; }
    if (lower == "vbcurrency")     { lastExpr_ = "6"; return; }
    if (lower == "vbdate")         { lastExpr_ = "7"; return; }
    if (lower == "vbstring")       { lastExpr_ = "8"; return; }
    if (lower == "vbobject")       { lastExpr_ = "9"; return; }
    if (lower == "vberror")        { lastExpr_ = "10"; return; }
    if (lower == "vbboolean")      { lastExpr_ = "11"; return; }
    if (lower == "vbvariant")      { lastExpr_ = "12"; return; }
    if (lower == "vbdataobject")   { lastExpr_ = "13"; return; }
    if (lower == "vbdecimal")      { lastExpr_ = "14"; return; }
    if (lower == "vbbyte")         { lastExpr_ = "17"; return; }
    if (lower == "vbarray")        { lastExpr_ = "8192"; return; }
    if (lower == "vbuserdefinedtype") { lastExpr_ = "36"; return; }

    // Fix 010q: StrConv constants
    if (lower == "vbunicode")      { lastExpr_ = "64"; return; }
    if (lower == "vbfromunicode")  { lastExpr_ = "128"; return; }
    if (lower == "vbuppercase")    { lastExpr_ = "1"; return; }
    if (lower == "vblowercase")    { lastExpr_ = "2"; return; }
    if (lower == "vbpropercase")   { lastExpr_ = "3"; return; }
    if (lower == "vbwide")         { lastExpr_ = "4"; return; }
    if (lower == "vbnarrow")       { lastExpr_ = "8"; return; }
    if (lower == "vbkatakana")    { lastExpr_ = "16"; return; }
    if (lower == "vbhiragana")     { lastExpr_ = "32"; return; }
    if (lower == "vbsimple")       { lastExpr_ = "256"; return; }

    // Fix 010q: CompareMethod constants
    if (lower == "vbbinarycompare")  { lastExpr_ = "0"; return; }
    if (lower == "vbtextcompare")    { lastExpr_ = "1"; return; }
    if (lower == "vbdatabasecompare") { lastExpr_ = "2"; return; }
    if (lower == "vbusecompare")     { lastExpr_ = "(-1)"; return; }

    // Fix 010q: Misc VB6 constants
    if (lower == "vbobjecterror")  { lastExpr_ = "(-2147221504)"; return; }  // &H800A0000
    if (lower == "vbnullchar")     { lastExpr_ = "vb6_BSTR_Empty()"; return; }  // Chr(0) ≈ empty BSTR
    if (lower == "vbobject")       { lastExpr_ = "9"; return; }  // already above but dedup-safe
    if (lower == "vbcallobject")   { lastExpr_ = "1"; return; }
    if (lower == "vbcallgetnext")  { lastExpr_ = "2"; return; }
    if (lower == "vbcalllet")      { lastExpr_ = "3"; return; }
    if (lower == "vbcallmethod")   { lastExpr_ = "13"; return; }
    if (lower == "vbcallset")      { lastExpr_ = "14"; return; }
    if (lower == "vbfirstjan1")    { lastExpr_ = "1"; return; }
    if (lower == "vbfirstfourdays") { lastExpr_ = "2"; return; }
    if (lower == "vbfirstfullweek") { lastExpr_ = "3"; return; }
    if (lower == "vbusesystem")    { lastExpr_ = "0"; return; }
    if (lower == "vbusesystemdayofweek") { lastExpr_ = "0"; return; }
    if (lower == "vbsunday")       { lastExpr_ = "1"; return; }
    if (lower == "vbmonday")       { lastExpr_ = "2"; return; }
    if (lower == "vbtuesday")      { lastExpr_ = "3"; return; }
    if (lower == "vbwednesday")    { lastExpr_ = "4"; return; }
    if (lower == "vbthursday")     { lastExpr_ = "5"; return; }
    if (lower == "vbfriday")       { lastExpr_ = "6"; return; }
    if (lower == "vbsaturday")     { lastExpr_ = "7"; return; }
    if (lower == "vbusesystemdayofweek") { lastExpr_ = "0"; return; }
    if (lower == "vbfirstdayofweek") { lastExpr_ = "1"; return; }  // vbSunday
    if (lower == "vbfirstweekofyear") { lastExpr_ = "1"; return; }  // vbFirstJan1
    if (lower == "vbgeneraldate")  { lastExpr_ = "0"; return; }
    if (lower == "vblongdate")     { lastExpr_ = "1"; return; }
    if (lower == "vbshortdate")    { lastExpr_ = "2"; return; }
    if (lower == "vblongtime")     { lastExpr_ = "3"; return; }
    if (lower == "vbshorttime")    { lastExpr_ = "4"; return; }

    // 先查符号表: 如果有用户定义的同名符号(变量/过程/常量等), 优先使用
    // 这避免了用户变量名与内置函数名冲突的问题 (如 Dim v As Long vs Val()函数)
    // 同时使用currentProc_->params来判断ByRef参数(比符号表查找更可靠)
    Symbol* foundSym = symTab_.lookup(node.name);
    Symbol* modSym = symTab_.lookupModule(node.name);

    // ByRef参数: 最优先检查 (通过currentProc_->params, 不依赖符号表作用域)
    if (currentProc_) {
        for (auto& param : currentProc_->params) {
            if (Symbol::toLower(param.name) == lower) {
                if (param.isParamArray) {
                    // P14.1.5: ParamArray is SAFEARRAY*, no dereference needed
                    lastExpr_ = cName;
                } else if (!param.isByVal) {
                    // Fix 010r-6 rev2: ByRef array parameters are vb6_SafeArray1D**,
                    // need dereference to get the actual pointer value (*name)
                    if (static_cast<uint16_t>(param.type) & static_cast<uint16_t>(Vb6Type::Array)) {
                        lastExpr_ = "(*" + cName + ")";
                    } else {
                        lastExpr_ = "(*" + cName + ")";
                    }
                } else {
                    lastExpr_ = cName;
                }
                return;
            }
        }
    }

    // Fix 010r-12c + 010r-16b: 局部变量必须遮蔽跨模块外部符号 + 同模块的 Function/Property
    // codegen期间 symTab_.current_ 处于模块作用域(语义分析已完成),
    // lookup()会找到模块级 Public/Private Sub/Function/Property. 如果局部Dim变量
    // 与这些符号同名, VB6 作用域语义规定局部变量优先 (局部 > 模块 > 跨模块).
    // 注意: 自引用(过程内引用自身函数名作为返回值)已在上方 line 181-191 处理,
    // 此处的局部变量一定不是当前过程的自引用.
    if (knownLocalVars_.count(lower)) {
        // Dim As New 自动实例化守卫 (与下方Variable块的逻辑一致)
        auto itNew = knownNewVars_.find(lower);
        if (itNew != knownNewVars_.end()) {
            std::string newExpr;
            auto itCom = knownTypedComVars_.find(lower);
            if (itCom != knownTypedComVars_.end()) {
                const std::string& _progId = itCom->second->comProgId.empty() ? itCom->second->name : itCom->second->comProgId;
                newExpr = "(void*)vb6_NewObject(L\"" + _progId + "\")";
            } else {
                newExpr = "vb6_cls_" + itNew->second + "_New()";
            }
            // Fix 086: Variant局部变量持有对象时, 守卫用 VariantToObjectVal 判空,
            // 赋值用 _Generic vb6_VariantFromValue 包装 (避免 !VARIANT 与 C2440)
            if (knownVariantVars_.count(lower)) {
                c_.emitLine("if (vb6_VariantToObjectVal(" + cName + ") == NULL) " + cName
                            + " = vb6_VariantFromValue(" + newExpr + ");  /* Dim As New auto-instantiate (Variant) */");
            } else {
                c_.emitLine("if (!" + cName + ") " + cName + " = " + newExpr + ";  /* Dim As New auto-instantiate */");
            }
        }
        lastExpr_ = cName;
        return;
    }

    // Fix 056: Form 内置属性隐式访问 (WindowState/ScaleWidth/ScaleHeight)
    // VB6: 在 Form 模块内, 裸属性名等同于 Me.属性名
    if (isFormModule_ && !knownFormName_.empty()) {
        std::string formHwnd;
        auto itOrig = knownFormControlOriginalNames_.find(knownFormName_);
        if (itOrig != knownFormControlOriginalNames_.end()) {
            formHwnd = "vb6_hwnd_" + cIdent(itOrig->second);
        } else {
            formHwnd = "vb6_hwnd_" + moduleName_;
        }
        std::string readFn = getControlPropReadFn(FrmControlType::Form, node.name);
        if (!readFn.empty()) {
            lastExpr_ = readFn + "(" + formHwnd + ")";
            return;
        }
    }

    // Fix 086: 跨模块窗体默认实例引用 (cLogs 里 Unload FLogs / FLogs.Show).
    // 窗体名作为值 → 该窗体HWND访问器 vb6_form_hwnd_<Form>().
    // 成员访问 (.Visible/.Show) 由 MemberAccessExpr 的窗体模块分支处理.
    if (knownFormModuleNames_.count(lower) && lower != knownFormName_) {
        lastExpr_ = "vb6_form_hwnd_" + cIdent(node.name) + "()  /* form default instance */";
        return;
    }

    // 枚举成员引用
    if (foundSym && foundSym->kind == SymbolKind::EnumMember) {
        if (foundSym->hasConstValue) {
            lastExpr_ = std::to_string(foundSym->constIntValue);
        } else {
            lastExpr_ = cName;
        }
        return;
    }

    // P24-04: ComGlobalNs — VB_GlobalNameSpace promoted函数 (如VBMAN)
    // 当VBMAN作为独立标识符出现时 (不在MemberAccessExpr.object位置),
    // 生成sGlobal单例创建 + promoted方法调用
    if (foundSym && foundSym->kind == SymbolKind::ComGlobalNs) {
        std::string progIdWide = "L\"" + foundSym->comProgId + "\"";
        std::string methodName = foundSym->comGlobalNsMethodName;
        lastExpr_ = "vb6_ComCallObject(vb6_CreateObject(" + progIdWide + "), L\"" + methodName + "\", NULL, 0)";
        return;
    }

    // 用户变量/常量 (非参数、非函数)
    if (foundSym && (foundSym->kind == SymbolKind::Variable
                  || foundSym->kind == SymbolKind::Constant)) {
        // Fix 017: 数值常量 (内置 vbMethod=2/vbDirectory=16, 或用户 Public Const,
        // 或跨模块注入的 EnumMember-as-Constant) 直接输出数值, 避免发出裸标识符
        // (C 代码中无对应 #define → C2065).
        // Fix 081d: 仅对整型/浮点/布尔常量内联数值; 字符串常量(hasConstValue=true
        // 但 constType==String) 必须走正常标识符路径, 引用 #define 宏名, 否则
        // constIntValue==0 被错误输出为 "0".
        if (foundSym->kind == SymbolKind::Constant && foundSym->hasConstValue) {
            if (foundSym->constType == Vb6Type::Long || foundSym->constType == Vb6Type::Integer
                || foundSym->constType == Vb6Type::Boolean || foundSym->constType == Vb6Type::Byte
                || foundSym->constType == Vb6Type::Error) {
                lastExpr_ = std::to_string(foundSym->constIntValue);
                return;
            } else if (foundSym->constType == Vb6Type::Single || foundSym->constType == Vb6Type::Double) {
                lastExpr_ = std::to_string(foundSym->constFloatValue);
                return;
            }
            // 字符串常量和其他类型: 不内联, 让代码继续走标识符路径引用 #define 宏
        }
        // P11.7: 如果是内置Object类型变量(=窗体控件), 优先走默认属性读取
        if (foundSym->isBuiltin && foundSym->type == Vb6Type::Object) {
            // P17.1: With块内抑制默认属性解析, 返回HWND引用
            if (suppressDefaultProp_) {
                lastExpr_ = "vb6_hwnd_" + cIdent(node.name);
                return;
            }

            auto itCtrl = knownFormControls_.find(lower);
            if (itCtrl != knownFormControls_.end()) {
                const char* defaultProp = getDefaultPropertyName(itCtrl->second);
                if (defaultProp) {
                    std::string readFn = getControlPropReadFn(itCtrl->second, defaultProp);
                    if (!readFn.empty()) {
                        lastExpr_ = readFn + "(" + makeCtrlHwndArg(lower, itCtrl->second) + ")  /* default prop: ." + std::string(defaultProp) + " */";
                        return;
                    }
                }
            }
            // P20-31: WithEvents控件变量默认属性读取
            if (!suppressDefaultProp_) {
                auto itWECtrl = knownWithEventsCtrlVars_.find(lower);
                if (itWECtrl != knownWithEventsCtrlVars_.end()) {
                    const char* defaultProp = getDefaultPropertyName(itWECtrl->second);
                    if (defaultProp) {
                        std::string readFn = getControlPropReadFn(itWECtrl->second, defaultProp);
                        if (!readFn.empty()) {
                            auto itOrig = knownWithEventsCtrlOrigNames_.find(lower);
                            std::string weVarName = (itOrig != knownWithEventsCtrlOrigNames_.end()) ? itOrig->second : cName;
                            lastExpr_ = readFn + "(" + weVarName + ")  /* WithEvents ctrl default prop: ." + std::string(defaultProp) + " */";
                            return;
                        }
                    }
                }
            }
        }
        // 类模块变量通过me->访问
        // Fix 010o: 但如果该名称是局部变量(Dim/For循环变量)或外部模块变量, 不加me->前缀
        if (isClassModule_ && currentProc_ && foundSym->kind == SymbolKind::Variable
            && !knownLocalVars_.count(lower)
            && !foundSym->isExternal) {
            Symbol* paramSym = symTab_.lookupLocal(node.name);
            if (!paramSym || paramSym->kind != SymbolKind::Parameter) {
                lastExpr_ = "me->" + cName;
                // P14.3.1: Dim As New自动实例化 (类模块成员)
                auto itNewM = knownNewVars_.find(lower);
                if (itNewM != knownNewVars_.end()) {
                    {
                std::string newExpr;
                auto itCom = knownTypedComVars_.find(lower);
                if (itCom != knownTypedComVars_.end()) {
                    const std::string& _progId = itCom->second->comProgId.empty() ? itCom->second->name : itCom->second->comProgId;
                    newExpr = "(void*)vb6_NewObject(L\"" + _progId + "\")";
                } else {
                    newExpr = "vb6_cls_" + itNewM->second + "_New()";
                }
                c_.emitLine("if (!me->" + cName + ") me->" + cName + " = " + newExpr + ";  /* Dim As New auto-instantiate */");
            }
                }
                return;
            }
        }
        // P14.3.1: Dim As New自动实例化守卫
        auto itNew = knownNewVars_.find(lower);
        if (itNew != knownNewVars_.end()) {
            {
            std::string newExpr;
            auto itCom = knownTypedComVars_.find(lower);
            if (itCom != knownTypedComVars_.end()) {
                const std::string& _progId = itCom->second->comProgId.empty() ? itCom->second->name : itCom->second->comProgId;
                newExpr = "(void*)vb6_NewObject(L\"" + _progId + "\")";
            } else {
                newExpr = "vb6_cls_" + itNew->second + "_New()";
            }
            // Fix 086: Variant局部变量持有对象时, 守卫用 VariantToObjectVal 判空,
            // 赋值用 _Generic vb6_VariantFromValue 包装 (避免 !VARIANT 与 C2440)
            if (knownVariantVars_.count(lower)) {
                c_.emitLine("if (vb6_VariantToObjectVal(" + cName + ") == NULL) " + cName
                            + " = vb6_VariantFromValue(" + newExpr + ");  /* Dim As New auto-instantiate (Variant) */");
            } else {
                c_.emitLine("if (!" + cName + ") " + cName + " = " + newExpr + ";  /* Dim As New auto-instantiate */");
            }
        }
        }
        lastExpr_ = cName;
        return;
    }

    // M22-Issue5: foundSym为null但可能是cgen过程级跟踪的Dim As New变量
    // 符号表在语义分析后current_指向模块scope, 过程级局部变量查找失败
    // 此时knownNewVars_/knownClassVars_仍然有效(cgen过程中维护)
    if (!foundSym) {
        auto itNew = knownNewVars_.find(lower);
        if (itNew != knownNewVars_.end()) {
            {
            std::string newExpr;
            auto itCom = knownTypedComVars_.find(lower);
            if (itCom != knownTypedComVars_.end()) {
                const std::string& _progId = itCom->second->comProgId.empty() ? itCom->second->name : itCom->second->comProgId;
                newExpr = "(void*)vb6_NewObject(L\"" + _progId + "\")";
            } else {
                newExpr = "vb6_cls_" + itNew->second + "_New()";
            }
            // Fix 086: Variant局部变量持有对象时, 守卫用 VariantToObjectVal 判空,
            // 赋值用 _Generic vb6_VariantFromValue 包装 (避免 !VARIANT 与 C2440)
            if (knownVariantVars_.count(lower)) {
                c_.emitLine("if (vb6_VariantToObjectVal(" + cName + ") == NULL) " + cName
                            + " = vb6_VariantFromValue(" + newExpr + ");  /* Dim As New auto-instantiate (Variant) */");
            } else {
                c_.emitLine("if (!" + cName + ") " + cName + " = " + newExpr + ";  /* Dim As New auto-instantiate */");
            }
        }
            lastExpr_ = cName;
            return;
        }
        if (knownClassVars_.find(lower) != knownClassVars_.end()) {
            lastExpr_ = cName;
            return;
        }
    }

    // 内置函数映射 (名称 → RTL函数名) - 仅当符号表中没有用户定义的函数时使用
    static const std::unordered_map<std::string, std::string> builtinFuncs = {
        {"len",      "vb6_Len"},
        {"lenb",     "vb6_LenB"},  // Fix 048: LenB built-in
        {"msgbox",   "vb6_MsgBox"},
        {"unload",   "vb6_UnloadForm"},
        {"inputbox", "vb6_InputBox"},
        {"input$",  "vb6_InputString"},  // P15.4: Input function
        {"format",   "vb6_Format"},
        {"cstr",     "vb6_CStr"},
        {"cint",     "vb6_CInt"},
        {"clng",     "vb6_CLng"},
        {"cdbl",     "vb6_CDbl"},
        {"csng",     "vb6_CSng"},
        {"cbool",    "vb6_CBool"},
        {"cdate",    "vb6_CDate"},
        {"cbyt",     "vb6_CByte"},
        {"instr",    "vb6_InStr"},
        {"instrb",   "vb6_InStrB"},
        {"left",     "vb6_Left"},
        {"right",    "vb6_Right"},
        {"mid",      "vb6_Mid"},
        {"trim",     "vb6_Trim"},
        {"ltrim",    "vb6_LTrim"},
        {"rtrim",    "vb6_RTrim"},
        {"ucase",    "vb6_UCase"},
        {"lcase",    "vb6_LCase"},
        {"chr",      "vb6_Chr"},
        {"asc",      "vb6_Asc"},
        {"abs",      "vb6_Abs"},
        {"int",      "vb6_Int"},
        {"fix",      "vb6_Fix"},
        {"sgn",      "vb6_Sgn"},
        {"sqr",      "vb6_Sqr"},
        {"ubound",   "vb6_UBound"},
        {"lbound",   "vb6_LBound"},
        {"isarray",  "vb6_IsArray"},
        {"isnumeric","vb6_IsNumeric"},
        {"isnothing","vb6_IsNothing"},
        {"typeof",   "vb6_TypeOf"},
        {"typename", "vb6_TypeName"},
        {"createobject","vb6_CreateObject"},
        {"getobject","vb6_GetObject"},
        {"loadpicture","vb6_LoadPictureEx"},
        // 字符串函数 (P4新增)
        {"replace",  "vb6_Replace"},
        {"space",    "vb6_Space"},
        {"string",   "vb6_String"},   // String$函数
        {"strcomp",  "vb6_StrComp"},
        {"strreverse","vb6_StrReverse"},
        {"instrrev", "vb6_InStrRev"},
        {"val",      "vb6_Val"},
        {"str",      "vb6_Str"},
        // 数学函数 (P4新增)
        {"sin",      "vb6_Sin"},
        {"cos",      "vb6_Cos"},
        {"tan",      "vb6_Tan"},
        {"atn",      "vb6_Atn"},
        {"log",      "vb6_Log"},
        {"exp",      "vb6_Exp"},
        {"round",    "vb6_Round"},
        {"rnd",      "vb6_Rnd"},
        {"randomize","vb6_Randomize"},
        // 日期时间 (P4新增)
        {"now",      "vb6_Now"},
        {"date",     "vb6_Date"},
        {"time",     "vb6_Time"},
        {"year",     "vb6_Year"},
        {"month",    "vb6_Month"},
        {"day",      "vb6_Day"},
        {"hour",     "vb6_Hour"},
        {"minute",   "vb6_Minute"},
        {"second",   "vb6_Second"},
        // 日期函数 P14.2.4
        {"dateadd",  "vb6_DateAdd"},
        {"datediff", "vb6_DateDiff"},
        {"datepart", "vb6_DatePart"},
        {"dateserial", "vb6_DateSerial"},
        // P21-B: 日期/数组函数
        {"weekday",  "vb6_Weekday"},
        {"datevalue","vb6_DateValue"},
        {"timeserial","vb6_TimeSerial"},
        {"timevalue","vb6_TimeValue"},
        // 类型转换 (P4新增)
        {"cbyte",    "vb6_CByte"},
        {"cvar",     "vb6_CVar"},
        {"hex",      "vb6_Hex"},
        {"oct",      "vb6_Oct"},
        // 类型检查 (P4新增)
        {"isnull",   "vb6_IsNull"},
        {"isempty",  "vb6_IsEmpty"},
        {"isobject", "vb6_IsObject"},
        {"isdate",   "vb6_IsDate"},
        {"iserror",  "vb6_IsError"},
        {"vartype",  "vb6_VarType"},
        // 文件I/O (P4新增)
        {"freefile", "vb6_FreeFile"},
        {"eof",      "vb6_EOF"},
        {"lof",      "vb6_LOF"},
        {"loc",      "vb6_Loc"},
        {"kill",     "vb6_Kill"},
        // 系统函数 (P14.2.2新增)
        
        {"shell",    "vb6_Shell"},
        {"environ",  "vb6_Environ"},
        {"dir",      "vb6_Dir"},
        {"curdir",   "vb6_CurDir"},
        {"command",  "vb6_Command"},
        {"command$", "vb6_Command"},  // P14.2.2: Command$ alias
        {"split",    "vb6_Split"},
        {"join",     "vb6_Join"},

        // ParamArray (P14.1.5)

        {"ismissing", "vb6_IsMissing"},
        // P14.3.5: CallByName
        {"callbyname", "vb6_CallByName"},
        // P18-A: 兼容性填平新增内置函数
        {"ccur",       "vb6_CCur"},
        {"cdec",       "vb6_CDec"},
        {"rgb",        "vb6_RGB"},
        {"qbcolor",    "vb6_QBColor"},
        {"filedatetime", "vb6_FileDateTime"},
        {"filelen",    "vb6_FileLen"},
        {"sendkeys",   "vb6_SendKeys"},
        {"appactivate", "vb6_AppActivate"},
        // P18-D: 兼容性填平新增内置函数
        {"ascw",       "vb6_AscW"},
        {"chrw",       "vb6_ChrW"},
        {"ascb",       "vb6_AscB"},
        {"chrb",       "vb6_ChrB"},
        {"timer",      "vb6_Timer"},
        {"strconv",    "vb6_StrConv"},
        {"filter",     "vb6_Filter"},
        {"strptr",     "vb6_StrPtr"},
        {"objptr",     "vb6_ObjPtr"},
        {"lset",       "vb6_LSet"},
        {"rset",       "vb6_RSet"},
        {"weekdayname", "vb6_WeekdayName"},
        {"monthname",  "vb6_MonthName"},
        {"formatcurrency", "vb6_FormatCurrency"},
        {"formatnumber",  "vb6_FormatNumber"},
        {"formatpercent",  "vb6_FormatPercent"},
        // P18-E: 
        {"sln",       "vb6_SLN"},
        {"syd",       "vb6_SYD"},
        {"ddb",       "vb6_DDB"},
        {"fv",        "vb6_FV"},
        {"pv",        "vb6_PV"},
        {"pmt",       "vb6_Pmt"},
        {"ipmt",      "vb6_IPmt"},
        {"ppmt",      "vb6_PPmt"},
        {"rate",      "vb6_RATE"},
        {"npv",       "vb6_NPV"},
        {"partition", "vb6_Partition"},
        // P21-C: new functions
        {"cverr",          "vb6_CVErr"},
        {"formatdatetime", "vb6_FormatDateTime"},
        {"getattr",        "vb6_GetAttr"},
        {"setattr",        "vb6_SetAttr"},
        {"seek",           "vb6_SeekFunc"},
        {"doevents",       "vb6_DoEvents"},
        {"savepicture",    "vb6_SavePicture"},
        {"load",           "vb6_LoadForm"},
        {"nper",           "vb6_NPer"},
        {"fileattr",       "vb6_FileAttr"},
        {"erl",            "vb6_Erl"},
        {"tab",            "vb6_Tab"},
        {"spc",            "vb6_Spc"},
        {"irr",            "vb6_IRR"},
        {"mirr",           "vb6_MIRR"},
        // Fix 048: LoadResData
        {"loadresdata", "vb6_LoadResData"},
        // P20-37: Registry functions
        {"savesetting",    "vb6_SaveSetting"},
        {"getsetting",     "vb6_GetSetting"},
        {"deletesetting",  "vb6_DeleteSetting"},
        {"getallsettings", "vb6_GetAllSettings"},

    };

    // Fix 048: Strip $ type suffix before builtin lookup (Mid$ -> mid, Left$ -> left, etc.)
    std::string lookupName = lower;
    if (!lookupName.empty() && lookupName.back() == '$') {
        lookupName.pop_back();
    }
    auto it = builtinFuncs.find(lookupName);
    if (it != builtinFuncs.end()) {
        // 无参内置函数: VB6允许省略括号(如 Now, Date, Time)
        // 当IdentifierExpr引用这些函数时，必须生成调用(带括号)
        static const std::unordered_set<std::string> zeroArgBuiltinFuncs = {
            "now", "date", "time", "freefile", "command", "curdir", "timer",
            "rnd", "erl", "doevents"
        };
        if (zeroArgBuiltinFuncs.count(lower)) {
            // Fix 041: Rnd() requires 1 arg (int32_t seed). Emit vb6_Rnd(0) not vb6_Rnd().
            if (lower == "rnd") {
                lastExpr_ = "vb6_Rnd(0)";
            } else {
                lastExpr_ = it->second + "()";
            }
        } else {
            lastExpr_ = it->second;
        }
        return;
    }

    // 用户定义的过程: 加vb6_前缀
    // 查找模块级符号
    Symbol* sym = symTab_.lookupModule(node.name);
    if (sym && (sym->kind == SymbolKind::Sub || sym->kind == SymbolKind::Function
             || sym->kind == SymbolKind::DeclareSub || sym->kind == SymbolKind::DeclareFunc)) {
        // M22: VB6语义 — 在函数体内引用自身函数名等同于引用返回值变量
        // e.g. Function Add(): Add = a & b → vb6_ret_Add = ...; Module1.myName = Add → ...myName = vb6_ret_Add
        if (currentProc_ && !currentReturnVar_.empty() &&
            sym->kind == SymbolKind::Function && !sym->isExternal) {
            std::string procLower = currentProc_->name;
            std::transform(procLower.begin(), procLower.end(), procLower.begin(), ::tolower);
            std::string nameLower = node.name;
            std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
            if (procLower == nameLower) {
                lastExpr_ = currentReturnVar_;
                return;
            }
        }
        // Declare函数: 使用VB6函数名的cIdent形式 (通过#define映射到导出名)
        if (sym->kind == SymbolKind::DeclareSub || sym->kind == SymbolKind::DeclareFunc) {
            lastExpr_ = cIdent(node.name);
        } else if (sym->isExternal) {
            // 跨模块函数: 使用 vb6_<ModuleName>_<ProcName> 格式
            lastExpr_ = cProcName(node.name, sym->access, sym->sourceModule);
        } else {
            lastExpr_ = cProcName(node.name, sym->access);
        }
        // Fix 086: VB6语义 — 无括号引用函数/过程即调用 (x = GetTickCount,
        // MySub 语句形式). callee上下文 (asCallCallee_) 由 IndexOrCallExpr 追加
        // 括号, 此处不重复; 含必选参数的过程无括号引用在 VB6 中是编译错误,
        // 保持裸名让其延续原有行为 (ParamArray 视同可选).
        // Fix 089f: 类模块内无括号引用本类成员 Function (pvSessionID =
        // GenerateSessionID) — C 签名带 me 首参, 只加 "()" 会漏 me → C2198
        // 参数太少. 与 CallStmt (cgen_stmt P6.6) / PropertyGet (上方 832-836)
        // 同一判定方式: 函数名以 "vb6_<本类>_" 前缀开头即本类成员方法.
        if (!asCallCallee_ && !std::getenv("C3_NO_BAREFN")) {
            bool hasRequired = false;
            for (const auto& p : sym->params) {
                if (!p.isOptional && !p.isParamArray) { hasRequired = true; break; }
            }
            if (!hasRequired) {
                if (isClassModule_ && currentProc_) {
                    std::string modPrefix = "vb6_" + cIdent(moduleName_) + "_";
                    if (lastExpr_.find(modPrefix) == 0) {
                        lastExpr_ += "((void*)me)";
                    } else {
                        lastExpr_ += "()";
                    }
                } else {
                    lastExpr_ += "()";
                }
            }
        }
        return;
    }

    // Fix 010: Property Get — bare reference calls the getter
    // e.g. If frMessageHWnd = 0 → vb6_cAsyncSocket_prop_get_frMessageHWnd((void*)me)
    if (sym && sym->kind == SymbolKind::PropertyGet) {
        std::string propCName = "prop_get_" + node.name;
        std::string funcName = cProcName(propCName, sym->access,
                                          sym->isExternal ? sym->sourceModule : "");
        if (asCallCallee_) {
            // Used as callee in IndexOrCallExpr — return function name, caller adds args + me
            lastExpr_ = funcName;
        } else if (isClassModule_ && currentProc_ && !sym->isExternal) {
            // Bare reference in class method — call getter with me
            // Fix 086: 外部属性 (跨模块, 如 ToolsIDE.IsIDE 标准模块属性) 不带 me —
            // 其C签名无 me 参数, 追加会 C2197 (参数太多)
            lastExpr_ = funcName + "((void*)me)";
        } else {
            // Standard module or external — no me parameter
            lastExpr_ = funcName + "()";
        }
        return;
    }

    // 外部变量/常量: 使用 vb6_<ModuleName>_<Name> 格式
    if (sym && sym->isExternal) {
        lastExpr_ = "vb6_" + cIdent(sym->sourceModule) + "_" + cName;
        return;
    }

    // 类符号引用 (直接使用类名作为标识符, 如 Dim x As MyClass)
    if (sym && sym->kind == SymbolKind::Class) {
        lastExpr_ = cName;  // 类名本身就是一个类型标识符
        return;
    }

    // M22: Cross-module variable resolution - if identifier is not found locally,
    // check if it exists as a public variable in an external module
    {
        Symbol* extSym = symTab_.lookupModule(node.name);
        if (extSym && extSym->kind == SymbolKind::Variable && extSym->isExternal) {
            // When module is #included, use the unprefixed name directly
            // (Module1.h declares "extern BSTR myName;" which is already visible)
            std::string srcLower = extSym->sourceModule;
            std::transform(srcLower.begin(), srcLower.end(), srcLower.begin(), ::tolower);
            bool isIncluded = false;
            for (const auto& extMod : externalModules_) {
                std::string extLower = extMod;
                std::transform(extLower.begin(), extLower.end(), extLower.begin(), ::tolower);
                if (extLower == srcLower) { isIncluded = true; break; }
            }
            if (isIncluded) {
                lastExpr_ = cName;
            } else {
                lastExpr_ = "vb6_" + cIdent(extSym->sourceModule) + "_" + cName;
            }
            return;
        }
    }
    lastExpr_ = cName;
}

} // namespace vb6c3
