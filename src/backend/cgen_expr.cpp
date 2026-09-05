#include "backend/cgen.hpp"
#include <algorithm>
#include <cctype>
#include <iostream>
#include <functional>
#include <cstdio>

namespace vb6c3 {

// --- cgen_expr.cpp: 表达式求值 + 数组辅助 + 运算符映射 ---

void CCodeGen::emitExpr(Expr& expr) {
    switch (expr.kind) {
        case ASTNodeKind::LiteralExpr:
            visit(static_cast<LiteralExpr&>(expr)); break;
        case ASTNodeKind::IdentifierExpr:
            visit(static_cast<IdentifierExpr&>(expr)); break;
        case ASTNodeKind::BinaryExpr:
            visit(static_cast<BinaryExpr&>(expr)); break;
        case ASTNodeKind::UnaryExpr:
            visit(static_cast<UnaryExpr&>(expr)); break;
        case ASTNodeKind::MemberAccessExpr:
            visit(static_cast<MemberAccessExpr&>(expr)); break;
        case ASTNodeKind::DictionaryAccessExpr:
            visit(static_cast<DictionaryAccessExpr&>(expr)); break;
        case ASTNodeKind::IndexOrCallExpr:
            visit(static_cast<IndexOrCallExpr&>(expr)); break;
        case ASTNodeKind::NewExpr:
            visit(static_cast<NewExpr&>(expr)); break;
        case ASTNodeKind::TypeOfExpr:
            visit(static_cast<TypeOfExpr&>(expr)); break;
        case ASTNodeKind::AddressOfExpr:
            visit(static_cast<AddressOfExpr&>(expr)); break;
        case ASTNodeKind::MeExpr:
            visit(static_cast<MeExpr&>(expr)); break;
        case ASTNodeKind::WithMemberExpr:
            visit(static_cast<WithMemberExpr&>(expr)); break;
        default:
            lastExpr_ = "/* unknown expr */";
            break;
    }
}

// ============================================================
// 表达式 visit 方法
// ============================================================

void CCodeGen::visit(LiteralExpr& node) {
    switch (node.literalKind) {
        case LiteralKind::Integer:
            lastExpr_ = std::to_string(static_cast<int>(node.intValue));
            break;
        case LiteralKind::Long:
            lastExpr_ = std::to_string(node.longValue) + "L";
            break;
        case LiteralKind::Single:
            lastExpr_ = std::to_string(node.floatValue) + "f";
            break;
        case LiteralKind::Double:
        case LiteralKind::Currency:
        case LiteralKind::Decimal:
            lastExpr_ = std::to_string(node.doubleValue);
            break;
        case LiteralKind::String: {
            // VB6字符串 → C宽字符串字面量 L"..."
            // rawText包含引号, 需要strip
            std::string inner = node.rawText;
            if (inner.size() >= 2 && inner.front() == '"' && inner.back() == '"') {
                inner = inner.substr(1, inner.size() - 2);
            }
            // VB6的""转义折叠: 字符串内""表示一个双引号 → 折叠为单个"
            // 必须在C转义之前处理, 否则""会被错误转义为\""\""(两个引号)
            std::string folded;
            folded.reserve(inner.size());
            for (size_t k = 0; k < inner.size(); k++) {
                if (inner[k] == '"' && k + 1 < inner.size() && inner[k + 1] == '"') {
                    folded += '"';  // "" → "
                    k++;            // skip second "
                } else {
                    folded += inner[k];
                }
            }
            inner = folded;
            // C转义: " → \" , \ → \\ , 控制字符 → \n/\r/\t 等; 非ASCII字符→\xNNNN宽字符转义
            std::string escaped;
            escaped.reserve(inner.size() + 16);
            for (size_t j = 0; j < inner.size(); ) {
                unsigned char ch = (unsigned char)inner[j];
                if (ch == '"') {
                    escaped.push_back('\\'); escaped.push_back('"');
                    j++;
                } else if (ch == '\\') {
                    escaped += "\\\\";
                    j++;
                } else if (ch == '\n') {
                    escaped += "\\n";
                    j++;
                } else if (ch == '\r') {
                    escaped += "\\r";
                    j++;
                } else if (ch == '\t') {
                    escaped += "\\t";
                    j++;
                } else if (ch < 0x80) {
                    escaped += (char)ch;
                    j++;
                } else {
                    // Fix 021: UTF-8多字节解码 Unicode 码点
                    // 两处修复:
                    // (a) 4-byte UTF-8 lead byte 掩码错误: 原 (0xF8==0xF8) 匹配
                    //     11111xxx (0xF8-0xFF, 非法 UTF-8 字节), 应为 (0xF8==0xF0)
                    //     匹配 11110xxx (0xF0-0xF7, 4-byte UTF-8 lead).
                    // (b) \x%04X 在 C 中会被预处理器贪婪吃掉所有后续十六进制数字,
                    //     当 VB6 字符串的下一个 ASCII 字符恰好是数字/字母 (如 "第1条"
                    //     的 '1') 时, 拼成超长 hex escape 超出 wchar_t 范围 (16位)
                    //     -> MSVC C7744 转义序列超出范围.
                    //     改用 \u%04X (C99 universal character escape), 恰好消费 4 位,
                    //     后续 '1' 被视为独立字符. \u 不接受 0x00-0x9F 范围, 但本分支
                    //     只对 ch >= 0x80 调用, 多数为 CJK / 拉丁扩展 (>= 0xA0), 安全.
                    //     罕见字符 < 0xA0 (C1 控制字符) 不出现在 VB6 源码中.
                    uint32_t cp = 0;
                    int bytes = 0;
                    if ((ch & 0xE0) == 0xC0) { cp = ch & 0x1F; bytes = 2; }
                    else if ((ch & 0xF0) == 0xE0) { cp = ch & 0x0F; bytes = 3; }
                    else if ((ch & 0xF8) == 0xF0) { cp = ch & 0x07; bytes = 4; }
                    else { cp = ch; bytes = 1; }
                    for (int b = 1; b < bytes && j + b < inner.size(); b++) {
                        cp = (cp << 6) | ((unsigned char)inner[j + b] & 0x3F);
                    }
                    j += bytes;
                    char hex[16];
                    if (cp <= 0xFFFF) {
                        snprintf(hex, sizeof(hex), "\\u%04X", cp);
                    } else {
                        // Supplementary plane (cp > 0xFFFF, 如 emoji): 拆为 UTF-16
                        // surrogate pair 作为两个 wchar_t 输出. wchar_t 在 Windows
                        // 是 16 位, 单个 \u 无法直接表达.
                        uint32_t v = cp - 0x10000;
                        uint16_t hi = 0xD800 + (v >> 10);
                        uint16_t lo = 0xDC00 + (v & 0x3FF);
                        snprintf(hex, sizeof(hex), "\\u%04X\\u%04X", hi, lo);
                    }
                    escaped += hex;
                }
            }
            lastExpr_ = "vb6_BSTR_FromStr(L\"" + escaped + "\")";
            break;
        }
        case LiteralKind::Boolean:
            lastExpr_ = node.boolValue ? "(-1)" : "0";  // VB6: True=-1
            break;
        case LiteralKind::Nothing:
            lastExpr_ = "NULL";
            break;
        case LiteralKind::Empty:
            lastExpr_ = "vb6_VariantEmpty()";
            break;
        case LiteralKind::Null:
            lastExpr_ = "vb6_VariantNull()";
            break;
        case LiteralKind::Date:
            lastExpr_ = std::to_string(node.doubleValue);  // OLE date as double
            break;
    }
}

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

// M22: 将非BSTR表达式包装为BSTR (用于字符串连接 & 运算符)
std::string CCodeGen::wrapToBSTR(const std::string& expr, Expr& node) {
    // P24-01: 后期绑定COM调用返回VARIANT*, 需解包为BSTR(必须在vb6_BSTR检查之前)
    // P24-02: Variant数组索引返回vb6_VARIANT, 需转BSTR
    if (expr.find("vb6_VariantArrayGet") == 0) {
        return "vb6_VariantToString(" + expr + ")";
    }
    if (expr.find("vb6_ComCall(") == 0) {
        return "vb6_VariantToString(vb6_VariantFromComResult(" + expr + "))";
    }
    // P24-01: COM属性返回int/double, 需转BSTR
    if (expr.find("vb6_ComGetIntProp(") != std::string::npos ||
        expr.find("vb6_ComVtableGetInt(") != std::string::npos) {
        return "vb6_CStrLong(" + expr + ")";
    }
    if (expr.find("vb6_ComGetDoubleProp(") != std::string::npos ||
        expr.find("vb6_ComVtableGetDouble(") != std::string::npos) {
        return "vb6_CStrDbl(" + expr + ")";
    }
    if (expr.find("vb6_BSTR") != std::string::npos) return expr;
    if (expr.find("vb6_CStr") != std::string::npos) return expr;
    if (expr.find("vb6_GetControlText") != std::string::npos) return expr;
    if (expr.find("vb6_GetControlCaption") != std::string::npos) return expr;
    // Fix 049a: VB6_SA_AT(BSTR, arr, idx) returns BSTR — no wrapping needed
    if (expr.find("VB6_SA_AT(BSTR,") != std::string::npos) return expr;
    // vb6_Now() returns double (Date), NOT BSTR — removed early return
    // vb6_Now will fall through to inferExprType → Vb6Type::Date → vb6_CStrDate()
    if (expr.find("vb6_Left") != std::string::npos) return expr;
    if (expr.find("vb6_Right") != std::string::npos) return expr;
    if (expr.find("vb6_Mid") != std::string::npos) return expr;
    if (expr.find("vb6_Format") != std::string::npos) return expr;
    if (expr.find("vb6_Str") != std::string::npos) return expr;
    if (expr.find("vb6_Chr") != std::string::npos) return expr;
    if (expr.find("vb6_Replace") != std::string::npos) return expr;
    if (expr.find("vb6_Space") != std::string::npos) return expr;
    if (expr.find("vb6_InputBox") != std::string::npos) return expr;
    if (expr.find("vb6_Dir") != std::string::npos) return expr;
    if (expr.find("vb6_Command") != std::string::npos) return expr;
    if (expr.find("vb6_Environ") != std::string::npos) return expr;
    if (expr.find("vb6_CurDir") != std::string::npos) return expr;
    if (expr.find("vb6_App_Path") != std::string::npos) return expr;
    if (expr.find("vb6_App_EXEName") != std::string::npos) return expr;
    // BSTR变量: 已知BSTR变量或者vb6_Module1_xxx 格式的BSTR
    // 简化: 如果以vb6_开头且非数值函数, 假定是BSTR
    if (expr.find("vb6_") == 0) {
        // 数值/日期函数需要包装为BSTR
        // Date类: vb6_Now/vb6_Date/vb6_Time 返回double(Date)
        if (expr.find("vb6_Now") != std::string::npos ||
            expr.find("vb6_Date") != std::string::npos ||
            expr.find("vb6_Time") != std::string::npos) {
            return "vb6_CStrDate(" + expr + ")";
        }
        // 数值函数: 返回int/double等
        if (expr.find("vb6_CLng") != std::string::npos ||
            expr.find("vb6_CInt") != std::string::npos ||
            expr.find("vb6_CDbl") != std::string::npos ||
            expr.find("vb6_CSng") != std::string::npos ||
            expr.find("vb6_CBool") != std::string::npos ||
            expr.find("vb6_CByte") != std::string::npos ||
            expr.find("vb6_Abs") != std::string::npos ||
            expr.find("vb6_Len") != std::string::npos ||
            expr.find("vb6_LenB") != std::string::npos ||
            expr.find("vb6_InStr") != std::string::npos ||
            expr.find("vb6_InStrRev") != std::string::npos ||
            expr.find("vb6_Timer") != std::string::npos ||
            expr.find("vb6_Rnd") != std::string::npos ||
            expr.find("vb6_Sqr") != std::string::npos ||
            expr.find("vb6_Sgn") != std::string::npos ||
            expr.find("vb6_Fix") != std::string::npos ||
            expr.find("vb6_Int") != std::string::npos ||
            expr.find("vb6_Val") != std::string::npos ||
            expr.find("vb6_VarType") != std::string::npos) {
            return "vb6_CStrLong(" + expr + ")";
        }
        return expr;  // 其他vb6_函数假定为BSTR
    }
    // string literal L"..."
    if (expr.find("vb6_BSTR_FromStr(") != std::string::npos) return expr;
    // 推断类型
    Vb6Type t = inferExprType(node);
    switch (t) {
        case Vb6Type::String: return expr;
        case Vb6Type::Integer:
        case Vb6Type::Long:   return "vb6_CStrLong(" + expr + ")";
        case Vb6Type::Single:
        case Vb6Type::Double: return "vb6_CStrDbl(" + expr + ")";
        case Vb6Type::Boolean: return "vb6_CStrBool(" + expr + ")";
        case Vb6Type::Byte:   return "vb6_CStrByte(" + expr + ")";
        case Vb6Type::Date:   return "vb6_CStrDate(" + expr + ")";
        case Vb6Type::Variant: {
            // Fix 049a: inferExprType falls back to Variant for unknown symbols.
            // If the AST node is a known BSTR variable, return as-is to avoid
            // generating vb6_CStr(BSTR_expr) which causes C2440 (BSTR→VARIANT).
            // For all other cases (including actual Variant variables), keep
            // vb6_CStr(expr) which correctly converts VARIANT→BSTR.
            if (node.kind == ASTNodeKind::IdentifierExpr) {
                auto& id = static_cast<IdentifierExpr&>(node);
                std::string lower = id.name;
                std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
                if (knownBstrVars_.count(lower)) return expr;
            }
            // Fix 084: inferExprType 回退 Variant 的表达式可能是标量(int32_t等)、
            // BSTR 或已是 vb6_VARIANT. 直接 vb6_CStr(expr) 会因实参类型不匹配触发
            // C2440 (如 vb6_CStr((int32_t)GetCurrentThreadId())). 用 vb6_VariantFromValue
            // 按 C 实参类型 _Generic 自动包装: 标量→VariantLong, BSTR→VariantString,
            // 已是 vb6_VARIANT→identity 直通, 再交给 vb6_CStr 统一转 BSTR.
            return "vb6_CStr(vb6_VariantFromValue(" + expr + "))";
        }
        default: return "vb6_CStrLong(" + expr + ")";  // fallback
    }
}

void CCodeGen::visit(BinaryExpr& node) {
    emitExpr(*node.left);
    // COM标记解析: 如果左操作数是COM属性, 解析为值
    // P24-02: 算术运算默认Long解包
    if (isComMarker_) {
        if (node.op == BinaryOp::Concat) resolveComValue();
        else resolveComValue("Long");
    }
    std::string left = std::move(lastExpr_);
    emitExpr(*node.right);
    // COM标记解析: 如果右操作数是COM属性, 解析为值
    if (isComMarker_) {
        if (node.op == BinaryOp::Concat) resolveComValue();
        else resolveComValue("Long");
    }
    std::string right = std::move(lastExpr_);

    // 字符串连接运算: VB6 & → vb6_BSTR_Concat / vb6_BSTR_ConcatFree
    // 嵌套Concat时用ConcatFree释放中间临时BSTR，避免内存泄漏
    // M22: 非BSTR操作数自动包装为BSTR (int→vb6_CStr(vb6_CLng(x)), double→vb6_CStr(vb6_CDbl(x)))
    if (node.op == BinaryOp::Concat) {
        left = wrapToBSTR(left, *node.left);
        right = wrapToBSTR(right, *node.right);
        bool leftIsConcat = (left.find("vb6_BSTR_Concat") != std::string::npos);
        if (leftIsConcat) {
            lastExpr_ = "vb6_BSTR_ConcatFree(" + left + ", " + right + ")";
        } else {
            lastExpr_ = "vb6_BSTR_Concat(" + left + ", " + right + ")";
        }
        return;
    }

    // P14.1.1: VB6 + 运算符 — 两端String时等同&拼接
    // VB6允许 "a" + "b" 作为字符串连接，语义与 & 相同
    if (node.op == BinaryOp::Add && inferExprType(node) == Vb6Type::String) {
        left = wrapToBSTR(left, *node.left);
        right = wrapToBSTR(right, *node.right);
        bool leftIsConcat = (left.find("vb6_BSTR_Concat") != std::string::npos);
        if (leftIsConcat) {
            lastExpr_ = "vb6_BSTR_ConcatFree(" + left + ", " + right + ")";
        } else {
            lastExpr_ = "vb6_BSTR_Concat(" + left + ", " + right + ")";
        }
        return;
    }

    // 幂运算: VB6 ^ → pow()
    if (node.op == BinaryOp::Pow) {
        lastExpr_ = "vb6_Pow(" + left + ", " + right + ")";
        return;
    }

    // 整除: VB6 \ → vb6_IntDiv (确保整数截断)
    // Fix 084o: 操作数为 Variant 时需显式转 Long (vb6_IntDiv 形参是 int32_t),
    // 否则 cToolsHttp 等文件中 "nAsc \ 2^6" (nAsc As Variant) 产生 C2440.
    if (node.op == BinaryOp::IntDiv) {
        lastExpr_ = "vb6_IntDiv(" + toLongIfVariant(left, node.left.get())
                  + ", " + toLongIfVariant(right, node.right.get()) + ")";
        return;
    }

    // 浮点除法: VB6 / → (double)left / (double)right
    if (node.op == BinaryOp::Div) {
        lastExpr_ = "((double)(" + left + ") / (double)(" + right + "))";
        return;
    }

    // Fix 039: VB6 Eqv → ~(a^b), cast to int32_t for non-integer operands
    // (double from vb6_Pow, pointer from BSTR/void*/SafeArray*)
    // Fix 039b: For Variant operands, use vb6_VariantToLong() instead of (int32_t)() cast.
    if (node.op == BinaryOp::Eqv) {
        auto castBitwise = [&](const std::string& cExpr, const Expr* astExpr) -> std::string {
            if (cExprIsVariant(cExpr)) return "vb6_VariantToLong(" + cExpr + ")";
            if (astExpr && astExpr->kind == ASTNodeKind::IdentifierExpr) {
                auto& ident = static_cast<IdentifierExpr&>(const_cast<Expr&>(*astExpr));
                std::string lower = ident.name;
                std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
                if (knownVariantVars_.count(lower)) return "vb6_VariantToLong(" + cExpr + ")";
            }
            return "(int32_t)(" + cExpr + ")";
        };
        lastExpr_ = "(~(" + castBitwise(left, node.left.get()) + " ^ " + castBitwise(right, node.right.get()) + "))";
        return;
    }

    // Fix 039: VB6 Imp → (~a | b), cast to int32_t for non-integer operands
    if (node.op == BinaryOp::Imp) {
        auto castBitwise = [&](const std::string& cExpr, const Expr* astExpr) -> std::string {
            if (cExprIsVariant(cExpr)) return "vb6_VariantToLong(" + cExpr + ")";
            if (astExpr && astExpr->kind == ASTNodeKind::IdentifierExpr) {
                auto& ident = static_cast<IdentifierExpr&>(const_cast<Expr&>(*astExpr));
                std::string lower = ident.name;
                std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
                if (knownVariantVars_.count(lower)) return "vb6_VariantToLong(" + cExpr + ")";
            }
            return "(int32_t)(" + cExpr + ")";
        };
        lastExpr_ = "((~" + castBitwise(left, node.left.get()) + ") | " + castBitwise(right, node.right.get()) + ")";
        return;
    }

    // Like运算: VB6 Like → vb6_Like
    if (node.op == BinaryOp::Like) {
        lastExpr_ = "vb6_Like(" + left + ", " + right + ")";
        return;
    }

    // P24-07: 字符串比较运算 — BSTR不能用==做指针比较, 需用vb6_StrCmp
    if (node.op == BinaryOp::Eq || node.op == BinaryOp::Neq ||
        node.op == BinaryOp::Lt || node.op == BinaryOp::Gt ||
        node.op == BinaryOp::Le || node.op == BinaryOp::Ge) {
        Vb6Type lt = inferExprType(*node.left);
        Vb6Type rt = inferExprType(*node.right);
        if (lt == Vb6Type::String || rt == Vb6Type::String) {
            // 两侧都需要是BSTR: 非BSTR端用wrapToBSTR转换
            if (lt != Vb6Type::String) left = wrapToBSTR(left, *node.left);
            if (rt != Vb6Type::String) right = wrapToBSTR(right, *node.right);
            std::string cmpOp;
            switch (node.op) {
                case BinaryOp::Eq:       cmpOp = "== 0"; break;
                case BinaryOp::Neq:    cmpOp = "!= 0"; break;
                case BinaryOp::Lt:        cmpOp = "< 0";  break;
                case BinaryOp::Gt:     cmpOp = "> 0";  break;
                case BinaryOp::Le:   cmpOp = "<= 0"; break;
                case BinaryOp::Ge:cmpOp = ">= 0"; break;
                default: cmpOp = "== 0"; break;
            }
            lastExpr_ = "(vb6_StrCmp(" + left + ", " + right + ") " + cmpOp + ")";
            return;
        }
    }

    // P24-Bug2: Variant比较运算 — vb6_VARIANT不能用C内置比较运算符
    if (node.op == BinaryOp::Eq || node.op == BinaryOp::Neq ||
        node.op == BinaryOp::Lt || node.op == BinaryOp::Gt ||
        node.op == BinaryOp::Le || node.op == BinaryOp::Ge) {
        Vb6Type lt = inferExprType(*node.left);
        Vb6Type rt = inferExprType(*node.right);
        // P25: inferExprType对后期绑定COM属性返回Variant, 但实际代码生成的是类型化getter
        // 修正类型以避免对rvalue取地址或选择错误的比较函数
        if (lt == Vb6Type::Variant) {
            if (left.find("vb6_ComGetIntProp") == 0 || left.find("vb6_ComVtableGetInt") == 0) lt = Vb6Type::Long;
            else if (left.find("vb6_ComGetDoubleProp") == 0 || left.find("vb6_ComVtableGetDouble") == 0) lt = Vb6Type::Double;
            else if (left.find("vb6_ComGetStringProp") == 0) lt = Vb6Type::String;
            else if (left.find("vb6_ComGetObjectProp") == 0) lt = Vb6Type::Object;
        }
        if (rt == Vb6Type::Variant) {
            if (right.find("vb6_ComGetIntProp") == 0 || right.find("vb6_ComVtableGetInt") == 0) rt = Vb6Type::Long;
            else if (right.find("vb6_ComGetDoubleProp") == 0 || right.find("vb6_ComVtableGetDouble") == 0) rt = Vb6Type::Double;
            else if (right.find("vb6_ComGetStringProp") == 0) rt = Vb6Type::String;
            else if (right.find("vb6_ComGetObjectProp") == 0) rt = Vb6Type::Object;
        }
        // Bug #2 fix: also check knownLongVars_/knownLongPtrVars_ for simple variable names
        // because inferExprType may return Variant for optional params or out-of-scope variables
        auto isSimpleIdent = [](const std::string& s) -> bool {
            if (s.empty()) return false;
            if (!(std::isalpha(static_cast<unsigned char>(s[0])) || s[0] == '_')) return false;
            for (char c : s) { if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_') return false; }
            return true;
        };
        if (lt == Vb6Type::Variant && isSimpleIdent(left)) {
            std::string lower = left;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            if (knownLongPtrVars_.count(lower)) lt = Vb6Type::LongPtr;
            else if (knownLongVars_.count(lower)) lt = Vb6Type::Long;
        }
        if (rt == Vb6Type::Variant && isSimpleIdent(right)) {
            std::string lower = right;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            if (knownLongPtrVars_.count(lower)) rt = Vb6Type::LongPtr;
            else if (knownLongVars_.count(lower)) rt = Vb6Type::Long;
        }
        // Bug #2 fix: LongPtr (intptr_t) comparisons should use direct C operators
        // instead of VarCmpLong which treats the operand as vb6_VARIANT*
        if (lt == Vb6Type::LongPtr || rt == Vb6Type::LongPtr) {
            std::string op;
            switch (node.op) {
                case BinaryOp::Eq:  op = "=="; break;
                case BinaryOp::Neq: op = "!="; break;
                case BinaryOp::Lt:  op = "<";  break;
                case BinaryOp::Gt:  op = ">";  break;
                case BinaryOp::Le:  op = "<="; break;
                case BinaryOp::Ge:  op = ">="; break;
                default: op = "=="; break;
            }
            lastExpr_ = "(" + left + " " + op + " " + right + ")";
            return;
        }
        if (lt == Vb6Type::Variant || rt == Vb6Type::Variant) {
            // 确定比较函数后缀
            std::string cmpFn;
            switch (node.op) {
                case BinaryOp::Eq:  cmpFn = "Eq";  break;
                case BinaryOp::Neq: cmpFn = "Ne";  break;
                case BinaryOp::Lt:  cmpFn = "Lt";  break;
                case BinaryOp::Gt:  cmpFn = "Gt";  break;
                case BinaryOp::Le:  cmpFn = "Le";  break;
                case BinaryOp::Ge:  cmpFn = "Ge";  break;
                default: cmpFn = "Eq"; break;
            }
            // Variant vs NonVariant: 使用VarCmpLong快捷函数
            if (lt == Vb6Type::Variant && rt != Vb6Type::Variant) {
                Vb6Type rActual = rt;
                if (rActual == Vb6Type::Long || rActual == Vb6Type::Integer || rActual == Vb6Type::Boolean) {
                    // P25: left可能是VARIANT rvalue(vb6_VariantFromComResult), 需要临时变量
                    // Fix 084aa: 常量宏 (#define) 不可取址 → 视为非左值走临时变量
                    bool leftIsLvalue = !left.empty() && (std::isalpha(static_cast<unsigned char>(left[0])) || left[0] == '_') && !isConstIdent(left);
                    if (leftIsLvalue) { for (char c : left) { if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_') { leftIsLvalue = false; break; } } }
                    if (leftIsLvalue) {
                        lastExpr_ = "(vb6_VarCmpLong" + cmpFn + "(&" + left + ", " + right + "))";
                    } else {
                        std::string tmp = "_vcmp_" + std::to_string(vcmpCounter_++);
                        // Fix 024: left 被 inferExprType 误判为 Variant, 但实际标量 (LenB/Asc/int 等).
                        // 用 vb6_VariantFromValue 在编译期按实类型选择 variant 构造函数, 消除 C2440.
                        c_.emitLine("vb6_VARIANT " + tmp + " = vb6_VariantFromValue(" + left + ");");
                        lastExpr_ = "(vb6_VarCmpLong" + cmpFn + "(&" + tmp + ", " + right + "))";
                    }
                    return;
                }
            }
            if (rt == Vb6Type::Variant && lt != Vb6Type::Variant) {
                Vb6Type lActual = lt;
                if (lActual == Vb6Type::Long || lActual == Vb6Type::Integer || lActual == Vb6Type::Boolean) {
                    // 反转比较方向: Long op Variant → Variant reverseOp Long
                    std::string revCmpFn;
                    switch (node.op) {
                        case BinaryOp::Lt: revCmpFn = "Gt"; break;
                        case BinaryOp::Gt: revCmpFn = "Lt"; break;
                        case BinaryOp::Le: revCmpFn = "Ge"; break;
                        case BinaryOp::Ge: revCmpFn = "Le"; break;
                        default: revCmpFn = cmpFn; break;  // Eq/Ne是对称的
                    }
                    // Fix 084aa: 常量宏不可取址 → 视为非左值
                    bool rightIsLvalue = !right.empty() && (std::isalpha(static_cast<unsigned char>(right[0])) || right[0] == '_') && !isConstIdent(right);
                    if (rightIsLvalue) { for (char c : right) { if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_') { rightIsLvalue = false; break; } } }
                    if (rightIsLvalue) {
                        lastExpr_ = "(vb6_VarCmpLong" + revCmpFn + "(&" + right + ", " + left + "))";
                    } else {
                        std::string tmp = "_vcmp_" + std::to_string(vcmpCounter_++);
                        // Fix 024: right 被 inferExprType 误判为 Variant, 但实际标量. 用 FromValue 包装.
                        c_.emitLine("vb6_VARIANT " + tmp + " = vb6_VariantFromValue(" + right + ");");
                        lastExpr_ = "(vb6_VarCmpLong" + revCmpFn + "(&" + tmp + ", " + left + "))";
                    }
                    return;
                }
            }
            // Variant vs Variant: 使用VarCmp函数
            // P25: 检测rvalue, 非左值需要存临时变量
            // Fix 084aa: 常量宏 (#define) 不可取址 → 视为非左值走临时变量
            auto isLvalue = [this](const std::string& s) -> bool {
                if (s.empty()) return false;
                if (isConstIdent(s)) return false;
                if (!(std::isalpha(static_cast<unsigned char>(s[0])) || s[0] == '_')) return false;
                for (char c : s) { if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_') return false; }
                return true;
            };
            std::string leftAddr = isLvalue(left) ? ("&" + left) : ([&]{ std::string tmp = "_vcmp_" + std::to_string(vcmpCounter_++); c_.emitLine("vb6_VARIANT " + tmp + " = vb6_VariantFromValue(" + left + ");"); return "&" + tmp; }());
            std::string rightAddr = isLvalue(right) ? ("&" + right) : ([&]{ std::string tmp = "_vcmp_" + std::to_string(vcmpCounter_++); c_.emitLine("vb6_VARIANT " + tmp + " = vb6_VariantFromValue(" + right + ");"); return "&" + tmp; }());
            lastExpr_ = "(vb6_VarCmp" + cmpFn + "(" + leftAddr + ", " + rightAddr + "))";
            return;
        }
    }

    std::string op = mapBinaryOp(node.op);

    // Fix 039: VB6 And/Or/Xor are bitwise operators that require integer operands.
    // Cast to int32_t to handle double (from vb6_Pow) and pointer (BSTR, void*, SafeArray*)
    // operands. VB6 semantics: And/Or/Xor convert operands to Long before bitwise op.
    // Fix 039b: For Variant operands (knownVariantVars_ or cExprIsVariant), use
    // vb6_VariantToLong() instead of (int32_t)() cast, since VARIANT can't be cast to int.
    if (node.op == BinaryOp::And || node.op == BinaryOp::Or || node.op == BinaryOp::Xor) {
        auto castBitwise = [&](const std::string& cExpr, const Expr* astExpr) -> std::string {
            if (cExprIsVariant(cExpr)) return "vb6_VariantToLong(" + cExpr + ")";
            if (astExpr && astExpr->kind == ASTNodeKind::IdentifierExpr) {
                auto& ident = static_cast<IdentifierExpr&>(const_cast<Expr&>(*astExpr));
                std::string lower = ident.name;
                std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
                if (knownVariantVars_.count(lower)) return "vb6_VariantToLong(" + cExpr + ")";
            }
            return "(int32_t)(" + cExpr + ")";
        };
        lastExpr_ = "(" + castBitwise(left, node.left.get()) + " " + op + " " + castBitwise(right, node.right.get()) + ")";
        return;
    }

    // Fix 086: 算术运算 (+,-,*) 的 Variant 操作数提取 — VARIANT 结构体不能直接
    // 参与 C 算术运算 (C2088). 用 vb6_VariantToLong 提取 (覆盖 ByRef Variant
    // 参数解引用 (*maxLen) 与 vb6_VariantArrayGet 等返回 Variant 的表达式).
    if (node.op == BinaryOp::Add || node.op == BinaryOp::Sub || node.op == BinaryOp::Mul) {
        auto extractVar86 = [&](std::string& cExpr, const Expr* astExpr) {
            bool isVar86 = cExprIsVariant(cExpr);
            if (!isVar86 && astExpr && astExpr->kind == ASTNodeKind::IdentifierExpr) {
                auto& id86 = static_cast<IdentifierExpr&>(const_cast<Expr&>(*astExpr));
                if (knownVariantVars_.count(Symbol::toLower(id86.name))) isVar86 = true;
            }
            if (!isVar86 && cExpr.size() > 4 && cExpr[0] == '(' && cExpr[1] == '*'
                && cExpr.back() == ')' && cExpr.find('(') == std::string::npos) {
                // (*name) 解引用形式 — ByRef Variant 参数
                std::string inner86 = cExpr.substr(2, cExpr.size() - 3);
                std::string innerLower86 = Symbol::toLower(inner86);
                if (knownVariantVars_.count(innerLower86)) {
                    isVar86 = true;
                } else if (currentProc_) {
                    for (const auto& p : currentProc_->params) {
                        if (Symbol::toLower(p.name) == innerLower86
                            && (p.type == Vb6Type::Variant || p.type == Vb6Type::Empty)) {
                            isVar86 = true;
                            break;
                        }
                    }
                }
            }
            if (isVar86) cExpr = "vb6_VariantToLong(" + cExpr + ")";
        };
        extractVar86(left, node.left.get());
        extractVar86(right, node.right.get());
    }

    // VB6的And/Or/Not是逻辑运算也是位运算（取决于操作数类型）
    // 简化处理: 直接映射为C位运算, VB6语义兼容
    lastExpr_ = "(" + left + " " + op + " " + right + ")";
}

void CCodeGen::visit(UnaryExpr& node) {
    emitExpr(*node.operand);
    if (isComMarker_) resolveComValue();
    std::string operand = std::move(lastExpr_);

    switch (node.op) {
        case UnaryOp::Negate:
            // Fix 067: vb6_ComGetObjectProp 返回 void*, 不能直接取反
            if (operand.find("vb6_ComGetObjectProp(") == 0) {
                lastExpr_ = "(-(intptr_t)(" + operand + "))";
            } else {
                lastExpr_ = "(-" + operand + ")";
            }
            break;
        case UnaryOp::Not:
            // Fix 039: VB6 Not = 位取反 (C: ~), cast to int32_t for non-integer
            // operands (double from vb6_Pow, pointer from BSTR/void*/SafeArray*)
            // Fix 039b: For Variant operands, use vb6_VariantToLong() instead.
            if (cExprIsVariant(operand)) {
                lastExpr_ = "(~vb6_VariantToLong(" + operand + "))";
            } else if (node.operand && node.operand->kind == ASTNodeKind::IdentifierExpr) {
                auto& ident = static_cast<IdentifierExpr&>(*node.operand);
                std::string lower = ident.name;
                std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
                if (knownVariantVars_.count(lower)) {
                    lastExpr_ = "(~vb6_VariantToLong(" + operand + "))";
                } else {
                    lastExpr_ = "(~(int32_t)(" + operand + "))";
                }
            } else {
                lastExpr_ = "(~(int32_t)(" + operand + "))";
            }
            break;
    }
}

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
                std::string fnLower86 = Symbol::toLower(fnName);
                if (fnLower86 == "rnd") {
                    lastExpr_ = "vb6_Rnd(0)";
                } else if (vbaZeroArgFns.count(fnLower86)) {
                    lastExpr_ = "vb6_" + cIdent(fnName) + "()";
                } else {
                    lastExpr_ = "vb6_" + cIdent(fnName);
                }
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
        if (knownObjectVars_.count(objLower)) {
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
                } else if (currentReturnCType_ == "void*") {
                    // Fix 089c2: 当前函数返回内置 COM 对象 (As Collection / As Object →
                    // C void*) 时, 函数体内 FuncName.Add(...)/FuncName.Item(...) 走
                    // COM dispatch (vb6_ComCall) — 如 cZipArchive.pvEnumFiles As
                    // Collection → pvEnumFiles.Add path, key. 此前 isCls084z false 走
                    // appendUdtObjFieldMarker(void* 无字段) → 生成
                    // vb6_ret_X.Add(...) 结构成员调用 → C2224 (void* 上 .Add).
                    // 与 Fix 088d (类返回) / Fix 085 (UDT 返回) 对齐.
                    lastExpr_ = currentReturnVar_;
                    comObjExpr_ = currentReturnVar_;   // void* Collection 对象表达式
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
        if (memSym && (memSym->kind == SymbolKind::Sub || memSym->kind == SymbolKind::Function
                    || memSym->kind == SymbolKind::PropertyGet
                    || memSym->kind == SymbolKind::PropertyLet
                    || memSym->kind == SymbolKind::PropertySet)) {

            // 优先级2: 类实例成员访问: obj.Method → vb6_ClassName_MethodName(obj)
            // Fix 010r-10: 使用map查找, 可获取类名用于方法分发
            // Fix 011r-1: 当obj在knownClassVars_中时, 优先用resolveClassMemberCall
            // 精确解析该类的方法/属性, 避免memSym捡错模块的同类同名方法/属性
            auto itClassVar = knownClassVars_.find(objLower);
            if (itClassVar != knownClassVars_.end()) {
                // 用对象的真实类名查找方法, 防止跨模块同名冲突
                std::string resolvedFn = resolveClassMemberCall(itClassVar->second, node.memberName);
                if (!resolvedFn.empty()) {
                    emitExpr(*node.object);
                    std::string objExpr = std::move(lastExpr_);
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
                if (memSym->isExternal) {
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
                lastExpr_ = resolvedFn + "((void*)" + objExpr + ")";
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
                    lastExpr_ = resolvedFn + "(" + thisArg + ")";
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
                        std::string argList89h = obj;
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
                        lastExpr_ = resolvedFn + "(" + obj + ")";
                    }
                } else {
                    lastExpr_ = resolvedFn + "(" + obj + ")";
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
                if (isVoidPtr) {
                    lastExpr_ = obj + "->" + cIdent(node.memberName)
                              + "  /* class var ." + node.memberName + " field voidptr */";
                } else {
                    lastExpr_ = obj + "->" + cIdent(node.memberName)
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

void CCodeGen::visit(DictionaryAccessExpr& node) {
    emitExpr(*node.object);
    std::string obj = std::move(lastExpr_);
    // obj!key → 字典访问, 暂用函数调用
    lastExpr_ = "vb6_DictAccess(" + obj + ", L\"" + node.key + "\")";
}

void CCodeGen::visit(IndexOrCallExpr& node) {
    // 检测数组访问: arr(i) — callee 是 IdentifierExpr 且在已知数组集合中
    // VB6 不区分数组索引和函数调用, 统一为 IndexOrCallExpr
    bool isArrayAccess = false;
    std::string arrName;
    std::string arrNameLower;  // Fix 056: 小写名称用于查找 arrayUdtElemTypes_ / arrayDimCounts_
    Vb6Type arrElemType = Vb6Type::Variant;



    // P14.1.5: Check if identifier is a ParamArray parameter of current procedure

    bool isParamArrayAccess = false;

    std::string paName;

    if (node.callee && node.callee->kind == ASTNodeKind::IdentifierExpr && node.named.empty() && currentProc_) {

        auto& ident = static_cast<IdentifierExpr&>(*node.callee);

        for (auto& p : currentProc_->params) {

            if (p.isParamArray) {

                std::string pLower = p.name;

                std::transform(pLower.begin(), pLower.end(), pLower.begin(), ::tolower);

                std::string iLower = ident.name;

                std::transform(iLower.begin(), iLower.end(), iLower.begin(), ::tolower);

                if (pLower == iLower) {

                    isParamArrayAccess = true;

                    paName = cIdent(p.name);

                    break;

                }

            }

        }

    }



    if (isParamArrayAccess) {

        // ParamArray access: args(i) -> vb6_PA_GetLong(args, i) or vb6_PA_GetBSTR(args, i)

        if (node.positional.size() == 1) {

            emitExpr(*node.positional[0]);

            std::string index = std::move(lastExpr_);

            lastExpr_ = "vb6_PA_GetLong(" + paName + ", " + index + ")";

        } else {

            lastExpr_ = paName;  // bare reference to the SAFEARRAY*

        }

        return;

    }

    if (node.callee && node.callee->kind == ASTNodeKind::IdentifierExpr && node.named.empty()) {
        auto& ident = static_cast<IdentifierExpr&>(*node.callee);
        std::string lower = ident.name;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

        // 先查已知数组集合 (cgen过程中维护)
        if (knownArrays_.count(lower)) {
            isArrayAccess = true;
            arrName = cIdent(ident.name);
            arrNameLower = lower;  // Fix 056: 保存小写名
            arrElemType = arrayElemTypes_[lower];
            // Fix 010o: 类模块成员数组需要 me-> 前缀 (除非是局部变量)
            if (isClassModule_ && currentProc_ && !knownLocalVars_.count(lower)) {
                arrName = "me->" + arrName;
            }
            // Fix 010r-6 rev2: ByRef array param needs dereference (*name) since it's now **
            if (currentProc_) {
                for (auto& param : currentProc_->params) {
                    if (Symbol::toLower(param.name) == lower && !param.isByVal
                        && (static_cast<uint16_t>(param.type) & static_cast<uint16_t>(Vb6Type::Array))) {
                        arrName = "(*" + arrName + ")";
                        break;
                    }
                }
            }
        } else {
            // 再查符号表 (模块级数组)
            Symbol* sym = symTab_.lookupModule(ident.name);
            if (sym && sym->kind == SymbolKind::Variable && sym->isArray) {
                isArrayAccess = true;
                arrName = cIdent(ident.name);
                arrNameLower = lower;  // Fix 056: 保存小写名
                arrElemType = sym->type;
                // Fix 010o: 类模块成员数组需要 me-> 前缀
                if (isClassModule_ && currentProc_ && !sym->isExternal && !knownLocalVars_.count(lower)) {
                    arrName = "me->" + arrName;
                }
                // Fix 010r-6 rev2: ByRef array param needs dereference (*name) since it's now **
                if (currentProc_) {
                    for (auto& param : currentProc_->params) {
                        if (Symbol::toLower(param.name) == lower && !param.isByVal
                            && (static_cast<uint16_t>(param.type) & static_cast<uint16_t>(Vb6Type::Array))) {
                            arrName = "(*" + arrName + ")";
                            break;
                        }
                    }
                }
            }
        }
    }
    // Variant数组索引: a(i) 其中a是Variant变量(可能持有SafeArray)
    // VB6: a = Array(1,2,3); MsgBox a(0) → vb6_VariantArrayGet(&a, 0)
    if (!isArrayAccess && node.callee && node.callee->kind == ASTNodeKind::IdentifierExpr && node.named.empty() && !node.positional.empty()) {
        auto& vIdent = static_cast<IdentifierExpr&>(*node.callee);
        std::string vLower = vIdent.name;
        std::transform(vLower.begin(), vLower.end(), vLower.begin(), ::tolower);
        if (knownVariantVars_.count(vLower)) {
            emitExpr(*node.positional[0]);
            // Fix 084o: 索引为 Variant 时转 Long (vb6_VariantArrayGet 第二参是 int32_t)
            std::string vIndex = toLongIfVariant(std::move(lastExpr_), node.positional[0].get());
            lastExpr_ = "vb6_VariantArrayGet(&" + cIdent(vIdent.name) + ", " + vIndex + ")";
            return;
        }
    // P24-10: COM默认属性调用 — obj(args) 其中obj是COM变量, 等价于 obj.DefaultMember(args)
    // VB6: dict(0) → dict.Item(0), collection(1) → collection._Item(1)
    // DISPID_VALUE=0标识默认成员, 在TypeLib解析时已提取到Symbol::comDefaultMemberName
    if (!isArrayAccess && node.callee && node.callee->kind == ASTNodeKind::IdentifierExpr && node.named.empty() && !node.positional.empty()) {
        auto& comIdent = static_cast<IdentifierExpr&>(*node.callee);
        std::string comLower = comIdent.name;
        std::transform(comLower.begin(), comLower.end(), comLower.begin(), ::tolower);

        // 前期绑定COM变量 (Dim d As Dictionary → knownTypedComVars_["d"] = &Dictionary)
        auto itTyped = knownTypedComVars_.find(comLower);
        if (itTyped != knownTypedComVars_.end() && !itTyped->second->comDefaultMemberName.empty()) {
            const Symbol* comSym = itTyped->second;
            emitExpr(*node.callee);
            std::string objExpr = std::move(lastExpr_);
            std::string defMember = comSym->comDefaultMemberRealName;

            // 查找默认成员签名以确定返回类型
            std::string defLower = comSym->comDefaultMemberName;
            auto itSig = comSym->comMethods.find(defLower);

            // 有参数的默认属性调用: obj(args) → vb6_ComCall*(obj, L"Item", args, argc)
            std::vector<std::string> packedArgs;
            for (size_t i = 0; i < node.positional.size(); i++) {
                std::string packFn = comPackExpr(*node.positional[i]);
                emitExpr(*node.positional[i]);
                { std::string resolved = resolveComMarkerForPack(packFn); if (!resolved.empty()) lastExpr_ = resolved; }
                packedArgs.push_back(packFn + "(" + lastExpr_ + ")");
            }
            int32_t argc = (int32_t)packedArgs.size();
            std::string argsArray = "(void*[]){";
            for (int i = 0; i < argc; i++) {
                if (i > 0) argsArray += ", ";
                argsArray += packedArgs[i];
            }
            argsArray += "}";

            std::string callArgs = objExpr + ", L\"" + defMember + "\", " + argsArray + ", " + std::to_string(argc);

            // 根据签名返回类型选择类型化调用函数
            if (itSig != comSym->comMethods.end()) {
                std::string returnType = mapType(itSig->second.returnType);
                if (returnType == "BSTR") {
                    lastExpr_ = "vb6_ComCallBSTR(" + callArgs + ")";
                } else if (returnType == "int32_t" || returnType == "int16_t") {
                    lastExpr_ = "vb6_ComCallInt(" + callArgs + ")";
                } else if (returnType == "double" || returnType == "float") {
                    lastExpr_ = "vb6_ComCallDouble(" + callArgs + ")";
                } else if (returnType == "void*") {
                    lastExpr_ = "vb6_ComCallObject(" + callArgs + ")";
                } else {
                    lastExpr_ = "vb6_ComCall(" + callArgs + ")";
                }
            } else {
                lastExpr_ = "vb6_ComCall(" + callArgs + ")";
            }
            return;
        }

        // P24-10 TODO: 后期绑定COM变量 (Dim obj As Object → knownObjectVars_)
        // 后期绑定的 obj(args) 需要VARIANT返回类型推导, 暂不支持
        // 前期绑定 (Dim d As Dictionary) 已完全支持
    }

    }


    if (isArrayAccess) {
        // P8.1: 数组元素访问, 支持多维
        emitExpr(*node.callee);
        std::string callee = std::move(lastExpr_);

        int dimCount = 1;
        std::string dcKey = arrNameLower.empty() ? arrName : arrNameLower;
        auto itDc = arrayDimCounts_.find(dcKey);
        if (itDc != arrayDimCounts_.end()) dimCount = itDc->second;

        // Fix 055: UDT数组元素使用实际UDT C类型而非vb6_VARIANT
        std::string elemCType;
        std::string udKey = arrNameLower.empty() ? arrName : arrNameLower;
        auto itUdtArr = arrayUdtElemTypes_.find(udKey);
        if (itUdtArr != arrayUdtElemTypes_.end()) {
            elemCType = itUdtArr->second;  // e.g. "vb6_type_RECT"
        } else {
            elemCType = mapSaElemCType(arrElemType);
        }

        // Fix 081f: Dynamic array params default dimCount=1, but VB6 arr(i,j) has 2 indices.
        // Use positional.size() as the authoritative dimCount when it's >= 2.
        int actualDimCount = dimCount;
        if ((int)node.positional.size() >= 2 && (int)node.positional.size() > dimCount) {
            actualDimCount = (int)node.positional.size();
        }

        if (actualDimCount == 1 || node.positional.size() == 1) {
            // 一维访问: arr(i) -> VB6_SA_AT(type, arr, i)
            std::string index = "0";
            if (!node.positional.empty()) {
                emitExpr(*node.positional[0]);
                index = std::move(lastExpr_);
            }
            lastExpr_ = "VB6_SA_AT(" + elemCType + ", " + arrName + ", " + index + ")";
        } else if (actualDimCount == 2 && node.positional.size() == 2) {
            // 二维访问: arr(i, j) -> VB6_SA_ND_AT2(type, (vb6_SafeArrayND*)arr, i, j)
            // Fix 056: 动态数组声明为vb6_SafeArray1D*但ReDim后可能是ND, 需要强转
            emitExpr(*node.positional[0]);
            std::string idx0 = std::move(lastExpr_);
            emitExpr(*node.positional[1]);
            std::string idx1 = std::move(lastExpr_);
            std::string ndArr = "(vb6_SafeArrayND*)" + arrName;
            lastExpr_ = "VB6_SA_ND_AT2(" + elemCType + ", " + ndArr + ", " + idx0 + ", " + idx1 + ")";
        } else if (actualDimCount == 3 && node.positional.size() == 3) {
            // 三维访问: arr(i, j, k) -> VB6_SA_ND_AT3(type, (vb6_SafeArrayND*)arr, i, j, k)
            emitExpr(*node.positional[0]);
            std::string idx0 = std::move(lastExpr_);
            emitExpr(*node.positional[1]);
            std::string idx1 = std::move(lastExpr_);
            emitExpr(*node.positional[2]);
            std::string idx2 = std::move(lastExpr_);
            std::string ndArr = "(vb6_SafeArrayND*)" + arrName;
            lastExpr_ = "VB6_SA_ND_AT3(" + elemCType + ", " + ndArr + ", " + idx0 + ", " + idx1 + ", " + idx2 + ")";
        } else {
            // 4+维: 通用通过vb6_SafeArrayND_Offset + 直接指针访问
            std::vector<std::string> indices;
            for (auto& arg : node.positional) {
                emitExpr(*arg);
                indices.push_back(std::move(lastExpr_));
            }
            // 构建indices数组 + offset计算
            std::string offVar = "_ndoff_" + std::to_string(tempCounter_++);
            c_.emitLine("int " + offVar + " = vb6_SafeArrayND_Offset(" + arrName + ", " + std::to_string(actualDimCount) + ", (int[]){" + indices[0] + ", " + indices[1] + "});");
            lastExpr_ = "((" + elemCType + "*)(((char*)" + arrName + "->data) + " + offVar + "))[0]";
        }
        return;
    }

    // P14.2.4: IIf特殊处理 - 调用类型化RTL函数(函数调用语义确保两个分支都求值,符合VB6规范)
    if (node.callee && node.callee->kind == ASTNodeKind::IdentifierExpr && node.positional.size() == 3) {
        auto& ident = static_cast<IdentifierExpr&>(*node.callee);
        std::string iifLower = ident.name;
        std::transform(iifLower.begin(), iifLower.end(), iifLower.begin(), ::tolower);
        if (iifLower == "iif") {
            emitExpr(*node.positional[0]);
            std::string cond = std::move(lastExpr_);
            // VB6 True=-1, C needs explicit !=0
            cond = "((" + cond + ") != 0)";
            emitExpr(*node.positional[1]);
            std::string trueVal = std::move(lastExpr_);
            emitExpr(*node.positional[2]);
            std::string falseVal = std::move(lastExpr_);
            // Type dispatch: BSTR > double > long
            auto isBstrResult = [&](const std::string& e) -> bool {
                if (e.compare(0, 8, "vb6_BSTR") == 0) return true;
                if (e.find("VB6_SA_AT(BSTR,") != std::string::npos) return true;
                static const char* bstrPfx[] = {"vb6_BSTR_FromStr","vb6_Left","vb6_Right","vb6_Mid",
                    "vb6_UCase","vb6_LCase","vb6_Trim","vb6_LTrim","vb6_RTrim","vb6_Chr",
                    "vb6_Str","vb6_CStr","vb6_Format","vb6_Replace","vb6_Space","vb6_String",
                    "vb6_Command","vb6_CurDir","vb6_Environ","vb6_Dir",
                    "vb6_IIfBSTR","vb6_InputBox","vb6_App_Path","vb6_App_EXEName",
                    "vb6_GetControlText","vb6_GetControlCaption",nullptr};
                for (int i = 0; bstrPfx[i]; i++)
                    if (e.compare(0, strlen(bstrPfx[i]), bstrPfx[i]) == 0) return true;
                std::string low = e;
                std::transform(low.begin(), low.end(), low.begin(), ::tolower);
                return knownBstrVars_.count(low) > 0;
            };
            // Fix 046: IIf Variant arg extraction — when typed IIf function
            // is selected but an arg is a Variant, extract the concrete value.
            auto iifArgIsVariant = [&](int idx) -> bool {
                const std::string& val = (idx == 1) ? trueVal : falseVal;
                if (cExprIsVariant(val)) return true;
                auto& arg = node.positional[idx];
                if (arg->kind == ASTNodeKind::IdentifierExpr) {
                    auto& idArg = static_cast<IdentifierExpr&>(*arg);
                    std::string argLower = idArg.name;
                    std::transform(argLower.begin(), argLower.end(), argLower.begin(), ::tolower);
                    if (knownVariantVars_.count(argLower)) return true;
                }
                return false;
            };
            auto extractVariantArg = [&](std::string& val, int idx, const char* extractFn) {
                if (iifArgIsVariant(idx)) {
                    val = std::string(extractFn) + "(" + val + ")";
                }
            };
            if (isBstrResult(trueVal) || isBstrResult(falseVal)) {
                extractVariantArg(trueVal, 1, "vb6_VariantToString");
                extractVariantArg(falseVal, 2, "vb6_VariantToString");
                lastExpr_ = "vb6_IIfBSTR(" + cond + ", " + trueVal + ", " + falseVal + ")";
            } else if (trueVal.find('.') != std::string::npos || falseVal.find('.') != std::string::npos) {
                extractVariantArg(trueVal, 1, "vb6_VariantToDouble");
                extractVariantArg(falseVal, 2, "vb6_VariantToDouble");
                lastExpr_ = "vb6_IIfDouble(" + cond + ", " + trueVal + ", " + falseVal + ")";
            } else if (iifArgIsVariant(1) && iifArgIsVariant(2)) {
                // Both args are Variant — use vb6_IIfVariant (accepts VARIANT args)
                lastExpr_ = "vb6_IIfVariant(" + cond + ", " + trueVal + ", " + falseVal + ")";
            } else {
                extractVariantArg(trueVal, 1, "vb6_VariantToLong");
                extractVariantArg(falseVal, 2, "vb6_VariantToLong");
                lastExpr_ = "vb6_IIfLong(" + cond + ", " + trueVal + ", " + falseVal + ")";
            }
            return;
        }
    }

    // P18-D: VarPtr special handling - returns address of variable
    if (node.callee && node.callee->kind == ASTNodeKind::IdentifierExpr && node.positional.size() == 1) {
        auto& ident = static_cast<IdentifierExpr&>(*node.callee);
        std::string vpLower = ident.name;
        std::transform(vpLower.begin(), vpLower.end(), vpLower.begin(), ::tolower);
        if (vpLower == "varptr") {
            emitExpr(*node.positional[0]);
            /* Fix 082: VarPtr must return intptr_t, not int32_t.
               On x64, (int32_t)(intptr_t) truncates 8-byte pointers.
               Use (intptr_t) instead — safe on both x86 (4 bytes) and x64 (8 bytes). */
            /* Fix 084aa: VarPtr(函数调用) — 调用结果不是左值, &(call) → C2102.
               用复合字面量 &(void*){call} 包装 (VarPtr(.Glob(0)) 中的 .Glob 是 COM 属性
               调用 vb6_ComCall(...)); 变量/字段/数组元素保持原 &(expr). */
            bool vpIsCall = false;
            if (!lastExpr_.empty()
                && (std::isalpha(static_cast<unsigned char>(lastExpr_[0])) || lastExpr_[0] == '_')) {
                size_t vpParen = lastExpr_.find('(');
                if (vpParen != std::string::npos) {
                    vpIsCall = true;
                    for (size_t j = 0; j < vpParen; j++) {
                        char ch = lastExpr_[j];
                        if (!std::isalnum(static_cast<unsigned char>(ch)) && ch != '_') {
                            vpIsCall = false;
                            break;
                        }
                    }
                }
            }
            if (vpIsCall) {
                lastExpr_ = "(intptr_t)&(void*){" + lastExpr_ + "}";
            } else if (!lastExpr_.empty() && (std::isdigit(static_cast<unsigned char>(lastExpr_[0])) || lastExpr_[0] == '(')) {
                // Fix 086: VarPtr(常量) — 常量被内联为字面量 (或括号表达式), 不可取址.
                // 用 int32_t 复合字面量承载 (VB6 语义: 取常量临时副本地址).
                lastExpr_ = "(intptr_t)&(int32_t){" + lastExpr_ + "}";
            } else {
                lastExpr_ = "(intptr_t)&(" + lastExpr_ + ")";
            }
            return;
        }
    }


    // P18-E: Choose special handling - nested ternary chain
    if (node.callee && node.callee->kind == ASTNodeKind::IdentifierExpr && !node.positional.empty()) {
        auto& ident = static_cast<IdentifierExpr&>(*node.callee);
        std::string csLower = ident.name;
        std::transform(csLower.begin(), csLower.end(), csLower.begin(), ::tolower);
        if (csLower == "choose" && node.positional.size() >= 2) {
            emitExpr(*node.positional[0]);
            std::string idx = std::move(lastExpr_);
            std::string result = "NULL";
            for (int i = (int)node.positional.size() - 1; i >= 1; i--) {
                emitExpr(*node.positional[i]);
                result = "((" + idx + ")==" + std::to_string(i) + " ? (" + lastExpr_ + ") : (" + result + "))";
            }
            lastExpr_ = result;
            return;
        }
        if (csLower == "switch" && node.positional.size() >= 2 && node.positional.size() % 2 == 0) {
            std::string result = "NULL";
            for (int i = (int)node.positional.size() - 2; i >= 0; i -= 2) {
                emitExpr(*node.positional[i]);
                std::string cond = "((" + lastExpr_ + ")!=0)";
                emitExpr(*node.positional[i + 1]);
                result = "(" + cond + " ? (" + lastExpr_ + ") : (" + result + "))";
            }
            lastExpr_ = result;
            return;
        }
    }

        // P21-B: Array(arglist) special handling - creates a Variant SafeArray
    // Array(1, "hello", 3.14) → vb6_ArrayCreate(3) + vb6_ArraySetLong/SetBSTR/SetDouble
    if (node.callee && node.callee->kind == ASTNodeKind::IdentifierExpr) {
        auto& arrIdent = static_cast<IdentifierExpr&>(*node.callee);
        std::string arrLower = arrIdent.name;
        std::transform(arrLower.begin(), arrLower.end(), arrLower.begin(), ::tolower);
        if (arrLower == "array" && node.positional.size() >= 0) {
            int count = (int)node.positional.size();
            std::string arrVar = "_arr_" + std::to_string(tempCounter_++);
            if (count > 0) {
                c_.emitLine("vb6_SafeArray1D* " + arrVar + " = vb6_ArrayCreate(" + std::to_string(count) + ");");
                for (int i = 0; i < count; i++) {
                    emitExpr(*node.positional[i]);
                    std::string argExpr = std::move(lastExpr_);
                    // Detect argument type for proper setter selection
                    bool isLongArg = false;
                    bool isDoubleArg = false;
                    if (node.positional[i]->kind == ASTNodeKind::LiteralExpr) {
                        auto& lit = static_cast<LiteralExpr&>(*node.positional[i]);
                        if (lit.literalKind == LiteralKind::Integer) isLongArg = true;
                        else if (lit.literalKind == LiteralKind::Double) isDoubleArg = true;
                    }
                    // Fix 038b-3: 检测 Variant 实参 — Array() 元素通过 vb6_ArraySetLong/
                    // SetBSTR/SetDouble 设置, 这些函数期望具体类型. 当实参是 Variant
                    // (如 vb6_VariantArrayGet, Variant 变量等) 时, 需要先提取具体值.
                    // 仅使用 cExprIsVariant (C 字符串级) 和 knownVariantVars_ 检测.
                    bool argIsVariant = cExprIsVariant(argExpr);
                    if (!argIsVariant && node.positional[i]->kind == ASTNodeKind::IdentifierExpr) {
                        auto& idArg = static_cast<IdentifierExpr&>(*node.positional[i]);
                        std::string argLower = idArg.name;
                        std::transform(argLower.begin(), argLower.end(), argLower.begin(), ::tolower);
                        if (knownVariantVars_.count(argLower)) argIsVariant = true;
                    }
                    // 也检测 BSTR 变量
                    bool argIsBstr = false;
                    if (node.positional[i]->kind == ASTNodeKind::IdentifierExpr) {
                        auto& idArg = static_cast<IdentifierExpr&>(*node.positional[i]);
                        std::string argLower = idArg.name;
                        std::transform(argLower.begin(), argLower.end(), argLower.begin(), ::tolower);
                        if (knownBstrVars_.count(argLower)) argIsBstr = true;
                    }
                    if (isLongArg) {
                        c_.emitLine("vb6_ArraySetLong(" + arrVar + ", " + std::to_string(i) + ", " + argExpr + ");");
                    } else if (isDoubleArg) {
                        c_.emitLine("vb6_ArraySetDouble(" + arrVar + ", " + std::to_string(i) + ", " + argExpr + ");");
                    } else if (argIsVariant) {
                        // Variant 实参: 无法确定具体类型, 统一用 VariantToLong 转换
                        // (Array() 的元素通常是数值索引). 若为 BSTR Variant 则需
                        // VariantToString, 但 Array() 上下文中 Long 是最常见的默认.
                        // 更安全: 使用 vb6_VariantFromValue 让 _Generic 选择, 然后
                        // 直接用 vb6_ArraySetVariant (如果存在) 或回退到 Long.
                        bool looksLikeBSTR = (argExpr.find("vb6_BSTR") != std::string::npos ||
                                              argExpr.find("L\"") != std::string::npos);
                        // Fix 081i: 也用 inferExprType 检测 BSTR 类型
                        if (!looksLikeBSTR) {
                            Vb6Type argVt = inferExprType(*node.positional[i]);
                            if (argVt == Vb6Type::String) looksLikeBSTR = true;
                        }
                        if (looksLikeBSTR) {
                            c_.emitLine("vb6_ArraySetBSTR(" + arrVar + ", " + std::to_string(i) + ", vb6_VariantToString(" + argExpr + "));");
                        } else {
                            c_.emitLine("vb6_ArraySetLong(" + arrVar + ", " + std::to_string(i) + ", vb6_VariantToLong(" + argExpr + "));");
                        }
                    } else if (argIsBstr) {
                        c_.emitLine("vb6_ArraySetBSTR(" + arrVar + ", " + std::to_string(i) + ", " + argExpr + ");");
                    } else {
                        bool looksLikeBSTR = (argExpr.find("vb6_BSTR") != std::string::npos ||
                                              argExpr.find("L\"") != std::string::npos);
                        // Fix 081i: 也用 inferExprType 检测, 避免内置函数返回 BSTR
                        // (如 vb6_ErrSource/vb6_ErrDescription) 被误判为 Long
                        if (!looksLikeBSTR) {
                            Vb6Type argVt = inferExprType(*node.positional[i]);
                            if (argVt == Vb6Type::String) looksLikeBSTR = true;
                        }
                        if (looksLikeBSTR) {
                            c_.emitLine("vb6_ArraySetBSTR(" + arrVar + ", " + std::to_string(i) + ", " + argExpr + ");");
                        } else {
                            // Default: treat as Long (covers variable references etc.)
                            c_.emitLine("vb6_ArraySetLong(" + arrVar + ", " + std::to_string(i) + ", " + argExpr + ");");
                        }
                    }
                }
            } else {
                c_.emitLine("vb6_SafeArray1D* " + arrVar + " = vb6_ArrayCreate(0);");
            }
            lastExpr_ = arrVar;
            return;
        }
    }

    // 函数调用路径 (原有逻辑)
    // P20-36: IsMissing(arg) -> (!_has_arg) for Optional params
    // Must intercept BEFORE callee mapping to vb6_IsMissing
    if (node.callee && node.callee->kind == ASTNodeKind::IdentifierExpr && node.named.empty()) {
        auto& ismIdent = static_cast<IdentifierExpr&>(*node.callee);
        std::string ismLower = ismIdent.name;
        std::transform(ismLower.begin(), ismLower.end(), ismLower.begin(), ::tolower);
        if (ismLower == "ismissing" && node.positional.size() == 1) {
            auto& arg = node.positional[0];
            if (arg->kind == ASTNodeKind::IdentifierExpr) {
                std::string argName = static_cast<IdentifierExpr&>(*arg).name;
                std::string argLower = argName;
                std::transform(argLower.begin(), argLower.end(), argLower.begin(), ::tolower);
                if (currentProc_) {
                    for (auto& p : currentProc_->params) {
                        std::string pLower = p.name;
                        std::transform(pLower.begin(), pLower.end(), pLower.begin(), ::tolower);
                        if (pLower == argLower && p.isOptional && !p.isParamArray) {
                            lastExpr_ = "(!_has_" + cIdent(p.name) + ")";
                            return;
                        }
                    }
                }
                // Not an Optional param - IsMissing returns False (0)
                lastExpr_ = "(0)";
                return;
            }
        }
    }

    // Fix 038: Len(udt) → sizeof(udt) — VB6 Len on UDT returns size in bytes.
    // vb6_Len is declared as int32_t vb6_Len(BSTR), so passing a UDT causes C2440.
    // Must intercept BEFORE callee mapping to vb6_Len.
    if (node.callee && node.callee->kind == ASTNodeKind::IdentifierExpr && node.named.empty()) {
        auto& lenIdent = static_cast<IdentifierExpr&>(*node.callee);
        std::string lenLower = lenIdent.name;
        std::transform(lenLower.begin(), lenLower.end(), lenLower.begin(), ::tolower);
        if (lenLower == "len" && node.positional.size() == 1) {
            std::string udtCType = inferUdtTypeOfExpr(*node.positional[0]);
            if (!udtCType.empty()) {
                emitExpr(*node.positional[0]);
                lastExpr_ = "((int32_t)sizeof(" + lastExpr_ + "))";
                return;
            }
        }
    }

    // ---- Fix 037: IdentifierExpr callee — implicit Me.field / ByRef Object param / local Object var ----
    // 处理 name(idx) 模式 (无显式 obj. 前缀), name 可能是:
    //   Pattern G: 类模块隐式 Me.field (void*/Variant) — VB6 在类模块内省略 Me. 时
    //              codegen 会 emit me->field, 但 field 是数据字段不是函数 → C2064
    //              → vb6_ComCall(me->field, L"Item", ...) / vb6_VariantArrayGet(&me->field, idx)
    //   Pattern F: ByRef Object 参数 (*name)(idx) — VB6 Items(idx) 其中 Items 是 ByRef Object
    //              参数, C 中是 void** → (*Items) 是 void*, 不可调用 → C2064
    //              → vb6_ComCall((*name), L"Item", args, argc)
    //   Pattern F2: 局部 Object 变量 (Dim x As Object) — knownObjectVars_ 后期绑定
    //              → vb6_ComCall(name, L"Item", args, argc)
    // 检查顺序: Pattern F (参数优先, 遮蔽类字段) > Pattern G (类字段) > Pattern F2 (局部 Object)
    if (node.callee && node.callee->kind == ASTNodeKind::IdentifierExpr
        && !node.positional.empty() && node.named.empty()) {
        auto& idExpr = static_cast<IdentifierExpr&>(*node.callee);
        std::string nameLower = Symbol::toLower(idExpr.name);
        std::string nameMLower = "m_" + nameLower;
        bool handled = false;

        // ---- Pattern F: Object 参数 (ByRef 或 ByVal) ----
        // currentProc_->params 中查找同名参数, 非数组, 非ParamArray, 类型为 Object/Variant.
        // ByRef Object 在 C 中是 void** → emitExpr emit "(*name)"; ByVal Object 是 void* → emit "name".
        if (!handled && currentProc_) {
            for (auto& p : currentProc_->params) {
                if (Symbol::toLower(p.name) == nameLower) {
                    bool isArray = (static_cast<uint16_t>(p.type) & static_cast<uint16_t>(Vb6Type::Array)) != 0;
                    if (!isArray && !p.isParamArray
                        && (p.type == Vb6Type::Object || p.type == Vb6Type::Variant)) {
                        emitExpr(*node.callee);  // emits "(*name)" for ByRef or "name" for ByVal
                        std::string objExpr = std::move(lastExpr_);
                        std::vector<std::string> packedArgs;
                        for (size_t i = 0; i < node.positional.size(); i++) {
                            std::string packFn = comPackExpr(*node.positional[i]);
                            emitExpr(*node.positional[i]);
                            { std::string resolved = resolveComMarkerForPack(packFn);
                              if (!resolved.empty()) lastExpr_ = resolved; }
                            packedArgs.push_back(packFn + "(" + lastExpr_ + ")");
                        }
                        int32_t argc = (int32_t)packedArgs.size();
                        std::string argsArray = "(void*[]){";
                        for (int i = 0; i < argc; i++) {
                            if (i > 0) argsArray += ", ";
                            argsArray += packedArgs[i];
                        }
                        argsArray += "}";
                        lastExpr_ = "vb6_ComCall(" + objExpr + ", L\"Item\", "
                                  + argsArray + ", " + std::to_string(argc) + ")";
                        handled = true;
                    }
                    break;  // 参数匹配即终止 (无论是否 handled)
                }
            }
        }

        // ---- Pattern G: 隐式 Me.field (类模块上下文) ----
        // isClassModule_ && name 不是局部变量 && 不是参数 → 类数据字段.
        // emitExpr(*node.callee) 会 emit "me->field" (+ Dim As New 自动实例化守卫).
        if (!handled && isClassModule_ && currentProc_
            && !knownLocalVars_.count(nameLower)) {
            // 确认 name 不是当前过程的参数 (Pattern F 已处理)
            bool isParam = false;
            for (auto& p : currentProc_->params) {
                if (Symbol::toLower(p.name) == nameLower) { isParam = true; break; }
            }
            if (!isParam) {
                // Variant 字段 → vb6_VariantArrayGet(&me->field, idx)
                if (classVariantMembers_.count(nameLower) || classVariantMembers_.count(nameMLower)) {
                    emitExpr(*node.callee);  // emits "me->field"
                    std::string fieldExpr = std::move(lastExpr_);
                    emitExpr(*node.positional[0]);
                    // Fix 084o: 索引为 Variant 时转 Long
                    std::string idx = toLongIfVariant(std::move(lastExpr_), node.positional[0].get());
                    lastExpr_ = "vb6_VariantArrayGet(&" + fieldExpr + ", " + idx + ")";
                    handled = true;
                } else {
                    // void* COM 字段 → vb6_ComCall(me->field, L"Item", args, argc)
                    bool isVoidPtr = false;
                    if (classVoidFieldMap_) {
                        auto itV = classVoidFieldMap_->find(moduleName_);
                        if (itV != classVoidFieldMap_->end()) {
                            if (itV->second.count(nameLower) || itV->second.count(nameMLower)) {
                                isVoidPtr = true;
                            }
                        }
                    }
                    if (isVoidPtr) {
                        emitExpr(*node.callee);  // emits "me->field" (+ auto-instantiate guard)
                        std::string objExpr = std::move(lastExpr_);
                        std::vector<std::string> packedArgs;
                        for (size_t i = 0; i < node.positional.size(); i++) {
                            std::string packFn = comPackExpr(*node.positional[i]);
                            emitExpr(*node.positional[i]);
                            { std::string resolved = resolveComMarkerForPack(packFn);
                              if (!resolved.empty()) lastExpr_ = resolved; }
                            packedArgs.push_back(packFn + "(" + lastExpr_ + ")");
                        }
                        int32_t argc = (int32_t)packedArgs.size();
                        std::string argsArray = "(void*[]){";
                        for (int i = 0; i < argc; i++) {
                            if (i > 0) argsArray += ", ";
                            argsArray += packedArgs[i];
                        }
                        argsArray += "}";
                        lastExpr_ = "vb6_ComCall(" + objExpr + ", L\"Item\", "
                                  + argsArray + ", " + std::to_string(argc) + ")";
                        handled = true;
                    } else if (classTypedFieldMap_) {
                        // Fix 037b: typed (非 void*) 对象字段 — 项目类或 COM 接口
                        auto itT = classTypedFieldMap_->find(moduleName_);
                        if (itT != classTypedFieldMap_->end()) {
                            auto itF = itT->second.find(nameLower);
                            if (itF == itT->second.end()) itF = itT->second.find(nameMLower);
                            if (itF != itT->second.end()) {
                                const std::string& fieldType = itF->second;
                                if (fieldType.compare(0, 4, "COM:") == 0) {
                                    // Pattern K: COM 接口字段 (如 Header As Dictionary)
                                    // → vb6_ComCall((void*)me->field, L"Item", args, argc)
                                    emitExpr(*node.callee);
                                    std::string objExpr = std::move(lastExpr_);
                                    std::vector<std::string> packedArgs;
                                    for (size_t i = 0; i < node.positional.size(); i++) {
                                        std::string packFn = comPackExpr(*node.positional[i]);
                                        emitExpr(*node.positional[i]);
                                        { std::string resolved = resolveComMarkerForPack(packFn);
                                          if (!resolved.empty()) lastExpr_ = resolved; }
                                        packedArgs.push_back(packFn + "(" + lastExpr_ + ")");
                                    }
                                    int32_t argc = (int32_t)packedArgs.size();
                                    std::string argsArray = "(void*[]){";
                                    for (int i = 0; i < argc; i++) {
                                        if (i > 0) argsArray += ", ";
                                        argsArray += packedArgs[i];
                                    }
                                    argsArray += "}";
                                    lastExpr_ = "vb6_ComCall((void*)" + objExpr + ", L\"Item\", "
                                              + argsArray + ", " + std::to_string(argc) + ")";
                                    handled = true;
                                } else if (node.positional.size() == 1) {
                                    // Pattern L: 项目类字段 (如 Rows As cCollection)
                                    // → vb6_<Type>_prop_get_Item(me->field, vb6_VariantFromValue(arg))
                                    std::string itemFn = resolveClassMemberCall(fieldType, "Item");
                                    if (!itemFn.empty()) {
                                        emitExpr(*node.callee);
                                        std::string objExpr = std::move(lastExpr_);
                                        emitExpr(*node.positional[0]);
                                        std::string arg = std::move(lastExpr_);
                                        lastExpr_ = itemFn + "(" + objExpr + ", vb6_VariantFromValue(" + arg + "))";
                                        lastExprNeedsObjectUnpack_ = true;  // Set 语句需转 void*
                                        handled = true;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        // ---- Pattern F2: 局部 Object 变量 (后期绑定) ----
        // knownObjectVars_: Dim x As Object → C 中 x 是 void*, x(idx) 应为 vb6_ComCall(x, ...).
        if (!handled && knownObjectVars_.count(nameLower)) {
            emitExpr(*node.callee);  // emits "name"
            std::string objExpr = std::move(lastExpr_);
            std::vector<std::string> packedArgs;
            for (size_t i = 0; i < node.positional.size(); i++) {
                std::string packFn = comPackExpr(*node.positional[i]);
                emitExpr(*node.positional[i]);
                { std::string resolved = resolveComMarkerForPack(packFn);
                  if (!resolved.empty()) lastExpr_ = resolved; }
                packedArgs.push_back(packFn + "(" + lastExpr_ + ")");
            }
            int32_t argc = (int32_t)packedArgs.size();
            std::string argsArray = "(void*[]){";
            for (int i = 0; i < argc; i++) {
                if (i > 0) argsArray += ", ";
                argsArray += packedArgs[i];
            }
            argsArray += "}";
            lastExpr_ = "vb6_ComCall(" + objExpr + ", L\"Item\", "
                      + argsArray + ", " + std::to_string(argc) + ")";
            handled = true;
        }

        if (handled) return;
    }

    // ---- Fix 037: MemberAccessExpr callee with positional args — array field/COM dispatch ----
    // 处理 obj.field(idx) 模式, field 可能是:
    //   - UDT 数组字段 (固定/动态)     — Pattern A/B → obj.member[idx] / VB6_SA_AT(...)
    //   - 类 Variant 字段持有 SafeArray — Pattern C → vb6_VariantArrayGet(&obj->member, idx)
    //   - 类 void* 字段持有 COM 对象    — Pattern D → vb6_ComCall(obj->member, L"Item", args, argc)
    // 若 member 是该类的方法/属性, resolveClassMemberCall 返回非空 → 不拦截 (正常函数调用路径).
    // 不拦截则 fallback path (line ~2158) 把 field 误当函数调用 → C2064.
    if (node.callee && node.callee->kind == ASTNodeKind::MemberAccessExpr
        && !node.positional.empty() && node.named.empty()) {
        auto& maExpr = static_cast<MemberAccessExpr&>(*node.callee);
        std::string memLower = Symbol::toLower(maExpr.memberName);
        std::string memLowerM = "m_" + memLower;
        bool handled = false;

        // ---- Pattern A/B: UDT 数组字段 ----
        // 仅当 maExpr.object 的 UDT 类型可推断 (IdentifierExpr / WithMemberExpr / 嵌套
        // MemberAccessExpr). inferUdtTypeOfExpr 找到 UDT C 类型 → 在 udtMembers 中查 member.
        if (maExpr.object) {
            std::string udtCType = inferUdtTypeOfExpr(*maExpr.object);
            if (!udtCType.empty()) {
                const std::string prefix = "vb6_type_";
                if (udtCType.size() > prefix.size()
                    && udtCType.compare(0, prefix.size(), prefix) == 0) {
                    std::string udtName = udtCType.substr(prefix.size());
                    Symbol* udtSym = symTab_.lookupModule(udtName);
                    if (udtSym && udtSym->kind == SymbolKind::UserDefinedType) {
                        for (auto& mi : udtSym->udtMembers) {
                            if (Symbol::toLower(mi.name) == memLower) {
                                if (mi.arraySize > 0) {
                                    // Pattern A: 固定大小数组成员 → obj.member[idx]
                                    emitExpr(*node.callee);  // emits "obj.member"
                                    std::string fieldExpr = std::move(lastExpr_);
                                    emitExpr(*node.positional[0]);
                                    std::string idx = std::move(lastExpr_);
                                    lastExpr_ = fieldExpr + "[" + idx + "]";
                                    handled = true;
                                } else if (mi.isArrayDynamic) {
                                    // Pattern B: 动态数组成员 → VB6_SA_AT(elemType, obj.member, idx)
                                    emitExpr(*node.callee);
                                    std::string fieldExpr = std::move(lastExpr_);
                                    emitExpr(*node.positional[0]);
                                    std::string idx = std::move(lastExpr_);
                                    std::string elemCType = mapSaElemCType(mi.type);
                                    lastExpr_ = "VB6_SA_AT(" + elemCType + ", "
                                              + fieldExpr + ", " + idx + ")";
                                    handled = true;
                                } else if (mi.type == Vb6Type::Variant) {
                                    // Pattern J: UDT Variant 字段 (持有 SafeArray) →
                                    // vb6_VariantArrayGet(&obj.field, idx)
                                    emitExpr(*node.callee);  // emits "obj.field"
                                    std::string fieldExpr = std::move(lastExpr_);
                                    emitExpr(*node.positional[0]);
                                    // Fix 084o: 索引为 Variant 时转 Long
                                    std::string idx = toLongIfVariant(std::move(lastExpr_), node.positional[0].get());
                                    lastExpr_ = "vb6_VariantArrayGet(&" + fieldExpr + ", " + idx + ")";
                                    handled = true;
                                } else if (mi.type == Vb6Type::Object) {
                                    // Pattern H: UDT void* (Object) 字段 (持有 COM 对象) →
                                    // vb6_ComCall(obj.field, L"Item", args, argc)
                                    emitExpr(*node.callee);
                                    std::string objExpr = std::move(lastExpr_);
                                    std::vector<std::string> packedArgs;
                                    for (size_t i = 0; i < node.positional.size(); i++) {
                                        std::string packFn = comPackExpr(*node.positional[i]);
                                        emitExpr(*node.positional[i]);
                                        { std::string resolved = resolveComMarkerForPack(packFn);
                                          if (!resolved.empty()) lastExpr_ = resolved; }
                                        packedArgs.push_back(packFn + "(" + lastExpr_ + ")");
                                    }
                                    int32_t argc = (int32_t)packedArgs.size();
                                    std::string argsArray = "(void*[]){";
                                    for (int i = 0; i < argc; i++) {
                                        if (i > 0) argsArray += ", ";
                                        argsArray += packedArgs[i];
                                    }
                                    argsArray += "}";
                                    lastExpr_ = "vb6_ComCall(" + objExpr + ", L\"Item\", "
                                              + argsArray + ", " + std::to_string(argc) + ")";
                                    handled = true;
                                }
                                break;
                            }
                        }
                    }
                }
            }
        }

        // ---- Pattern C/D: 类实例字段访问 ----
        if (!handled && maExpr.object) {
            std::string className = inferClassTypeOfExpr(*maExpr.object);
            if (!className.empty()) {
                // resolveClassMemberCall 验证 member 是否为方法/属性:
                // 是 → 跳过 (正常函数调用 fallback 处理); 否 → 数据字段 → 检查 Variant/void*
                std::string resolvedFn = resolveClassMemberCall(className, maExpr.memberName);
                if (resolvedFn.empty()) {
                    // Pattern C: Variant 字段 → vb6_VariantArrayGet(&obj->member, idx)
                    if (classVariantMembers_.count(memLower)
                        || classVariantMembers_.count(memLowerM)) {
                        emitExpr(*node.callee);  // emits "obj->member  /* class var .X field */"
                        std::string fieldExpr = std::move(lastExpr_);
                        emitExpr(*node.positional[0]);
                        // Fix 084o: 索引为 Variant 时转 Long
                        std::string idx = toLongIfVariant(std::move(lastExpr_), node.positional[0].get());
                        lastExpr_ = "vb6_VariantArrayGet(&" + fieldExpr + ", " + idx + ")";
                        handled = true;
                    } else {
                        // Pattern D: void* COM 字段 → vb6_ComCall(obj->member, L"Item", args, argc)
                        bool isVoidPtr = false;
                        if (classVoidFieldMap_) {
                            auto itV = classVoidFieldMap_->find(className);
                            if (itV != classVoidFieldMap_->end()) {
                                if (itV->second.count(memLower)
                                    || itV->second.count(memLowerM)) {
                                    isVoidPtr = true;
                                }
                            }
                        }
                        if (isVoidPtr) {
                            // 模仿 line 1880-1938 COM 默认属性调用 (comPackExpr/resolveComMarkerForPack)
                            emitExpr(*node.callee);  // emits "obj->member  /* ... voidptr */"
                            std::string objExpr = std::move(lastExpr_);
                            std::vector<std::string> packedArgs;
                            for (size_t i = 0; i < node.positional.size(); i++) {
                                std::string packFn = comPackExpr(*node.positional[i]);
                                emitExpr(*node.positional[i]);
                                { std::string resolved = resolveComMarkerForPack(packFn);
                                  if (!resolved.empty()) lastExpr_ = resolved; }
                                packedArgs.push_back(packFn + "(" + lastExpr_ + ")");
                            }
                            int32_t argc = (int32_t)packedArgs.size();
                            std::string argsArray = "(void*[]){";
                            for (int i = 0; i < argc; i++) {
                                if (i > 0) argsArray += ", ";
                                argsArray += packedArgs[i];
                            }
                            argsArray += "}";
                            // 默认成员名 "Item" — VB6 Collection / ADODB.Recordset 等大多数
                            // void* 字段都是用 .Item(idx) 索引的 (DISPID_VALUE 默认成员).
                            lastExpr_ = "vb6_ComCall(" + objExpr + ", L\"Item\", "
                                      + argsArray + ", " + std::to_string(argc) + ")";
                            handled = true;
                        } else if (classTypedFieldMap_) {
                            // Fix 037b: typed (非 void*) 对象字段 — 项目类或 COM 接口
                            auto itT = classTypedFieldMap_->find(className);
                            if (itT != classTypedFieldMap_->end()) {
                                auto itF = itT->second.find(memLower);
                                if (itF == itT->second.end()) itF = itT->second.find(memLowerM);
                                if (itF != itT->second.end()) {
                                    const std::string& fieldType = itF->second;
                                    if (fieldType.compare(0, 4, "COM:") == 0) {
                                        // Pattern K: COM 接口字段 (如 Header As Dictionary)
                                        // → vb6_ComCall((void*)obj->member, L"Item", args, argc)
                                        emitExpr(*node.callee);
                                        std::string objExpr = std::move(lastExpr_);
                                        std::vector<std::string> packedArgs;
                                        for (size_t i = 0; i < node.positional.size(); i++) {
                                            std::string packFn = comPackExpr(*node.positional[i]);
                                            emitExpr(*node.positional[i]);
                                            { std::string resolved = resolveComMarkerForPack(packFn);
                                              if (!resolved.empty()) lastExpr_ = resolved; }
                                            packedArgs.push_back(packFn + "(" + lastExpr_ + ")");
                                        }
                                        int32_t argc = (int32_t)packedArgs.size();
                                        std::string argsArray = "(void*[]){";
                                        for (int i = 0; i < argc; i++) {
                                            if (i > 0) argsArray += ", ";
                                            argsArray += packedArgs[i];
                                        }
                                        argsArray += "}";
                                        lastExpr_ = "vb6_ComCall((void*)" + objExpr + ", L\"Item\", "
                                                  + argsArray + ", " + std::to_string(argc) + ")";
                                        handled = true;
                                    } else if (node.positional.size() == 1) {
                                        // Pattern L: 项目类字段 (如 Rows As cCollection)
                                        // → vb6_<Type>_prop_get_Item(obj->member, vb6_VariantFromValue(arg))
                                        std::string itemFn = resolveClassMemberCall(fieldType, "Item");
                                        if (!itemFn.empty()) {
                                            emitExpr(*node.callee);
                                            std::string objExpr = std::move(lastExpr_);
                                            emitExpr(*node.positional[0]);
                                            std::string arg = std::move(lastExpr_);
                                            lastExpr_ = itemFn + "(" + objExpr + ", vb6_VariantFromValue(" + arg + "))";
                                            lastExprNeedsObjectUnpack_ = true;  // Set 语句需转 void*
                                            handled = true;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        if (handled) return;
    }

    // ---- Fix 060: WithMemberExpr callee with positional args — With block UDT array field ----
    // 处理 With 块内 .Data(index) 模式, 其中 .Data 是 UDT 动态数组字段
    // WithMemberExpr 不同于 MemberAccessExpr, 需要单独处理
    if (node.callee && node.callee->kind == ASTNodeKind::WithMemberExpr
        && !node.positional.empty() && node.named.empty()
        && !withObjectVars_.empty() && !withObjectInfoStack_.empty()) {
        auto& wmExpr = static_cast<WithMemberExpr&>(*node.callee);
        const auto& info = withObjectInfoStack_.back();
        if (info.kind == WithObjKind::Unknown) {
            // UDT With block — 查找 UDT 类型
            const std::string& tempVar = withObjectVars_.back();
            std::string tempLower = Symbol::toLower(tempVar);
            auto it = knownUdtVars_.find(tempLower);
            if (it != knownUdtVars_.end()) {
                const std::string prefix = "vb6_type_";
                const std::string& udtCType = it->second;
                if (udtCType.size() > prefix.size()
                    && udtCType.compare(0, prefix.size(), prefix) == 0) {
                    std::string udtName = udtCType.substr(prefix.size());
                    Symbol* udtSym = symTab_.lookupModule(udtName);
                    if (udtSym && udtSym->kind == SymbolKind::UserDefinedType) {
                        std::string memLower = Symbol::toLower(wmExpr.memberName);
                        for (auto& mi : udtSym->udtMembers) {
                            if (Symbol::toLower(mi.name) == memLower) {
                                bool isHandled = false;
                                if (mi.arraySize > 0) {
                                    // Pattern A: 固定大小数组字段 → tempVar->member[idx]
                                    // Fix 081j-2: With 块临时变量是指针，用 -> 访问成员
                                    emitExpr(*node.positional[0]);
                                    std::string idx = std::move(lastExpr_);
                                    lastExpr_ = tempVar + "->" + cIdent(mi.name) + "[" + idx + "]";
                                    isHandled = true;
                                } else if (mi.isArrayDynamic) {
                                    // Pattern B: 动态数组字段 → VB6_SA_AT(elemType, tempVar->member, idx)
                                    // Fix 081j-2: With 块临时变量是指针，用 -> 访问成员
                                    emitExpr(*node.positional[0]);
                                    std::string idx = std::move(lastExpr_);
                                    std::string elemCType = mapSaElemCType(mi.type);
                                    lastExpr_ = "VB6_SA_AT(" + elemCType + ", "
                                              + tempVar + "->" + cIdent(mi.name) + ", " + idx + ")";
                                    isHandled = true;
                                }
                                if (isHandled) return;
                                break;
                            }
                        }
                    }
                }
            }
        }
    }

    // M22: 设置asCallCallee_标志, 让IdentifierExpr知道当前是函数调用callee上下文
    // 这确保递归调用时(如 Factorial(n-1))返回函数名而非返回值变量
    // Fix 015: 加 save/restore. 原代码硬编码 `asCallCallee_=false` 会丢失嵌套 callee
    // 上下文 (如 CallStmt→MemberAccessExpr.Fix015→IndexOrCallExpr 链中, 内层 IndexOrCallExpr
    // 反复重置成 false, 外层 Fix 015 看到的是 false 而非 CallStmt 设置的 true).
    // 同时清空 pendingChainObj_ 防止跨调用泄漏 (Fix 015 在 callee emission 时设置).
    pendingChainObj_.clear();
    bool savedAsCallCallee = asCallCallee_;
    asCallCallee_ = true;
    emitExpr(*node.callee);
    asCallCallee_ = savedAsCallCallee;
    std::string callee = std::move(lastExpr_);

    // Bug #3 fix: 清除COM Picture标志 — 每次新的IndexOrCallExpr重置
    // 后续根据被调用函数的返回类型重新设置
    lastExprIsComPicture_ = false;

    // Fix 032: args (positional + named + early-COM 早期路径) 是 值上下文, 不可能是
    // callee 上下文. 外层 CallStmt(2159-2162) 或嵌套 IndexOrCallExpr 可能把
    // asCallCallee_ 留在 true 状态. 若不强制 false, 后续所有 emitExpr(子表达式)
    // 作为参数发联时, 会被 IdentifierExpr 的自引用检查 (line ~199-213) 误判为
    // callee 上下文, 导致 Property Get / Function 返回值变量名引用错误地返回
    // 过程名 (如 vb6_cTlsReMaster_LocalHostName 而非 vb6_ret_LocalHostName).
    // RAII: 函数退出时 (任何 return 或自然走到末尾) 自动恢复为 savedAsCallCallee.
    struct CallCalleeValueScope {
        bool& ref;
        bool saved;
        CallCalleeValueScope(bool& r, bool s) : ref(r), saved(s) { ref = false; }
        ~CallCalleeValueScope() { ref = saved; }
    } _argsValueScope{asCallCallee_, savedAsCallCallee};
    (void)_argsValueScope;  // suppress unused-warning

    // --- P7.9: WebBrowser控件方法调用 ---
    // Navigate/GoBack/GoForward/Refresh via isComMarker_ flag set by MemberAccessExpr
    if (isComMarker_) {
        auto itCtrl = knownFormControls_.find(comObjExpr_);
        if (itCtrl != knownFormControls_.end() && itCtrl->second == FrmControlType::WebBrowser) {
            isComMarker_ = false;
            std::string method = std::move(comMemberName_);
            std::string ctrlName = cIdent(knownFormControlOriginalNames_.count(comObjExpr_) ? knownFormControlOriginalNames_[comObjExpr_] : comObjExpr_);
            comObjExpr_.clear();
            comMemberName_.clear();
            if (method == "navigate") {
                std::string urlArg = "0";
                if (!node.positional.empty()) {
                    emitExpr(*node.positional[0]);
                    urlArg = std::move(lastExpr_);
                }
                lastExpr_ = "(void)vb6_WebViewNavigate((void*)vb6_hwnd_" + ctrlName + ", " + urlArg + ")";
                return;
            } else if (method == "goback" || method == "goforward" || method == "refresh") {
                // Simplified: not yet implemented
                lastExpr_ = "(void)0";
                return;
            }
        }
    }

    // P13.3: ListBox/ComboBox methods (AddItem/RemoveItem/Clear/List) via isComMarker_ flag
    if (isComMarker_) {
        auto itCtrl = knownFormControls_.find(comObjExpr_);
        if (itCtrl != knownFormControls_.end() &&
            (itCtrl->second == FrmControlType::ListBox || itCtrl->second == FrmControlType::ComboBox)) {
            isComMarker_ = false;
            std::string method = std::move(comMemberName_);
            std::string ctrlName = cIdent(knownFormControlOriginalNames_.count(comObjExpr_) ? knownFormControlOriginalNames_[comObjExpr_] : comObjExpr_);
            comObjExpr_.clear();
            comMemberName_.clear();
            if (method == "additem") {
                std::string itemArg = "0";
                if (!node.positional.empty()) {
                    emitExpr(*node.positional[0]);
                    itemArg = std::move(lastExpr_);
                }
                c_.emitLine("vb6_AddItem((void*)vb6_hwnd_" + ctrlName + ", " + itemArg + ");  /* ListBox.AddItem */");
                lastExpr_ = "0";
                return;
            } else if (method == "removeitem") {
                std::string idxArg = "0";
                if (!node.positional.empty()) {
                    emitExpr(*node.positional[0]);
                    idxArg = std::move(lastExpr_);
                }
                c_.emitLine("vb6_RemoveItem((void*)vb6_hwnd_" + ctrlName + ", " + idxArg + ");  /* ListBox.RemoveItem */");
                lastExpr_ = "0";
                return;
            } else if (method == "clear") {
                c_.emitLine("vb6_ClearList((void*)vb6_hwnd_" + ctrlName + ");  /* ListBox.Clear */");
                lastExpr_ = "0";
                return;
            } else if (method == "list") {
                // List(idx) property read - List1.List(0)
                std::string idxArg = "0";
                if (!node.positional.empty()) {
                    emitExpr(*node.positional[0]);
                    idxArg = std::move(lastExpr_);
                }
                lastExpr_ = "vb6_GetListItem((void*)vb6_hwnd_" + ctrlName + ", " + idxArg + ")";
                return;
            }
        }
    }
    // --- COM后期绑定检测 (P6.2) + 前期绑定检测 (P6.3) + P6.4接口调用 ---
    // MemberAccessExpr为COM对象设置isComMarker_标志 + comObjExpr_/comMemberName_
    if (isComMarker_) {
        // ActiveX控件COM属性: 先获取属性对象, 再用Item(idx)索引
        // comObjExpr_以"vb6_com_"开头 = ActiveX控件变量
        if (comObjExpr_.find("vb6_com_") == 0 && !node.positional.empty()) {
            std::string axObjExpr = std::move(comObjExpr_);
            std::string axMember = std::move(comMemberName_);
            isComMarker_ = false;
            // Step 1: 获取属性对象 (如 ListImages 集合)
            std::string collectionExpr = "vb6_ComGetObjectProp(" + axObjExpr + ", L\"" + axMember + "\")";
            // Step 2: 调用 Item(idx) 获取集合中的元素
            std::vector<std::string> packedArgs;
            for (size_t i = 0; i < node.positional.size(); i++) {
                std::string packFn = comPackExpr(*node.positional[i]);
                emitExpr(*node.positional[i]);
                { std::string resolved = resolveComMarkerForPack(packFn); if (!resolved.empty()) lastExpr_ = resolved; }
                packedArgs.push_back(packFn + "(" + lastExpr_ + ")");
            }
            int32_t argc = (int32_t)packedArgs.size();
            std::string argsArray = "(void*[]){";
            for (int i = 0; i < argc; i++) {
                if (i > 0) argsArray += ", ";
                argsArray += packedArgs[i];
            }
            argsArray += "}";
            // 返回对象类型 (用于后续 .Picture 等链式访问)
            lastExpr_ = "vb6_ComCallObject(" + collectionExpr + ", L\"Item\", " + argsArray + ", " + std::to_string(argc) + ")";
            return;
        }
        isComMarker_ = false;  // 消费标记
        std::string objExpr = std::move(comObjExpr_);
        std::string memberName = std::move(comMemberName_);

        // Fix 057: Me.Controls.Add(ProgID, Name) → vb6_Form_ControlsAdd(hwnd, L"ProgID", L"Name")
        // 检测链式COM: objExpr = "vb6_ComGetObjectProp(vb6_hwnd_Form1, L"Controls")", memberName = "Add"
        {
            std::string memLower = memberName;
            std::transform(memLower.begin(), memLower.end(), memLower.begin(), ::tolower);
            std::string objExprLower = objExpr;
            std::transform(objExprLower.begin(), objExprLower.end(), objExprLower.begin(), ::tolower);
            if (memLower == "add" &&
                objExpr.find("vb6_ComGetObjectProp(vb6_hwnd_") == 0 &&
                objExprLower.find("l\"controls\")") != std::string::npos) {
                // 提取 form HWND 变量: vb6_ComGetObjectProp(vb6_hwnd_Form1, L"Controls") → vb6_hwnd_Form1
                size_t parenStart = strlen("vb6_ComGetObjectProp(");
                size_t commaPos = objExpr.find(", ", parenStart);
                std::string hwndExpr = objExpr.substr(parenStart, commaPos - parenStart);
                // 发射参数: Controls.Add(ProgID, Name)
                std::vector<std::string> argExprs;
                for (size_t i = 0; i < node.positional.size() && i < 2; i++) {
                    emitExpr(*node.positional[i]);
                    argExprs.push_back(lastExpr_);
                }
                if (argExprs.size() >= 2) {
                    lastExpr_ = "vb6_Form_ControlsAdd(" + hwndExpr + ", " + argExprs[0] + ", " + argExprs[1] + ")";
                    return;
                }
                // 参数不足, 降级为普通COM调用
            }
        }

        // P6.4: 接口引用方法调用 (Dim x As IFoo → x.Method → x.vtbl->Method(x.obj, args))
        {
            std::string objLower = objExpr;
            std::transform(objLower.begin(), objLower.end(), objLower.begin(), ::tolower);
            auto itIfaceVar = knownIfaceVars_.find(objLower);
            if (itIfaceVar != knownIfaceVars_.end() && !isEarlyBoundCom_) {
                std::string ifaceName = itIfaceVar->second;
                std::string ifaceId = cIdent(ifaceName);
                // 生成参数列表
                std::vector<std::string> callArgs;
                for (size_t i = 0; i < node.positional.size(); i++) {
                    emitExpr(*node.positional[i]);
                    callArgs.push_back(lastExpr_);
                }
                for (auto& named : node.named) {
                    emitExpr(*named.value);
                    callArgs.push_back(lastExpr_);
                }
                std::string argsStr;
                for (size_t i = 0; i < callArgs.size(); i++) {
                    if (i > 0) argsStr += ", ";
                    argsStr += callArgs[i];
                }
                // x.vtbl->Method(x.obj, args...)
                std::string call = objExpr + ".vtbl->" + cIdent(memberName) + "(" + objExpr + ".obj";
                if (!argsStr.empty()) call += ", " + argsStr;
                call += ")";
                lastExpr_ = call;
                return;
            }
        }

        // P6.3: 前期绑定 — 利用类型签名确定返回类型，统一走后期绑定(IDispatch)
        // 原因: vtable直接调用的函数签名可能不是VARIANT* (如get_Count用long*)，
        // vb6_ComVtableGet*辅助函数统一用VARIANT*签名会导致调用错误
        if (isEarlyBoundCom_ && earlyBoundSym_) {
            isEarlyBoundCom_ = false;
            const Symbol* comSym = earlyBoundSym_;
            earlyBoundSym_ = nullptr;

            // 查找方法签名以确定返回类型
            std::string memLower = memberName;
            std::transform(memLower.begin(), memLower.end(), memLower.begin(), ::tolower);
            auto it = comSym->comMethods.find(memLower);
            bool hasArgs = (!node.positional.empty() || !node.named.empty());


            // 无参属性Get: 用后期绑定属性读取，根据返回类型选函数
            if (it != comSym->comMethods.end() && it->second.isPropertyGet && !hasArgs) {
                const auto& sig = it->second;
                std::string returnType = mapType(sig.returnType);
                if (returnType == "BSTR") {
                    lastExpr_ = "vb6_ComGetStringProp(" + objExpr + ", L\"" + memberName + "\")";
                } else if (returnType == "int32_t" || returnType == "int16_t") {
                    lastExpr_ = "vb6_ComGetIntProp(" + objExpr + ", L\"" + memberName + "\")";
                } else if (returnType == "double" || returnType == "float") {
                    lastExpr_ = "vb6_ComGetDoubleProp(" + objExpr + ", L\"" + memberName + "\")";
                } else if (returnType == "void*") {
                    lastExpr_ = "vb6_ComGetObjectProp(" + objExpr + ", L\"" + memberName + "\")";
                } else {
                    lastExpr_ = "vb6_ComGetStringProp(" + objExpr + ", L\"" + memberName + "\")";
                }
                return;
            }
            // P24-07: 有参数的前期绑定方法 → 利用签名选择类型化COM调用函数
            if (it != comSym->comMethods.end() && hasArgs) {
                const auto& sig = it->second;
                std::string returnType = mapType(sig.returnType);
                std::vector<std::string> packedArgs;
                for (size_t i = 0; i < node.positional.size(); i++) {
                    std::string packFn = comPackExpr(*node.positional[i]);
                    emitExpr(*node.positional[i]);
                    { std::string resolved = resolveComMarkerForPack(packFn); if (!resolved.empty()) lastExpr_ = resolved; }
                    packedArgs.push_back(packFn + "(" + lastExpr_ + ")");
                }
                for (auto& named : node.named) {
                    std::string packFn = comPackExpr(*named.value);
                    emitExpr(*named.value);
                    { std::string resolved = resolveComMarkerForPack(packFn); if (!resolved.empty()) lastExpr_ = resolved; }
                    packedArgs.push_back(packFn + "(" + lastExpr_ + ")");
                }
                int32_t argc = (int32_t)packedArgs.size();
                std::string argsArray = "(void*[]){";
                for (int i = 0; i < argc; i++) {
                    if (i > 0) argsArray += ", ";
                    argsArray += packedArgs[i];
                }
                argsArray += "}";
                std::string callArgs = objExpr + ", L\"" + memberName + "\", " + argsArray + ", " + std::to_string(argc);
                if (returnType == "BSTR") {
                    lastExpr_ = "vb6_ComCallBSTR(" + callArgs + ")";
                } else if (returnType == "int32_t" || returnType == "int16_t") {
                    lastExpr_ = "vb6_ComCallInt(" + callArgs + ")";
                } else if (returnType == "double" || returnType == "float") {
                    lastExpr_ = "vb6_ComCallDouble(" + callArgs + ")";
                } else if (returnType == "void*") {
                    lastExpr_ = "vb6_ComCallObject(" + callArgs + ")";
                } else {
                    lastExpr_ = "vb6_ComCall(" + callArgs + ")";  // 未知返回类型: 返回void*
                }
                isComMarker_ = false;
                return;
            }
            // 有参数的方法/属性Put/签名未找到 → 降级为后期绑定 (fall through)
            isEarlyBoundCom_ = false;
        }
        isEarlyBoundCom_ = false;

        if (!node.positional.empty() || !node.named.empty()) {
            // 有参数: obj.Method(args) → vb6_ComCall(obj, L"Method", variantArgs, argc)
            std::vector<std::string> packedArgs;
            for (size_t i = 0; i < node.positional.size(); i++) {
                std::string packFn = comPackExpr(*node.positional[i]);
                emitExpr(*node.positional[i]);
                { std::string resolved = resolveComMarkerForPack(packFn); if (!resolved.empty()) lastExpr_ = resolved; }
                packedArgs.push_back(packFn + "(" + lastExpr_ + ")");
            }
            for (auto& named : node.named) {
                std::string packFn = comPackExpr(*named.value);
                emitExpr(*named.value);
                { std::string resolved = resolveComMarkerForPack(packFn); if (!resolved.empty()) lastExpr_ = resolved; }
                packedArgs.push_back(packFn + "(" + lastExpr_ + ")");
            }

            int32_t argc = (int32_t)packedArgs.size();
            std::string argsArray;
            argsArray = "(void*[]){";
            for (int i = 0; i < argc; i++) {
                if (i > 0) argsArray += ", ";
                argsArray += packedArgs[i];
            }
            argsArray += "}";

            lastExpr_ = "vb6_ComCall(" + objExpr + ", L\"" + memberName + "\", " +
                        argsArray + ", " + std::to_string(argc) + ")";
            isComMarker_ = false;  // P24-04: 参数emission可能设置脏isComMarker_
            return;
        } else {
            // 无参数: obj.Method() → vb6_ComCall(obj, L"Method", NULL, 0)
            lastExpr_ = "vb6_ComCall(" + objExpr + ", L\"" + memberName + "\", NULL, 0)";
            isComMarker_ = false;  // P24-04: 清除残留标记
            return;
        }
    }

    // Fix 044b: Check if callee is a WithMemberExpr class method that might need
    // Optional param padding. If so, skip the early-return and split the callee
    // to allow Optional padding in the normal flow below.
    bool needsSplitForOptionalPad = false;
    if (node.callee && node.callee->kind == ASTNodeKind::WithMemberExpr) {
        if (!withObjectInfoStack_.empty()) {
            const auto& info = withObjectInfoStack_.back();
            if (info.kind == WithObjKind::ClassInstance && !info.className.empty()) {
                needsSplitForOptionalPad = true;
            }
        }
    }

    // 如果callee已经是func(args)形式(如类方法调用 vb6_Counter_GetCount(c)),
    // 且IndexOrCallExpr没有额外参数, 直接使用callee避免双重括号
    if (callee.size() >= 2 && callee.back() == ')' && node.positional.empty() && node.named.empty()) {
        // 检查是否是完整的函数调用（以右括号结尾且匹配左括号）
        int depth = 0;
        bool isCompleteCall = false;
        for (int i = (int)callee.size() - 2; i >= 0; i--) {
            if (callee[i] == ')') depth++;
            else if (callee[i] == '(') {
                if (depth == 0) { isCompleteCall = true; break; }
                depth--;
            }
        }
        if (isCompleteCall && !needsSplitForOptionalPad) {
            lastExpr_ = callee;
            return;
        }
    }

    // 如果callee已经是func()形式(无参内置函数调用如vb6_Now())，
    // 需要拆开重组为func(args)，因为IndexOrCallExpr表示带参数调用
    bool calleeIsZeroArgCall = false;
    if (callee.size() >= 2 && callee.substr(callee.size() - 2) == "()") {
        calleeIsZeroArgCall = true;
        callee = callee.substr(0, callee.size() - 2);
    }

    // Fix 083e: Variant 数组嵌套索引 — vGateway(lIdx)(0) 不是 f(a,b) 而是嵌套元素访问.
    // callee 若是 vb6_VariantArrayGet(&arr, idx) 完整调用, 外层 (0) 应对内层结果再取元素.
    // (否则 P6.5 拆开会把索引合并成 vb6_VariantArrayGet(&arr, idx, 0) → C2197 参数太多)
    // Fix 084f: 旧实现用 (vb6_VARIANT){<内层调用>} 复合字面量以值初始化结构体,
    // 首成员是 vb6_vartype → 触发 C2440 "vb6_VARIANT → vb6_vartype".
    // 改用按值辅助函数 vb6_VariantArrayGetVal(内层调用, 外层索引).
    if (callee.compare(0, 20, "vb6_VariantArrayGet(") == 0 && !node.positional.empty()) {
        std::string innerCall = callee;
        std::string outerIdx;
        emitExpr(*node.positional[0]);
        // Fix 084o: 外层索引为 Variant 时转 Long
        outerIdx = toLongIfVariant(std::move(lastExpr_), node.positional[0].get());
        lastExpr_ = "vb6_VariantArrayGetVal(" + innerCall + ", " + outerIdx + ")";
        return;
    }

    // P6.5修复: 如果callee已经是func(obj)形式(如类方法调用 vb6_Button_SetCaption(btn)),
    // 且IndexOrCallExpr有额外参数, 需要拆开重组为func(obj, userArgs...),
    // 避免生成 func(obj)(userArgs) 双重括号
    std::string classMethodObjArg;  // 如果非空, 表示callee已被拆开, 需要前置此参数

    // Fix 015: 若 MemberAccessExpr.Fix015 路径已通过 pendingChainObj_ 交付对象参数,
    // 移交给 classMethodObjArg (随后会被前置到参数列表).
    // 此时 callee 是裸函数名 (如 "vb6_cDataBase_Exec"), 上面的 split 路径因
    // callee.back() != ')' 不会触发, 故不会被双重设置.
    if (!pendingChainObj_.empty()) {
        classMethodObjArg = std::move(pendingChainObj_);
        pendingChainObj_.clear();
    }

    if (callee.size() >= 2 && callee.back() == ')'
        && ((!node.positional.empty() || !node.named.empty()) || needsSplitForOptionalPad)) {
        // 检查是否是完整的函数调用（以右括号结尾且匹配左括号）
        int depth = 0;
        int openPos = -1;
        for (int i = (int)callee.size() - 2; i >= 0; i--) {
            if (callee[i] == ')') depth++;
            else if (callee[i] == '(') {
                if (depth == 0) { openPos = i; break; }
                depth--;
            }
        }
        if (openPos > 0) {
            // callee = "funcName(existingArgs)" → 拆开
            std::string funcPart = callee.substr(0, openPos);
            classMethodObjArg = callee.substr(openPos + 1, callee.size() - openPos - 2);
            callee = funcPart;
            // Fix 086: 链式默认属性调用 — 内层是无参 prop_get_(如 .Root("data")) 或
            // COM 调用 (Dic(N)(RouteName)) 时, 内层返回 COM 对象, 外层索引是对返回
            // 对象的 Item 调用. 不能并入内层参数表 (C2197 参数太多).
            // 有声明参数的 prop_get (Prop(k)) 仍走合并 (内层只发了this).
            bool innerIsChainedObj = false;
            if (!node.positional.empty() && node.named.empty()) {
                // funcPart 是拆开后的裸函数名 (无括号)
                if (funcPart == "vb6_ComCall" || funcPart == "vb6_ComCallObject"
                    || funcPart == "vb6_ComGetObjectProp"
                    || funcPart == "vb6_VariantFromComResult") {
                    innerIsChainedObj = true;
                } else {
                    size_t gp = funcPart.find("_prop_get_");
                    if (gp != std::string::npos) {
                        std::string propName = funcPart.substr(gp + 10);
                        Symbol* propSym86 = symTab_.lookupModuleByKind(propName, SymbolKind::PropertyGet);
                        if (propSym86 && propSym86->params.empty()) innerIsChainedObj = true;
                    }
                }
            }
            if (innerIsChainedObj) {
                std::string innerObj = funcPart + "(" + classMethodObjArg + ")";
                std::vector<std::string> packedArgs86;
                for (size_t i = 0; i < node.positional.size(); i++) {
                    std::string packFn86 = comPackExpr(*node.positional[i]);
                    emitExpr(*node.positional[i]);
                    { std::string resolved86 = resolveComMarkerForPack(packFn86); if (!resolved86.empty()) lastExpr_ = resolved86; }
                    packedArgs86.push_back(packFn86 + "(" + lastExpr_ + ")");
                }
                int32_t argc86 = (int32_t)packedArgs86.size();
                std::string argsArray86 = "(void*[]){";
                for (int i = 0; i < argc86; i++) {
                    if (i > 0) argsArray86 += ", ";
                    argsArray86 += packedArgs86[i];
                }
                argsArray86 += "}";
                lastExpr_ = "vb6_VariantFromComResult(vb6_ComCall(" + innerObj + ", L\"Item\", "
                          + argsArray86 + ", " + std::to_string(argc86) + "))";
                return;
            }
        }
    }

    // 位置参数
    // 需要检查被调用函数的参数签名: ByRef参数在调用点需要传指针(&arg)
    std::vector<ParameterInfo> calleeParams;
    // Fix 030b: 跟踪被调用者是否为 builtin — builtin 用 calleeParams 仅为触发
    // Fix 024 P2/Fix 029 的参数包装, 但 RTL C 签名不接受 Optional padding 和
    // IsMissing _has_ flag 尾叜 (那些只适用于用户定义函数). 见 line 3207/3237.
    bool calleeIsBuiltin = false;
    // Fix 042a: Declare 函数 (DeclareSub/DeclareFunc) 的 C 签名不接受 _has_ 尾叜,
    // 与 builtin 类似 — Optional padding 仍需要 (C 函数期望所有参数),
    // 但 IsMissing _has_ flags 不应追加.
    bool calleeIsDeclare = false;
    // Fix 041b: Track whether calleeParams was successfully resolved (even if 0 params).
    // Used to distinguish "params not looked up" from "looked up with 0 params" (e.g., Property Get
    // with no params) — needed for arg truncation when args > params.
    bool calleeParamsFound = false;
    // 从IdentifierExpr或MemberAccessExpr获取被调用函数名, 在符号表中查找
    // Fix 027: 加入 DeclareSub / DeclareFunc — 否则 WinAPI Declare 的 ByRef 参数无法 emit &,
    //          导致 ByRef UDT/SafeArray/标量 全部按值传递 (例如 SOCKADDR_IN → SOCKADDR_IN* 错误).
    if (node.callee && node.callee->kind == ASTNodeKind::IdentifierExpr) {
        auto& idExpr = static_cast<IdentifierExpr&>(*node.callee);
        // 尝试多种查找方式: 先lookupModule(过程符号), 再lookup(嵌套作用域)
        Symbol* funcSym = symTab_.lookupModule(idExpr.name);
        if (!funcSym || (funcSym->kind != SymbolKind::Sub && funcSym->kind != SymbolKind::Function
            && funcSym->kind != SymbolKind::PropertyGet && funcSym->kind != SymbolKind::PropertyLet
            && funcSym->kind != SymbolKind::PropertySet
            && funcSym->kind != SymbolKind::DeclareSub && funcSym->kind != SymbolKind::DeclareFunc)) {
            funcSym = symTab_.lookup(idExpr.name);
        }
        if (funcSym && (funcSym->kind == SymbolKind::Sub || funcSym->kind == SymbolKind::Function
            || funcSym->kind == SymbolKind::PropertyGet || funcSym->kind == SymbolKind::PropertyLet
            || funcSym->kind == SymbolKind::PropertySet
            || funcSym->kind == SymbolKind::DeclareSub || funcSym->kind == SymbolKind::DeclareFunc)) {
            calleeParams = funcSym->params;
            calleeIsBuiltin = funcSym->isBuiltin;
            calleeParamsFound = true;
            // Fix 042a: Track Declare functions for _has_ flag suppression
            if (funcSym->kind == SymbolKind::DeclareSub || funcSym->kind == SymbolKind::DeclareFunc) {
                calleeIsDeclare = true;
            }
            // Bug #3 fix: 检测函数返回类型为StdPicture/IPictureDisp (COM Picture)
            // 用于Picture属性赋值时选择vb6_SetControlPictureFromCom
            if (!funcSym->variableTypeName.empty()) {
                std::string vtLower = funcSym->variableTypeName;
                std::transform(vtLower.begin(), vtLower.end(), vtLower.begin(), ::tolower);
                if (vtLower == "stdpicture" || vtLower == "ipicturedisp" || vtLower == "ipicture") {
                    lastExprIsComPicture_ = true;
                }
            }
        }
    } else if (node.callee && node.callee->kind == ASTNodeKind::MemberAccessExpr) {
        // Module.Method 或 obj.Method 调用: 查找方法名的参数签名
        // Fix 027: 同步加入 DeclareSub/DeclareFunc (模块方法形式的 declare 调用).
        auto& maExpr = static_cast<MemberAccessExpr&>(*node.callee);

        // Fix 033: 优先用类感知查找. 原 symTab_.lookupModule(maExpr.memberName) 在
        // 跨模块同名方法冲突下 (例如 cAsyncSocket.Create / cTlsSocket.Create / cPassword.Create
        // 6+ 个类共享 storageKey="create"), 命中首个注册者而非对象真实类的方法,
        // 导致 Optional 参数 _has_ 标志个数填错 → C2197 ("too many arguments").
        // 限制条件: 仅当 maExpr.object 是 IdentifierExpr 时启用 — 此时无副作用,
        // 不需要重复 emit obj 表达式即可推断 className. 复杂链式 obj 留给原回退路径.
        bool classAwareResolved = false;
        if (maExpr.object && maExpr.object->kind == ASTNodeKind::IdentifierExpr) {
            std::string className;
            auto& idObj = static_cast<IdentifierExpr&>(*maExpr.object);
            std::string nameLower = Symbol::toLower(idObj.name);
            if (nameLower == "me") {
                // 当前类模块实例 — className = 本模块名
                if (isClassModule_) className = moduleName_;
            } else {
                auto it = knownClassVars_.find(nameLower);
                if (it != knownClassVars_.end()) {
                    className = it->second;
                } else {
                    // 不是已知类变量 → 可能是模块名或外部类名自身
                    // (例: ToolsStr.HasStr — ToolsStr 是 .bas 模块; cWinsock.SomeStaticMethod — cWinsock 类)
                    // findClassMemberCallParams 内部会按 sourceModule/moduleName_ 匹配
                    // Fix 084z-3: 对象是当前模块的属性 (Property Get 返回类实例, 如
                    // cTlsRemaster.pvSocket → cTlsSocket) 时, inferClassTypeOfExpr
                    // 可推断出类名 → 优先按类解析方法形参; 否则 findClassMemberCallParams
                    // 按属性名查找失败, 回退 lookupModule 命中 storageKey 同名冲突的
                    // 错误类 (SyncReceiveArray 按 cWinsock 12 参展开 → C2197).
                    className = idObj.name;
                    std::string inferredClass = inferClassTypeOfExpr(*maExpr.object);
                    if (!inferredClass.empty()) {
                        className = inferredClass;
                    }
                }
            }
            if (!className.empty()) {
                std::vector<ParameterInfo> params;
                bool isBuiltin = false;
                if (findClassMemberCallParams(className, maExpr.memberName, params, isBuiltin)) {
                    calleeParams = std::move(params);
                    calleeIsBuiltin = isBuiltin;
                    classAwareResolved = true;
                    calleeParamsFound = true;
                }
            }
        }

        // Fix 042b: For method chains and complex object expressions (e.g.,
        // db.Table("t").OrderByDesc("id").Limit(10).Offset(20)), the object is
        // not a simple IdentifierExpr but an IndexOrCallExpr or MemberAccessExpr.
        // Use inferClassTypeOfExpr to determine the class type, then look up
        // member params. This enables Optional param padding for cross-module
        // method chain calls.
        if (!classAwareResolved && maExpr.object
            && maExpr.object->kind != ASTNodeKind::IdentifierExpr) {
            std::string className = inferClassTypeOfExpr(*maExpr.object);
            if (!className.empty()) {
                std::vector<ParameterInfo> params;
                bool isBuiltin = false;
                if (findClassMemberCallParams(className, maExpr.memberName, params, isBuiltin)) {
                    calleeParams = std::move(params);
                    calleeIsBuiltin = isBuiltin;
                    classAwareResolved = true;
                    calleeParamsFound = true;
                }
            }
        }

        // Fix 033 回退: 类感知未命中 (对象为链式表达式 / className 找不到方法符号 /
        // 需要匹配 DeclareSub/DeclareFunc 等) → 用原 class-unaware lookupModule 兜底,
        // 保留旧行为兼容性.
        if (!classAwareResolved) {
            Symbol* funcSym = symTab_.lookupModule(maExpr.memberName);
            // Fix 084y-7: VBA 内置函数 (VBA.Replace / VBA.Mid$ / VBA.Val 等) 注册在
            // 全局内置符号表, 不在模块作用域 — lookupModule 必然失败. 回退全表
            // lookup (去 $ 后缀: Mid$ → Mid), 拿到含 Optional 的完整形参表,
            // 才能填充默认参数 (vb6_Replace 声明6参, VB6 调用只传3参).
            if (!funcSym) {
                std::string fnName = maExpr.memberName;
                if (!fnName.empty() && fnName.back() == '$') fnName.pop_back();
                funcSym = symTab_.lookup(fnName);
            }
            if (funcSym && (funcSym->kind == SymbolKind::Sub || funcSym->kind == SymbolKind::Function
                || funcSym->kind == SymbolKind::PropertyGet || funcSym->kind == SymbolKind::PropertyLet
                || funcSym->kind == SymbolKind::PropertySet
                || funcSym->kind == SymbolKind::DeclareSub || funcSym->kind == SymbolKind::DeclareFunc)) {
                calleeParams = funcSym->params;
                calleeIsBuiltin = funcSym->isBuiltin;
                calleeParamsFound = true;
                // Fix 042a: Track Declare functions for _has_ flag suppression
                if (funcSym->kind == SymbolKind::DeclareSub || funcSym->kind == SymbolKind::DeclareFunc) {
                    calleeIsDeclare = true;
                }
                // Bug #3 fix: 检测跨模块函数返回类型为StdPicture/IPictureDisp
                if (!funcSym->variableTypeName.empty()) {
                    std::string vtLower = funcSym->variableTypeName;
                    std::transform(vtLower.begin(), vtLower.end(), vtLower.begin(), ::tolower);
                    if (vtLower == "stdpicture" || vtLower == "ipicturedisp" || vtLower == "ipicture") {
                        lastExprIsComPicture_ = true;
                    }
                }
            }
        }
    }

    // Fix 041b: WithMemberExpr callee — With-block member call (e.g., .Add(...) inside With)
    // needs calleeParams for Optional param padding and IsMissing _has_ flags. Without this,
    // cross-module With-block calls with Optional params generate C2198 (too few arguments).
    if (node.callee && node.callee->kind == ASTNodeKind::WithMemberExpr) {
        auto& wmExpr = static_cast<WithMemberExpr&>(*node.callee);
        if (!withObjectInfoStack_.empty()) {
            const auto& info = withObjectInfoStack_.back();
            if (info.kind == WithObjKind::ClassInstance && !info.className.empty()) {
                std::vector<ParameterInfo> params;
                bool isBuiltin = false;
                if (findClassMemberCallParams(info.className, wmExpr.memberName, params, isBuiltin)) {
                    calleeParams = std::move(params);
                    calleeIsBuiltin = isBuiltin;
                    calleeParamsFound = true;
                }
            }
        }
    }

    // M22: 检测Declare ANSI函数调用 - 需要BSTR->ANSI转换
    bool isDeclareAnsiCall = false;
    if (node.callee && node.callee->kind == ASTNodeKind::IdentifierExpr) {
        auto& idExpr = static_cast<IdentifierExpr&>(*node.callee);
        std::string funcLower = idExpr.name;
        std::transform(funcLower.begin(), funcLower.end(), funcLower.begin(), ::tolower);
        if (knownDeclareAnsi_.count(funcLower)) isDeclareAnsiCall = true;
    }

    std::vector<std::string> args;
    for (size_t i = 0; i < node.positional.size(); i++) {
        emitExpr(*node.positional[i]);
      // COM属性标记残留: MsgBox dic.Count 等场景 — 参数是COM属性读取但标记未被消费
        // 统一用后期绑定(IDispatch), 避免vtable签名不匹配问题
        if (isComMarker_) {
            isComMarker_ = false;
            std::string objExpr = std::move(comObjExpr_);
            std::string memName = std::move(comMemberName_);
            if (isEarlyBoundCom_ && earlyBoundSym_) {
                isEarlyBoundCom_ = false;
                const Symbol* comSym = earlyBoundSym_;
                earlyBoundSym_ = nullptr;
                std::string memLower = memName;
                std::transform(memLower.begin(), memLower.end(), memLower.begin(), ::tolower);
                auto it = comSym->comMethods.find(memLower);
                if (it != comSym->comMethods.end() && it->second.isPropertyGet) {
                    const auto& sig = it->second;
                    std::string returnType = mapType(sig.returnType);
                    if (returnType == "int32_t" || returnType == "int16_t") {
                        lastExpr_ = "vb6_ComGetIntProp(" + objExpr + ", L\"" + memName + "\")";
                    } else if (returnType == "BSTR") {
                        lastExpr_ = "vb6_ComGetStringProp(" + objExpr + ", L\"" + memName + "\")";
                    } else if (returnType == "double" || returnType == "float") {
                        lastExpr_ = "vb6_ComGetDoubleProp(" + objExpr + ", L\"" + memName + "\")";
                    } else if (returnType == "void*") {
                        lastExpr_ = "vb6_ComGetObjectProp(" + objExpr + ", L\"" + memName + "\")";
                    } else {
                        lastExpr_ = "vb6_VariantFromComResult(vb6_ComGetProp(" + objExpr + ", L\"" + memName + "\"))";
                    }
                } else {
                    lastExpr_ = "vb6_VariantFromComResult(vb6_ComGetProp(" + objExpr + ", L\"" + memName + "\"))";
                }
            } else {
                lastExpr_ = "vb6_VariantFromComResult(vb6_ComGetProp(" + objExpr + ", L\"" + memName + "\"))";  /* P25: late-bound VARIANT */
            }
        }
        std::string argVal = std::move(lastExpr_);

        // M22: Declare ANSI函数 - ByVal String参数需要BSTR->ANSI转换
        // 生成临时char*变量, 调用后释放, 无内存泄露
        if (isDeclareAnsiCall && i < calleeParams.size() && calleeParams[i].isByVal
            && calleeParams[i].type == Vb6Type::String) {
            std::string ansiVar = "_ansi_" + std::to_string(ansiCounter_++);
            c_.emitLine("char* " + ansiVar + " = vb6_BSTR_ToANSI(" + argVal + ");");
            ansiTempsToFree_.push_back(ansiVar);
            argVal = ansiVar;
        }

        // ByRef参数: 调用点传指针. 如果实参已经是解引用形式(*x), 取地址还原为x;
        // 如果是普通变量, 加&取地址
        // Fix 072: VB6 允许 ByVal 覆盖 ByRef 声明 (如 SHCreateMemStream(ByVal 0, 0))
        bool argHasByValOverride = node.byvalOverrides.count(i) > 0;
        // Fix 078 rev2: ByRef array parameters are now vb6_SafeArray1D**,
        // so they DO need & at the call site — same as other ByRef params.
        // Removed isArrayParamType exclusion so arrays get & just like non-arrays.
        bool isByRef = (i < calleeParams.size() && !calleeParams[i].isByVal && !calleeParams[i].isParamArray)
                       && !argHasByValOverride;
        bool calleeParamIsArray = (i < calleeParams.size()
            && (static_cast<uint16_t>(calleeParams[i].type) & static_cast<uint16_t>(Vb6Type::Array)));
        if (isByRef) {
            if (argVal.size() > 3 && argVal.substr(0, 2) == "(*" && argVal.back() == ')') {
                // (*x) → depends on callee param type:
                // If callee param is array (vb6_SafeArray1D**), arg (*x) means x is already vb6_SafeArray1D**,
                // so pass x directly (not &x which would be ***).
                // If callee param is non-array, (*x) → depends on whether x is a ByRef param of current function:
                //   - If x is a current-function ByRef param (C type T*), then (*x) is T, and callee expects T*.
                //     Since x itself is already T*, pass x directly (not &x which would be T**).
                //   - Otherwise (e.g. local variable holding a pointer), (*x) → &x to restore the pointer.
                std::string innerName = argVal.substr(2, argVal.size() - 3);
                if (calleeParamIsArray) {
                    argVal = innerName;
                } else {
                    // Fix 079: check if innerName is a ByRef param of the current function
                    bool isCurrentByRefParam = false;
                    if (currentProc_) {
                        for (auto& p : currentProc_->params) {
                            if (Symbol::toLower(p.name) == Symbol::toLower(innerName) && !p.isByVal && !p.isParamArray) {
                                bool paramIsArray = (static_cast<uint16_t>(p.type) & static_cast<uint16_t>(Vb6Type::Array)) != 0;
                                if (!paramIsArray) {
                                    isCurrentByRefParam = true;
                                }
                                break;
                            }
                        }
                    }
                    if (isCurrentByRefParam) {
                        // ByRef param of current function → already T*, pass directly
                        argVal = innerName;
                    } else {
                        // Local pointer variable → restore pointer with &
                        argVal = "&" + innerName;
                    }
                }
            } else if (argVal.size() > 2 && argVal.substr(0, 2) == "me" && argVal[2] == '-') {
                // me->field → &(me->field) (类成员字段取地址)
                argVal = "&(" + argVal + ")";
            } else {
                // Fix 064: As Any (Vb6Type::Unknown) ByRef 参数 — 传 void* 指针,
                // 不包装为 VARIANT. VB6 中 As Any 表示"任意类型指针",
                // 对数组元素应传 &(VB6_SA_AT(...)) 即首元素地址.
                // UDT ByRef 参数同理 — C 函数签名已是 UDT*, 传 &argVal 即可.
                bool isAsAnyParam = (i < calleeParams.size() && calleeParams[i].type == Vb6Type::Unknown);
                bool isUdtByRefParam = (i < calleeParams.size() && calleeParams[i].type == Vb6Type::UserDefinedType);

                // Fix 064b: VB6_SA_AT(...) 数组元素作为 ByRef 实参时,
                // 不应包装为 VARIANT 复合字面量, 应直接取地址.
                // 典型: PolyPolygon(hDC, uPoints(0), aSizes(0), nCount)
                //   uPoints(0) → VB6_SA_AT(vb6_type_POINTAPI, uPoints, 0)
                //   应生成 (void*)&(VB6_SA_AT(...)) 而非 (&(vb6_VARIANT){.bstrVal=...})
                bool isSaAtExpr = (argVal.find("VB6_SA_AT(") == 0);
                if (isSaAtExpr && !isAsAnyParam && !isUdtByRefParam) {
                    // 数组元素作为 ByRef 参数: 当 calleeParams 缺失或类型不匹配时,
                    // 对 Declare 函数的 As Any 参数一律传 void*
                    if (calleeIsDeclare || i >= calleeParams.size()) {
                        isAsAnyParam = true;
                    }
                }

                if (isAsAnyParam || isUdtByRefParam) {
                    // As Any ByRef / UDT ByRef: 左值取地址, 非左值(字面量等)强转为void*
                    // Fix 069b: 对字面量(如 0)不能取地址 &(0) → C2101,
                    //   应直接强转 (void*)(intptr_t)(0).
                    bool isSimpleIdent = !argVal.empty() && (std::isalpha(static_cast<unsigned char>(argVal[0])) || argVal[0] == '_');
                    if (isSimpleIdent) {
                        for (char c : argVal) {
                            if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_') {
                                isSimpleIdent = false;
                                break;
                            }
                        }
                    }
                    // Fix 056b: UDT字段链 (uFile.Data.ftLastWriteTime) 也是左值,
                    // 作为 ByRef UDT/AsAny 实参应取地址 &(x) 而非强转 (void*)(intptr_t)(x)
                    // Fix 084m: 字段链须允许 -> — With变量展开 (._vb6_with_2->SendBuffer)
                    // 也是左值字段链, 若判为非左值会生成 (void*)(intptr_t)(udt) → C2440
                    bool isUdtFieldChain = !argVal.empty() && (std::isalpha(static_cast<unsigned char>(argVal[0])) || argVal[0] == '_');
                    if (isUdtFieldChain) {
                        for (size_t ci = 0; ci < argVal.size(); ci++) {
                            char c = argVal[ci];
                            if (std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '.') continue;
                            if (c == '-' && ci + 1 < argVal.size() && argVal[ci + 1] == '>') { ci++; continue; }
                            isUdtFieldChain = false;
                            break;
                        }
                    }
                    bool isSaAt = (argVal.find("VB6_SA_AT(") == 0);
                    // Fix 086: 宏常量/NULL 不是左值 — TLS_LOCAL_LEGACY_VERSION 是
                    // #define, &(宏) → C2101; NULL 同理.
                    bool isConstMacro = (argVal == "NULL") || isConstIdent(argVal);
                    bool isLValue = (isSimpleIdent || isUdtFieldChain || isSaAt || argVal.find("me->") == 0
                        || (argVal.size() > 4 && argVal[0] == '(' && argVal[1] == '*' && argVal.back() == ')'))
                        && !isConstMacro;
                    if (isLValue) {
                        // 左值: 变量名、数组元素、me->field、(*ptr) 解引用 — 可以取地址
                        argVal = "(void*)&(" + argVal + ")";
                    } else if (isConstMacro && argVal != "NULL") {
                        // Fix 086: 宏常量 → 用对应类型的复合字面量承载地址
                        std::string ccType = mapType(constIdentType(argVal));
                        if (ccType.empty() || ccType == "vb6_VARIANT") ccType = "int32_t";
                        argVal = "(void*)&(" + ccType + "){" + argVal + "}";
                    } else {
                        // 非左值: 字面量或复杂表达式 — 直接强转为 void*
                        argVal = "(void*)(intptr_t)(" + argVal + ")";
                    }
                } else {
                // Fix 072b: ByVal 覆盖 As Any 参数 — 传 (void*)(intptr_t)(val),
                // 不走复合字面量路径. 典型: SHCreateMemStream(ByVal 0, 0) 中
                // pInit As Any 是 ByRef, 但 ByVal 0 覆盖为传值, 应生成 (void*)0.
                bool isAsAnyByVal = (i < calleeParams.size() && calleeParams[i].type == Vb6Type::Unknown)
                                    && argHasByValOverride;
                if (isAsAnyByVal) {
                    argVal = "(void*)(intptr_t)(" + argVal + ")";
                } else {
                // 变量/非左值 → 复合字面量取地址; 变量 → &变量
                // 检查是否是简单标识符 (变量名, 以字母/下划线开头)
                bool isSimpleIdent = !argVal.empty() && (std::isalpha(static_cast<unsigned char>(argVal[0])) || argVal[0] == '_');
                if (isSimpleIdent) {
                    for (char c : argVal) {
                        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_') {
                            isSimpleIdent = false;
                            break;
                        }
                    }
                }
                if (isSimpleIdent && !argVal.empty()) {
                    // Fix 079: If the simple identifier is a ByRef param of the current function,
                    // it's already a pointer (T*) — pass directly, don't add &.
                    bool argIsCurrentByRefParam = false;
                    if (currentProc_) {
                        for (auto& p : currentProc_->params) {
                            if (Symbol::toLower(p.name) == Symbol::toLower(argVal) && !p.isByVal && !p.isParamArray) {
                                argIsCurrentByRefParam = true;
                                break;
                            }
                        }
                    }
                    if (argIsCurrentByRefParam) {
                        // ByRef param of current function → already T*, pass directly
                    } else if (isConstIdent(argVal)) {
                        // Fix 084aa: 常量是 #define 宏 (vb6_BSTR_FromStr(...) 或数值字面量),
                        // 取址 &ERR_X 展开为函数结果取址 → C2102. 按形参类型复合字面量包装.
                        Vb6Type pType = Vb6Type::Variant;
                        if (i < calleeParams.size()) pType = calleeParams[i].type;
                        argVal = wrapConstArgForByRef(argVal, pType);
                    } else {
                        argVal = "&" + argVal;
                    }
                } else {
                    // 字面量或复杂表达式: 使用C11复合字面量
                    // &(int32_t){10} 或 &(double){3.14}
                    std::string cType = "int32_t";
                    if (i < calleeParams.size()) {
                        cType = mapType(calleeParams[i].type);
                    }
                    // M22: VARIANT类型需要指定字段初始化 (.vt=VT_xxx, .field=value)
                    if (cType == "vb6_VARIANT") {
                        // 推断实参的VB6类型来决定VARIANT字段
                        Vb6Type argVbType = inferExprType(*node.positional[i]);
                        switch (argVbType) {
                            case Vb6Type::String:
                                // Fix 027b: 外层加括号让预处理器把 {a,b} 内的逗号视为同一参数
                                // (避免宏调用时 #define RtlCopyMemory(D,s,l) memcpy(D,s,l) 把复合字面量
                                //  内部的逗号错算成宏实参分隔符 → C4002 参数过多)
                                argVal = "(&(vb6_VARIANT){.vt=VT_BSTR, .bstrVal=" + argVal + "})";
                                break;
                            case Vb6Type::Long:
                            case Vb6Type::Integer:
                                argVal = "(&(vb6_VARIANT){.vt=VT_I4, .lVal=(int32_t)(" + argVal + ")})";
                                break;
                            case Vb6Type::Double:
                            case Vb6Type::Single:
                                argVal = "(&(vb6_VARIANT){.vt=VT_R8, .dblVal=(double)(" + argVal + ")})";
                                break;
                            case Vb6Type::Boolean:
                                argVal = "(&(vb6_VARIANT){.vt=VT_BOOL, .boolVal=(int16_t)(" + argVal + ")})";
                                break;
                            case Vb6Type::Byte:
                                argVal = "(&(vb6_VARIANT){.vt=VT_UI1, .bVal=(uint8_t)(" + argVal + ")})";
                                break;
                            case Vb6Type::Date:
                                argVal = "(&(vb6_VARIANT){.vt=VT_DATE, .dblVal=(double)(" + argVal + ")})";
                                break;
                            default:
                                // Fix 054: Variant 或未知类型
                                // 1. 实参是 UDT 的 Variant 字段 (With 成员如 .SourceFile,
                                //    .FileName 等 As Variant) → 值已是 vb6_VARIANT,
                                //    ByRef Variant 形参直接取地址 &(...), 不要包装成
                                //    {.vt=VT_BSTR, .bstrVal=...} (Variant→BSTR C2440).
                                // 2. C 表达式已是 Variant (vb6_VariantArrayGet 等),
                                //    提取 BSTR 值到 .bstrVal (避免 VARIANT→BSTR C2440)
                                // 3. 其他未知类型保持原样 (UDT 等单独处理)
                                if (i < node.positional.size()
                                    && inferUdtFieldVb6Type(node.positional[i].get()) == Vb6Type::Variant) {
                                    argVal = "&(" + argVal + ")";
                                } else if (cExprIsVariant(argVal)) {
                                    argVal = "(&(vb6_VARIANT){.vt=VT_BSTR, .bstrVal=vb6_VariantToString(" + argVal + ")})";
                                } else {
                                    argVal = "(&(vb6_VARIANT){.vt=VT_BSTR, .bstrVal=" + argVal + "})";
                                }
                                break;
                        }
                    } else {
                        // Fix 027b: 同样加外层括号防止预处理器把复合字面量 `{a, b}` 内的逗号算成宏实参分隔符.
                        argVal = "(&(" + cType + "){" + argVal + "})";
                    }
                }
                } // end Fix 072b else (As Any ByVal override)
                    } // end Fix 064 else
            }
        } // Fix 024 P2: ByVal Variant 参数 — 调用点用 vb6_VariantFromValue 包装实参.
        // 处理 callee 声明 "ByVal x As Variant" 而实参是标量/BSTR/SafeArray/class ptr 等情形.
        // _Generic 在编译期按实类型选择 ctor: 标量->VariantLong/Int/Double, BSTR->String,
        // SafeArray1D*->Array, void*/class ptr->Object, vb6_VARIANT->Identity(no-op).
        // 仅对 ByVal Variant 生效 (ByRef Variant 走上面复合字面量路径, 取地址需左值).
        if (!isByRef && i < calleeParams.size() && calleeParams[i].isByVal
            && calleeParams[i].type == Vb6Type::Variant) {
            // Fix 084d: 实参若是类对象表达式 (如 me->Database, 类方法返回类对象),
            // 保持对象指针直传, 不要套 vb6_VariantFromValue — 符号表可能把类类型
            // 参数误记为 Variant, 若包装成 vb6_VARIANT 传给 vb6_cls_cXxx* 参数
            // 会触发 C2440 (如 cHttpServer.c LoadFromDatabase(..., me->Database)).
            bool argIsClassObj = false;
            if (i < node.positional.size()) {
                argIsClassObj = !inferClassTypeOfExpr(*node.positional[i]).empty();
            }
            if (!argIsClassObj) {
                argVal = "vb6_VariantFromValue(" + argVal + ")";
            }
        }
        // Fix 029: 反向强制 — ByVal 具体类型参数 + 实参确定为 Variant: 自动调用提取函数.
        // 与 Fix 024 P2 互补: P2 处理 V(callee)=Variant,V(arg)=scalar; 此处处理
        // V(callee)=concrete,V(arg)=Variant. 覆盖 C2440 子类 to_int32/to_BSTR/to_SafeArray.
        // 严格判断: 用 isDefinitelyVariantExpr 避免 inferExprType 默认回退到 Variant
        // 造成对内置函数 (LenB 等) / UDT 字段访问 (uAddr.sin_addr) 的错误包装.
        if (!isByRef && i < calleeParams.size() && calleeParams[i].isByVal
            && calleeParams[i].type != Vb6Type::Variant) {
            // 处理 Array 标志位 (例如 Variant() 参数对应 Vb6Type::Variant|Array)
            bool paramIsArray = (static_cast<uint16_t>(calleeParams[i].type)
                                  & static_cast<uint16_t>(Vb6Type::Array)) != 0;
            Vb6Type paramBase = static_cast<Vb6Type>(
                static_cast<uint16_t>(calleeParams[i].type)
                & ~static_cast<uint16_t>(Vb6Type::Array));
            bool argIsVariantArr = false;
            bool argIsVariant = isDefinitelyVariantExpr(*node.positional[i], &argIsVariantArr);
            // Fix 038b-2: 字符串级 Variant 检测回退 — 补充 isDefinitelyVariantExpr
            // 无法识别的 C 级 Variant 表达式 (vb6_VariantArrayGet, vb6_VariantFromComResult 等)
            if (!argIsVariant && !argIsVariantArr) {
                argIsVariant = cExprIsVariant(argVal);
            }
            if (argIsVariant || argIsVariantArr) {
                if (paramIsArray && (paramBase == Vb6Type::Variant || paramBase == Vb6Type::Byte
                    || paramBase == Vb6Type::String || paramBase == Vb6Type::Long)) {
                    // 数组参数: 从 Variant 提取 SafeArray1D*
                    argVal = "vb6_VariantToSafeArray1D(" + argVal + ")";
                } else if (paramBase == Vb6Type::Long || paramBase == Vb6Type::Integer
                           || paramBase == Vb6Type::Byte || paramBase == Vb6Type::Boolean) {
                    argVal = "vb6_VariantToLong(" + argVal + ")";
                } else if (paramBase == Vb6Type::Double || paramBase == Vb6Type::Single
                           || paramBase == Vb6Type::Currency) {
                    argVal = "vb6_VariantToDouble(" + argVal + ")";
                } else if (paramBase == Vb6Type::String) {
                    // Fix 049b: Skip extraction if the C expression is already BSTR
                    // (e.g., VB6_SA_AT(BSTR, arr, idx) — isDefinitelyVariantExpr may
                    // return true due to symbol table/actual type mismatch)
                    if (argVal.find("VB6_SA_AT(BSTR,") == std::string::npos
                        && argVal.find("vb6_BSTR") == std::string::npos) {
                        argVal = "vb6_VariantToString(" + argVal + ")";
                    }
                } else if (paramBase == Vb6Type::Object) {
                    // 右值兼容: 避免对函数返回值取址
                    argVal = "vb6_VariantToObjectVal(" + argVal + ")";
                }
            }
        }
        // Fix 086: ByVal Variant形参兜底 — 实参为具体标量/BSTR/对象指针时,
        // 用 _Generic vb6_VariantFromValue 包装. _Generic 按实参C类型自动选择
        // 构造函数 (int→VariantLong, BSTR→VariantString, SafeArray*→VariantArray,
        // 类指针→VariantObject, 已是VARIANT→恒等), 从根源消除 int/BSTR/double→
        // vb6_VARIANT 方向的 C2440.
        if (!isByRef && i < calleeParams.size() && calleeParams[i].isByVal
            && (calleeParams[i].type == Vb6Type::Variant
                || calleeParams[i].type == Vb6Type::Empty)) {
            bool alreadyVariant = cExprIsVariant(argVal);
            if (!alreadyVariant && node.positional[i]->kind == ASTNodeKind::IdentifierExpr) {
                auto& idArg86 = static_cast<IdentifierExpr&>(*node.positional[i]);
                if (knownVariantVars_.count(Symbol::toLower(idArg86.name))) alreadyVariant = true;
            }
            // &(x) 形态是 ByRef 风格临时/取址, 不适合按值包装
            if (!alreadyVariant && argVal.compare(0, 2, "&(") != 0) {
                Vb6Type argT86 = inferExprType(*node.positional[i]);
                bool argIsArr86 = (static_cast<uint16_t>(argT86) & static_cast<uint16_t>(Vb6Type::Array)) != 0;
                Vb6Type argBase86 = static_cast<Vb6Type>(
                    static_cast<uint16_t>(argT86) & ~static_cast<uint16_t>(Vb6Type::Array));
                bool scalarLike =
                    argT86 == Vb6Type::Long || argT86 == Vb6Type::Integer
                    || argT86 == Vb6Type::Boolean || argT86 == Vb6Type::Byte
                    || argT86 == Vb6Type::Double || argT86 == Vb6Type::Single
                    || argT86 == Vb6Type::Currency || argT86 == Vb6Type::Date
                    || argT86 == Vb6Type::LongPtr || argT86 == Vb6Type::String
                    || argT86 == Vb6Type::Object || argBase86 == Vb6Type::Object
                    || argIsArr86;
                if (scalarLike) {
                    argVal = "vb6_VariantFromValue(" + argVal + ")";
                }
            }
        }
        // Fix 038b-2: calleeParams 为空 (运行时/内置函数) 时的参数类型转换.
        // 通过 getRuntimeParamCType 查找期望的 C 类型, 当实参为 Variant 时
        // 自动插入 VARIANT→具体类型提取函数. 解决 vb6_ErrRaise, vb6_BSTR_Assign,
        // vb6_BSTR_Concat, vb6_StrCmp 等运行时函数的 C2440 错误.
        // 注意: 仅使用 cExprIsVariant (C 字符串级) 和 knownVariantVars_ 检测,
        // 不使用 isDefinitelyVariantExpr (AST 级), 因为符号表中的 Variant 返回类型
        // 可能与实际 C 函数返回类型不一致 (如 prop_get 返回 void* 而非 vb6_VARIANT).
        if (!isByRef && i >= calleeParams.size()) {
            std::string rtParamType = getRuntimeParamCType(callee, i);
            if (!rtParamType.empty() && rtParamType != "vb6_VARIANT"
                 && rtParamType != "vb6_VARIANT*") {
                bool argIsVariant = cExprIsVariant(argVal);
                // 也检查已知 Variant 变量
                if (!argIsVariant && node.positional[i]->kind == ASTNodeKind::IdentifierExpr) {
                    auto& idArg = static_cast<IdentifierExpr&>(*node.positional[i]);
                    std::string argLower = idArg.name;
                    std::transform(argLower.begin(), argLower.end(), argLower.begin(), ::tolower);
                    if (knownVariantVars_.count(argLower)) argIsVariant = true;
                }
                if (argIsVariant) {
                    if (rtParamType == "int32_t" || rtParamType == "int16_t"
                        || rtParamType == "uint8_t" || rtParamType == "LONG") {
                        argVal = "vb6_VariantToLong(" + argVal + ")";
                    } else if (rtParamType == "double" || rtParamType == "float") {
                        argVal = "vb6_VariantToDouble(" + argVal + ")";
                    } else if (rtParamType == "BSTR") {
                        // Fix 049b: Skip extraction if the C expression is already BSTR
                        if (argVal.find("VB6_SA_AT(BSTR,") == std::string::npos
                            && argVal.find("vb6_BSTR") == std::string::npos) {
                            argVal = "vb6_VariantToString(" + argVal + ")";
                        }
                    } else if (rtParamType == "void*") {
                        argVal = "vb6_VariantToObjectVal(" + argVal + ")";
                    } else if (rtParamType == "vb6_SafeArray1D*") {
                        argVal = "vb6_VariantToSafeArray1D(" + argVal + ")";
                    }
                }
            }
        }
        args.push_back(std::move(argVal));
    }

    // Fix 081c: For named-arg path, track which Optional params were actually passed
    // (before gap-filling sets filled[i]=true for padding params too)
    std::vector<bool> actuallyPassedParams;

    // P14.3.3: 命名参数位置展开 - 按参数名映射到正确位置
    if (!node.named.empty() && !calleeParams.empty()) {
        // Build name->index map from callee params (case-insensitive)
        std::unordered_map<std::string, size_t> paramMap;
        for (size_t i = 0; i < calleeParams.size(); i++) {
            std::string lower = calleeParams[i].name;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            if (!lower.empty()) paramMap[lower] = i;
        }

        // Create position-mapped args vector
        size_t totalParams = calleeParams.size();
        std::vector<std::string> orderedArgs(totalParams);
        std::vector<bool> filled(totalParams, false);

        // Place positional args (already in args[])
        for (size_t i = 0; i < args.size() && i < totalParams; i++) {
            orderedArgs[i] = std::move(args[i]);
            filled[i] = true;
        }

        // ByRef handling helper lambda
        auto applyByRef = [&](std::string& argVal, size_t pi) {
            // Fix 078 rev2: ByRef array params are now vb6_SafeArray1D**,
            // so they need & at call site — same as other ByRef params.
            bool piHasByValOverride = node.byvalOverrides.count(pi) > 0;
            bool isByRef = (pi < calleeParams.size() && !calleeParams[pi].isByVal && !calleeParams[pi].isParamArray)
                           && !piHasByValOverride;
            if (!isByRef) return;
            bool piCalleeParamIsArray = (pi < calleeParams.size()
                && (static_cast<uint16_t>(calleeParams[pi].type) & static_cast<uint16_t>(Vb6Type::Array)));
            if (argVal.size() > 3 && argVal.substr(0, 2) == "(*" && argVal.back() == ')') {
                // (*x) → depends on callee param type (see Fix 078 rev2 details in position-arg path)
                std::string innerName = argVal.substr(2, argVal.size() - 3);
                if (piCalleeParamIsArray) {
                    argVal = innerName;
                } else {
                    // Fix 079: check if innerName is a ByRef param of the current function
                    bool piIsCurrentByRefParam = false;
                    if (currentProc_) {
                        for (auto& p : currentProc_->params) {
                            if (Symbol::toLower(p.name) == Symbol::toLower(innerName) && !p.isByVal && !p.isParamArray) {
                                bool paramIsArray = (static_cast<uint16_t>(p.type) & static_cast<uint16_t>(Vb6Type::Array)) != 0;
                                if (!paramIsArray) {
                                    piIsCurrentByRefParam = true;
                                }
                                break;
                            }
                        }
                    }
                    if (piIsCurrentByRefParam) {
                        argVal = innerName;
                    } else {
                        argVal = "&" + innerName;
                    }
                }
            } else if (argVal.size() > 2 && argVal[0] == '&') {
                // already has &, keep as-is
            } else if (argVal.size() > 2 && argVal.substr(0, 2) == "me" && argVal[2] == '-') {
                argVal = "&(" + argVal + ")";
            } else {
                bool isSimpleIdent = !argVal.empty() && (std::isalpha(static_cast<unsigned char>(argVal[0])) || argVal[0] == '_');
                if (isSimpleIdent) {
                    for (char c : argVal) {
                        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_') {
                            isSimpleIdent = false; break;
                        }
                    }
                }
                if (isSimpleIdent && !argVal.empty()) {
                    // Fix 079: If the simple identifier is a ByRef param of the current function,
                    // it's already a pointer (T*) — pass directly, don't add &.
                    bool piArgIsCurrentByRefParam = false;
                    if (currentProc_) {
                        for (auto& p : currentProc_->params) {
                            if (Symbol::toLower(p.name) == Symbol::toLower(argVal) && !p.isByVal && !p.isParamArray) {
                                piArgIsCurrentByRefParam = true;
                                break;
                            }
                        }
                    }
                    if (piArgIsCurrentByRefParam) {
                        // ByRef param of current function → already T*, pass directly
                    } else if (argVal == "NULL" || argVal == "0") {
                        // Fix 086: NULL/0 无可取址, 直接传空指针 (ByRef 形参收到 NULL)
                    } else if (isConstIdent(argVal)) {
                        // Fix 084aa: 常量宏不可取址 → 按形参类型复合字面量包装
                        Vb6Type pType = Vb6Type::Variant;
                        if (pi < calleeParams.size()) pType = calleeParams[pi].type;
                        argVal = wrapConstArgForByRef(argVal, pType);
                    } else {
                        argVal = "&" + argVal;
                    }
                } else {
                    std::string cType = "int32_t";
                    if (pi < calleeParams.size()) cType = mapType(calleeParams[pi].type);
                    if (cType == "vb6_VARIANT") {
                        // Fix 086: 非左值表达式传 ByRef Variant — 不能用 {argVal}
                        // 首字段初始化 (C2440: 指针初始化VARTYPE). 空对象/零值
                        // (如内置App对象 (void*)0) → 零初始化VARIANT (VT_EMPTY,
                        // 与VB6传Nothing语义一致); 其余退回零初始化以保编译通过.
                        if (!(argVal == "(void*)0" || argVal == "NULL" || argVal == "0")) {
                            diag_.warn(DiagnosticID::CodeGenUnsupportedFeature, SourceLocation{},
                                "P7.5: ByRef Variant arg is not addressable, passing empty Variant (value dropped): " + argVal);
                        }
                        argVal = "&(vb6_VARIANT){0}";
                    } else {
                        argVal = "&(" + cType + "){" + argVal + "}";
                    }
                }
            }
        };

        // Place named args at their parameter positions
        for (auto& named : node.named) {
            std::string lower = named.name;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            auto it = paramMap.find(lower);
            if (it != paramMap.end()) {
                size_t pi = it->second;
                emitExpr(*named.value);
                std::string argVal = std::move(lastExpr_);
                applyByRef(argVal, pi);
                if (pi < orderedArgs.size()) {
                    orderedArgs[pi] = std::move(argVal);
                    filled[pi] = true;
                }
            }
        }

        // Fill gaps with optional/missing param defaults
        for (size_t i = 0; i < totalParams; i++) {
            if (!filled[i]) {
                if (calleeParams[i].isParamArray) {
                    orderedArgs[i] = "NULL";
                    // filled[i] stays false for actuallyPassed tracking
                } else {
                    orderedArgs[i] = defaultValue(calleeParams[i].type);
                    // filled[i] stays false for actuallyPassed tracking
                }
            }
        }

        // Fix 081c: Save which params were actually passed (before gap-filling set all to true)
        actuallyPassedParams = filled;

        args = std::move(orderedArgs);
    } else {
        // No named args or no param info: append named args as-is (fallback)
        for (auto& named : node.named) {
            emitExpr(*named.value);
            args.push_back(std::move(lastExpr_));
        }
    }

    // P14.1.5: ParamArray packing - if callee has a ParamArray parameter,

    // pack extra arguments into a SAFEARRAY* and adjust argList

    std::string argList;

    int paIndex = -1;  // index of ParamArray parameter in calleeParams

    for (size_t i = 0; i < calleeParams.size(); i++) {

        if (calleeParams[i].isParamArray) { paIndex = (int)i; break; }

    }



    if (paIndex >= 0) {

        // Split args: normal args [0..paIndex-1] + ParamArray args [paIndex..end]

        int normalCount = paIndex;  // number of non-ParamArray params

        int paArgCount = (int)args.size() - normalCount;

        if (paArgCount < 0) paArgCount = 0;



        // Build normal argList

        for (int i = 0; i < normalCount && i < (int)args.size(); i++) {

            if (i > 0) argList += ", ";

            argList += args[i];

        }



        // Pack ParamArray args into SAFEARRAY* using a temp variable

        std::string paVar = "_pa_" + std::to_string(tempCounter_++);

        if (paArgCount > 0) {

            c_.emitLine("SAFEARRAY* " + paVar + " = vb6_PA_Create(" + std::to_string(paArgCount) + ");");

            for (int i = 0; i < paArgCount; i++) {

                int argIdx = normalCount + i;

                if (argIdx < (int)args.size()) {

                    std::string paArgExpr = args[argIdx];

                    bool isLongArg = false;

                    bool isDoubleArg = false;

                    if (argIdx < (int)node.positional.size()) {

                        auto& paArg = node.positional[argIdx];

                        if (paArg->kind == ASTNodeKind::LiteralExpr) {

                            auto& lit = static_cast<LiteralExpr&>(*paArg);
                            if (lit.literalKind == LiteralKind::Integer) isLongArg = true;
                            else if (lit.literalKind == LiteralKind::Double) isDoubleArg = true;

                        }
                    }

                    /* Fix 082: Check if arg is a COM interface pointer or VarPtr result.
                       These are pointer-sized and must use PA_SetLongPtr on x64. */
                    bool isComPtrArg = false;
                    if (argIdx < (int)node.positional.size()) {
                        auto& paArg = node.positional[argIdx];
                        // Check for VarPtr/ObjPtr/StrPtr call
                        if (paArg->kind == ASTNodeKind::IndexOrCallExpr) {
                            auto& callNode = static_cast<IndexOrCallExpr&>(*paArg);
                            if (callNode.callee && callNode.callee->kind == ASTNodeKind::IdentifierExpr) {
                                auto& ident = static_cast<IdentifierExpr&>(*callNode.callee);
                                std::string vpLower = ident.name;
                                std::transform(vpLower.begin(), vpLower.end(), vpLower.begin(), ::tolower);
                                if (vpLower == "varptr" || vpLower == "objptr" || vpLower == "strptr") {
                                    isComPtrArg = true;
                                }
                            }
                        }
                        // Check for LongPtr/Object variable (COM interface pointer)
                        if (paArg->kind == ASTNodeKind::IdentifierExpr) {
                            auto& ident = static_cast<IdentifierExpr&>(*paArg);
                            std::string identLower = ident.name;
                            std::transform(identLower.begin(), identLower.end(), identLower.begin(), ::tolower);
                            if (knownLongPtrVars_.count(identLower)) {
                                isComPtrArg = true;
                            }
                        }
                    }
                    /* Also detect from C expression: VarPtr result, address-of, COM iface */
                    if (!isComPtrArg) {
                        isComPtrArg = (paArgExpr.find("&(") != std::string::npos ||
                                      paArgExpr.find("(intptr_t)") != std::string::npos ||
                                      paArgExpr.find("vb6_ComIface_") != std::string::npos);
                    }

                    if (isLongArg) {

                        c_.emitLine("vb6_PA_SetLong(" + paVar + ", " + std::to_string(i) + ", " + paArgExpr + ");");

                    } else if (isDoubleArg) {

                        c_.emitLine("vb6_PA_SetDouble(" + paVar + ", " + std::to_string(i) + ", " + paArgExpr + ");");

                    } else {

                        bool looksLikeBSTR = (paArgExpr.find("vb6_BSTR") != std::string::npos ||

                                             paArgExpr.find("L\"") != std::string::npos);

                        if (looksLikeBSTR) {

                            c_.emitLine("vb6_PA_SetBSTR(" + paVar + ", " + std::to_string(i) + ", " + paArgExpr + ");");

                        } else if (isComPtrArg) {

                            /* Fix 082: COM interface pointers and VarPtr() results need PA_SetLongPtr
                               (VT_I8 / intptr_t) to avoid pointer truncation on x64. */
                            c_.emitLine("vb6_PA_SetLongPtr(" + paVar + ", " + std::to_string(i) + ", (intptr_t)" + paArgExpr + ");");

                        } else {

                            c_.emitLine("vb6_PA_SetLong(" + paVar + ", " + std::to_string(i) + ", " + paArgExpr + ");");

                        }

                    }

                }

            }

        } else {

            c_.emitLine("SAFEARRAY* " + paVar + " = NULL;");

        }



        // Append SAFEARRAY* to argList

        if (!argList.empty()) argList += ", ";

        argList += paVar;



        // P14.1.4 Optional padding for params BEFORE the ParamArray

        if (normalCount > (int)args.size()) {

            for (int i = (int)args.size(); i < normalCount; i++) {

                if (!argList.empty()) argList += ", ";

                const auto& param = calleeParams[i];

                std::string defVal;

                if (param.hasDefaultValue && !param.defaultValueExpr.empty()) {

                    defVal = param.defaultValueExpr;

                } else {

                    defVal = defaultValue(param.type);

                }

                if (param.isByVal) {

                    argList += defVal;

                } else {

                    std::string cType = mapType(param.type);

                    // P20-36: Variant/struct types can't use {funcCall()} compound literal
                    if (param.type == Vb6Type::Variant || param.type == Vb6Type::Empty ||
                        param.type == Vb6Type::Null || param.type == Vb6Type::Object) {
                        argList += "&(" + cType + "){0}";
                    } else {
                        argList += "&(" + cType + "){" + defVal + "}";
                    }

                }

            }

        }

        // P20-36: IsMissing _has_ flags for Optional params before ParamArray
        for (int i = 0; i < normalCount; i++) {
            const auto& param = calleeParams[i];
            if (param.isOptional && !param.isParamArray) {
                if (!argList.empty()) argList += ", ";
                argList += (i < (int)args.size()) ? "1" : "0";
            }
        }

    } else {

        // No ParamArray - normal argList construction
        // Fix 041c: Truncate extra args when more args than calleeParams.
        // This handles Declare functions called with more args than their C signature
        // (e.g., CallWindowProcW with 8 VB6 args but 5 C params) and Property Get
        // called with index args that should have been default-member calls.
        size_t maxArgs = args.size();
        if (calleeParamsFound && !calleeIsBuiltin && args.size() > calleeParams.size()) {
            maxArgs = calleeParams.size();
        }

        for (size_t i = 0; i < maxArgs; i++) {

            if (i > 0) argList += ", ";

            argList += args[i];

        }

    }

    // P8.1: UBound/LBound - 1D鐢╲b6_UBound, ND鐢╲b6_UBoundND/vb6_LBoundND

    // P14.1.5: Also handle UBound/LBound on ParamArray parameters

    if (callee == "vb6_UBound" || callee == "vb6_LBound") {

        // P14.1.5: Check if first arg is a ParamArray parameter

        bool firstArgIsPA = false;

        if (node.positional.size() >= 1 && currentProc_) {

            auto& firstArg = node.positional[0];

            if (firstArg->kind == ASTNodeKind::IdentifierExpr) {

                std::string argLower = static_cast<IdentifierExpr&>(*firstArg).name;

                std::transform(argLower.begin(), argLower.end(), argLower.begin(), ::tolower);

                for (auto& p : currentProc_->params) {

                    if (p.isParamArray) {

                        std::string pLower = p.name;

                        std::transform(pLower.begin(), pLower.end(), pLower.begin(), ::tolower);

                        if (pLower == argLower) {

                            firstArgIsPA = true;

                            break;

                        }

                    }

                }

            }

        }



        if (firstArgIsPA) {

            // ParamArray: UBound(args) -> vb6_PA_UBound(args), LBound(args) -> vb6_PA_LBound(args)

            if (callee == "vb6_UBound") {

                callee = "vb6_PA_UBound";

            } else {

                callee = "vb6_PA_LBound";

            }

            // argList should be just the PA variable name (no dimension param)

            if (args.size() == 1) {

                argList = args[0];

            }

        } else {

            if (args.size() == 1) {

                // 缺省维度参数, 补1

                argList += ", 1";

            }

            // 检查是否为ND数组, 需要用ND版本

            // Bug #1 fix: 两种检测方式:
            // 1) arrayDimCounts_中有记录且维度>1 (适用于Dim声明的ND数组)
            // 2) dimension参数值>1 (适用于ByRef参数, 声明时维度未知但调用时指定)

            bool useND = false;

            if (node.positional.size() >= 1) {

                auto& firstArg = node.positional[0];

                std::string arrLower;

                if (firstArg->kind == ASTNodeKind::IdentifierExpr) {

                    arrLower = static_cast<IdentifierExpr&>(*firstArg).name;

                    std::transform(arrLower.begin(), arrLower.end(), arrLower.begin(), ::tolower);

                }

                auto itDc = arrayDimCounts_.find(arrLower);

                if (itDc != arrayDimCounts_.end() && itDc->second > 1) {

                    useND = true;

                }

                // Bug #1 fix: 如果dimension参数值>1, 也使用ND版本

                // dimension参数是第二个参数 (index 1 in args, after the array arg)

                if (!useND && args.size() >= 2) {

                    // args[1] 是 dimension 参数的C表达式, 检查是否是常量>1

                    const std::string& dimExpr = args[1];

                    // 尝试解析为整数常量

                    try {

                        int dimVal = std::stoi(dimExpr);

                        if (dimVal > 1) {

                            useND = true;

                        }

                    } catch (...) {

                        // 非常量表达式, 无法确定; 不启用ND

                    }

                }

                // Bug #1 fix (082h): 第三种检测 - 同一过程中已有UBound(arr,N>1)使用ND版本

                // 例如: UBound(uVectors, 2) 用了ND版本, 则 UBound(uVectors, 1) 也应使用ND版本

                if (!useND && knownNDArraysInProc_.count(arrLower)) {

                    useND = true;

                }

                if (useND) {

                    // 注册到过程内ND数组集合, 后续 UBound(arr,1) 也会用ND版本

                    if (!arrLower.empty()) {

                        knownNDArraysInProc_.insert(arrLower);

                    }

                    // ND数组 -> 使用vb6_UBoundND/vb6_LBoundND

                    if (callee == "vb6_UBound") {

                        callee = "vb6_UBoundND";

                    } else {

                        callee = "vb6_LBoundND";

                    }

                    // ND版本需要(vb6_SafeArrayND*)转换第一个参数

                    // argList格式是 "arrExpr, dimExpr", 需要改为 "(vb6_SafeArrayND*)(arrExpr), dimExpr"

                    if (args.size() >= 2) {

                        argList = "(vb6_SafeArrayND*)(" + args[0] + "), " + args[1];

                    } else if (args.size() == 1) {

                        argList = "(vb6_SafeArrayND*)(" + args[0] + "), 1";

                    }

                }

            }

        }

    }    // InStr: VB6允许2参数形式 InStr(string1, string2)
    // RTL: vb6_InStr(start, haystack, needle) → 2参数时补start=1
    if (callee == "vb6_InStr") {
        if (args.size() == 2) {
            argList = "1, " + argList;
        }
    }

    // P14.2.2: CurDir - VB6 allows 0-arg CurDir() → vb6_CurDir(NULL)
    if (callee == "vb6_CurDir") {
        if (args.empty()) {
            argList = "NULL";
        }
    }

    // P14.2.2: Dir - VB6 allows 1-arg Dir(pattern) → vb6_Dir(pattern, 0)
    if (callee == "vb6_Dir") {
        if (args.size() == 1) {
            argList += ", 0";
        }
        // Fix 086: 无参 Dir() (继续上次搜索) → 传 NULL, 0
        if (args.empty()) {
            argList = "NULL, 0";
        }
    }

    // P14.2.3: Split - VB6 Split(expr[, delim[, limit[, compare]]])
    // RTL: vb6_Split(expr, delim, limit, compare)
    if (callee == "vb6_Split") {
        if (args.size() == 1) {
            argList += ", NULL";        // default delimiter = space
        }
        if (args.size() <= 2) {
            argList += ", -1";          // limit = -1 (unlimited)
        }
        if (args.size() <= 3) {
            argList += ", 0";           // compare = binary
        }
    }

    // P14.2.3: Join - VB6 Join(arr[, delimiter])
    // RTL: vb6_Join(arr, delimiter)
    if (callee == "vb6_Join") {
        if (args.size() == 1) {
            argList += ", NULL";        // default delimiter = space
        }
    }

    // P14.2.4: DateDiff - VB6 DateDiff(interval, date1, date2[, firstDayOfWeek[, firstWeekOfYear]])
    // RTL: vb6_DateDiff(interval, date1, date2, firstDayOfWeek, firstWeekOfYear)
    if (callee == "vb6_DateDiff") {
        if (args.size() == 3) {
            argList += ", 1, 1";  // vbSunday, vbFirstJan1
        } else if (args.size() == 4) {
            argList += ", 1";     // vbFirstJan1
        }
    }

    // P14.2.4: DatePart - VB6 DatePart(interval, date[, firstDayOfWeek[, firstWeekOfYear]])
    // RTL: vb6_DatePart(interval, date, firstDayOfWeek, firstWeekOfYear)
    if (callee == "vb6_DatePart") {
        if (args.size() == 2) {
            argList += ", 1, 1";  // vbSunday, vbFirstJan1
        } else if (args.size() == 3) {
            argList += ", 1";     // vbFirstJan1
        }
    }

    // Replace: VB6允许3参数形式 Replace(string, find, replacement)
    // RTL: Replace(string, find, replacement, start, count, compare)
    if (callee == "vb6_Replace") {
        if (args.size() == 3) {
            argList += ", 1, -1, 0";
        } else if (args.size() == 4) {
            argList += ", -1, 0";
        } else if (args.size() == 5) {
            argList += ", 0";
        }
    }

    // P21-B: Weekday(date[, firstDayOfWeek]) - default firstDayOfWeek=1 (vbSunday)
    if (callee == "vb6_Weekday") {
        if (args.size() == 1) {
            argList += ", 1";
        }
    }

    // P21-10: FormatDateTime(date[, namedFormat]) - default namedFormat=0 (vbGeneralDate)
    if (callee == "vb6_FormatDateTime") {
        if (args.size() == 1) {
            argList += ", 0";
        }
    }

    // P21-16: NPer(rate, pmt, pv[, fv][, type]) - defaults: fv=0, type=0
    if (callee == "vb6_NPer") {
        if (args.size() == 3) argList += ", 0, 0";
        else if (args.size() == 4) argList += ", 0";
    }

    // P21-19: IRR(values[, guess]) - default guess=0.1
    if (callee == "vb6_IRR") {
        if (args.size() == 1) argList += ", 0.1";
    }

    // P20-37: GetSetting(app, section, key[, default]) - default is empty string
    if (callee == "vb6_GetSetting") {
        if (args.size() == 3) argList += ", vb6_BSTR_Empty()";
    }

    // Fix 010e: Err.Raise with Source/Description arguments
    // Err.Raise always calls vb6_ErrRaise(number, source, description) — 3 args
    // Pad with NULL BSTRs when fewer args provided
    if (callee == "vb6_ErrRaise" && args.size() < 3) {
        // Pad to 3 args
        while (args.size() < 3) {
            args.push_back("(BSTR)0");
            if (!argList.empty()) argList += ", ";
            argList += "(BSTR)0";
        }
    }
    if (callee == "vb6_ErrRaise" && args.size() > 3) {
        // Truncate to 3 (Err.Raise can have up to 5 VB6 args, C RTL handles 3)
        argList = args[0] + ", " + args[1] + ", " + args[2];
    }

    // Fix 010r: Builtin RTL functions with optional parameters — pad to required arg count
    // VB6 allows omitting optional args; C RTL functions require all args
    if (callee == "vb6_Mid" && args.size() < 3) {
        while (args.size() < 3) {
            args.push_back("0");
            if (!argList.empty()) argList += ", ";
            argList += "0";
        }
    }
    if (callee == "vb6_StrConv" && args.size() < 3) {
        while (args.size() < 3) {
            args.push_back("0");
            if (!argList.empty()) argList += ", ";
            argList += "0";
        }
    }
    if (callee == "vb6_FormatNumber" && args.size() < 5) {
        while (args.size() < 5) {
            args.push_back("-1");  // -1 = vbUseDefault for optional args
            if (!argList.empty()) argList += ", ";
            argList += "-1";
        }
    }
    // Fix 034: Builtin RTL functions with Optional params — pad to required C RTL arg count.
    // calleeParams 已对 builtin 标 isOptional, 但 general padding (line 3291+) 被 !calleeIsBuiltin
    // 跳过 (RTL C 函数不接受 _has_ 尾叜). 各函数硬编码补默认值, 默认值取自 VB6 语义.
    // InStrRev(string1, string2[, start[, compare]]) — start=-1 (从末尾), compare=0 (Binary)
    if (callee == "vb6_InStrRev") {
        if (args.size() == 2) {
            argList += ", -1, 0";
        } else if (args.size() == 3) {
            argList += ", 0";
        }
    }
    // Round(x[, decimals]) — decimals default = 0
    if (callee == "vb6_Round" && args.size() == 1) {
        argList += ", 0";
    }
    // StrComp(s1, s2[, compare]) — compare default = 0 (Binary)
    if (callee == "vb6_StrComp" && args.size() == 2) {
        argList += ", 0";
    }
    // Shell(pathname[, windowstyle]) — windowstyle default = 2 (vbMinimizedFocus)
    if (callee == "vb6_Shell" && args.size() == 1) {
        argList += ", 2";
    }
    // Rnd([seed]) — seed default = 0 (Rnd with no arg uses last seed or random)
    // Randomize([seed]) — handled as Sub bare-call; see cgen_stmt.cpp Randomize dispatch
    if (callee == "vb6_Rnd" && args.empty()) {
        argList = "0";
    }
    // Randomize As Function-call form `Randomize()` — pad seed = 0.0
    if (callee == "vb6_Randomize" && args.empty()) {
        argList = "0.0";
    }
    // Fix 041: InStr(start, string1, string2, compare) — C function vb6_InStr takes 3 args
    // (no compare parameter). Truncate the 4th arg (compare) when present.
    if (callee == "vb6_InStr" && args.size() > 3) {
        argList = args[0] + ", " + args[1] + ", " + args[2];
    }

    // P14.1.4: General Optional parameter padding for user-defined functions
    // calleeParams is empty for builtin RTL functions (registered without params), so they're auto-skipped
    // Fix 030b: builtin 即使现在有 calleeParams (用于触发包装), 也不走 padding/IsMissing 路径
    // (RTL C 签名不接受尾叜 _has_ flag, 默认值由 builtin 的特殊 codegen 处理如 UBound 补 dimension=
    if (calleeParams.size() > 0 && args.size() < calleeParams.size() && paIndex < 0 && !calleeIsBuiltin) {
        for (size_t i = args.size(); i < calleeParams.size(); i++) {
            if (i > 0 || !args.empty()) argList += ", ";
            const auto& param = calleeParams[i];
            // Determine the default value (explicit or type-zero)
            std::string defVal;
            if (param.hasDefaultValue && !param.defaultValueExpr.empty()) {
                defVal = param.defaultValueExpr;
            } else {
                defVal = defaultValue(param.type);
            }
            // ByRef params need pointer, ByVal need value
            if (param.isByVal) {
                argList += defVal;
            } else {
                // ByRef: pass address of compound literal: &(type){defVal}
                std::string cType = mapType(param.type);
                // P20-36: Variant/struct types can't use {funcCall()} compound literal
                if (param.type == Vb6Type::Variant || param.type == Vb6Type::Empty ||
                    param.type == Vb6Type::Null || param.type == Vb6Type::Object) {
                    argList += "&(" + cType + "){0}";
                } else {
                    argList += "&(" + cType + "){" + defVal + "}";
                }
            }
                    }

                    // Fix 056: ND版本需要vb6_SafeArrayND*参数, 动态数组声明为1D*
                    if (!args.empty()) {
                        args[0] = "(vb6_SafeArrayND*)" + args[0];
                    }

                }
    // P20-36: IsMissing support - append _has_ flags for Optional params
    // For each Optional param in calleeParams: 1 if actually passed, 0 if padded
    // Fix 030b: builtin 跳过 (RTL C 函数无 _has_ 尾叜)
    // Fix 042a: Declare 函数也跳过 (Declare C 签名无 _has_ 尾叜, 但 Optional padding 仍需要)
    if (calleeParams.size() > 0 && paIndex < 0 && !calleeIsBuiltin && !calleeIsDeclare) {
        bool hasOptional = false;
        for (size_t i = 0; i < calleeParams.size(); i++) if (calleeParams[i].isOptional && !calleeParams[i].isParamArray) { hasOptional = true; break; }
        if (hasOptional) {
        for (size_t i = 0; i < calleeParams.size(); i++) {
            const auto& param = calleeParams[i];
            if (param.isOptional && !param.isParamArray) {
                if (!argList.empty()) argList += ", ";
                // Fix 081c: For named-arg path, use actuallyPassedParams to determine _has_ flag.
                // For positional-arg path, args.size() already equals actual arg count (padding
                // doesn't increase args.size()), so (i < args.size()) is correct.
                if (!actuallyPassedParams.empty() && i < actuallyPassedParams.size()) {
                    argList += actuallyPassedParams[i] ? "1" : "0";
                } else {
                    argList += (i < args.size()) ? "1" : "0";
                }
            }
        }
        }  // end if (hasOptional)
    }  // end if (calleeParams.size() > 0 && ...)

    // P6.6: 类模块中调用同类方法(包括递归), 需要自动添加me作为第一个参数
    // 如 Factorial(N-1) -> vb6_MathLib_Factorial(me, (N-1))
    if (classMethodObjArg.empty() && isClassModule_ && currentProc_) {
        std::string modPrefix = "vb6_" + cIdent(moduleName_) + "_";
        if (callee.find(modPrefix) == 0) {
            classMethodObjArg = "(void*)me";
        }
    }

    // P6.5: 如果classMethodObjArg非空, 需要将其作为第一个参数插入
    if (!classMethodObjArg.empty()) {
        if (argList.empty()) {
            argList = classMethodObjArg;
        } else {
            argList = classMethodObjArg + ", " + argList;
        }
    }

    // M22-fix: CStr类型适配 — 根据参数类型选择正确的CStr变体
    if (callee == "vb6_CStr" && !node.positional.empty()) {
        auto& firstArg = node.positional[0];
        if (firstArg->kind == ASTNodeKind::IdentifierExpr) {
            auto& idArg = static_cast<IdentifierExpr&>(*firstArg);
            std::string argLower = idArg.name;
            std::transform(argLower.begin(), argLower.end(), argLower.begin(), ::tolower);
            // 参数是BSTR变量 → CStr是空操作, 直接使用参数
            if (knownBstrVars_.count(argLower)) {
                emitExpr(*firstArg);
                return;
            }
            // 参数是int32_t/Long变量 → 用vb6_CStrLong
            if (knownLongVars_.count(argLower)) {
                callee = "vb6_CStrLong";
            }
            // 参数是double变量 → 用vb6_CStrDbl
            else if (knownDoubleVars_.count(argLower)) {
                callee = "vb6_CStrDbl";
            }
            // 参数是Variant变量 → 保留vb6_CStr(VARIANT)
        } else {


            // 非标识符表达式: 推断类型选择CStr变体
            Vb6Type argType = inferExprType(*firstArg);
            if (argType == Vb6Type::String) {
                // Fix 056b: CStr(String) 是空操作 → 直接使用参数
                // (否则生成 vb6_CStr(BSTR) → C2440: 无法从BSTR转换为vb6_VARIANT)
                if (!args.empty()) { lastExpr_ = args[0]; return; }
                lastExpr_ = "vb6_BSTR_Empty()";
                return;
            }
            if (argType == Vb6Type::Long || argType == Vb6Type::Integer) {
                callee = "vb6_CStrLong";
            } else if (argType == Vb6Type::Double || argType == Vb6Type::Single) {
                callee = "vb6_CStrDbl";
            }
            // P25: COM属性取值已在args[0]中, 根据取值函数选择CStr变体或跳过
            if (!args.empty()) {
                const std::string& a0 = args[0];
                if (a0.find("vb6_ComGetStringProp") == 0 || a0.find("vb6_ComCallBSTR") == 0) {
                    // 已是BSTR, CStr是空操作
                    lastExpr_ = a0;
                    return;
                } else if (a0.find("vb6_ComGetIntProp") == 0 || a0.find("vb6_ComVtableGetInt") == 0) {
                    callee = "vb6_CStrLong";
                } else if (a0.find("vb6_ComGetDoubleProp") == 0 || a0.find("vb6_ComVtableGetDouble") == 0) {
                    callee = "vb6_CStrDbl";
                }
            }
        }
        // Fix 056b: callee 仍为 vb6_CStr 且实参非Variant → 用 vb6_VariantFromValue 包装
        // (如 CStr((int32_t)GetCurrentThreadId()) / CStr(模块级Long变量) →
        //  vb6_CStr(vb6_VariantFromValue(x)), 由 _Generic 按实参类型自动包装, 消除 C2440)
        if (callee == "vb6_CStr" && !args.empty()) {
            lastExpr_ = "vb6_CStr(vb6_VariantFromValue(" + args[0] + "))";
            return;
        }
    }
    // P8.4: Variant参数适配 — 如果目标函数不接受Variant但参数是Variant类型, 使用V后缀函数
    // CInt(Variant)→vb6_CIntV, CDbl(Variant)→vb6_CDblV, CLng(Variant)→vb6_CLngV
    if (callee == "vb6_CInt" || callee == "vb6_CLng" || callee == "vb6_CDbl") {
        // 检查第一个参数是否为Variant变量
        bool firstArgIsVariant = false;
        bool firstArgIsBstr = false;
        if (!node.positional.empty()) {
            auto& firstArg = node.positional[0];
            if (firstArg->kind == ASTNodeKind::IdentifierExpr) {
                auto& idArg = static_cast<IdentifierExpr&>(*firstArg);
                std::string argLower = idArg.name;
                std::transform(argLower.begin(), argLower.end(), argLower.begin(), ::tolower);
                if (knownVariantVars_.count(argLower)) firstArgIsVariant = true;
                else if (knownBstrVars_.count(argLower)) firstArgIsBstr = true;
            }
            // Fix 036: 非标识符 Variant 表达式 (函数返回 Variant 的调用/类方法等) 也需 V 后缀
            if (!firstArgIsVariant) {
                firstArgIsVariant = isDefinitelyVariantExpr(*firstArg);
            }
            // Fix 038b-4: 字符串级 Variant 检测回退 — 捕获 vb6_VariantArrayGet 等表达式
            if (!firstArgIsVariant && !args.empty()) {
                firstArgIsVariant = cExprIsVariant(args[0]);
            }
        }
        if (firstArgIsVariant) {
            if (callee == "vb6_CInt") callee = "vb6_CIntV";
            else if (callee == "vb6_CLng") callee = "vb6_CLngV";
            else if (callee == "vb6_CDbl") callee = "vb6_CDblV";
            // Fix 084l: 撤销 Fix 038b-2 的 Variant→double 提前提取 (getRuntimeParamCType
            // 把 CLng/CDbl/CInt 形参当作 double, 已将 lRet 替换为 vb6_VariantToDouble(lRet)).
            // V 后缀函数直接接受 vb6_VARIANT — 保留提取会产生 vb6_CLngV(double) → C2440.
            if (!args.empty()) {
                static const char* extractPrefixes[] = {
                    "vb6_VariantToDouble(", "vb6_VariantToLong("};
                for (auto* pre : extractPrefixes) {
                    size_t pl = strlen(pre);
                    if (args[0].compare(0, pl, pre) == 0 && args[0].size() > pl + 1) {
                        args[0] = args[0].substr(pl, args[0].size() - pl - 1);
                        argList.clear();
                        for (size_t ai = 0; ai < args.size(); ai++) {
                            if (ai > 0) argList += ", ";
                            argList += args[ai];
                        }
                        break;
                    }
                }
            }
        }
        // Fix 036: BSTR 参数 → vb6_Val 转换为 double (CInt/CLng/CDbl 接 double)
        // Fix 084c: 扩展检测到字符串级 BSTR 表达式 (vb6_Trim/vb6_BSTR_Concat/vb6_CStr
        // /vb6_VariantToString 等), 不仅限于已知 BSTR 标识符 — 否则
        // vb6_CLng(vb6_Trim(vb6_VariantToString(...))) 触发 C2440 "BSTR → double".
        bool argIsBstrExpr = firstArgIsBstr;
        if (!argIsBstrExpr && !args.empty()) {
            std::string& a0 = args[0];
            argIsBstrExpr =
                a0.find("vb6_Trim(") == 0 || a0.find("vb6_LTrim(") == 0 ||
                a0.find("vb6_RTrim(") == 0 || a0.find("vb6_StrConv(") == 0 ||
                a0.find("vb6_Mid(") == 0 || a0.find("vb6_Left(") == 0 ||
                a0.find("vb6_Right(") == 0 || a0.find("vb6_Replace(") == 0 ||
                a0.find("vb6_String(") == 0 || a0.find("vb6_Format(") == 0 ||
                a0.find("vb6_UCase(") == 0 || a0.find("vb6_LCase(") == 0 ||
                a0.find("vb6_Space(") == 0 || a0.find("vb6_IIfBSTR(") == 0 ||
                a0.find("vb6_Chr(") == 0 || a0.find("vb6_ChrW(") == 0 ||
                a0.find("vb6_BSTR_Concat(") == 0 || a0.find("vb6_BSTR_FromStr(") == 0 ||
                a0.find("vb6_CStr") == 0 || a0.find("vb6_VariantToString(") == 0 ||
                a0.find("VB6_SA_AT(BSTR,") != std::string::npos;
        }
        if (argIsBstrExpr && !args.empty()) {
            args[0] = "vb6_Val(" + args[0] + ")";
            argList.clear();
            for (size_t i = 0; i < args.size(); i++) {
                if (i > 0) argList += ", ";
                argList += args[i];
            }
        }
    }
    // MsgBox自动BSTR转换: MsgBox期望BSTR, 非BSTR的prompt参数需包装
    // 注意: 只转换第一个参数(prompt), 不能把整个argList包进去(MsgBox v, , title时argList含3个参数)
    if (callee == "vb6_MsgBox" || callee == "vb6_MsgBox1") {
        if (!args.empty()) {
            std::string& firstArg = args[0];
            if (firstArg.find("vb6_ComGetIntProp") == 0 ||
                firstArg.find("vb6_ComVtableGetInt") == 0) {
                firstArg = "vb6_CStrLong(" + firstArg + ")";
            } else if (firstArg.find("vb6_ComGetDoubleProp") == 0 ||
                       firstArg.find("vb6_ComVtableGetDouble") == 0) {
                firstArg = "vb6_CStrDbl(" + firstArg + ")";
            } else if (firstArg.find("vb6_VariantFromComResult") == 0) {
                // COM调用结果(VARIANT*)→vb6_VARIANT, 需转BSTR
                firstArg = "vb6_VariantToString(" + firstArg + ")";
            } else if (firstArg.find("vb6_ComCall(") == 0) {
                // P24-01: 后期绑定COM调用返回VARIANT*, 需解包转BSTR
                firstArg = "vb6_VariantToString(vb6_VariantFromComResult(" + firstArg + "))";
            } else if (!node.positional.empty() && inferExprType(*node.positional[0]) == Vb6Type::Variant) {
                // Variant类型变量/表达式: MsgBox v → vb6_VariantToString(v)
                firstArg = "vb6_VariantToString(" + firstArg + ")";
            }
        }
    }
    // P25: MsgBox BSTR转换可能修改了args[0], 需重建argList
    if (callee == "vb6_MsgBox" || callee == "vb6_MsgBox1") {
        argList.clear();
        for (size_t i = 0; i < args.size(); i++) {
            if (i > 0) argList += ", ";
            argList += args[i];
        }
    }
    // MsgBox(prompt) -> vb6_MsgBox1(prompt)
    // MsgBox(prompt, buttons) -> vb6_MsgBox(prompt, buttons, NULL)
    if (callee == "vb6_MsgBox") {
        if (node.positional.size() == 1) {
            callee = "vb6_MsgBox1";
        } else if (node.positional.size() == 2) {
            argList += ", NULL";
        }
    }
    // P26: Format 第一个参数需要包装为 vb6_VARIANT
    if (callee == "vb6_Format" && !args.empty()) {
        Vb6Type argType = inferExprType(*node.positional[0]);
        if (argType == Vb6Type::Long || argType == Vb6Type::Integer) {
            args[0] = "vb6_VariantLong(" + args[0] + ")";
        } else if (argType == Vb6Type::Double || argType == Vb6Type::Single) {
            args[0] = "vb6_VariantDouble(" + args[0] + ")";
        } else if (argType == Vb6Type::String) {
            args[0] = "vb6_VariantString(" + args[0] + ")";
        } else if (argType == Vb6Type::Boolean) {
            args[0] = "vb6_VariantInt((int16_t)(" + args[0] + "))";
        } else if (argType == Vb6Type::Byte) {
            args[0] = "vb6_VariantInt((int16_t)(" + args[0] + "))";
        } else if (argType == Vb6Type::Date) {
            args[0] = "vb6_VariantDouble((double)(" + args[0] + "))";
        }
        // Fix 036: Format fallback — 未匹配类型 (Currency/Unknown 等, 非 Variant) 用
        // vb6_VariantFromValue 包装. Variant 类型无需包装 (已是 vb6_VARIANT).
        else if (argType != Vb6Type::Variant) {
            args[0] = "vb6_VariantFromValue(" + args[0] + ")";
        }
        argList.clear();
        for (size_t i = 0; i < args.size(); i++) {
            if (i > 0) argList += ", ";
            argList += args[i];
        }
    }
    // Fix 036: CStr fallback — 当特殊分支未匹配 (Boolean/Byte/Date/Currency/常量/
    // 未注册函数返回值/Const BSTR 等), callee 仍为 vb6_CStr (接 vb6_VARIANT). 用
    // vb6_VariantFromValue 包装 args[0], _Generic 按实参 C 类型自动选择 Variant ctor,
    // 避免 concrete → VARIANT C2440. 对已是 vb6_VARIANT 的实参为 identity (no-op), 安全.
    // 但需排除确定 Variant 的表达式 (如 Me.Segments(i) 返回 Variant): _Generic 宏
    // 对某些 Variant 表达式展开可能产生逗号问题 → C2197, 故用 isDefinitelyVariantExpr
    // 跳过, 保留 vb6_CStr(variantExpr) 原样 (vb6_CStr 接 VARIANT, 直接可用).
    if (callee == "vb6_CStr" && !args.empty()) {
        bool argIsDefVariant = false;
        if (!node.positional.empty()) {
            argIsDefVariant = isDefinitelyVariantExpr(*node.positional[0]);
        }
        // Fix 038b-4: 字符串级 Variant 检测 — 如果是 C 级 Variant 表达式,
        // 也跳过 VariantFromValue 包装 (vb6_CStr 直接接 VARIANT)
        if (!argIsDefVariant) {
            argIsDefVariant = cExprIsVariant(args[0]);
        }
        if (!argIsDefVariant) {
            args[0] = "vb6_VariantFromValue(" + args[0] + ")";
            argList.clear();
            for (size_t i = 0; i < args.size(); i++) {
                if (i > 0) argList += ", ";
                argList += args[i];
            }
        }
    }
    // Fix 040d: CallByName variadic args packing
    // C signature: vb6_CallByName(void* obj, const wchar_t* procName, int32_t callType,
    //                              void* args, int32_t argc)
    // First 3 VB6 args map to obj/procName/callType. Extra args (4+) are method
    // arguments, packed into (void*[]){vb6_ComPackValue(arg), ...} with argc.
    if (callee == "vb6_CallByName" && args.size() >= 3) {
        if (args.size() == 3) {
            // No extra method args: pass NULL, 0
            argList = args[0] + ", " + args[1] + ", " + args[2] + ", NULL, 0";
        } else {
            // Pack extra args [3..] into void*[] array using vb6_ComPackValue
            std::string argsArray = "(void*[]){";
            for (size_t i = 3; i < args.size(); i++) {
                if (i > 3) argsArray += ", ";
                argsArray += "vb6_ComPackValue(" + args[i] + ")";
            }
            argsArray += "}";
            int32_t extraArgc = (int32_t)args.size() - 3;
            argList = args[0] + ", " + args[1] + ", " + args[2] + ", "
                    + argsArray + ", " + std::to_string(extraArgc);
        }
    }
    // Fix 065: LenB(UDT) — MSVC _Generic 对自定义结构体类型匹配有问题,
    // 直接生成 sizeof(vb6_type_XXX) 而不经 vb6_LenB _Generic 宏
    if (callee == "vb6_LenB" && args.size() == 1) {
        // 检查参数是否是已知 UDT 变量
        std::string argLower = args[0];
        // 去除可能的 With 前缀: _vb6_with_N. → 提取变量名
        // 去除 me-> 前缀
        if (argLower.substr(0, 4) == "me->") argLower = argLower.substr(4);
        std::transform(argLower.begin(), argLower.end(), argLower.begin(), ::tolower);
        auto udtIt = knownUdtVars_.find(argLower);
        if (udtIt != knownUdtVars_.end()) {
            lastExpr_ = "(int32_t)sizeof(" + udtIt->second + ")";
            return;
        }
        // Fix 084h: UDT 数组元素 LenB(VB6_SA_AT(vb6_type_XXX, arr, idx)) →
        // sizeof(vb6_type_XXX)。_Generic 宏无法匹配自定义结构体类型, 且
        // 数组元素类型未知, 直接由类型名生成 sizeof。
        {
            std::string rawArg = args[0];
            std::string rawLower = rawArg;
            std::transform(rawLower.begin(), rawLower.end(), rawLower.begin(), ::tolower);
            const char* saPrefix = "vb6_sa_at(vb6_type_";
            if (rawLower.compare(0, 19, saPrefix) == 0) {
                size_t comma = rawArg.find(',');
                if (comma != std::string::npos) {
                    std::string typeName = rawArg.substr(19, comma - 19);
                    lastExpr_ = "(int32_t)sizeof(" + typeName + ")";
                    return;
                }
            }
        }
    }
    lastExpr_ = callee + "(" + argList + ")";
}

void CCodeGen::visit(NewExpr& node) {
    // 查找是否为本工程内的类模块
    std::string clsLower = node.className;
    std::transform(clsLower.begin(), clsLower.end(), clsLower.begin(), ::tolower);

    // 尝试在符号表中查找类符号
    auto* clsSym = symTab_.lookupModule(node.className);
    if (clsSym && clsSym->kind == SymbolKind::Class) {
        // 本工程类: 调用类工厂函数
        std::string clsStruct = "vb6_cls_" + cIdent(clsSym->name);
        lastExpr_ = "(" + clsStruct + "_New())";
    } else if (clsSym && clsSym->kind == SymbolKind::ComClass) {
        // P24-11: COM early-bound class: use real ProgID from TypeLib, not the raw class name
        std::string progId = clsSym->comProgId.empty() ? node.className : clsSym->comProgId;
        lastExpr_ = "(void*)vb6_NewObject(L\"" + progId + "\")";
    } else {
        // 外部/COM对象: 回退到运行时
        lastExpr_ = "vb6_NewObject(L\"" + node.className + "\")";
    }
}
void CCodeGen::visit(TypeOfExpr& node) {
    emitExpr(*node.object);
    std::string obj = std::move(lastExpr_);
    // Fix 040b: vb6_TypeOf expects void* (IDispatch*). If the operand is a
    // Variant (vb6_VARIANT struct), extract the object pointer first.
    if (cExprIsVariant(obj)) {
        obj = "vb6_VariantToObjectVal(" + obj + ")";
    } else if (node.object && node.object->kind == ASTNodeKind::IdentifierExpr) {
        auto& ident = static_cast<IdentifierExpr&>(*node.object);
        std::string lower = ident.name;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        if (knownVariantVars_.count(lower)) {
            obj = "vb6_VariantToObjectVal(" + obj + ")";
        }
    }
    lastExpr_ = "vb6_TypeOf(" + obj + ", L\"" + node.typeName + "\")";
}

void CCodeGen::visit(AddressOfExpr& node) {
    // Fix 086: AddressOf 跨模块函数解析. VB6 里 AddressOf DelayTimerProc 的
    // 目标可能定义在其他模块 (mDelay.DelayTimerProc), 或显式模块限定
    // (AddressOf ToolsTimer.TimerProc). 此前固定用当前模块前缀+Private,
    // 生成不存在的 vb6_<本模块>_<函数> → C2065/C2129.
    std::string fnName = node.funcName;
    size_t dot = fnName.find('.');
    if (dot != std::string::npos) fnName = fnName.substr(dot + 1);
    Symbol* aoSym = symTab_.lookupModule(fnName);
    if (!aoSym) aoSym = symTab_.lookup(fnName);
    if (aoSym && (aoSym->kind == SymbolKind::Sub || aoSym->kind == SymbolKind::Function)
        && aoSym->isExternal && !aoSym->sourceModule.empty()) {
        lastExpr_ = "(void*)" + cProcName(fnName, aoSym->access, aoSym->sourceModule);
        return;
    }
    lastExpr_ = "(void*)" + cProcName(node.funcName, AccessLevel::Private);
}

void CCodeGen::visit(MeExpr& node) {
    // 类模块中: me 是方法参数
    if (isClassModule_) {
        lastExpr_ = "me";
    } else if (isFormModule_ && !knownFormName_.empty()) {
        // 窗体模块: Me => vb6_hwnd_<FormName> (保持原始大小写)
        auto it = knownFormControlOriginalNames_.find(knownFormName_);
        if (it != knownFormControlOriginalNames_.end()) {
            lastExpr_ = "vb6_hwnd_" + cIdent(it->second);
        } else {
            lastExpr_ = "vb6_hwnd_" + moduleName_;
        }
    } else {
        lastExpr_ = "vb6_Me";
    }
}

void CCodeGen::visit(WithMemberExpr& node) {
    // P17.1: With块内 .Member — 根据对象类型分发
    if (withObjectVars_.empty() || withObjectInfoStack_.empty()) {
        lastExpr_ = "/* .Member outside With */";
        return;
    }

    const auto& info = withObjectInfoStack_.back();
    const std::string& tempVar = withObjectVars_.back();
    std::string memLower = node.memberName;
    std::transform(memLower.begin(), memLower.end(), memLower.begin(), ::tolower);

    switch (info.kind) {
    case WithObjKind::FormControl: {
        // .Property → vb6_GetControlXxx(tempVar) or Menu prop
        std::string readFn = getControlPropReadFn(info.ctrlType, node.memberName);
        if (!readFn.empty()) {
            if (info.ctrlType == FrmControlType::Menu) {  // P20-36: Menu uses (hmenu, menuId) args
                std::string mnuLower = info.ctrlOrigName;
                std::transform(mnuLower.begin(), mnuLower.end(), mnuLower.begin(), ::tolower);
                lastExpr_ = readFn + "(" + makeCtrlHwndArg(mnuLower, info.ctrlType) + ")  /* With menu .Property */";
            } else {
                lastExpr_ = readFn + "(" + tempVar + ")  /* With ctrl .Property */";
            }
            return;
        }
        diag_.warn(DiagnosticID::CodeGenUnsupportedFeature, SourceLocation{},
            std::string("P17.1: Unknown control property '.'") + node.memberName + "' in With block");
        lastExpr_ = tempVar + "." + cIdent(node.memberName);
        return;
    }
    case WithObjKind::WithEventsCtrl: {
        // .Property → vb6_GetControlXxx(ctrlOrigName)
        std::string readFn = getControlPropReadFn(info.ctrlType, node.memberName);
        if (!readFn.empty()) {
            lastExpr_ = readFn + "(" + info.ctrlOrigName + ")  /* With WE ctrl .Property */";
            return;
        }
        lastExpr_ = info.ctrlOrigName + "." + cIdent(node.memberName);
        return;
    }
    case WithObjKind::COMObject: {
        // .Property → 设置COM标记，让下游(IndexOrCallExpr/AssignmentStmt)处理
        comObjExpr_ = tempVar;
        comMemberName_ = node.memberName;
        isComMarker_ = true;
        isEarlyBoundCom_ = false;
        earlyBoundSym_ = nullptr;
        lastExpr_ = tempVar + "  /* With COM ." + node.memberName + " */";
        return;
    }
    case WithObjKind::ClassInstance: {
        // .Method/Property → 类方法调用 funcName(tempVar)
        // .DataMember → 尝试查找属性/方法, 找不到则用COM后期绑定
        // Fix 011r-1: 若className已知, 优先用resolveClassMemberCall精确解析该类成员
        // 避免symTab_.lookupModule捡错模块(Pattern A2)
        if (!info.className.empty()) {
            std::string funcName = resolveClassMemberCall(info.className, node.memberName);
            if (!funcName.empty()) {
                // Fix 044a: When used as standalone expression (!asCallCallee_),
                // pad Optional params (value + _has_ flags). When used as callee
                // in IndexOrCallExpr (asCallCallee_=true), the IndexOrCallExpr
                // will handle padding via Fix 044b.
                if (!asCallCallee_) {
                    std::vector<ParameterInfo> params;
                    bool isBuiltin = false;
                    if (findClassMemberCallParams(info.className, node.memberName,
                                                   params, isBuiltin)
                        && !params.empty() && !isBuiltin) {
                        std::string argList = tempVar;
                        for (size_t i = 0; i < params.size(); i++) {
                            const auto& param = params[i];
                            argList += ", ";
                            std::string defVal;
                            if (param.hasDefaultValue && !param.defaultValueExpr.empty()) {
                                defVal = param.defaultValueExpr;
                            } else {
                                defVal = defaultValue(param.type);
                            }
                            if (param.isByVal) {
                                argList += defVal;
                            } else {
                                std::string cType = mapType(param.type);
                                if (param.type == Vb6Type::Variant
                                    || param.type == Vb6Type::Empty
                                    || param.type == Vb6Type::Null
                                    || param.type == Vb6Type::Object) {
                                    argList += "&(" + cType + "){0}";
                                } else {
                                    argList += "&(" + cType + "){" + defVal + "}";
                                }
                            }
                        }
                        for (size_t i = 0; i < params.size(); i++) {
                            const auto& param = params[i];
                            if (param.isOptional && !param.isParamArray) {
                                argList += ", 0";
                            }
                        }
                        lastExpr_ = funcName + "(" + argList + ")";
                    } else {
                        lastExpr_ = funcName + "(" + tempVar + ")";
                    }
                } else {
                    lastExpr_ = funcName + "(" + tempVar + ")";
                }
                return;
            }
            // 未找到方法/属性 → 假设是数据字段: tempVar->member
            // (此时tempVar类型为 vb6_cls_<className>*, ->访问正确编译)
            lastExpr_ = tempVar + "->" + cIdent(node.memberName)
                      + "  /* With class ." + node.memberName + " field */";
            return;
        }

        // className未知 — 使用原symTab查找(可能捡错模块, 但无法避免)
        Symbol* memSym = symTab_.lookupModule(node.memberName);
        if (!memSym) {
            memSym = symTab_.lookup(node.memberName);
        }
        if (memSym) {
            if (memSym->kind == SymbolKind::PropertyGet || memSym->kind == SymbolKind::PropertyLet
                || memSym->kind == SymbolKind::PropertySet || memSym->kind == SymbolKind::Sub
                || memSym->kind == SymbolKind::Function) {
                std::string memberCName = node.memberName;
                if (memSym->kind == SymbolKind::PropertyGet)
                    memberCName = "prop_get_" + node.memberName;
                else if (memSym->kind == SymbolKind::PropertyLet)
                    memberCName = "prop_let_" + node.memberName;
                else if (memSym->kind == SymbolKind::PropertySet)
                    memberCName = "prop_set_" + node.memberName;
                std::string funcName = cProcName(memberCName, memSym->access,
                    memSym->isExternal ? memSym->sourceModule : "");
                lastExpr_ = funcName + "(" + tempVar + ")";
                return;
            }
            // 变量/常量/枚举成员 → 尝试属性查找
        }
        // Fix 010n/023d: 未知成员 → COM后期绑定. 改为设置 COM marker
        // 让下游 (IndexOrCallExpr / AssignmentStmt / BinaryExpr / 外层
        // MemberAccessExpr) 在 resolveComValue 时根据上下文 unpackType 选择
        // 正确的 vb6_ComGet*Prop / vb6_ComCall 函数.
        // 此前直接 emit `vb6_ComGetStringProp(tempVar, L"Member")` 会让外层
        // MemberAccessExpr 的链式 COM 检测2 (visit(MemberAccessExpr) ~line 1479)
        // 无法识别为 COM 对象表达式 — 因为该检测只匹配 vb6_ComCallObject /
        // ComGetObjectProp / ComCall / ComGetProp, 不匹配 ComGetStringProp,
        // 导致 `.X.Y` 被错生成成 `vb6_ComGetStringProp(obj, L"X").Y` (C2224:
        // .Y of BSTR-结构体类型). 与 WithObjKind::COMObject case (line ~3362)
        // 行为一致 — 走 COM marker 通道让 resolveComValue("Object") 在链式
        // 外层自然展开为 vb6_ComGetObjectProp.
        comObjExpr_ = tempVar;
        comMemberName_ = node.memberName;
        isComMarker_ = true;
        isEarlyBoundCom_ = false;
        earlyBoundSym_ = nullptr;
        lastExpr_ = tempVar + "  /* With class ." + node.memberName + " COM dispatch */";
        return;
    }
    case WithObjKind::BuiltinObject: {
        // Fix 010l: .Property on builtin object (Err/App/etc.)
        const std::string& bn = info.ctrlOrigName;  // builtin name (lowercase)
        if (bn == "err") {
            if (memLower == "number")      { lastExpr_ = "vb6_ErrNumber()";      return; }
            if (memLower == "description") { lastExpr_ = "vb6_ErrDescription()"; return; }
            if (memLower == "source")      { lastExpr_ = "vb6_ErrSource()";      return; }
            if (memLower == "lastdllerror") { lastExpr_ = "GetLastError()";       return; }
            if (memLower == "helpfile")    { lastExpr_ = "(BSTR)0";              return; }
            if (memLower == "helpcontext") { lastExpr_ = "0";                    return; }
            if (memLower == "clear")       { lastExpr_ = "vb6_ErrClear";         return; }
            if (memLower == "raise")       { lastExpr_ = "vb6_ErrRaise";         return; }
        }
        // Fallback: unknown builtin member
        lastExpr_ = "/* With " + bn + "." + node.memberName + " */ 0";
        return;
    }
    case WithObjKind::Unknown:
    default:
        // UDT/fallback: struct.field访问
        // Fix 081j: With块临时变量改为指针，用 -> 访问成员
        // Fix 085: UDT With 块中字段为对象 (Collection/COM/项目类) 时追加标记,
        // 供外层 MemberAccessExpr 消费转 COM/类方法路径.
        // 例: With uCtx: .LocalCertificates.Item(lIdx) → _vb6_with_15->LocalCertificates
        {
            std::string tvLower = Symbol::toLower(tempVar);
            std::string udtCType =
                knownUdtVars_.count(tvLower) ? knownUdtVars_[tvLower] : "";
            lastExpr_ = appendUdtObjFieldMarker(tempVar, udtCType, node.memberName, "->");
        }
        return;
    }
}

// ============================================================
// 数组辅助方法
// ============================================================

std::string CCodeGen::mapSaElemType(Vb6Type type) const {
    switch (type) {
        case Vb6Type::Boolean:  return "vb6_sa_bool";
        case Vb6Type::Byte:     return "vb6_sa_byte";
        case Vb6Type::Integer:  return "vb6_sa_int";
        case Vb6Type::Long:     return "vb6_sa_long";
        case Vb6Type::Single:   return "vb6_sa_single";
        case Vb6Type::Double:   return "vb6_sa_double";
        case Vb6Type::Date:     return "vb6_sa_double";
        case Vb6Type::Currency: return "vb6_sa_currency";  // P1-9修复: 8-byte Currency
        case Vb6Type::String:   return "vb6_sa_bstr";
        case Vb6Type::Variant:  return "vb6_sa_variant";
        case Vb6Type::Object:   return "vb6_sa_ptr";
        default:                return "vb6_sa_variant";
    }
}

std::string CCodeGen::mapSaElemCType(Vb6Type type) const {
    switch (type) {
        case Vb6Type::Boolean:  return "int16_t";
        case Vb6Type::Byte:     return "uint8_t";
        case Vb6Type::Integer:  return "int16_t";
        case Vb6Type::Long:     return "int32_t";
        case Vb6Type::Single:   return "float";
        case Vb6Type::Double:   return "double";
        case Vb6Type::Date:     return "double";
        case Vb6Type::String:   return "BSTR";
        case Vb6Type::Variant:  return "vb6_VARIANT";
        case Vb6Type::Currency: return "int64_t";
        case Vb6Type::Object:   return "void*";
        default:                return "vb6_VARIANT";
    }
}

Vb6Type CCodeGen::resolveArrayElemType(ASTNode* typeRef) const {
    if (!typeRef) return Vb6Type::Variant;
    if (typeRef->kind == ASTNodeKind::ArrayTypeRef) {
        auto& arrType = static_cast<ArrayTypeRef&>(*typeRef);
        return resolveArrayElemType(arrType.elementType.get());
    }
    if (typeRef->kind == ASTNodeKind::SimpleTypeRef) {
        auto& simple = static_cast<SimpleTypeRef&>(*typeRef);
        Vb6Type t = typeSys_.resolveTypeName(simple.name);
        if (t != Vb6Type::Unknown) return t;
        // Fix 049b: 项目内 UDT/Enum/类名 需查符号表 (typeSys_ 只含内置类型)。
        // 否则 Dim x() As 某UDT (且该 UDT 声明位于 Dim 之后, Fix 049 预扫描已注册)
        // 的元素类型退回 Variant → arrayElemTypes_ 记录 Variant → 元素访问生成
        // VB6_SA_AT(vb6_VARIANT, ...).字段 (C2039/C2223) → With 对象类型也变 Variant,
        // 成员解析退化为跨模块类查找 (如 .Pos 误解析到 cToast.Pos, C2198)。
        // 与 resolveArrayUdtElemCType (Fix 055b) 的符号表回退保持一致。
        if (auto* sym = symTab_.lookup(simple.name)) {
            if (sym->kind == SymbolKind::UserDefinedType) return Vb6Type::UserDefinedType;
            if (sym->kind == SymbolKind::EnumType) return Vb6Type::Long;
            if (sym->kind == SymbolKind::Class || sym->kind == SymbolKind::ComClass ||
                sym->kind == SymbolKind::ComInterface || sym->kind == SymbolKind::ComModule ||
                sym->kind == SymbolKind::ComGlobalNs) {
                return Vb6Type::Object;
            }
        }
        return Vb6Type::Variant;
    }
    return Vb6Type::Variant;
}

std::string CCodeGen::resolveArrayUdtElemCType(ASTNode* typeRef) const {
    if (!typeRef) return "";
    if (typeRef->kind == ASTNodeKind::ArrayTypeRef) {
        auto& arrType = static_cast<ArrayTypeRef&>(*typeRef);
        return resolveArrayUdtElemCType(arrType.elementType.get());
    }
    if (typeRef->kind == ASTNodeKind::SimpleTypeRef) {
        auto& simple = static_cast<SimpleTypeRef&>(*typeRef);
        // 检查类型系统
        Vb6Type vtype = typeSys_.resolveTypeName(simple.name);
        if (vtype == Vb6Type::UserDefinedType) {
            return "vb6_type_" + cIdent(simple.name);
        }
        // Fix 055b: resolveTypeName 对项目内 UDT 返回 Unknown,
        // 需要在符号表中查找是否为 UserDefinedType 符号
        auto* sym = symTab_.lookup(simple.name);
        if (sym && (sym->type == Vb6Type::UserDefinedType || sym->kind == SymbolKind::UserDefinedType)) {
            return "vb6_type_" + cIdent(simple.name);
        }
        sym = symTab_.lookupModule(simple.name);
        if (sym && (sym->type == Vb6Type::UserDefinedType || sym->kind == SymbolKind::UserDefinedType)) {
            return "vb6_type_" + cIdent(simple.name);
        }
    }
    return "";
}

// ============================================================
// 二元运算符映射
// ============================================================

std::string CCodeGen::mapBinaryOp(BinaryOp op) const {
    switch (op) {
        case BinaryOp::Or:     return "|";    // VB6 Or = 位或
        case BinaryOp::Xor:    return "^";    // VB6 Xor = 位异或
        case BinaryOp::And:    return "&";    // VB6 And = 位与
        case BinaryOp::Eq:     return "==";
        case BinaryOp::Neq:    return "!=";
        case BinaryOp::Lt:     return "<";
        case BinaryOp::Gt:     return ">";
        case BinaryOp::Le:     return "<=";
        case BinaryOp::Ge:     return ">=";
        case BinaryOp::Add:    return "+";
        case BinaryOp::Sub:    return "-";
        case BinaryOp::Mod:    return "%";
        case BinaryOp::Mul:    return "*";
        case BinaryOp::Div:    return "/";
        case BinaryOp::Is:     return "==";   // 对象引用比较
        // 以下运算符在visit(BinaryExpr&)中已特殊处理, 此处不应到达
        case BinaryOp::Concat: return "/* CONCAT */";
        case BinaryOp::IntDiv: return "/* INTDIV */";
        case BinaryOp::Pow:    return "/* POW */";
        case BinaryOp::Eqv:    return "/* EQV */";
        case BinaryOp::Imp:    return "/* IMP */";
        case BinaryOp::Like:   return "/* LIKE */";
        default:               return "/* unhandled BinaryOp */";
    }
}

// ============================================================
// 语句 visit 方法
// ============================================================


} // namespace vb6c3
