#include "backend/cgen.hpp"
#include <algorithm>
#include <cctype>
#include <iostream>
#include <functional>
#include <cstdio>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>

namespace vb6c3 {

static std::string floatingLiteral(double value, int precision) {
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::setprecision(precision) << value;
    std::string text = out.str();
    if (text.find_first_of(".eE") == std::string::npos) text += ".0";
    return text;
}

// --- cgen_expr.cpp: emitExpr 分发 + 字面量/一元/字典访问/New/TypeOf/AddressOf/Me 表达式 ---

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

void CCodeGen::visit(LiteralExpr& node) {
    switch (node.literalKind) {
        case LiteralKind::Integer:
            lastExpr_ = std::to_string(static_cast<int>(node.intValue));
            break;
        case LiteralKind::Long:
            lastExpr_ = std::to_string(node.longValue) + "L";
            break;
        case LiteralKind::Single:
            lastExpr_ = floatingLiteral(node.floatValue, std::numeric_limits<float>::max_digits10) + "f";
            break;
        case LiteralKind::Double:
        case LiteralKind::Currency:
        case LiteralKind::Decimal:
            lastExpr_ = floatingLiteral(node.doubleValue, std::numeric_limits<double>::max_digits10);
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
            lastExpr_ = floatingLiteral(node.doubleValue, std::numeric_limits<double>::max_digits10);  // OLE date as double
            break;
    }
}

// Fix 100: 比较类表达式 (及 TypeOf) 在生成码里落在 C 的 0/1 上, 而 VB6 布尔是
// -1/0. 对这类布尔结果直接按位取反 (C: ~) 只会得到 -1 (0 → ~0) 或 -2 (1 → ~1),
// 两者都非零 → `Not X Is Nothing` / `Not (a = b)` 恒为真.
// 实例: cHttpServer.StopMe 的 `If Not m_oServer Is Nothing Then m_oServer.CloseSck`
// 恒进分支, 对未赋值的 m_oServer 调用 CloseSck(NULL) → 释放对象时段错误.
static bool exprYieldsVbBoolean(const Expr& e) {
    switch (e.kind) {
        case ASTNodeKind::TypeOfExpr:
            return true;   // vb6_TypeOf() 返回 0/1
        case ASTNodeKind::BinaryExpr: {
            const auto& b = static_cast<const BinaryExpr&>(e);
            switch (b.op) {
                case BinaryOp::Eq:  case BinaryOp::Neq:
                case BinaryOp::Lt:  case BinaryOp::Gt:
                case BinaryOp::Le:  case BinaryOp::Ge:
                case BinaryOp::Like: case BinaryOp::Is:
                    return true;
                default:
                    return false;
            }
        }
        case ASTNodeKind::UnaryExpr:
            // 修正后 Not 自身产出标准 VB6 布尔 (-1/0), 嵌套 Not 同样按布尔处理
            return static_cast<const UnaryExpr&>(e).op == UnaryOp::Not;
        default:
            return false;
    }
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
            } else if (operand.find("vb6_ComGetStringProp(") == 0) {
                // Fix 092r: COM 对象属性默认按字符串读取 (属性类型未知回退
                // BSTR) 且一元取负 → 生成 -BSTR (C2171). 改为按 Long 数值属性
                // 重读再取负. 例: QRCodegenResizePicture → -pPicture.Height.
                // substr 剥离 "vb6_ComGetStringProp(" 前缀, 保留 "obj, L\"Prop\""
                // 实参串给签名一致的 vb6_ComGetIntProp.
                std::string inner092r = operand.substr(std::strlen("vb6_ComGetStringProp("));
                if (!inner092r.empty() && inner092r.back() == ')') inner092r.pop_back();
                lastExpr_ = "(-vb6_ComGetIntProp(" + inner092r + "))";
            } else if (operand.find("vb6_VariantFromComResult(") == 0) {
                // Fix 092r: 晚绑定 COM 属性读取结果 (vb6_VARIANT) 取负 →
                // -(vb6_VARIANT) C2440. 先转数值再取负.
                lastExpr_ = "(-vb6_VariantToDouble(" + operand + "))";
            } else {
                lastExpr_ = "(-" + operand + ")";
            }
            break;
        case UnaryOp::Not:
            // Fix 100: 布尔型操作数必须用逻辑取反 —— VB6 Not True = False(0),
            // Not False = True(-1). 理由见 exprYieldsVbBoolean() 注释.
            if (node.operand && exprYieldsVbBoolean(*node.operand)) {
                lastExpr_ = "((((int32_t)(" + operand + ")) != 0) ? 0 : -1)";
                break;
            }
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

void CCodeGen::visit(DictionaryAccessExpr& node) {
    emitExpr(*node.object);
    std::string obj = std::move(lastExpr_);
    // obj!key → 字典访问, 暂用函数调用
    lastExpr_ = "vb6_DictAccess(" + obj + ", L\"" + node.key + "\")";
}

void CCodeGen::visit(NewExpr& node) {
    // 查找是否为本工程内的类模块
    std::string clsLower = node.className;
    std::transform(clsLower.begin(), clsLower.end(), clsLower.begin(), ::tolower);

    // 尝试在符号表中查找类符号
    // Fix 102: 用 lookupModuleDotted 而非裸 lookupModule —— 源码里的早绑定类型常写
    // 「类型库名.coclass名」(WinHttp.WinHttpRequest / Scripting.Dictionary / ADODB.Stream),
    // 而类型库解析后 coclass 是按**裸名**(WinHttpRequest / Dictionary) 登记的.
    // 裸 lookupModule 按全名查 key "winhttp.winhttprequest" 必然落空 → 落下方 else 分支,
    // 把源码限定名当 ProgID 传给 vb6_NewObject → 运行期 CLSIDFromProgID 失败 → 429
    // (WinHttp 只注册了版本化的 "WinHttp.WinHttpRequest.5.1", 裸名无注册, 实测
    // CLSIDFromProgID("WinHttp.WinHttpRequest") = 0x800401F3).
    // 修好后命中 ComClass 分支, 用类型库 ProgIDFromCLSID 反查到的真实 ProgID.
    // 与 cgen_with.cpp 的 NewExpr 处理 (lookupModuleDotted) 及声明路径
    // (cgen_decl_var.cpp:198 knownTypedComVars_ 注册) 保持一致.
    auto* clsSym = lookupModuleDotted(node.className);
    if (clsSym && clsSym->kind == SymbolKind::Class) {
        // 本工程类: 调用类工厂函数
        std::string clsStruct = "vb6_cls_" + cIdent(clsSym->name);
        lastExpr_ = "(" + clsStruct + "_New())";
    } else if (clsSym && clsSym->kind == SymbolKind::ComClass) {
        // P24-11: COM early-bound class: use real ProgID from TypeLib, not the raw class name
        std::string progId = clsSym->comProgId.empty() ? node.className : clsSym->comProgId;
        // Fix 177b: coclass 被本工程同名类遮蔽时, 创建点改走工程类工厂 + IDispatch 包装
        std::string projNew = comNewExprFor(clsSym);
        if (!projNew.empty()) {
            lastExpr_ = projNew;
        } else {
            lastExpr_ = "(void*)vb6_NewObject(L\"" + progId + "\")";
        }
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

} // namespace vb6c3
