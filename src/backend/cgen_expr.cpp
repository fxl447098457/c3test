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
            // VB6的""转义 (双引号在字符串内) → C的\"转义
            // 同时转义反斜杠; 非ASCII字符→\xNNNN宽字符转义
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
                    // UTF-8多字节: 解码Unicode码点, 输出\xNNNN
                    uint32_t cp = 0;
                    int bytes = 0;
                    if ((ch & 0xE0) == 0xC0) { cp = ch & 0x1F; bytes = 2; }
                    else if ((ch & 0xF0) == 0xE0) { cp = ch & 0x0F; bytes = 3; }
                    else if ((ch & 0xF8) == 0xF8) { cp = ch & 0x07; bytes = 4; }
                    else { cp = ch; bytes = 1; }
                    for (int b = 1; b < bytes && j + b < inner.size(); b++) {
                        cp = (cp << 6) | ((unsigned char)inner[j + b] & 0x3F);
                    }
                    j += bytes;
                    char hex[8];
                    snprintf(hex, sizeof(hex), "\\x%04X", cp);
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
    if (lower == "vbnewline")      { lastExpr_ = "vb6_BSTR_FromStr(L\"\\r\\n\")"; return; }
    if (lower == "vbnullstring")   { lastExpr_ = "vb6_BSTR_Empty()"; return; }
    if (lower == "vbempty")        { lastExpr_ = "vb6_VariantEmpty()"; return; }
    if (lower == "vbnothing")      { lastExpr_ = "NULL"; return; }
    if (lower == "vbtrue")         { lastExpr_ = "(-1)"; return; }
    if (lower == "vbfalse")        { lastExpr_ = "0"; return; }

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
                    lastExpr_ = "(*" + cName + ")";
                } else {
                    lastExpr_ = cName;
                }
                return;
            }
        }
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

    // 用户变量/常量 (非参数、非函数)
    if (foundSym && (foundSym->kind == SymbolKind::Variable
                  || foundSym->kind == SymbolKind::Constant)) {
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
        if (isClassModule_ && currentProc_ && foundSym->kind == SymbolKind::Variable) {
            Symbol* paramSym = symTab_.lookupLocal(node.name);
            if (!paramSym || paramSym->kind != SymbolKind::Parameter) {
                lastExpr_ = "me->" + cName;
                // P14.3.1: Dim As New自动实例化 (类模块成员)
                auto itNewM = knownNewVars_.find(lower);
                if (itNewM != knownNewVars_.end()) {
                    c_.emitLine("if (!me->" + cName + ") me->" + cName + " = vb6_New_" + itNewM->second + "();  /* Dim As New auto-instantiate */");
                }
                return;
            }
        }
        // P14.3.1: Dim As New自动实例化守卫
        auto itNew = knownNewVars_.find(lower);
        if (itNew != knownNewVars_.end()) {
            c_.emitLine("if (!" + cName + ") " + cName + " = vb6_New_" + itNew->second + "();  /* Dim As New auto-instantiate */");
        }
        lastExpr_ = cName;
        return;
    }

    // 内置函数映射 (名称 → RTL函数名) - 仅当符号表中没有用户定义的函数时使用
    static const std::unordered_map<std::string, std::string> builtinFuncs = {
        {"len",      "vb6_Len"},
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
        {"loadpicture","vb6_LoadPictureFromFile"},
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

    };

    auto it = builtinFuncs.find(lower);
    if (it != builtinFuncs.end()) {
        // 无参内置函数: VB6允许省略括号(如 Now, Date, Time)
        // 当IdentifierExpr引用这些函数时，必须生成调用(带括号)
        static const std::unordered_set<std::string> zeroArgBuiltinFuncs = {
            "now", "date", "time", "freefile", "command", "curdir", "timer"
        };
        if (zeroArgBuiltinFuncs.count(lower)) {
            lastExpr_ = it->second + "()";
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
    // 已经是BSTR表达式: vb6_BSTR_xxx, vb6_CStr, L"...", vb6_MsgBox, etc.
    if (expr.find("vb6_BSTR") != std::string::npos) return expr;
    if (expr.find("vb6_CStr") != std::string::npos) return expr;
    if (expr.find("vb6_GetControlText") != std::string::npos) return expr;
    if (expr.find("vb6_GetControlCaption") != std::string::npos) return expr;
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
            expr.find("vb6_Val") != std::string::npos) {
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
        case Vb6Type::Variant: return "vb6_CStr(" + expr + ")";
        default: return "vb6_CStrLong(" + expr + ")";  // fallback
    }
}

void CCodeGen::visit(BinaryExpr& node) {
    emitExpr(*node.left);
    // COM标记解析: 如果左操作数是COM属性, 解析为值
    if (isComMarker_) resolveComValue();
    std::string left = std::move(lastExpr_);
    emitExpr(*node.right);
    // COM标记解析: 如果右操作数是COM属性, 解析为值
    if (isComMarker_) resolveComValue();
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
    if (node.op == BinaryOp::IntDiv) {
        lastExpr_ = "vb6_IntDiv(" + left + ", " + right + ")";
        return;
    }

    // 等价运算: VB6 Eqv → ~(a^b)
    if (node.op == BinaryOp::Eqv) {
        lastExpr_ = "(~(" + left + " ^ " + right + "))";
        return;
    }

    // 蕴含运算: VB6 Imp → (~a | b)
    if (node.op == BinaryOp::Imp) {
        lastExpr_ = "((~" + left + ") | " + right + ")";
        return;
    }

    // Like运算: VB6 Like → vb6_Like
    if (node.op == BinaryOp::Like) {
        lastExpr_ = "vb6_Like(" + left + ", " + right + ")";
        return;
    }

    std::string op = mapBinaryOp(node.op);

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
            lastExpr_ = "(-" + operand + ")";
            break;
        case UnaryOp::Not:
            // VB6 Not = 位取反 (C: ~)
            lastExpr_ = "(~" + operand + ")";
            break;
    }
}

void CCodeGen::visit(MemberAccessExpr& node) {
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
        }

        // P18-C: Clipboard 对象
        if (objLower == "clipboard") {
            if (memLower == "settext") { lastExpr_ = "vb6_Clipboard_SetText"; return; }
            if (memLower == "gettext") { lastExpr_ = "vb6_Clipboard_GetText()"; return; }
            if (memLower == "clear") { lastExpr_ = "vb6_Clipboard_Clear"; return; }
            if (memLower == "getformat") { lastExpr_ = "vb6_Clipboard_GetFormat"; return; }
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

        // 查找成员名称的符号
        auto* memSym = symTab_.lookupModule(node.memberName);
        if (memSym && (memSym->kind == SymbolKind::Sub || memSym->kind == SymbolKind::Function
                    || memSym->kind == SymbolKind::PropertyGet
                    || memSym->kind == SymbolKind::PropertyLet
                    || memSym->kind == SymbolKind::PropertySet)) {

            // 优先级2: 类实例成员访问: obj.Method → vb6_Method(obj) 或 vb6_Counter_Method(obj)
            if (knownClassVars_.count(objLower)) {
                // Property需要加前缀: prop_get_/prop_let_/prop_set_
                std::string memberCName = node.memberName;
                if (memSym->kind == SymbolKind::PropertyGet) {
                    memberCName = "prop_get_" + node.memberName;
                } else if (memSym->kind == SymbolKind::PropertyLet) {
                    memberCName = "prop_let_" + node.memberName;
                } else if (memSym->kind == SymbolKind::PropertySet) {
                    memberCName = "prop_set_" + node.memberName;
                }
                std::string funcName = cProcName(memberCName, memSym->access,
                                                  memSym->isExternal ? memSym->sourceModule : "");
                emitExpr(*node.object);
                std::string objExpr = std::move(lastExpr_);
                lastExpr_ = funcName + "(" + objExpr + ")";
                return;
            }

            // 优先级3: 模块名.方法名: MathUtils.Add → vb6_MathUtils_Add
            //    object名称不是已知变量, 但成员是函数 → 视为模块限定调用
            bool isVarName = false;
            auto* objSym = symTab_.lookup(objIdent.name);
            if (!objSym) objSym = symTab_.lookupModule(objIdent.name);
            if (objSym && (objSym->kind == SymbolKind::Variable || objSym->kind == SymbolKind::Parameter)) {
                isVarName = true;
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
        if (!isVarName2) {
            // object不是已知变量 → 假设是模块名限定符
            // 成员是变量: Module1.myName
            // When module is #included, use the unprefixed name directly
            std::string modName2 = objIdent2.name;
            std::string varName2 = cIdent(node.memberName);
            std::string modLower2 = modName2;
            std::transform(modLower2.begin(), modLower2.end(), modLower2.begin(), ::tolower);
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

    // 如果object是类实例指针变量, 使用 -> 而非 .
    {
        std::string objLower = obj;
        std::transform(objLower.begin(), objLower.end(), objLower.begin(), ::tolower);
        if (knownClassVars_.count(objLower) || knownTypedComVars_.count(objLower)) {
            lastExpr_ = obj + "->" + cIdent(node.memberName);
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
            arrElemType = arrayElemTypes_[lower];
        } else {
            // 再查符号表 (模块级数组)
            Symbol* sym = symTab_.lookupModule(ident.name);
            if (sym && sym->kind == SymbolKind::Variable && sym->isArray) {
                isArrayAccess = true;
                arrName = cIdent(ident.name);
                arrElemType = sym->type;
            }
        }
    }

    if (isArrayAccess) {
        // P8.1: 数组元素访问, 支持多维
        emitExpr(*node.callee);
        std::string callee = std::move(lastExpr_);

        int dimCount = 1;
        auto itDc = arrayDimCounts_.find(arrName);
        if (itDc != arrayDimCounts_.end()) dimCount = itDc->second;

        if (dimCount == 1 || node.positional.size() == 1) {
            // 一维访问: arr(i) -> VB6_SA_AT(type, arr, i)
            std::string index = "0";
            if (!node.positional.empty()) {
                emitExpr(*node.positional[0]);
                index = std::move(lastExpr_);
            }
            std::string elemCType = mapSaElemCType(arrElemType);
            lastExpr_ = "VB6_SA_AT(" + elemCType + ", " + arrName + ", " + index + ")";
        } else if (dimCount == 2 && node.positional.size() == 2) {
            // 二维访问: arr(i, j) -> VB6_SA_ND_AT2(type, arr, i, j)
            emitExpr(*node.positional[0]);
            std::string idx0 = std::move(lastExpr_);
            emitExpr(*node.positional[1]);
            std::string idx1 = std::move(lastExpr_);
            std::string elemCType = mapSaElemCType(arrElemType);
            lastExpr_ = "VB6_SA_ND_AT2(" + elemCType + ", " + arrName + ", " + idx0 + ", " + idx1 + ")";
        } else if (dimCount == 3 && node.positional.size() == 3) {
            // 三维访问: arr(i, j, k) -> VB6_SA_ND_AT3(type, arr, i, j, k)
            emitExpr(*node.positional[0]);
            std::string idx0 = std::move(lastExpr_);
            emitExpr(*node.positional[1]);
            std::string idx1 = std::move(lastExpr_);
            emitExpr(*node.positional[2]);
            std::string idx2 = std::move(lastExpr_);
            std::string elemCType = mapSaElemCType(arrElemType);
            lastExpr_ = "VB6_SA_ND_AT3(" + elemCType + ", " + arrName + ", " + idx0 + ", " + idx1 + ", " + idx2 + ")";
        } else {
            // 4+维: 通用通过vb6_SafeArrayND_Offset + 直接指针访问
            std::vector<std::string> indices;
            for (auto& arg : node.positional) {
                emitExpr(*arg);
                indices.push_back(std::move(lastExpr_));
            }
            std::string elemCType = mapSaElemCType(arrElemType);
            // 构建indices数组 + offset计算
            std::string offVar = "_ndoff_" + std::to_string(tempCounter_++);
            c_.emitLine("int " + offVar + " = vb6_SafeArrayND_Offset(" + arrName + ", " + std::to_string(dimCount) + ", (int[]){" + indices[0] + ", " + indices[1] + "});");
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
            if (isBstrResult(trueVal) || isBstrResult(falseVal)) {
                lastExpr_ = "vb6_IIfBSTR(" + cond + ", " + trueVal + ", " + falseVal + ")";
            } else if (trueVal.find('.') != std::string::npos || falseVal.find('.') != std::string::npos) {
                lastExpr_ = "vb6_IIfDouble(" + cond + ", " + trueVal + ", " + falseVal + ")";
            } else {
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
            lastExpr_ = "(int32_t)(intptr_t)&(" + lastExpr_ + ")";
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

    // 函数调用路径 (原有逻辑)
    // M22: 设置asCallCallee_标志, 让IdentifierExpr知道当前是函数调用callee上下文
    // 这确保递归调用时(如 Factorial(n-1))返回函数名而非返回值变量
    asCallCallee_ = true;
    emitExpr(*node.callee);
    asCallCallee_ = false;
    std::string callee = std::move(lastExpr_);

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
        isComMarker_ = false;  // 消费标记
        std::string objExpr = std::move(comObjExpr_);
        std::string memberName = std::move(comMemberName_);

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

        // P6.3: 前期绑定 (vtable直接调用)
        if (isEarlyBoundCom_ && earlyBoundSym_) {
            isEarlyBoundCom_ = false;
            const Symbol* comSym = earlyBoundSym_;
            earlyBoundSym_ = nullptr;

            // 查找方法签名
            std::string memLower = memberName;
            std::transform(memLower.begin(), memLower.end(), memLower.begin(), ::tolower);
            auto it = comSym->comMethods.find(memLower);
            if (it != comSym->comMethods.end()) {
                const auto& sig = it->second;

                // 生成接口类型名
                std::string ifaceName = comSym->name;
                if (comSym->kind == SymbolKind::ComClass && !comSym->comDefaultIfaceName.empty()) {
                    ifaceName = comSym->comDefaultIfaceName;
                }
                std::string ifaceType = "vb6_ComIface_" + cIdent(ifaceName);

                // vtable调用: ((ReturnType(*)(Iface*))vt[idx])(obj, args...)
                // 或属性get: obj->vt[idx](obj)
                std::string vtOffset = std::to_string(sig.vtableIndex);

                // 参数生成 (不包含this指针, vtable辅助函数的第一个参数已经是obj)
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

                // 生成vtable间接调用
                // ((void*)obj)[idx] 是vtable中第idx个函数指针
                // 简化: 使用运行时辅助函数 vb6_ComVtableCall
                if (sig.isPropertyGet) {
                    // 属性Get: vb6_ComVtableGet<type>(obj, vtIndex, args...)
                    std::string returnType = mapType(sig.returnType);
                    if (returnType == "BSTR") {
                        lastExpr_ = "vb6_ComVtableGetBSTR(" + objExpr + ", " + vtOffset + ", " + argsStr + ")";
                    } else if (returnType == "int32_t" || returnType == "int16_t") {
                        lastExpr_ = "vb6_ComVtableGetInt(" + objExpr + ", " + vtOffset + ", " + argsStr + ")";
                    } else if (returnType == "double" || returnType == "float") {
                        lastExpr_ = "vb6_ComVtableGetDouble(" + objExpr + ", " + vtOffset + ", " + argsStr + ")";
                    } else if (returnType == "void*") {
                        lastExpr_ = "vb6_ComVtableGetObject(" + objExpr + ", " + vtOffset + ", " + argsStr + ")";
                    } else {
                        lastExpr_ = "vb6_ComVtableGetVoid(" + objExpr + ", " + vtOffset + ", " + argsStr + ")";
                    }
                } else if (sig.isPropertyPut || sig.isPropertyPutRef) {
                    // 属性Put: vb6_ComVtablePut(obj, vtIndex, value)
                    lastExpr_ = "vb6_ComVtableCallVoid(" + objExpr + ", " + vtOffset + ", " + argsStr + ")";
                } else {
                    // 方法调用
                    std::string returnType = mapType(sig.returnType);
                    if (returnType == "BSTR") {
                        lastExpr_ = "vb6_ComVtableGetBSTR(" + objExpr + ", " + vtOffset + ", " + argsStr + ")";
                    } else if (returnType == "int32_t" || returnType == "int16_t") {
                        lastExpr_ = "vb6_ComVtableGetInt(" + objExpr + ", " + vtOffset + ", " + argsStr + ")";
                    } else if (returnType == "double" || returnType == "float") {
                        lastExpr_ = "vb6_ComVtableGetDouble(" + objExpr + ", " + vtOffset + ", " + argsStr + ")";
                    } else if (returnType == "void*") {
                        lastExpr_ = "vb6_ComVtableGetObject(" + objExpr + ", " + vtOffset + ", " + argsStr + ")";
                    } else if (returnType == "void") {
                        lastExpr_ = "vb6_ComVtableCallVoid(" + objExpr + ", " + vtOffset + ", " + argsStr + ")";
                    } else {
                        // 默认: 返回VARIANT
                        lastExpr_ = "vb6_ComVtableGetVoid(" + objExpr + ", " + vtOffset + ", " + argsStr + ")";
                    }
                }
                return;
            }
            // 方法签名未找到 → 降级为后期绑定
            isEarlyBoundCom_ = false;
        }
        isEarlyBoundCom_ = false;

        if (!node.positional.empty() || !node.named.empty()) {
            // 有参数: obj.Method(args) → vb6_ComCall(obj, L"Method", variantArgs, argc)
            std::vector<std::string> packedArgs;
            for (size_t i = 0; i < node.positional.size(); i++) {
                std::string packFn = comPackExpr(*node.positional[i]);
                emitExpr(*node.positional[i]);
                packedArgs.push_back(packFn + "(" + lastExpr_ + ")");
            }
            for (auto& named : node.named) {
                std::string packFn = comPackExpr(*named.value);
                emitExpr(*named.value);
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
            return;
        } else {
            // 无参数: obj.Method() → vb6_ComCall(obj, L"Method", NULL, 0)
            lastExpr_ = "vb6_ComCall(" + objExpr + ", L\"" + memberName + "\", NULL, 0)";
            return;
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
        if (isCompleteCall) {
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

    // P6.5修复: 如果callee已经是func(obj)形式(如类方法调用 vb6_Button_SetCaption(btn)),
    // 且IndexOrCallExpr有额外参数, 需要拆开重组为func(obj, userArgs...),
    // 避免生成 func(obj)(userArgs) 双重括号
    std::string classMethodObjArg;  // 如果非空, 表示callee已被拆开, 需要前置此参数
    if (callee.size() >= 2 && callee.back() == ')'
        && (!node.positional.empty() || !node.named.empty())) {
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
        }
    }

    // 位置参数
    // 需要检查被调用函数的参数签名: ByRef参数在调用点需要传指针(&arg)
    std::vector<ParameterInfo> calleeParams;
    // 从IdentifierExpr或MemberAccessExpr获取被调用函数名, 在符号表中查找
    if (node.callee && node.callee->kind == ASTNodeKind::IdentifierExpr) {
        auto& idExpr = static_cast<IdentifierExpr&>(*node.callee);
        // 尝试多种查找方式: 先lookupModule(过程符号), 再lookup(嵌套作用域)
        Symbol* funcSym = symTab_.lookupModule(idExpr.name);
        if (!funcSym || (funcSym->kind != SymbolKind::Sub && funcSym->kind != SymbolKind::Function
            && funcSym->kind != SymbolKind::PropertyGet && funcSym->kind != SymbolKind::PropertyLet
            && funcSym->kind != SymbolKind::PropertySet)) {
            funcSym = symTab_.lookup(idExpr.name);
        }
        if (funcSym && (funcSym->kind == SymbolKind::Sub || funcSym->kind == SymbolKind::Function
            || funcSym->kind == SymbolKind::PropertyGet || funcSym->kind == SymbolKind::PropertyLet
            || funcSym->kind == SymbolKind::PropertySet)) {
            calleeParams = funcSym->params;
        }
    } else if (node.callee && node.callee->kind == ASTNodeKind::MemberAccessExpr) {
        // Module.Method 或 obj.Method 调用: 查找方法名的参数签名
        auto& maExpr = static_cast<MemberAccessExpr&>(*node.callee);
        Symbol* funcSym = symTab_.lookupModule(maExpr.memberName);
        if (funcSym && (funcSym->kind == SymbolKind::Sub || funcSym->kind == SymbolKind::Function
            || funcSym->kind == SymbolKind::PropertyGet || funcSym->kind == SymbolKind::PropertyLet
            || funcSym->kind == SymbolKind::PropertySet)) {
            calleeParams = funcSym->params;
        }
    }

    std::vector<std::string> args;
    for (size_t i = 0; i < node.positional.size(); i++) {
        emitExpr(*node.positional[i]);
        std::string argVal = std::move(lastExpr_);
        // ByRef参数: 调用点传指针. 如果实参已经是解引用形式(*x), 取地址还原为x;
        // 如果是普通变量, 加&取地址
        bool isByRef = (i < calleeParams.size() && !calleeParams[i].isByVal && !calleeParams[i].isParamArray);
        if (isByRef) {
            if (argVal.size() > 3 && argVal.substr(0, 2) == "(*" && argVal.back() == ')') {
                // (*x) → &x (ByRef参数传ByRef参数, 还原指针)
                argVal = "&" + argVal.substr(2, argVal.size() - 3);
            } else if (argVal.size() > 2 && argVal.substr(0, 2) == "me" && argVal[2] == '-') {
                // me->field → &(me->field) (类成员字段取地址)
                argVal = "&(" + argVal + ")";
            } else {
                // 晀量/非左值 → 复合字面量取地址; 变量 → &变量
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
                    argVal = "&" + argVal;
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
                                argVal = "&(vb6_VARIANT){.vt=VT_BSTR, .bstrVal=" + argVal + "}";
                                break;
                            case Vb6Type::Long:
                            case Vb6Type::Integer:
                                argVal = "&(vb6_VARIANT){.vt=VT_I4, .lVal=(int32_t)(" + argVal + ")}";
                                break;
                            case Vb6Type::Double:
                            case Vb6Type::Single:
                                argVal = "&(vb6_VARIANT){.vt=VT_R8, .dblVal=(double)(" + argVal + ")}";
                                break;
                            case Vb6Type::Boolean:
                                argVal = "&(vb6_VARIANT){.vt=VT_BOOL, .boolVal=(int16_t)(" + argVal + ")}";
                                break;
                            case Vb6Type::Byte:
                                argVal = "&(vb6_VARIANT){.vt=VT_UI1, .bVal=(uint8_t)(" + argVal + ")}";
                                break;
                            case Vb6Type::Date:
                                argVal = "&(vb6_VARIANT){.vt=VT_DATE, .dblVal=(double)(" + argVal + ")}";
                                break;
                            default:
                                // Variant or unknown: try bstrVal first (most common case)
                                argVal = "&(vb6_VARIANT){.vt=VT_BSTR, .bstrVal=" + argVal + "}";
                                break;
                        }
                    } else {
                        argVal = "&(" + cType + "){" + argVal + "}";
                    }
                }
            }
        }
        args.push_back(std::move(argVal));
    }

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
            bool isByRef = (pi < calleeParams.size() && !calleeParams[pi].isByVal && !calleeParams[pi].isParamArray);
            if (!isByRef) return;
            if (argVal.size() > 3 && argVal.substr(0, 2) == "(*" && argVal.back() == ')') {
                argVal = "&" + argVal.substr(2, argVal.size() - 3);
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
                    argVal = "&" + argVal;
                } else {
                    std::string cType = "int32_t";
                    if (pi < calleeParams.size()) cType = mapType(calleeParams[pi].type);
                    argVal = "&(" + cType + "){" + argVal + "}";
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
                    filled[i] = true;
                } else {
                    orderedArgs[i] = defaultValue(calleeParams[i].type);
                    filled[i] = true;
                }
            }
        }

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

                    if (isLongArg) {

                        c_.emitLine("vb6_PA_SetLong(" + paVar + ", " + std::to_string(i) + ", " + paArgExpr + ");");

                    } else if (isDoubleArg) {

                        c_.emitLine("vb6_PA_SetDouble(" + paVar + ", " + std::to_string(i) + ", " + paArgExpr + ");");

                    } else {

                        bool looksLikeBSTR = (paArgExpr.find("vb6_BSTR") != std::string::npos ||

                                             paArgExpr.find("L\"") != std::string::npos);

                        if (looksLikeBSTR) {

                            c_.emitLine("vb6_PA_SetBSTR(" + paVar + ", " + std::to_string(i) + ", " + paArgExpr + ");");

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

                    argList += "&(" + cType + "){" + defVal + "}";

                }

            }

        }

    } else {

        // No ParamArray - normal argList construction

        for (size_t i = 0; i < args.size(); i++) {

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

                // 缂虹渷缁村害鍙傛暟, 琛?

                argList += ", 1";

            }

            // 妫€鏌ユ槸鍚︿负ND鏁扮粍, 闇€瑕佺敤ND鐗堟湰

            if (node.positional.size() >= 1) {

                auto& firstArg = node.positional[0];

                std::string arrLower;

                if (firstArg->kind == ASTNodeKind::IdentifierExpr) {

                    arrLower = static_cast<IdentifierExpr&>(*firstArg).name;

                    std::transform(arrLower.begin(), arrLower.end(), arrLower.begin(), ::tolower);

                }

                auto itDc = arrayDimCounts_.find(arrLower);

                if (itDc != arrayDimCounts_.end() && itDc->second > 1) {

                    // ND鏁扮粍 -> 浣跨敤vb6_UBoundND/vb6_LBoundND

                    if (callee == "vb6_UBound") {

                        callee = "vb6_UBoundND";

                    } else {

                        callee = "vb6_LBoundND";

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

    // P14.1.4: General Optional parameter padding for user-defined functions
    // calleeParams is empty for builtin RTL functions (registered without params), so they're auto-skipped
    if (calleeParams.size() > 0 && args.size() < calleeParams.size() && paIndex < 0) {
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
                argList += "&(" + cType + "){" + defVal + "}";
            }
        }
    }

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

    // P8.4: Variant参数适配 — 如果目标函数不接受Variant但参数是Variant类型, 使用V后缀函数
    // CInt(Variant)→vb6_CIntV, CDbl(Variant)→vb6_CDblV, CLng(Variant)→vb6_CLngV
    if (callee == "vb6_CInt" || callee == "vb6_CLng" || callee == "vb6_CDbl") {
        // 检查第一个参数是否为Variant变量
        bool firstArgIsVariant = false;
        if (!node.positional.empty()) {
            auto& firstArg = node.positional[0];
            if (firstArg->kind == ASTNodeKind::IdentifierExpr) {
                auto& idArg = static_cast<IdentifierExpr&>(*firstArg);
                std::string argLower = idArg.name;
                std::transform(argLower.begin(), argLower.end(), argLower.begin(), ::tolower);
                if (knownVariantVars_.count(argLower)) firstArgIsVariant = true;
            }
        }
        if (firstArgIsVariant) {
            if (callee == "vb6_CInt") callee = "vb6_CIntV";
            else if (callee == "vb6_CLng") callee = "vb6_CLngV";
            else if (callee == "vb6_CDbl") callee = "vb6_CDblV";
        }
    }

    // MsgBox默认参数补全: MsgBox(prompt) -> vb6_MsgBox1(prompt)
    // MsgBox(prompt, buttons) -> vb6_MsgBox(prompt, buttons, NULL)
    if (callee == "vb6_MsgBox") {
        if (node.positional.size() == 1) {
            callee = "vb6_MsgBox1";
        } else if (node.positional.size() == 2) {
            argList += ", NULL";
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
    } else {
        // 外部/COM对象: 回退到运行时
        lastExpr_ = "vb6_NewObject(L\"" + node.className + "\")";
    }
}

void CCodeGen::visit(TypeOfExpr& node) {
    emitExpr(*node.object);
    std::string obj = std::move(lastExpr_);
    lastExpr_ = "vb6_TypeOf(" + obj + ", L\"" + node.typeName + "\")";
}

void CCodeGen::visit(AddressOfExpr& node) {
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
        // .Method/Property → 类方法调用 vb6_Method(tempVar)
        Symbol* memSym = symTab_.lookupModule(node.memberName);
        if (memSym) {
            std::string memberCName = node.memberName;
            if (memSym->kind == SymbolKind::PropertyGet)
                memberCName = "prop_get_" + node.memberName;
            else if (memSym->kind == SymbolKind::PropertyLet)
                memberCName = "prop_let_" + node.memberName;
            else if (memSym->kind == SymbolKind::PropertySet)
                memberCName = "prop_set_" + node.memberName;
            std::string funcName = cProcName(memberCName, memSym->access,
                memSym->isExternal ? memSym->sourceModule : "");
            lastExpr_ = funcName + "(" + tempVar + ")  /* With class .Member */";
            return;
        }
        lastExpr_ = tempVar + "." + cIdent(node.memberName);
        return;
    }
    case WithObjKind::Unknown:
    default:
        // UDT/fallback: struct.field访问
        lastExpr_ = tempVar + "." + cIdent(node.memberName);
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
        return typeSys_.resolveTypeName(simple.name);
    }
    return Vb6Type::Variant;
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
