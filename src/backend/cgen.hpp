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
             const TypeSystem& typeSys, bool verbose = false);

    // 主入口: 生成C代码，返回是否成功
    // externalModules: 当前模块引用的外部模块基名列表 (用于生成 #include)
    // isDll: P6.6 ActiveX DLL模式, 生成COM服务端代码
    // dllProgId: P6.6 DLL的ProgID前缀
    // frmDesc: P7 窗体描述 (仅.frm有效, nullptr=非窗体模块)
    bool generate(Module& module, const std::string& baseName,
                  const std::unordered_set<std::string>& externalModules = {},
                  bool isDll = false, const std::string& dllProgId = "",
                  const FrmFormDesc* frmDesc = nullptr);

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

    // 已知BSTR变量名集合 (小写) - 用于Debug.Print等场景判断表达式类型
    std::unordered_set<std::string> knownBstrVars_;

    // 已知double变量名集合 (小写) - 用于Debug.Print区分整数/浮点输出
    std::unordered_set<std::string> knownDoubleVars_;

    // 已知Long/Integer/Boolean变量名集合 (小写) - 用于COM值解封类型推断
    std::unordered_set<std::string> knownLongVars_;

    // P8.4: 已知Variant变量名集合 (小写) - 用于赋值时包装值
    std::unordered_set<std::string> knownVariantVars_;


    // P6.11: 类模块成员变量类型集合 (小写, 含m_前缀格式)
    // 在generate()开头从模块声明填充, 每个方法/属性入口的clear()后从此恢复
    // 防止类方法内对me->m_Xxx的BSTR赋值无法识别类型
    std::unordered_set<std::string> classBstrMembers_;
    std::unordered_set<std::string> classLongMembers_;
    std::unordered_set<std::string> classDoubleMembers_;
    // Fix 010r: ALL class member variable names (lowercase, both with/without m_ prefix)
    // Used for me-> prefix detection in Erase/ReDim/Assignment statements
    std::unordered_set<std::string> classMemberVars_;
    // Fix 010n: 类模块UDT成员变量 (小写var名 → UDT类型C标识符)
    // 用于在过程开始时恢复 knownUdtVars_ (因clear()会丢失类成员UDT变量)
    std::unordered_map<std::string, std::string> classUdtMembers_;
    // 已知类实例变量名 → 类名映射 (小写var名 → 类名, 如 "me" → "cDialog")
    // 用于方法调用翻译 c.Method → vb6_cls_ClassName_Method(c)
    // Fix 010r-10: 从 unordered_set 改为 unordered_map 以支持类名查找
    std::unordered_map<std::string, std::string> knownClassVars_;

    // Fix 010o: 过程局部变量名集合 (小写) — Dim声明的局部变量 + For/ForEach循环变量
    // 用于在IdentifierExpr中避免对局部变量错误添加 me-> 前缀
    std::unordered_set<std::string> knownLocalVars_;

    // UDT变量名集合 (小写var名 → UDT类型C标识符, 如 "p" → "vb6_type_Point")
    // 用于成员访问时区分"p.X"(结构体字段) vs "Module1.X"(模块变量)
    std::unordered_map<std::string, std::string> knownUdtVars_;

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
    std::string makeParamList(std::vector<std::unique_ptr<ParameterDecl>>& params);

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

    // ---- 二元运算符映射 ----
    std::string mapBinaryOp(BinaryOp op) const;

    // ---- 表达式类型推断 ----
    // 推断表达式的Vb6Type（简化版，用于Select Case等需要类型判断的场景）
    Vb6Type inferExprType(Expr& expr) const;

    // ---- AST辅助 ----
    // 检测语句列表中是否包含GoSubStmt
    bool hasGoSubInStmts(StmtList& stmts) const;
    // P12.3: 检测语句列表中是否包含OnErrorStmt
    bool hasOnErrorInStmts(StmtList& stmts) const;
    // P14.1.2: 检测语句列表中是否包含Resume/Resume Next
    bool hasResumeInStmts(StmtList& stmts) const;

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

    // ---- Fix 015: Method chaining 解析辅助 ----
    // 给定一个表达式 AST 节点, 推断其在运行时返回的类名 (如果它返回类实例)
    //  - IdentifierExpr: 查 knownClassVars_, 找到则返回该变量的声明类名
    //  - IndexOrCallExpr: 递归推断 callee.object 的类名, 再用 getClassMethodReturnType 找方法的返回类名
    //  - 其他: 返回空串 (不可推断为类实例)
    // 用于链式调用 db.Sql(s).Exec(...) 中 .Exec 的对象表达式 (db.Sql(s)) 类型推断
    std::string inferClassTypeOfExpr(const ASTNode& expr) const;

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
};

} // namespace vb6c3