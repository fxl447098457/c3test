#pragma once
// vb6c3 - C代码生成器
// 将语义分析后的AST+符号表翻译为C代码，交由MSVC编译
// 设计参考: cfront, Nim, Zig早期均采用C代码生成路线

#include "ast/ast.hpp"
#include "ast/ast_visitor.hpp"
#include "semantics/symbol_table.hpp"
#include "semantics/type_system.hpp"
#include "common/diagnostics.hpp"
#include "project/frm_parser.hpp"
#include <string>
#include <vector>
#include <sstream>
#include <unordered_set>
#include <unordered_map>
#include <set>

namespace vb6c3 {

// ============================================================
// 代码输出辅助: 缩进管理、格式化
// ============================================================

class CodeEmitter {
public:
    // 输出一行（自动缩进+换行）
    void emitLine(const std::string& line = "");

    // 输出内容（不带换行）
    void emit(const std::string& text);

    // 输出空行
    void emitBlank();

    // Fix 090q: 注册"下一行输出前须先落地的声明行"(如 As Any ByRef 实参的
    // UDT 临时变量声明). 表达式拼接期无法在行中插语句, 故挂到下一个 emitLine
    // 之前按当前缩进输出 — 表达式文本与该行同次 flush, 声明恒先于引用.
    void addPending(const std::string& line) { pendingLines_.push_back(line); }

    // 缩进控制
    void indent() { indentLevel_++; }
    void dedent() { if (indentLevel_ > 0) indentLevel_--; }

    // 获取生成的代码
    std::string str() const { return oss_.str(); }

    // 清空
    void clear() { oss_.str(""); oss_.clear(); indentLevel_ = 0; }

private:
    std::ostringstream oss_;
    int indentLevel_ = 0;
    // Fix 090q: 待"下一行前"输出的声明行(见 addPending 注释)
    std::vector<std::string> pendingLines_;

    // Fix 090q: 在下一行输出前先落地 pendingLines_ (flushPending 由 emitLine
    // 调用; 在 flushPending 中把调用权交还 oss_ 直接写入, 避免递归)
    void flushPending();
};

// ============================================================
// C代码生成器
//
// 输入: Module AST + SemanticAnalyzer的产出(符号表+类型系统)
// 输出: .h头文件 + .c源文件
//
// 架构:
//   1. generate() 主入口 → 生成 .h + .c
//   2. H文件: 类型定义、常量、变量声明、函数前向声明
//   3. C文件: #include ".h"、变量定义、函数实现
//   4. 各emit*方法按节点kind分派
// ============================================================

class CCodeGen : public ASTVisitor {
public:
    CCodeGen(Diagnostics& diag, const SymbolTable& symTab,
             const TypeSystem& typeSys,
             const std::unordered_map<std::string, std::set<std::string>>* classVoidFieldMap = nullptr,
              const std::unordered_map<std::string, std::unordered_map<std::string, std::string>>* classTypedFieldMap = nullptr,
              const std::unordered_set<std::string>* variantReturnFuncs = nullptr,
              bool verbose = false);

    // 主入口: 生成C代码，返回是否成功
    // externalModules: 当前模块引用的外部模块基名列表 (用于生成 #include)
    // isDll: P6.6 ActiveX DLL模式, 生成COM服务端代码
    // dllProgId: P6.6 DLL的ProgID前缀
    // frmDesc: P7 窗体描述 (仅.frm有效, nullptr=非窗体模块)
    bool generate(Module& module, const std::string& baseName,
                  const std::unordered_set<std::string>& externalModules = {},
                  bool isDll = false, const std::string& dllProgId = "",
                  const FrmFormDesc* frmDesc = nullptr);

    // opt4: 裁剪标准模块.c中未实际引用的跨模块#include (降低预处理量, 加速cl编译)
    void setTrimIncludes(bool b) { trimIncludes_ = b; }
    // opt4: 设置"当前模块实际引用的外部模块"集合(小写), 由driver通过AST扫描提供
    void setTrimModules(const std::unordered_set<std::string>& m) { trimModules_ = m; }

    // Fix 086: 设置工程内全部窗体模块名集合(小写), 由driver预扫描提供.
    // 跨模块窗体默认实例引用 (cLogs 里 FLogs.Visible / Unload FLogs) 需要知道
    // FLogs 是窗体, 才能生成 vb6_form_hwnd_FLogs() 访问器调用而非裸标识符.
    void setFormModuleNames(const std::unordered_set<std::string>& names) { knownFormModuleNames_ = names; }
    // Fix 086: 各模块 Public 常量表 (小写模块名 → 小写常量名 → 整数值).
    // 本地同名过程符号会阻止跨模块常量注入 (如 cSerialPort.Sub SetDTR 与
    // modSerialPortAPI.Const SETDTR), 此表兜底内联数值.
    void setModulePublicConsts(const std::unordered_map<std::string,
        std::unordered_map<std::string, long long>>* consts) { modulePublicConsts_ = consts; }

    // P6.6: 单独生成ActiveX DLL入口文件 (dll_entry.c)
    // 当DLL工程只有类模块(无标准模块)时, 由Driver调用此方法生成DLL导出代码
    // progId: DLL的ProgID前缀
    // 返回: 生成的dll_entry.c文件内容
    std::string generateDllEntry(const std::string& progId,
                                  const std::vector<SymbolTable*>& allSymTabs = {});

    // 获取生成的代码
    const std::string& headerCode() const { return header_; }
    const std::string& sourceCode() const { return source_; }

    // --- 声明 ---
    void visit(SubDecl& node) override;
    void visit(FunctionDecl& node) override;
    void visit(PropertyDecl& node) override;
    void visit(TypeDecl& node) override;
    void visit(TypeMember& node) override;
    void visit(EnumDecl& node) override;
    void visit(EnumMember& node) override;
    void visit(DeclareDecl& node) override;
    void visit(EventDecl& node) override;
    void visit(ConstDecl& node) override;
    void visit(VariableDecl& node) override;
    void visit(ParameterDecl& node) override;

    // --- 语句 ---
    void visit(Block& node) override;
    void visit(AssignmentStmt& node) override;
    void visit(SetStmt& node) override;
    void visit(LetStmt& node) override;
    void visit(IfStmt& node) override;
    void visit(ElseIfClause& node) override;
    void visit(ForStmt& node) override;
    void visit(ForEachStmt& node) override;
    void visit(DoLoopStmt& node) override;
    void visit(WhileWendStmt& node) override;
    void visit(SelectCaseStmt& node) override;
    void visit(CaseClause& node) override;
    void visit(WithStmt& node) override;
    void visit(GoToStmt& node) override;
    void visit(GoSubStmt& node) override;
    void visit(OnGoSubStmt& node) override;  // P17.4
    void visit(OnGoToStmt& node) override;  // P18-A
    void visit(MidStmt& node) override;  // P18-A
    void visit(OnErrorStmt& node) override;
    void visit(ResumeStmt& node) override;
    void visit(ErrorStmt& node) override;
    void visit(ExitStmt& node) override;
    void visit(CallStmt& node) override;
    void visit(ReDimStmt& node) override;
    void visit(EraseStmt& node) override;
    // Fix 084y-5: ReDim/Erase 目标的 C 标识符解析 — 处理成员访问形态
    // (.Field With块成员 → _vb6_with_N->Field; obj.Field UDT成员 →
    // (*obj).Field 或 obj.Field), 避免 cIdent 把 '.' 替换成 '_' 生成
    // 未声明的单标识符 (uOutput.Buffer → uOutput_Buffer → C2065)
    std::string resolveArrayTargetIdent(const std::string& varName);
    // Fix 084aa: 判断标识符是否引用 String 类型常量 (#define 宏, 不可取址)
    bool isStringConstIdent(const std::string& name) const;
    // Fix 084aa: 查找常量符号 (Constant kind), 无则返回 nullptr
    Symbol* lookupConstSym(const std::string& name) const;
    // Fix 084aa: 判断标识符是否引用任何类型的常量 (#define 宏)
    bool isConstIdent(const std::string& name) const;
    // Fix 084aa: 获取常量标识符的 VB6 类型
    Vb6Type constIdentType(const std::string& name) const;
    // Fix 084aa: 常量宏作为 ByRef 实参 → 生成可寻址复合字面量包装
    std::string wrapConstArgForByRef(const std::string& argVal, Vb6Type paramVb6Type) const;
    void visit(OpenStmt& node) override;
    void visit(CloseStmt& node) override;
    void visit(PrintStmt& node) override;
    void visit(WriteStmt& node) override;
    void visit(LineInputStmt& node) override;
    void visit(InputStmt& node) override;
    void visit(GetStmt& node) override;
    void visit(PutStmt& node) override;
    void visit(SeekStmt& node) override;
    void visit(LockStmt& node) override;
    void visit(UnlockStmt& node) override;
    void visit(ResetStmt& node) override;
    void visit(WidthStmt& node) override;
    void visit(KillStmt& node) override;
    void visit(NameStmt& node) override;
    void visit(MkDirStmt& node) override;
    void visit(RmDirStmt& node) override;
    void visit(ChDirStmt& node) override;
    void visit(ChDriveStmt& node) override;
    void visit(FileCopyStmt& node) override;
    void visit(LabelStmt& node) override;
    void visit(OptionStmt& node) override;
    void visit(LocalDeclStmt& node) override;
    // Fix 086: LocalDeclStmt 的实际声明发射代码 (visit 判重后调用; 提升阶段也直接调用)
    void emitLocalDeclCode(LocalDeclStmt& node);
    void visit(RaiseEventStmt& node) override;
    void visit(BeepStmt& node) override;
    void visit(DoEventsStmt& node) override;

    // --- 表达式 (返回C表达式字符串) ---
    void visit(BinaryExpr& node) override;
    void visit(UnaryExpr& node) override;
    void visit(LiteralExpr& node) override;
    void visit(IdentifierExpr& node) override;
    void visit(MemberAccessExpr& node) override;
    void visit(DictionaryAccessExpr& node) override;
    void visit(IndexOrCallExpr& node) override;
    void visit(NewExpr& node) override;
    void visit(TypeOfExpr& node) override;
    void visit(AddressOfExpr& node) override;
    void visit(MeExpr& node) override;
    void visit(WithMemberExpr& node) override;

    // --- 类型引用 ---
    void visit(SimpleTypeRef& node) override;
    void visit(ArrayTypeRef& node) override;
    void visit(FixedStringTypeRef& node) override;

    // --- 模块 ---
    void visit(Module& node) override;

    // 限定类型名符号查找: "Scripting.Dictionary" → 先查全名，失败则查短名 "Dictionary"
    Symbol* lookupModuleDotted(const std::string& name) const {
        auto* sym = symTab_.lookupModule(name);
        if (!sym) {
            size_t dot = name.find('.');
            if (dot != std::string::npos) {
                sym = symTab_.lookupModule(name.substr(dot + 1));
            }
        }
        return sym;
    }
    Symbol* lookupDotted(const std::string& name) const {
        auto* sym = symTab_.lookup(name);
        if (!sym) {
            size_t dot = name.find('.');
            if (dot != std::string::npos) {
                sym = symTab_.lookup(name.substr(dot + 1));
            }
        }
        return sym;
    }

private:
    Diagnostics& diag_;    const SymbolTable& symTab_;
    const TypeSystem& typeSys_;
    bool verbose_;

    // Fix 056b: 清理过程内局部数组注册, 保留模块级/类成员数组 (跨过程需要)
    // 过程开始时清空knownArrays_会丢失模块级UDT数组的arrayUdtElemTypes_注册,
    // 导致访问时元素类型回退vb6_VARIANT (MSVC C2440).
    void clearProcArrayTracking();

    // 代码输出
    CodeEmitter h_;   // .h 文件内容
    CodeEmitter c_;   // .c 文件内容
    std::string header_;
    std::string source_;
    std::string baseName_;        // 输出文件基名 (如 "hello")
    std::string moduleName_;      // 当前模块名 (用于跨模块函数名前缀)

    // 状态
    Module* currentModule_ = nullptr;
    Symbol* currentProc_ = nullptr;  // 当前过程符号
    std::string currentReturnVar_;   // Function返回值变量名 (如 "vb6_ret_CalculateSum")
    std::string currentReturnCType_; // Fix 054: 当前函数返回值的C类型名 (如 "vb6_type_QRCodegenSegment")
    int labelCounter_ = 0;           // 标签计数器 (避免C标签冲突)
    int tempCounter_ = 0;            // 临时变量计数器

    // 表达式求值结果 (累加器模式: 每个emitExpr调用设置lastExpr_)
    std::string lastExpr_;           // 最近一次表达式生成的C代码字符串

    // 已生成的C标识符 (避免重复)
    std::unordered_set<std::string> emittedSymbols_;

    // With语句名称栈
    std::vector<std::string> withObjectVars_;

    // P17.1: With对象元信息栈 — 与withObjectVars_平行，记录对象类别
    enum class WithObjKind {
        Unknown,        // UDT/未知: 生成 struct.field 访问
        FormControl,    // 窗体控件HWND: 使用 getControlPropReadFn/WriteFn
        WithEventsCtrl, // WithEvents控件变量HWND: 使用WE属性路径
        COMObject,      // COM IDispatch*: 使用 vb6_ComGetProp/ComSetProp
        ClassInstance,  // 类实例指针: 使用类方法调用
        BuiltinObject   // 内置全局对象(Err/App等): 使用函数调用
    };
    struct WithObjInfo {
        WithObjKind kind = WithObjKind::Unknown;
        FrmControlType ctrlType = FrmControlType::Unknown;
        std::string ctrlOrigName;    // 控件原始大小写名称(用于HWND变量名)
        // Fix 011r-1: 类实例的类名(如"cJson"), 仅 ClassInstance kind 有意义
        // 已知时优先通过 resolveClassMemberCall 精确解析该类的成员方法/属性,
        // 并把 tempType 设为 vb6_cls_<className>* (避免 void* 上的 .member/->member 错误)
        std::string className;
    };
    std::vector<WithObjInfo> withObjectInfoStack_;
    bool suppressDefaultProp_ = false;  // P17.1: With对象表达式时抑制默认属性读取

    // P14.3.1: Dim As New自动实例化变量集合 (小写key → 类名C标识)
    std::unordered_map<std::string, std::string> knownNewVars_;
    // Fix 090v: 模块/类级 As New 变量持久注册 (knownNewVars_ 在函数入口清理,
    // 需从本表恢复; 局部 As New 由 LocalDeclStmt 每函数重注册) — 否则跨函数
    // 同名局部变量 (如 cColl 的 Dim A As New cCollection vs JsonStr 的
    // Const A) 残留 knownNewVars_ → const 变量被注入 auto-instantiate (C2166).
    std::unordered_map<std::string, std::string> moduleNewVars_;

    // Fix 054: 模块级变量延迟初始化语句 (C2099: 文件作用域变量不能用运行时函数调用初始化)
    std::vector<std::string> moduleInitStmts_;

    // P14.3.2: 循环栈 - 支持嵌套Exit For/Exit Do跳转到正确层
    struct LoopInfo {
        ExitKind kind;          // For 或 Do
        std::string exitLabel;  // 跳出标签 (如 "vb6_loop_exit_0")
    };
    std::vector<LoopInfo> loopStack_;

    // 当前过程的已知数组变量名集合 (小写)
    // 用于IndexOrCallExpr中区分数组访问(vs函数调用)
    std::unordered_set<std::string> knownArrays_;
    // 数组名 → 元素Vb6Type (小写key)
    std::unordered_map<std::string, Vb6Type> arrayElemTypes_;
    // P8.1: 数组名 → 维度数 (小写key, 1=一维1D, 2+=多维ND)
    std::unordered_map<std::string, int> arrayDimCounts_;
    // Fix 055: 数组名 → UDT元素C类型 (小写key, e.g. "uvectors" → "vb6_type_RECT")
    // 当数组元素类型为 UserDefinedType 时, 记录实际 UDT C 类型名,
    // 让 VB6_SA_AT / VB6_SA_ND_AT2 使用正确的 struct 类型而非 vb6_VARIANT
    std::unordered_map<std::string, std::string> arrayUdtElemTypes_;

    // Bug #1 fix (082h): 过程内已确认的ND数组名集合 (小写)
    // 当 UBound(arr, N>1) 触发ND版本时将arr注册到此集合,
    // 后续 UBound(arr, 1) 检查此集合也使用ND版本
    std::unordered_set<std::string> knownNDArraysInProc_;

    // 已知BSTR变量名集合 (小写) - 用于Debug.Print等场景判断表达式类型
    std::unordered_set<std::string> knownBstrVars_;

    // 已知double变量名集合 (小写) - 用于Debug.Print区分整数/浮点输出
    std::unordered_set<std::string> knownDoubleVars_;

    // 已知Long/Integer/Boolean变量名集合 (小写) - 用于COM值解封类型推断
    std::unordered_set<std::string> knownLongVars_;

    // Bug #2 fix: 已知LongPtr/intptr_t变量名集合 (小写) - 用于直接比较而非VarCmpLong
    std::unordered_set<std::string> knownLongPtrVars_;

    // P8.4: 已知Variant变量名集合 (小写) - 用于赋值时包装值
    std::unordered_set<std::string> knownVariantVars_;

    // Fix 062: 已知Byte数组变量名集合 (小写) - 用于Variant→SafeArray1D*转换
    std::unordered_set<std::string> knownByteArrayVars_;


    // P6.11: 类模块成员变量类型集合 (小写, 含m_前缀格式)
    // 在generate()开头从模块声明填充, 每个方法/属性入口的clear()后从此恢复
    // 防止类方法内对me->m_Xxx的BSTR赋值无法识别类型
    std::unordered_set<std::string> classBstrMembers_;
    std::unordered_set<std::string> classLongMembers_;
    std::unordered_set<std::string> classDoubleMembers_;
    // Fix 037: 类 Variant 成员变量集合 (As Variant 字段, 含 m_ 前缀格式).
    // 用于 IndexOrCallExpr 处理 `me->VarField(idx)` 或 `obj.VarField(args)` 模式:
    // Variant 字段可能持有 Collection/SafeArray/Object, 通过 idx 访问时应走
    // vb6_VariantArrayGet(&me->VarField, idx) 而非误把 Variant 当函数调用 (C2064).
    std::unordered_set<std::string> classVariantMembers_;
    // Fix 010r: ALL class member variable names (lowercase, both with/without m_ prefix)
    // Used for me-> prefix detection in Erase/ReDim/Assignment statements
    std::unordered_set<std::string> classMemberVars_;
    // Fix 023: 全局类 void* (外部 COM 类型) 字段表 — 跨 CCodeGen 实例共享.
    // 由 driver.cpp 在 runCodeGeneration 主循环前一次性预扫描所有模块 AST 计算.
    // key: 类名 (如 "cDataBase"), value: 该类结构体中 void* 字段的 lowercase 名集合
    // (包含原名小写 + "m_" 前缀小写两种形式).
    // 在 cgen_expr.cpp knownClassVars_ fallback 路径查询以判断 obj->member 是否
    // 返回 void* COM 指针. 若是, 在 emit 的注释中加入 "voidptr" 标记, 由外层
    // MemberAccessExpr 的链式 COM 检测2 (~line 1477) 识别并切换为 COM dispatch
    // (vb6_ComCall/ComGet*Prop). 例: Db.Rs.EOF (Rs 是 ADODB.Recordset 字段)
    //   内层 Db.Rs → emit "Db->Rs  /* class var .Rs field voidptr */"
    //   外层 .EOF  → 识别 voidptr → vb6_ComGet*Prop(Db->Rs..., L"EOF")
    const std::unordered_map<std::string, std::set<std::string>>* classVoidFieldMap_ = nullptr;
    // Fix 037b: 类模块/窗体模块中 typed (非 void*) 对象字段的类型信息表.
    // key: 类名 (如 "cDataBase"), value: (字段名小写 → VB6类型名)
    //   - 项目类字段 (如 Rows As cCollection) → value="cCollection" (项目类名)
    //   - COM 接口字段 (如 Header As Dictionary) → value="COM:Dictionary" (COM标记)
    // 用于 IndexOrCallExpr 中 obj.typedField(idx) 模式识别 — typedField 不是函数
    // 而是对象数据字段, VB6 语义为调用其默认 Item 属性.
    // 项目类 → 直接调用 vb6_<Type>_prop_get_Item(obj.field, arg);
    // COM 接口 → vb6_ComCall((void*)obj.field, L"Item", args, argc).
    const std::unordered_map<std::string, std::unordered_map<std::string, std::string>>* classTypedFieldMap_ = nullptr;
    // Fix 045: 跨模块 Variant 返回值函数 C 名集合 — 由 driver.cpp 预扫描所有模块构建.
    // 包含所有返回 vb6_VARIANT 的项目函数的 C 名称 (如 "vb6_cAsyncSocket_Znl").
    // 在 cExprIsVariant() 中检查 C 表达式是否调用这些函数, 以正确识别 Variant 值.
    // 解决 cExprIsVariant() 无法检测项目类函数返回 Variant 的核心缺口.
    const std::unordered_set<std::string>* variantReturnFuncs_ = nullptr;
    // Fix 010n: 类模块UDT成员变量 (小写var名 → UDT类型C标识符)
    // 用于在过程开始时恢复 knownUdtVars_ (因clear()会丢失类成员UDT变量)
    std::unordered_map<std::string, std::string> classUdtMembers_;
    // Fix 010n (扩展): 普通模块(.bas/.frm)模块级UDT变量 (小写var名 → UDT类型C标识符)
    // classUdtMembers_ 仅覆盖类模块; 标准模块的模块级UDT变量 (如 Private m_uData As UcsCryptoData)
    // 在过程开头 knownUdtVars_.clear() 后同样会丢失, 导致 With m_uData 被误分类为 COM 对象.
    // 本集合在 generate() 扫描模块声明时填充, 过程开头 clear() 后恢复.
    std::unordered_map<std::string, std::string> moduleUdtMembers_;
    // opt4: 裁剪标准模块.c中未实际引用的跨模块#include
    bool trimIncludes_ = false;
    // opt4: 当前模块实际引用的外部模块(小写), 由 driver 的 AST 收集器填充
    std::unordered_set<std::string> trimModules_;
    // 已知类实例变量名 → 类名映射 (小写var名 → 类名, 如 "me" → "cDialog")
    // 用于方法调用翻译 c.Method → vb6_cls_ClassName_Method(c)
    // Fix 010r-10: 从 unordered_set 改为 unordered_map 以支持类名查找
    std::unordered_map<std::string, std::string> knownClassVars_;

    // Fix 037b: 标记最近一次 IndexOrCallExpr 生成的表达式是否为返回 VARIANT 的
    // 项目类 Item 属性调用 — Set 语句需要将其转换为 void* 对象引用.
    bool lastExprNeedsObjectUnpack_ = false;

    // Fix 010o: 过程局部变量名集合 (小写) — Dim声明的局部变量 + For/ForEach循环变量
    // 用于在IdentifierExpr中避免对局部变量错误添加 me-> 前缀
    std::unordered_set<std::string> knownLocalVars_;
    std::unordered_set<std::string> knownByRefParams_;  // Fix 081g: ByRef params (lowercase)

    // UDT变量名集合 (小写var名 → UDT类型C标识符, 如 "p" → "vb6_type_Point")
    // 用于成员访问时区分"p.X"(结构体字段) vs "Module1.X"(模块变量)
    std::unordered_map<std::string, std::string> knownUdtVars_;
    // Fix 090q: 已 emit 声明、返回 UDT 的函数/方法 (小写函数名 → C 返回类型,
    // 如 "pvtofiletime" → "vb6_type_FILETIME"). emitFunctionDecl 在函数体
    // (含调用点) 之前注册; 供 Declare As Any ByRef 打包判断实参是否为
    // "返回 UDT 的函数调用" — 需用临时 UDT 复合字面量取址传参, 不能强转
    // struct 值 (cZipArchive pvVfsSetEof: SetFileTime ..., pvToFileTime(...)).
    std::unordered_map<std::string, std::string> funcUdtRetCType_;

    // 定长字符串变量 (小写var名 → 长度表达式, 如 "a" → "10")
    // 用于LSet/RSet使用固定长度而非SysStringLen
    std::unordered_map<std::string, std::string> knownFixedStringLen_;

    // M22: Declare A版API函数名集合 (小写) - 需要BSTR->ANSI转换的ByVal String参数
    // 判断: 函数名或Alias以'A'结尾 (如 GetWindowTextA, Alias "WritePrivateProfileStringA")
    std::unordered_set<std::string> knownDeclareAnsi_;

    // M22: ANSI临时变量待释放列表 (变量名) + 计数器
    // 在Declare ANSI函数调用前声明临时char*变量, 调用后立即FreeANSI
    std::vector<std::string> ansiTempsToFree_;
    int ansiCounter_ = 0;
    int vcmpCounter_ = 0;  // P25: Variant比较临时变量计数器

    // 已知COM对象变量名集合 (小写) - 用于后期绑定 obj.Method → vb6_ComCall(obj, L"Method", ...)
    std::unordered_set<std::string> knownObjectVars_;

    // P6.3: 前期绑定COM变量 (小写变量名 → ComClass符号指针)
    // Dim fso As FileSystemObject → knownTypedComVars_["fso"] = &FileSystemObject符号
    // 生成vtable直接调用而非IDispatch后期绑定
    std::unordered_map<std::string, const Symbol*> knownTypedComVars_;

    // P6.3: 已使用的COM接口类型名 (如 "IFileSystem3")
    // 用于在.h文件中生成typedef前向声明, 使 vb6_ComIface_<Name>* 类型可用
    std::unordered_set<std::string> usedComIfaceTypes_;

    // P6.4: 已使用的VB6接口类型名 (如 "IFoo")
    // 用于在.h文件中生成typedef前向声明, 使 vb6_iface_<Name> 类型可用
    std::unordered_set<std::string> usedVb6IfaceTypes_;

    // Fix 010: 已使用的VB6类类型名 (如 "cHttpServerContext")
    // 用于在.h文件中生成typedef前向声明, 使 vb6_cls_<Name>* 类型可用
    // 解决循环#include导致的类型未定义问题 (C2081错误)
    std::unordered_set<std::string> usedClassTypes_;

    // Fix 010: 已使用的VB6 UDT类型名 (如 "TypeLang")
    // 用于在.h文件中生成typedef前向声明, 使 vb6_type_<Name> 类型可用
    std::unordered_set<std::string> usedUdtTypes_;

    // COM后期绑定中间状态 (P6.2)
    // MemberAccessExpr为COM对象设置此字段, IndexOrCallExpr/AssignmentStmt/SetStmt读取后清除
    // 当此字段非空时, lastExpr_中的"值"是对象表达式, comMemberName_是成员名
    std::string comObjExpr_;        // COM对象C表达式 (如 "fso")
    std::string comMemberName_;     // COM成员名 (如 "CreateTextFile")
    bool isComMarker_ = false;      // lastExpr_是否为COM标记

    // Bug #3 fix: 标记lastExpr_是vb6_ComIface_Picture*类型的函数返回值
    // 当函数返回类型为StdPicture/IPictureDisp时设置, 用于Picture属性赋值时
    // 选择vb6_SetControlPictureFromCom而非vb6_SetControlPicture
    bool lastExprIsComPicture_ = false;

    // P6.3: 前期绑定中间状态 (在isComMarker_基础上额外标记)
    bool isEarlyBoundCom_ = false;  // 当前COM标记是否为前期绑定 (vtable直接调用)
    const Symbol* earlyBoundSym_ = nullptr;  // 前期绑定的ComClass符号 (含方法签名)

    // 是否需要 setjmp.h (On Error GoTo label)
    bool needSetjmp_ = false;

    // 多模块项目标志 (影响Public函数命名: vb6_<Module>_<Proc> vs vb6_<Proc>)
    bool isMultiModule_ = false;

    // M22: 外部模块名称集合 (用于跨模块变量解析时判断模块是否已#include)
    std::unordered_set<std::string> externalModules_;

    // M22: 当前IdentifierExpr是否在IndexOrCallExpr的callee位置
    // VB6语义: 同名函数引用 — callee上下文返回函数名(供调用), 其他上下文返回返回值变量
    bool asCallCallee_ = false;

    // Fix 015: 类方法链式调用对象参数传递管道
    // visit(MemberAccessExpr) 在 Fix 015 路径中, 当 asCallCallee_=true (外层是
    // IndexOrCallExpr 或 CallStmt 的 callee context) 时, 不直接 emit "func(wrappedObj)"
    // (那样会让 IndexOrCallExpr 的空参数shortcut 或 CallStmt 的 bare-call 分支错误地
    // 把 callee 当作已完成调用, 跳过 Optional 参数默认值填充), 而是把 wrappedObj 存储
    // 到此字段, lastExpr_ 只返回裸函数名. visit(IndexOrCallExpr) / visit(CallStmt) 在
    // 完成参数处理后从此字段取出 wrappedObj, 作为首个 (this指针) 参数前置.
    // 一旦消费即清空, 防止跨调用泄漏.
    std::string pendingChainObj_;

    // 类模块标志
    bool isClassModule_ = false;
    bool isFormModule_ = false;

    // Fix 010: 类模块变量注册模式 — 只填充tracking set, 不生成变量声明(已在结构体中)
    bool trackOnly_ = false;
    std::string formName_;  // M22-Issue6: 当前窗体模块名 (用于Form Print)

    // P6.4: Implements 接口引用变量 (小写变量名 → 接口名)
    // Dim x As IFoo → knownIfaceVars_["x"] = "IFoo"
    std::unordered_map<std::string, std::string> knownIfaceVars_;

    // VB6 Static Sub/Function标志: 过程内所有局部变量都应生成C static
    bool inStaticProc_ = false;

    // GoSub返回地址计数器和标志 (每个过程独立)
    bool hasGoSub_ = false;
    int gosubReturnCounter_ = 0;

    // P12.3: On Error嵌套支持标志 (每个过程独立)
    bool hasOnError_ = false;

    // P14.1.2: Resume dispatch switch支持 (每个过程独立)
    bool hasResume_ = false;            // 当前过程使用了Resume/Resume Next
    bool inProtectedBlock_ = false;     // 在On Error GoTo和错误处理器标签之间
    int resumePointCounter_ = 0;        // 当前resume点索引
    std::vector<int> dispatchPoints_;   // 已生成的resume点索引列表
    std::string currentErrorHandlerLabel_; // 当前On Error GoTo的错误处理器标签名

    // P6.5: WithEvents变量 (小写变量名 → 源类名)
    // Dim WithEvents obj As ClassName → knownWithEventsVars_["obj"] = "ClassName"
    std::unordered_map<std::string, std::string> knownWithEventsVars_;
    // P16: WithEvents控件变量 (小写变量名 → FrmControlType)
    // Dim WithEvents cmd As CommandButton → knownWithEventsCtrlVars_["cmd"] = CommandButton
    std::unordered_map<std::string, FrmControlType> knownWithEventsCtrlVars_;
    // P16: WithEvents控件变量原始名 (小写 → cIdent原名, 如"cmd" → "cmd")
    std::unordered_map<std::string, std::string> knownWithEventsCtrlOrigNames_;

    // P7.5: 窗体控件名映射 (小写控件名 → FrmControlType)
    // 由emitFormFramework从FrmFormDesc填充，用于识别 ctrl.Property 的控件属性访问
    std::unordered_map<std::string, FrmControlType> knownFormControls_;

    // P7.9: Window control name mapping (lowercase -> original casing for HWND vars)
    std::unordered_map<std::string, std::string> knownFormControlOriginalNames_;

    // P20-36: 菜单项ID映射 (小写菜单名 → menuId, 与emitMenuItem/emitMenuClickDispatch一致)
    std::unordered_map<std::string, int> knownMenuIds_;
    std::string knownMenuFormHwnd_;  // 当前窗体HWND变量名 (如 "vb6_hwnd_Form1")

    // P7.5: 窗体名 (小写)，用于识别 Form.Caption 等窗体自身属性
    std::string knownFormName_;


    // P7.6: 控件数组名集合 (小写控件名 → 是否为数组)
    // 同名控件出现多次时标记为数组, 生成 vb6_CtrlArr 而非 void* HWND
    std::unordered_map<std::string, bool> knownControlArrays_;

    // P7.6: 控件数组名 → 控件ID映射 (控件ID → 数组Index, 用于事件分发)
    // key=小写控件名, value=vector<int>按控件ID顺序对应的Index
    std::unordered_map<std::string, std::vector<int>> controlIdToIndexMap_;

    // P6.6: ActiveX DLL模式
    bool isDll_ = false;                    // 编译为ActiveX DLL
    std::string dllProgId_;                  // DLL的ProgID前缀

    // ---- 类型映射 ----

    // Vb6Type → C类型字符串
    std::string mapType(Vb6Type type) const;

    // Vb6Type → COM vtable 方法参数 C类型字符串 (数组→SAFEARRAY*等)
    std::string mapComType(Vb6Type type) const;

    // IID字符串 {XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX} → GUID结构体初始化字符串
    std::string emitGuidInitializer(const std::string& iidStr) const;

    // TypeRefPtr → C类型字符串 (非const: P6.3收集COM接口类型名)
    std::string mapTypeRef(ASTNode* typeRef);

    // Fix 081e: Declare函数返回类型映射 - Long/LongPtr → intptr_t (x64指针安全)
    std::string mapDeclareType(ASTNode* typeRef);

    // Fix 010b: 尝试将AST表达式常量折叠为int64_t (用于enum成员值)
    // 成功返回true并设置result, 失败返回false
    bool tryEvalConstInt(ASTNode* expr, int64_t& result);

    // VB6默认值 → C表达式
    std::string defaultValue(Vb6Type type) const;

    // ---- 标识符命名 ----

    // VB6标识符 → 安全C标识符 (处理关键字冲突、特殊字符)
    std::string cIdent(const std::string& vb6Name) const;

    // P8.4: 根据表达式类型推断Variant构造函数
    // 例如: 整数字面量→vb6_VariantLong, 浮点→vb6_VariantDouble, BSTR→vb6_VariantString
    std::string wrapVariantValue(ASTNode* valueNode, const std::string& cExpr) const;

    // VB6标识符 → C全局函数名
    // 本模块: vb6_<ProcName>
    // 跨模块(外部): vb6_<ModuleName>_<ProcName>
    std::string cProcName(const std::string& procName, AccessLevel access,
                          const std::string& sourceModule = "") const;

    // ---- 表达式求值 ----

    // 生成C表达式字符串, 存入lastExpr_
    void emitExpr(Expr& expr);

    // ---- 语句生成 ----

    // 生成语句列表
    void emitStmtList(StmtList& stmts, bool emitResumePoints = false);

    // ---- 声明生成 ----

    // 生成模块级声明
    void emitModuleDecl(Decl& decl);

    // 生成函数签名 (不含函数体)
    std::string makeProcSignature(SubDecl& node);
    std::string makeProcSignature(FunctionDecl& node);

    // 生成参数列表
    std::string makeParamList(std::vector<std::unique_ptr<ParameterDecl>>& params, bool isDeclare = false);

    // Fix 084k: 生成单个参数的C类型+名字 ("int32_t x" / "vb6_cls_cWinsock** o"),
    // 与makeParamList逐参数逻辑完全一致, 供事件包装器等复用
    std::string makeParamCType(ParameterDecl* p, bool isDeclare = false);

    // 生成Property签名 (不含函数体)
    std::string makePropertySignature(PropertyDecl& node);

    // 生成类工厂函数 (New/Destroy)
    void emitClassFactory(Module& module);
    void emitInterfaceVtable(Module& module);
    void emitEventSink(Module& module);
    void emitComVtableSinkDecls();  // .h 前向声明
    void emitComVtableSinks();      // .c 实现



    // P7: 生成Win32窗体框架代码 (WndProc + 控件创建 + 消息映射)
    void emitFormFramework(const FrmFormDesc& frmDesc, Module& module);

    // P7.8: 递归生成菜单项 (VB.Menu子项)
    void emitMenuItem(const std::string& parentVar, const FrmControl& menuCtrl, int& menuId);

    // P7.8: 递归生成菜单点击事件派发 (WM_COMMAND中)
    void emitMenuClickDispatch(const FrmControl& menuCtrl, int& menuId);

    // M22: 转义宽C字符串（UTF-8→\xNNNN），用于AppendMenuW等宽字符API
    static std::string escapeWideCString(const std::string& s);

    // M22: 将非BSTR表达式包装为BSTR (用于字符串连接运算)
    std::string wrapToBSTR(const std::string& expr, Expr& node);

    // P7.8: 转义C字符串中的特殊字符
    static std::string escapeCString(const std::string& s);

    // P7.5: 判断控件属性访问 → 返回RTL读取函数名 (如"vb6_GetControlText")
    // 空字符串表示不是已知控件属性
    std::string getControlPropReadFn(FrmControlType ctrlType, const std::string& propName) const;

    // P7.5: 判断控件属性写入 → 返回RTL写入函数名 (如"vb6_SetControlText")
    std::string getControlPropWriteFn(FrmControlType ctrlType, const std::string& propName) const;
    // P20-36: 生成控件属性访问的HWND参数 (Menu控件用GetMenu+menuId)
    std::string makeCtrlHwndArg(const std::string& ctrlNameLower, FrmControlType ctrlType) const;

    // P11.7: 返回控件类型的默认属性名 (如 TextBox->"Text", Label->"Caption")
    // 空字符串表示该控件类型无默认属性
    static const char* getDefaultPropertyName(FrmControlType ctrlType);

    // 生成类方法函数体中的Me引用名
    std::string classMeParam() const;

    // Fix 019: 在事件包装函数体内把 void* handler 转换为类指针类型
    // 用于替代历史上误用的 classMeParam() (那是参数声明, 不能作为函数调用实参)
    std::string classHandlerCast() const;

    // ---- 二元运算符映射 ----
    std::string mapBinaryOp(BinaryOp op) const;

    // ---- 表达式类型推断 ----
    // 推断表达式的Vb6Type（简化版，用于Select Case等需要类型判断的场景）
    Vb6Type inferExprType(Expr& expr) const;

    // Fix 029: 严格 Variant 推断 — 仅当表达式"明确"为 Variant/Variant 数组时返回 true.
    // 与 inferExprType 不同: inferExprType 对内置函数/Udt 字段访问等无法识别的情况
    // 会回退到 Variant (默认值), 此处不认为那些是 Variant, 避免误用 vb6_VariantToXxx 包装.
    // isArrOut: 若不为 nullptr, 用于返回是否为 Variant() 数组类型.
    bool isDefinitelyVariantExpr(Expr& expr, bool* isArrOut = nullptr) const;

    // Fix 038b: 基于 C 表达式字符串的 Variant 检测 — 补充 isDefinitelyVariantExpr
    // 的 AST 级检测. 当 codegen 生成的 C 表达式包含已知返回 vb6_VARIANT 的函数调用
    // (如 vb6_VariantArrayGet, vb6_VariantFromComResult 等) 时, 判定为 Variant.
    // 仅检查顶层表达式 (去除前导括号后), 避免对子表达式误判.
    bool cExprIsVariant(const std::string& cExpr);

    // Fix 038b: 运行时函数参数 C 类型查找表 — 当 calleeParams 为空 (运行时/内置函数)
    // 时, 通过函数名和参数索引查找期望的 C 类型. 返回空字符串表示未知.
    // 用于在函数调用参数生成时插入类型转换 (VARIANT→具体, 具体→VARIANT).
    static std::string getRuntimeParamCType(const std::string& funcName, size_t paramIdx);

    // ---- AST辅助 ----
    // 检测语句列表中是否包含GoSubStmt
    bool hasGoSubInStmts(StmtList& stmts) const;
    // P12.3: 检测语句列表中是否包含OnErrorStmt
    bool hasOnErrorInStmts(StmtList& stmts) const;
    // P14.1.2: 检测语句列表中是否包含Resume/Resume Next
    bool hasResumeInStmts(StmtList& stmts) const;
    // Bug #1 fix (082h): 预扫描语句中的UBound/LBound(arr,N>1)收集ND数组名
    void scanNDArraysInStmts(StmtList& stmts);
    void scanNDArraysInExpr(Expr& expr);

    // Fix 086: 局部声明过程级作用域提升
    // VB6 的 Dim/Const 是过程级作用域 (块内 Dim 在块外仍可见), 而 C 块作用域
    // 会造成 "分支内 Dim, 另一分支使用" 时 C2065 未声明标识符. 过程体发射前
    // 预扫描收集所有可提升的 LocalDeclStmt (非数组标量/动态数组指针/Const),
    // 在函数序言处统一声明, 原位置的 visit 跳过.
    void hoistLocalDecls(StmtList& body);
    void collectLocalDeclStmts(StmtList& stmts, std::vector<LocalDeclStmt*>& out);
    // 已提升的声明节点集合 (按指针, 每过程清空)
    std::set<const ASTNode*> hoistedLocalDeclSet_;
    // Fix 086: 工程内窗体模块名集合 (小写), driver预扫描填充
    std::unordered_set<std::string> knownFormModuleNames_;
    // Fix 086: For方向拆分的循环体副本索引 (0=首份, 1=第二份); Label/GoTo配对加后缀
    int labelCopyIdx_ = 0;
    // Fix 090o: For方向拆分期间各被拆 body 内定义的标签名栈 (嵌套层)。
    // GoTo 目标若定义在(某个被拆的) For body 内 → 第二份副本 goto 须加 _dN 后缀与
    // 副本内 LabelStmt 配对; 若目标在 body 外 (函数级出口标签如 QH/EH/ErrHandler,
    // VB6: GoTo 跳出循环到过程尾) 只在过程尾定义一份 → 盲目加后缀会 C2094 标签未定义。
    std::vector<std::unordered_set<std::string>> forSplitLabelStack_;
    void collectForBodyLabels(const StmtList& stmts, std::unordered_set<std::string>& out);
    // Fix 090m/090p: ReDim/Erase 目标是 Variant 数组判定 (顶层 As Variant 变量 /
    // UDT 的 As Variant 字段如 (*uFile).BufferArray)
    bool isVariantArrayTarget(const std::string& name);
    // Fix 086: 各模块 Public 常量表 (driver预扫描填充, 本体归driver所有)
    const std::unordered_map<std::string,
        std::unordered_map<std::string, long long>>* modulePublicConsts_ = nullptr;

    // ---- COM辅助 (P6.2) ----
    // 推断COM参数的封装函数: 根据表达式类型选择vb6_ComPackBSTR/Int/Double/Object
    std::string comPackExpr(Expr& expr);

    // P25: 解析COM标记为类型化属性取值, 用于COM调用参数打包
    // 当isComMarker_为true时调用, 根据packFnHint选择ComGetObjectProp/GetIntProp/GetStringProp等
    // packFnHint: comPackExpr返回的封装函数名, 用于推断所需属性类型
    std::string resolveComMarkerForPack(const std::string& packFnHint);

    // 解析COM标记为C值表达式 (属性读取语义)
    // 当isComMarker_为true时调用, 生成vb6_ComGetProp+Unpack, 并清除标记
    // unresolvedType: 期望的解封类型, 默认为BSTR (最通用)
    std::string resolveComValue(const std::string& unresolvedType = "BSTR");

    // Fix 010r-16: COM/Property-Get 左值重写辅助
    // 当赋值语句(Let/Set/Assignment fallback)的 LHS 是非常量 C 表达式(非左值)时,
    // 尝试重写为 COM SetProp/SetPropArg 或 Property Let/Set 调用.
    // 参数:
    //   target  - LHS 的 C 表达式(已被 emitExpr 产生)
    //   value   - RHS 的 C 表达式(已被 emitExpr 产生, 已完成 COM 解封)
    //   valueExpr - RHS 的 AST 节点(用于 comPackExpr 推断封装函数)
    //   isSet   - true 表示 Set 语句(对象引用语义), false 表示 Let/Assignment
    // 返回值: 若成功重写并已 emit, 返回 true; 否则返回 false(让调用者继续 fallback)
    bool tryRewriteCOMLvalue(const std::string& target, const std::string& value,
                             Expr* valueExpr, bool isSet);

    // Fix 011r-1: 类实例成员调用解析辅助
    // 给定类名与成员名, 在当前作用域的模块级符号表中查找属于该类的方法/属性符号,
    // 返回构造的 C 函数名:
    //   - Sub/Function     → vb6_<className>_<memberName>
    //   - Property Get     → vb6_<className>_prop_get_<memberName>  (读上下文优先)
    //   - Property Let     → vb6_<className>_prop_let_<memberName>  (仅当没有 Get 时返回)
    //   - Property Set     → vb6_<className>_prop_set_<memberName>  (仅当没有 Get/Let 时返回)
    // 匹配条件 (跨模块): sym->isExternal && sym->sourceModule == className
    // 匹配条件 (同模块类): !sym->isExternal && isClassModule_ && moduleName_ == className  (Fix 013: moduleName_ = VB_Name)
    // 返回空串表示该类中无对应方法/属性, 调用者应视为数据字段访问 (obj->member)
    std::string resolveClassMemberCall(const std::string& className,
                                       const std::string& memberName) const;

    // Fix 033: 类感知方法/属性参数查找 (类符号 + memberParams 表的精确解析).
    // IndexOrCallExpr calleeParams 解析: 按 className 查找指定类的方法/属性参数表,
    // 避免跨模块 storageKey 冲突命中的首个注册者导致 Optional _has_ 标志数错 (→ C2197).
    // Phase A: 与 resolveClassMemberCall 同迭代规则 (匹配 sourceModule 或 同模块
    //          moduleName_), 在 moduleScope 找到真实 Sub/Function/Property 符号,
    //          返回其 params + isBuiltin. 命中即返回 true.
    // Phase B: Phase A 失败时 (storageKey 冲突让消费模块的 external Create 符号的
    //          sourceModule 不匹配 className), 退到 Class 符号自身的 memberParams[lower]
    //          (语义分析阶段已按 Get > Function > Sub > Let > Set 优先级填充), 返回该
    //          参数表 (isBuiltin=false). 命中即返回 true.
    // 都未命中返回 false, 调用者回退到 class-unaware lookupModule.
    bool findClassMemberCallParams(const std::string& className,
                                   const std::string& memberName,
                                   std::vector<ParameterInfo>& outParams,
                                   bool& outIsBuiltin) const;

    // ---- Fix 015: Method chaining 解析辅助 ----
    // 给定一个表达式 AST 节点, 推断其在运行时返回的类名 (如果它返回类实例)
    //  - IdentifierExpr: 查 knownClassVars_, 找到则返回该变量的声明类名
    //  - IndexOrCallExpr: 递归推断 callee.object 的类名, 再用 getClassMethodReturnType 找方法的返回类名
    //  - 其他: 返回空串 (不可推断为类实例)
    // 用于链式调用 db.Sql(s).Exec(...) 中 .Exec 的对象表达式 (db.Sql(s)) 类型推断
    std::string inferClassTypeOfExpr(const ASTNode& expr) const;

    // ---- Fix 037: UDT 类型推断辅助 ----
    // 给定一个表达式 AST 节点, 递归推断其 UDT C 类型标识符 (如 "vb6_type_UcsBuffer").
    //  - IdentifierExpr: 查 knownUdtVars_ 得到 UDT C 类型
    //  - MemberAccessExpr: 递归推断 object 的 UDT 类型, 再在 udtMembers 中找
    //    memberName 对应成员. 若该成员本身是 UDT (typeRefName 非空且符号表中有
    //    UserDefinedType), 返回 "vb6_type_<memberUdtName>"; 否则返回空串.
    //    支持嵌套 UDT: _vb6_with_3.DecrBuffer → 推断 DecrBuffer 的 UDT 类型.
    //  - 其他: 返回空串
    // 用于 IndexOrCallExpr 中 obj.udtArrayField(idx) 模式识别, 避免把 UDT 数组
    // 字段误当函数调用 (C2064).
    std::string inferUdtTypeOfExpr(const ASTNode& expr) const;

    // ---- Fix 085: UDT 对象字段类型推断 ----
    // 给定 UDT C 类型标识符 (如 "vb6_type_tZipFileItem") 与字段名(小写), 返回该字段
    // 的对象类别 C 类型:
    //   - ""                          → 非对象字段 (标量/字符串/数组等)
    //   - "vb6_type_Y"                → 嵌套 UDT 字段 (非对象, 供链式推断)
    //   - "vb6_cls_X*"                → 项目类对象字段 (X=类名, 走类方法调用)
    //   - "void*"                     → Collection/COM/接口 对象字段 (走 COM dispatch)
    // 依据语义阶段 TypeDecl 在 udtMembers 中登记的 type/typeRefName.
    std::string udtFieldObjCType(const std::string& udtCType,
                                 const std::string& memberLower) const;

    // Fix 085: 在 UDT 字段访问拼接处追加对象字段注释标记 (供外层 MemberAccessExpr
    // 消费): obj.field 追加 "  /* udt objfield <CType> */"; 非对象字段原样返回.
    // objExpr 为已生成的左值表达式, udtCType/member 用于判定字段类别.
    std::string appendUdtObjFieldMarker(const std::string& objExpr,
                                        const std::string& udtCType,
                                        const std::string& member,
                                        const std::string& accessOp = ".") const;

    // Fix 084n: 推断 target 是否为 UDT 字段链, 是则返回该字段的 Vb6Type (含 Array 标志), 否则 Unknown.
    // 供赋值语句判断 Variant RHS 需转换的目标类型 (如 cZipArchive 的 .FileName As String
    // ← vb6_VariantArrayGet → vb6_VariantToString; uBuf.MaxMatch As Long ← At() → vb6_VariantToLong)
    Vb6Type inferUdtFieldVb6Type(const ASTNode* target) const;

    // Fix 084o: 若 C 表达式为 Variant (字符串级检测或已知 Variant 变量), 返回
    // vb6_VariantToLong(...) 包装, 否则原样返回. 用于需要 int32_t 的上下文:
    // 整除运算符 \ 的操作数、vb6_VariantArrayGet/GetVal 的索引、For 循环 Variant
    // 控制变量的比较/步进等. astExpr 可为 nullptr.
    std::string toLongIfVariant(const std::string& cExpr, const Expr* astExpr);

    // 给定类名与成员名, 在当前模块作用域符号表中查找属于该类的 Function/PropertyGet 符号,
    // 若其返回类型为 Object 且记录了 variableTypeName (Fix 015), 返回经 canonicalClassName
    // 规范化后的类名; 否则返回空串. 用于推断方法返回值的类类型.
    std::string getClassMethodReturnType(const std::string& className,
                                         const std::string& memberName) const;

    // 将可能存在大小写差异的类型名 (来自源码 variableTypeName) 规范化为符号表中
    // Class 符号记录的标准名称 (clsSym->name 或 sourceModule), 与 struct 定义
    // vb6_cls_<canonicalName> 大小写一致. 找不到时原样返回.
    std::string canonicalClassName(const std::string& typeName) const;

    // ---- 数组辅助 ----
    // VB6类型 → SAFEARRAY元素类型C枚举名
    std::string mapSaElemType(Vb6Type type) const;
    // VB6类型 → SAFEARRAY元素C类型 (如int32_t)
    std::string mapSaElemCType(Vb6Type type) const;
    // 从ArrayTypeRef或asType获取元素Vb6Type
    Vb6Type resolveArrayElemType(ASTNode* typeRef) const;
    // Fix 055: 从ArrayTypeRef获取UDT元素C类型名 (如"vb6_type_RECT"), 非UDT返回空串
    std::string resolveArrayUdtElemCType(ASTNode* typeRef) const;
};

} // namespace vb6c3