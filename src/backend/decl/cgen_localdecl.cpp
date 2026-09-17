#include "backend/cgen.hpp"
#include <algorithm>
#include <cctype>
#include <iostream>
#include <functional>

namespace vb6c3 {

// --- cgen_localdecl.cpp: 局部声明与提升 (Option/Dim/Const/hoist) ---

void CCodeGen::visit(OptionStmt& node) {
    // P18-C: Option Compare Text/Binary
    if (node.optionKind == OptionKind::CompareText) {
        c_.emitLine("g_vb6_optionCompareText = 1; /* Option Compare Text */");
    } else if (node.optionKind == OptionKind::CompareBinary) {
        c_.emitLine("g_vb6_optionCompareText = 0; /* Option Compare Binary */");
    }
    // Option Explicit / Option Base 不影响C代码生成
}

void CCodeGen::visit(LocalDeclStmt& node) {
    // Fix 086: 已在过程序言处提升声明的节点, 原位置跳过
    if (hoistedLocalDeclSet_.count(&node)) return;
    emitLocalDeclCode(node);
}

void CCodeGen::emitLocalDeclCode(LocalDeclStmt& node) {
    if (!node.decl) return;

    switch (node.decl->kind) {
        case ASTNodeKind::VariableDecl: {
            auto& var = static_cast<VariableDecl&>(*node.decl);
            std::string cName = cIdent(var.name);

            // P8.1: 局部数组声明 (支持多维)
            if (!var.dimensions.empty()) {
                Vb6Type elemType = resolveArrayElemType(var.asType.get());
                std::string saElemType = mapSaElemType(elemType);
                // Bug4-Fix: UDT数组
                std::string udtCType = resolveArrayUdtElemCType(var.asType.get());
                bool isUdtArr = !udtCType.empty();
                int dimCount = (int)var.dimensions.size();

                if (dimCount == 1) {
                    // 一维数组: 保持原有1D代码
                    auto& dim = var.dimensions[0];
                    std::string lBound = "0";
                    std::string uBound = "0";
                    if (dim.lower) {
                        emitExpr(*dim.lower);
                        lBound = std::move(lastExpr_);
                    }
                    if (dim.upper) {
                        emitExpr(*dim.upper);
                        uBound = std::move(lastExpr_);
                    }
                    std::string initCode;
                    if (isUdtArr) {
                        initCode = "vb6_SafeArrayReDim1D_Udt((int32_t)sizeof(" + udtCType + "), " + lBound + ", " + uBound + ")";
                    } else {
                        initCode = "vb6_SafeArrayCreate1D(" + saElemType + ", " + lBound + ", " + uBound + ")";
                    }
                    c_.emitLine("vb6_SafeArray1D* " + cName + " = " + initCode + ";");
                } else {
                    // 多维数组: 使用ND运行时
                    std::string boundsVar = "_bounds_" + cName;
                    c_.emitLine("vb6_SafeArrayBound " + boundsVar + "[] = {");
                    c_.indent();
                    for (int d = 0; d < dimCount; d++) {
                        auto& dim = var.dimensions[d];
                        std::string lb = "0", ub = "0";
                        if (dim.lower) { emitExpr(*dim.lower); lb = std::move(lastExpr_); }
                        if (dim.upper) { emitExpr(*dim.upper); ub = std::move(lastExpr_); }
                        std::string trailing = (d < dimCount - 1) ? "," : "";
                        // vb6_SafeArrayBound = {lLbound, cElements}
                        // cElements = uBound - lBound + 1 (VB6 "0 To 3" has 4 elements)
                        c_.emitLine("{" + lb + ", (" + ub + " - " + lb + " + 1)}" + trailing);
                    }
                    c_.dedent();
                    c_.emitLine("};");
                    c_.emitLine("vb6_SafeArrayND* " + cName + " = vb6_SafeArrayCreateND(" + saElemType + ", " + std::to_string(dimCount) + ", " + boundsVar + ");");
                }

                // 注册到已知数组集合
                std::string lower = var.name;
                std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
                knownArrays_.insert(lower);
                arrayElemTypes_[lower] = elemType;
                arrayDimCounts_[lower] = dimCount;
                if (elemType == Vb6Type::Byte) knownByteArrayVars_.insert(lower);
                // Fix 055: 注册UDT数组元素C类型
                {
                    std::string udtCType = resolveArrayUdtElemCType(var.asType.get());
                    if (!udtCType.empty()) arrayUdtElemTypes_[lower] = udtCType;
                }
                knownLocalVars_.insert(lower);
                break;
            }

            // P8.1: 动态数组声明: Dim arr() As Long → 默认1D, ReDim时可能升级
            if (var.isDynamicArray) {
                Vb6Type elemType = resolveArrayElemType(var.asType.get());
                // Fix 084aa: #undef 防宏污染 — 模块常量被生成 #define 宏 (如 cStartUp 的
                // #define K (vb6_BSTR_FromStr(...))), 同名局部变量声明会被宏展开破坏.
                // 局部变量总是遮蔽模块常量, #undef 是安全且正确的.
                c_.emitLine("#undef " + cName);
                // Fix 092w: Dim arr() As Byte = <初始化表达式> — twinbasic 兼容.
                // 动态数组初值来自字符串/StrConv 时改用字节数组 helper 生成内容,
                // 否则保持 NULL 待 ReDim/赋值.
                std::string dynInit = "NULL";
                if (var.initializer && elemType == Vb6Type::Byte) {
                    emitExpr(*var.initializer);
                    dynInit = rewriteByteArrayValue(lastExpr_);
                }
                c_.emitLine("vb6_SafeArray1D* " + cName + " = " + dynInit + ";");

                // 注册到已知数组集合
                std::string lower = var.name;
                std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
                knownArrays_.insert(lower);
                arrayElemTypes_[lower] = elemType;
                arrayDimCounts_[lower] = 1;  // 动态数组默认1D
                if (elemType == Vb6Type::Byte) knownByteArrayVars_.insert(lower);
                // Fix 055: 注册UDT数组元素C类型
                {
                    std::string udtCType = resolveArrayUdtElemCType(var.asType.get());
                    if (!udtCType.empty()) arrayUdtElemTypes_[lower] = udtCType;
                }
                knownLocalVars_.insert(lower);
                break;
            }

            std::string cType = mapTypeRef(var.asType.get());

            // 记录变量类型集合 (用于Debug.Print和COM解封类型推断)
            if (cType == "BSTR") {
                std::string lower = var.name;
                std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
                knownBstrVars_.insert(lower);
            } else if (cType == "int32_t" || cType == "int16_t" || cType == "VBABOOL") {
                std::string lower = var.name;
                std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
                knownLongVars_.insert(lower);
            } else if (cType == "intptr_t") {
                // Bug #2 fix: LongPtr变量注册到独立集合
                std::string lower = var.name;
                std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
                knownLongPtrVars_.insert(lower);
            } else if (cType.find("vb6_ComIface_") == 0 || cType.find("vb6_ComIface_") != std::string::npos) {
                // Fix 082: COM interface pointer variables are also pointer-sized on x64
                std::string lower = var.name;
                std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
                knownLongPtrVars_.insert(lower);
            } else if (cType == "vb6_VARIANT") {
                // P8.4: 记录Variant类型全局变量
                std::string lower = var.name;
                std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
                knownVariantVars_.insert(lower);
            }

            // 记录Object类型变量名 (COM后期绑定)
            if (cType == "void*") {
                std::string lower = var.name;
                std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
                // 项目类实例 (Dim b As Button / Dim WithEvents b As Button) 的 C 类型同样
                // 是 void*, 但它不是 COM 后期绑定对象, 方法调用必须走直接分发
                // (vb6_Button_DoClick(...)). 若误注册进 knownObjectVars_, cgen_expr 的
                // "优先级1" 会把它当 IDispatch 处理, 生成 vb6_ComCall(b, L"DoClick", ...)
                // → 对纯 C 结构体解引用 vtable → 运行期 0xC0000005.
                // 注意: 本处先于下方 knownClassVars_ 注册, 故直接查类符号判断.
                bool isProjectClassVar = false;
                if (var.asType && var.asType->kind == ASTNodeKind::SimpleTypeRef) {
                    auto& st = static_cast<SimpleTypeRef&>(*var.asType);
                    auto* clsSym = lookupModuleDotted(st.name);
                    if (clsSym && clsSym->kind == SymbolKind::Class) isProjectClassVar = true;
                }
                if (!isProjectClassVar) {
                    knownObjectVars_.insert(lower);
                }
            }

            // P6.3: 记录前期绑定COM变量 (Dim x As FileSystemObject)
            // 查找类型名是否对应ComClass符号
            if (var.asType && var.asType->kind == ASTNodeKind::SimpleTypeRef) {
                auto& simple = static_cast<SimpleTypeRef&>(*var.asType);
                auto* comSym = lookupModuleDotted(simple.name);
                if (comSym && (comSym->kind == SymbolKind::ComClass || comSym->kind == SymbolKind::ComInterface)) {
                    std::string lower = var.name;
                    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
                    knownTypedComVars_[lower] = comSym;
                    // 从后期绑定集合中移除 (优先前期绑定)
                    knownObjectVars_.erase(lower);
                }
            }

            // 记录double/single类型变量名 (用于Debug.Print浮点输出)
            if (cType == "double" || cType == "float") {
                std::string lower = var.name;
                std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
                knownDoubleVars_.insert(lower);
            }

            // 记录类类型变量名, 默认值用NULL
            bool isLocalClassType = false;
            bool isLocalUdtType = false;
            bool isLocalEnumType = false;  // Fix 010q
            bool isLocalComIfaceType = false;
            bool isLocalVb6IfaceType = false;  // P6.4: VB6接口引用
            if (var.asType && var.asType->kind == ASTNodeKind::SimpleTypeRef) {
                auto& simple = static_cast<SimpleTypeRef&>(*var.asType);
                auto* clsSym = lookupModuleDotted(simple.name);
                if (clsSym && clsSym->kind == SymbolKind::Class) {
                    std::string lower = var.name;
                    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
                    // P6.4: 接口类 → knownIfaceVars_ (而非 knownClassVars_)
                    if (clsSym->isInterface) {
                        knownIfaceVars_[lower] = clsSym->name;
                        isLocalVb6IfaceType = true;
                    } else {
                        // Fix 010r-10: map赋值, 存储类名以便方法分发时查找
                        knownClassVars_[lower] = clsSym->name;
                        isLocalClassType = true;
                        // P14.3.1: Dim As New自动实例化
                        if (var.isNew) {
                            knownNewVars_[lower] = cIdent(clsSym->name);
                        }
                    }
                }
                if (clsSym && (clsSym->kind == SymbolKind::ComClass || clsSym->kind == SymbolKind::ComInterface)) {
                    isLocalComIfaceType = true;
                }
                // 检查是否是UDT类型
                auto* udtSym = lookupDotted(simple.name);
                if (udtSym && udtSym->kind == SymbolKind::UserDefinedType) {
                    isLocalUdtType = true;
                    // M22-fix: 注册到knownUdtVars_，防止成员访问被误判为模块名限定
                    std::string udtLower = var.name;
                    std::transform(udtLower.begin(), udtLower.end(), udtLower.begin(), ::tolower);
                    knownUdtVars_[udtLower] = "vb6_type_" + cIdent(simple.name);
                }
                // Fix 010q: 检查是否是Enum类型 (mapTypeRef映射为int32_t, 但defaultValue返回vb6_VariantEmpty())
                if (udtSym && udtSym->kind == SymbolKind::EnumType) {
                    isLocalEnumType = true;
                }
            }

            // VB6 Static变量: 跨调用持久化 → C static局部变量
            // 包括: 显式Static声明 或 Static Sub/Function内的所有局部变量
            std::string storageClass = (var.isStatic || inStaticProc_) ? "static " : "";

            // Fix 010r-12c: Register non-array local variable to knownLocalVars_
            // so it shadows cross-module external Public symbols with the same name.
            // (Array/dynamic-array cases already insert above; this covers all other types)
            {
                std::string lower = var.name;
                std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
                knownLocalVars_.insert(lower);
            }

            if (var.initializer) {
                // Fix 084aa: #undef 防宏污染 (见 P8.1 动态数组处注释)
                c_.emitLine("#undef " + cName);
                emitExpr(*var.initializer);
                c_.emitLine(storageClass + cType + " " + cName + " = " + lastExpr_ + ";");
            } else {
                std::string initVal;
                if (isLocalClassType || isLocalComIfaceType) {
                    initVal = "NULL";
                } else if (isLocalVb6IfaceType) {
                    initVal = "{0}";  // P6.4: 接口引用 = {vtbl=NULL, obj=NULL}
                } else if (isLocalUdtType) {
                    initVal = "{0}";
                } else if (isLocalEnumType) {  // Fix 010q
                    initVal = "0";
                } else if (var.asType && var.asType->kind == ASTNodeKind::FixedStringTypeRef) {
                    // String * N: 初始化为N个空格的BSTR, LSet/RSet使用固定长度
                    auto& fs = static_cast<FixedStringTypeRef&>(*var.asType);
                    emitExpr(*fs.length);
                    std::string fsLen = lastExpr_;
                    initVal = "vb6_BSTR_FixedSTR(" + fsLen + ")";
                    // 注册定长字符串变量名→长度
                    std::string fsLower = var.name;
                    std::transform(fsLower.begin(), fsLower.end(), fsLower.begin(), ::tolower);
                    knownFixedStringLen_[fsLower] = fsLen;
                } else {
                    initVal = defaultValue(
                        var.asType && var.asType->kind == ASTNodeKind::SimpleTypeRef
                            ? typeSys_.resolveTypeName(static_cast<SimpleTypeRef*>(var.asType.get())->name)
                            : Vb6Type::Variant
                    );
                }
                // Fix 084aa: 静态局部变量初始化必须是编译期常量 (C2099).
                // vb6_VariantEmpty()/vb6_BSTR_Empty() 是函数调用, 静态初始化会报错.
                // {0} (vt=0=VT_EMPTY) 与 vb6_VariantEmpty() 语义一致; NULL 即空BSTR.
                if (!storageClass.empty()) {
                    if (initVal == "vb6_VariantEmpty()") initVal = "{0}";
                    if (initVal == "vb6_BSTR_Empty()") initVal = "NULL";
                }
                // Fix 084aa: #undef 防宏污染 (见 P8.1 动态数组处注释)
                c_.emitLine("#undef " + cName);
                c_.emitLine(storageClass + cType + " " + cName + " = " + initVal + ";");
            }
            break;
        }
        case ASTNodeKind::ConstDecl: {
            auto& con = static_cast<ConstDecl&>(*node.decl);
            std::string cType = mapTypeRef(con.asType.get());
            // Fix 091d: 无 As 类型常量按字面量推断 C 类型. 此前一律 vb6_VARIANT →
            // `const vb6_VARIANT SW_SHOWNORMAL = 1;` 非法初始化 → C2440
            // (cToolsSystem.c 11/13); 且 Variant 常量参与位运算时操作数被包装
            // vb6_VariantToLong(<字面量>) → C2440 (cDialog.c 36 BIF_USENEWUI).
            if (!con.asType && con.value
                && con.value->kind == ASTNodeKind::LiteralExpr) {
                auto& lit091d = static_cast<LiteralExpr&>(*con.value);
                switch (lit091d.literalKind) {
                    case LiteralKind::Integer:
                    case LiteralKind::Long:
                        cType = "int32_t";
                        break;
                    case LiteralKind::Single:
                    case LiteralKind::Double:
                        cType = "double";
                        break;
                    case LiteralKind::String:
                        cType = "BSTR";
                        break;
                    case LiteralKind::Boolean:
                        cType = "VBABOOL";
                        break;
                    default:
                        break;
                }
            }
            std::string cName = cIdent(con.name);
            // Fix 010r-12c: Register local constant to knownLocalVars_
            {
                std::string lower = con.name;
                std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
                knownLocalVars_.insert(lower);
            }
            // Fix 049: Register local constant to type-specific known*Vars_ sets.
            // Same logic as Dim (cgen_decl.cpp:749-767). Without this, inferExprType
            // falls back to Variant for unknown identifiers, causing wrapToBSTR to
            // generate vb6_CStr(BSTR_const) which triggers C2440 (BSTR→VARIANT).
            {
                std::string lower = con.name;
                std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
                if (cType == "BSTR") {
                    knownBstrVars_.insert(lower);
            } else if (cType == "int32_t" || cType == "int16_t" || cType == "VBABOOL") {
                    knownLongVars_.insert(lower);
                } else if (cType == "intptr_t") {
                    // Bug #2 fix: LongPtr局部const变量注册到独立集合
                    knownLongPtrVars_.insert(lower);
                } else if (cType.find("vb6_ComIface_") != std::string::npos) {
                    // Fix 082: COM interface pointer types are pointer-sized on x64
                    knownLongPtrVars_.insert(lower);
                } else if (cType == "double" || cType == "float") {
                    knownDoubleVars_.insert(lower);
                } else if (cType == "vb6_VARIANT") {
                    knownVariantVars_.insert(lower);
                }
            }
            if (con.value) {
                emitExpr(*con.value);
                // Fix 010r-13: Local Const redefining Windows API macro? #undef first.
                // 必须在声明 const 变量之前 #undef, 防止名字被 <windows.h> 等头文件中的
                // 宏展开 (例: WHITE_BRUSH、MEM_COMMIT、CP_UTF8、SW_SHOWNORMAL 等
                // 都是 windows.h 中的 #define, 否则 `const int32_t WHITE_BRUSH = 0;`
                // 会被宏展开为 `const int32_t 0 = 0;` 引发 C2106).
                // #undef 对没有定义为宏的名字是空操作, 无副作用.
                c_.emitLine("#undef " + cName);
                c_.emitLine("const " + cType + " " + cName + " = " + lastExpr_ + ";");
            }
            break;
        }
        default:
            c_.emitLine("/* unhandled LocalDeclStmt: " + std::string(node.decl->kindName()) + " */");
            break;
    }
}

// ---- Fix 086: 局部声明过程级作用域提升 ----

void CCodeGen::collectLocalDeclStmts(StmtList& stmts, std::vector<LocalDeclStmt*>& out) {
    for (auto& stmt : stmts) {
        if (!stmt) continue;
        switch (stmt->kind) {
            case ASTNodeKind::LocalDeclStmt:
                out.push_back(static_cast<LocalDeclStmt*>(stmt.get()));
                break;
            case ASTNodeKind::Block:
                collectLocalDeclStmts(static_cast<Block&>(*stmt).stmts, out);
                break;
            case ASTNodeKind::IfStmt: {
                auto& n = static_cast<IfStmt&>(*stmt);
                collectLocalDeclStmts(n.thenBody, out);
                for (auto& ei : n.elseIfs)
                    if (ei) collectLocalDeclStmts(ei->body, out);
                collectLocalDeclStmts(n.elseBody, out);
                break;
            }
            case ASTNodeKind::ForStmt:
                collectLocalDeclStmts(static_cast<ForStmt&>(*stmt).body, out);
                break;
            case ASTNodeKind::ForEachStmt:
                collectLocalDeclStmts(static_cast<ForEachStmt&>(*stmt).body, out);
                break;
            case ASTNodeKind::DoLoopStmt:
                collectLocalDeclStmts(static_cast<DoLoopStmt&>(*stmt).body, out);
                break;
            case ASTNodeKind::WhileWendStmt:
                collectLocalDeclStmts(static_cast<WhileWendStmt&>(*stmt).body, out);
                break;
            case ASTNodeKind::SelectCaseStmt: {
                auto& n = static_cast<SelectCaseStmt&>(*stmt);
                for (auto& cc : n.cases)
                    if (cc) collectLocalDeclStmts(cc->body, out);
                collectLocalDeclStmts(n.elseCase, out);
                break;
            }
            case ASTNodeKind::WithStmt:
                collectLocalDeclStmts(static_cast<WithStmt&>(*stmt).body, out);
                break;
            default:
                break;
        }
    }
}

// Fix 090o: 递归收集语句序列内定义的标签名 (VB6 过程内标签唯一; 用于判定
// GoTo 目标是否在 For 方向拆分的 body 内, 决定第二份副本 goto 是否加 _dN 后缀)
void CCodeGen::collectForBodyLabels(const StmtList& stmts, std::unordered_set<std::string>& out) {
    for (auto& stmt : stmts) {
        if (!stmt) continue;
        switch (stmt->kind) {
            case ASTNodeKind::LabelStmt:
                out.insert(Symbol::toLower(static_cast<LabelStmt&>(*stmt).labelName));
                break;
            case ASTNodeKind::Block:
                collectForBodyLabels(static_cast<Block&>(*stmt).stmts, out);
                break;
            case ASTNodeKind::IfStmt: {
                auto& n = static_cast<IfStmt&>(*stmt);
                collectForBodyLabels(n.thenBody, out);
                for (auto& ei : n.elseIfs)
                    if (ei) collectForBodyLabels(ei->body, out);
                collectForBodyLabels(n.elseBody, out);
                break;
            }
            case ASTNodeKind::ForStmt:
                collectForBodyLabels(static_cast<ForStmt&>(*stmt).body, out);
                break;
            case ASTNodeKind::ForEachStmt:
                collectForBodyLabels(static_cast<ForEachStmt&>(*stmt).body, out);
                break;
            case ASTNodeKind::DoLoopStmt:
                collectForBodyLabels(static_cast<DoLoopStmt&>(*stmt).body, out);
                break;
            case ASTNodeKind::WhileWendStmt:
                collectForBodyLabels(static_cast<WhileWendStmt&>(*stmt).body, out);
                break;
            case ASTNodeKind::SelectCaseStmt: {
                auto& n = static_cast<SelectCaseStmt&>(*stmt);
                for (auto& cc : n.cases)
                    if (cc) collectForBodyLabels(cc->body, out);
                collectForBodyLabels(n.elseCase, out);
                break;
            }
            case ASTNodeKind::WithStmt:
                collectForBodyLabels(static_cast<WithStmt&>(*stmt).body, out);
                break;
            default:
                break;
        }
    }
}

// Fix 090m/090p: ReDim/Erase 目标是 Variant 数组判定 — 顶层 As Variant 变量
// 或 UDT 的 As Variant 字段 ((*uFile).BufferArray / vb6_ret_X.BufferArray)。
// 090m 修复 ReDim Preserve; 090p 使 Erase 复用同一判定 (cZipArchive pvVfsSetEof:
// Erase uFile.BufferArray 生成了裸 SafeArrayDestroy1D((*uFile).BufferArray) → C2440)
bool CCodeGen::isVariantArrayTarget(const std::string& name) {
    std::string checkName = name;
    if (checkName.substr(0, 4) == "me->") checkName = checkName.substr(4);
    if (checkName.size() > 4 && checkName[0] == '(' && checkName[1] == '*'
        && checkName.back() == ')') {
        checkName = checkName.substr(2, checkName.size() - 3);
    }
    std::string lower = checkName;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    if (knownVariantVars_.count(lower) > 0) return true;
    // Fix 090m: UDT 字段目标 — (*uFile).BufferArray / vb6_ret_X.BufferArray:
    // 对象是 UDT (ByRef 参数/局部/返回变量), 字段声明 As Variant 时按
    // Variant 数组处理 (cZipArchive pvVfsWrite: ReDim Preserve
    // uFile.BufferArray(...) As Byte → C2440 直接把 VARIANT 当 SafeArray*).
    size_t dotP090m = name.rfind('.');
    size_t arrowP090m = name.rfind("->");
    size_t sepP090m = (dotP090m == std::string::npos) ? arrowP090m
                    : (arrowP090m == std::string::npos) ? dotP090m : std::max(dotP090m, arrowP090m);
    if (sepP090m != std::string::npos && sepP090m + 1 < name.size()) {
        std::string objTxt090m = name.substr(0, sepP090m);
        std::string fieldTxt090m = name.substr(sepP090m + ((sepP090m >= 1 && name[sepP090m - 1] == '-') ? 2 : 1));
        if (objTxt090m.size() > 2 && objTxt090m.rfind("(*", 0) == 0
            && objTxt090m.back() == ')') {
            objTxt090m = objTxt090m.substr(2, objTxt090m.size() - 3);
        }
        std::string objLower090m = objTxt090m;
        std::transform(objLower090m.begin(), objLower090m.end(), objLower090m.begin(), ::tolower);
        auto itUdt090m = knownUdtVars_.find(objLower090m);
        if (itUdt090m != knownUdtVars_.end() && itUdt090m->second.rfind("vb6_type_", 0) == 0) {
            std::string udtName090m = itUdt090m->second.substr(8);
            // Fix 090m: knownUdtVars_ 值 = "vb6_type_" + cIdent(UDT名), cIdent 给
            // 私有 UDT 名加前导 '_' (vb6_type__ZipVfsType) → lookupModule 前需去 _
            if (udtName090m.size() > 1 && udtName090m[0] == '_') udtName090m = udtName090m.substr(1);
            Symbol* udtSym090m = symTab_.lookupModule(udtName090m);
            if (udtSym090m && udtSym090m->kind == SymbolKind::UserDefinedType) {
                std::string fieldLower090m = fieldTxt090m;
                std::transform(fieldLower090m.begin(), fieldLower090m.end(), fieldLower090m.begin(), ::tolower);
                for (auto& mi090m : udtSym090m->udtMembers) {
                    std::string miLower090m = mi090m.name;
                    std::transform(miLower090m.begin(), miLower090m.end(), miLower090m.begin(), ::tolower);
                    if (miLower090m == fieldLower090m) {
                        return mi090m.type == Vb6Type::Variant;
                    }
                }
            }
        }
    }
    return false;
}

void CCodeGen::hoistLocalDecls(StmtList& body) {
    if (const char* dis = std::getenv("C3_NO_HOIST")) {
        (void)dis;
        hoistedLocalDeclSet_.clear();
        return;
    }
    hoistedLocalDeclSet_.clear();
    std::vector<LocalDeclStmt*> decls;
    collectLocalDeclStmts(body, decls);
    for (auto* d : decls) {
        if (!d || !d->decl) continue;
        if (hoistedLocalDeclSet_.count(d)) continue;
        bool hoistable = false;
        if (d->decl->kind == ASTNodeKind::VariableDecl) {
            auto& var = static_cast<VariableDecl&>(*d->decl);
            // 固定边界数组保留原位 (边界表达式可能依赖执行到该点时的状态)
            hoistable = var.dimensions.empty();
        } else if (d->decl->kind == ASTNodeKind::ConstDecl) {
            hoistable = true;
        }
        if (hoistable) {
            hoistedLocalDeclSet_.insert(d);
            emitLocalDeclCode(*d);
        }
    }
}

} // namespace vb6c3
