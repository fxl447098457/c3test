// cgen_expr_ident_builtin.inc - visit(IdentifierExpr&) 函数体片段：内置函数表与用户过程解析
// 由 src/backend/expr/cgen_expr_ident.cpp 拆出（2026-09-17），在函数体内被 #include，非独立编译单元。
// 内容 = 原文件第 421~740 行，逐行未改。

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
            // Fix 094: CurDir 的 RTL 签名是 vb6_CurDir(BSTR drive)，0 参调用必须补
            // NULL (当前驱动器)，否则 C2198 参数太少。与 call 路径 fixup
            // (cgen_expr_call_builtin_fixup.inc P14.2.2) 同语义；ident 路径
            // (无括号引用 / 0 参) 不经过该 fixup，需在此处补。
            } else if (lower == "curdir") {
                lastExpr_ = "vb6_CurDir(NULL)";
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