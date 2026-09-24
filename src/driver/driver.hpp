#pragma once
// 编译器驱动 - 命令行解析 + 编译流程编排

#include "preprocessor/preprocessor.hpp"
#include "ast/ast.hpp"              // 泛型 (G2): Decl/Module AST 类型
#include "semantics/symbol_table.hpp"
#include "semantics/type_system.hpp"
#include "com/typelib_parser.hpp"
#include "project/frm_parser.hpp"
#include "semantics/generics_registry.hpp"
#include "semantics/interfaces_registry.hpp"  // Interface 契约 (tB, B02)
#include "semantics/class_chain_registry.hpp"  // 类继承链 (tB, B07)
#include <array>
#include <string>
#include <vector>
#include <memory>
#include <map>
#include <unordered_map>

namespace vb6c3 {

class Diagnostics;
class SourceBuffer;
class Module;
class SemanticAnalyzer;
class SessionManager;

// 编译选项
struct CompileOptions {
    std::vector<std::string> sourceFiles;   // 源文件列表
    std::string outputFile;                  // 输出文件路径
    std::string outputDir;                   // 输出目录 (默认: output)

    // 目标平台
    std::string target = "win-x64";         // win-x86, win-x64, linux-x64, etc.
    std::string arch = "x64";              // x64 (default) or x86 — output binary architecture

    // GUI模式
    std::string guiMode = "native";          // native, webview, none

    // 输出控制
    bool dumpTokens = false;
    bool dumpAST = false;
    bool dumpIR = false;
    bool dumpPreprocess = false;              // 输出预处理后的token列表
    bool dumpSymbols = false;                 // 输出符号表
    bool emitC = false;                       // 输出C代码 (.h/.c)
    bool emitLLVM = false;                   // 输出.ll文件
    bool syntaxOnly = false;                 // 只做语法检查
    bool verbose = false;

    // 条件编译
    std::vector<std::string> defines;        // -d:NAME=VALUE 或 --define NAME=VALUE

    // 优化
    int optimizationLevel = 0;               // 0=无, 1/2/3

    // 调试
    bool debugInfo = false;

    // 兼容性
    bool compatCheck = false;                // 跨平台兼容性检查

    // COM TypeLib引用 (P6.3, 前期绑定)
    std::vector<std::string> typelibRefs;    // TypeLib路径或ProgID列表
    bool autoTypelib = true;                 // 自动从源码中提取COM类型并加载TypeLib

    // ActiveX DLL (P6.6)
    bool isDll = false;                      // 编译为ActiveX DLL (而非EXE)
    std::string dllProgId;                   // DLL的ProgID前缀 (如 "MyLib")
    std::string libidStr;                    // P6.13: TypeLib的LibID (UUID格式, 空则自动生成)

    // 窗体调试 (P7)
    bool dumpFrm = false;                    // 输出.frm窗体描述解析结果
    bool keepTemps = false;                   // 保留中间文件 (调试用)

    // 警告抑制 (性能优化): 需要静默的诊断ID列表, 如 --no-warn 3001,3003
    std::vector<int> suppressedWarningIds;

    // 增量编译 (性能优化): 基于内容哈希的obj级缓存, 跳过未变化的.c编译
    bool incremental = false;

    // 裁剪include (性能优化): 只include实际引用的外部模块, 降低cl预处理量
    bool trimIncludes = false;
};

// 编译结果
struct CompileResult {
    bool success = false;
    std::string outputFile;
    int errorCount = 0;
    int warningCount = 0;
};

// 编译器驱动
class Driver {
public:
    Driver();
    ~Driver();

    // 从命令行参数编译
    CompileResult compile(int argc, char* argv[]);

    // 从选项编译
    CompileResult compile(const CompileOptions& options);

    // 解析命令行参数
    static std::pair<CompileOptions, int> parseArgs(int argc, char* argv[]);

    // 打印帮助/版本
    static void printHelp();
    static void printVersion();

private:
    std::unique_ptr<Diagnostics> diag_;
    std::vector<std::unique_ptr<Module>> modules_;  // 解析产出的AST

    // 语义分析产出 (供代码生成使用)
    std::vector<std::unique_ptr<SemanticAnalyzer>> analyzers_;

    // 窗体描述 (P7, .frm文件解析结果, 按模块名索引)
    std::map<std::string, FrmFile> frmFiles_;

    // ============================================================
    // 泛型 (tB 扩展, G2) — 单态化前端
    // ============================================================
    // 使用点登记表 (parse 期由 Parser 记录, runParser 合并到这里):
    //   key = 扁名小写 (Foo$gen1$Long), value = {模板基名, 实参扁名表}
    struct GenericUseRec { std::string base; std::vector<std::string> args; };
    std::unordered_map<std::string, GenericUseRec> genericUses_;
    // 模板登记表: lower(模板名) → 模板声明 + 宿主模块 + 类型参数名表
    struct GenericTemplateInfo {
        Module* module = nullptr;
        Decl* decl = nullptr;          // TypeDecl / SubDecl / FunctionDecl / PropertyDecl
        std::vector<std::string> typeParams;
    };
    std::unordered_map<std::string, GenericTemplateInfo> genericTemplates_;
    // 泛型类模板 (G4): lower(类名) → 模板 .cls 模块 (本体不发码, 特化=整模块克隆)
    std::unordered_map<std::string, Module*> genericClassTemplates_;
    // 模板只读视图 (analyzer 推断用; 与 genericTemplates_ 同步构建)
    GenRegistry genView_;
    // Interface 契约 (tB, B02): stage 2.7 建好的只读登记表 (小写接口名 → 展平槽表).
    // 挂 Driver 而非某个符号表: 每模块一张符号表, 而接口名是工程级唯一的.
    IfaceRegistry ifaces_;
    std::vector<std::string> ifaceOrder_;  // 登记序, 保证诊断输出确定性
    // 委托式实现 (tB, B10): stage 2.7 Pass D 建好的只读裁决表 (小写类模块名 → Via 子句).
    // 消费方 = 语义层 (据此免掉逐槽 VB3012) 与发码层 (据此转调持有对象的接口槽).
    ViaRegistry vias_;
    // 类继承 (tB, B07a): stage 2.8 建好的只读链登记表 (小写类名 → 父先己后的链).
    // 同上, 挂 Driver 而非符号表; B07b 的成员合并是唯一消费者.
    ClassChainRegistry classes_;
    std::vector<std::string> classOrder_;  // 登记序, 保证诊断输出确定性
    // 已物化扁名 (fixpoint 去重; 值为 true 即"已注入为普通声明")
    std::unordered_map<std::string, bool> genericMaterialized_;
    // cap 护栏 (计划冻结版): 总量 ≤1024, 单名嵌套深度 ≤16
    static constexpr size_t kGenericMaxInstances = 1024;
    static constexpr size_t kGenericMaxDepth = 16;

    // P6.8: VBP指定的类CLSID映射 (模块名小写 -> CLSID字符串)
    std::unordered_map<std::string, std::string> classClsidMap_;

    // VBP工程基名 (用于多模块工程的输出文件命名)
    std::string projectBaseName_;
    std::string projectPath32_;    // P11.1: VBP Path32 field (output dir)

    // Fix 142: VBP 的 Startup= 启动对象 ("Sub Main" 或窗体模块名).
    // 决定多模块工程中哪个模块生成进程入口点 (WinMain/main).
    std::string startupObject_;

    // Fix 143: vbp Object= 的 OCX 文件表 (CLSID 小写去花括号 → ocx 绝对路径).
    // 第三方 OCX 控件免注册加载用.
    std::map<std::string, std::string> ocxFiles_;

    // Fix 143b: vbp Object= 原始引用列表 (CLSID 小写去花括号, ocx 绝对路径).
    // 控件实例化 CLSID 以此为准 (typelib coclass GUID ≠ 实例 CLSID).
    std::vector<std::pair<std::string, std::string>> ocxRefs_;

    // Fix 160: vbp ComLib= 声明的组件 DLL (canonical 小写绝对路径 → 相对 exe 路径).
    // driver_compile 填, runTypeLibImport 按 TypeLibResult 路径反查:
    // 命中者其全部 coclass 进免注册表, 未命中者 (普通 Reference= / auto-typelib) 不进,
    // 运行期对未声明组件零变化.
    std::unordered_map<std::string, std::string> comLibCanonMap_;

    // Fix 160: ComLib= 组件表 {ProgID, CLSID, coclass名, 相对exe路径},
    // runTypeLibImport 收集, cgen 烘焙进产物入口点 (vb6_ComLibRegister).
    std::vector<std::array<std::string, 4>> comLibRefs_;

    // P23-05: VBP version info (for VS_VERSION_INFO resource)
    int verMajor_ = 1;
    int verMinor_ = 0;
    int verRevision_ = 0;
    std::string verCompanyName_;
    std::string verFileDescription_;
    std::string verLegalCopyright_;
    std::string verProductName_;
    std::string verComments_;
    std::string verLegalTrademarks_;
    std::string verOriginalFileName_;
    std::string verTitle_;
    std::string userResFile_;         // P23-03: VBP ResFile= .res path (absolute)

    // TypeLib解析器 (P6.3, COM类型导入)
    std::unique_ptr<TypeLibParser> typelibParser_;

    // 编译流水线各阶段
    bool runLexer(const CompileOptions& options);
    bool runPreprocess(const CompileOptions& options);
    bool runParser(const CompileOptions& options);
    bool runTypeLibImport(const CompileOptions& options);  // P6.3: 加载TypeLib+注册COM类型
    bool runSemanticAnalysis(const CompileOptions& options);
    bool runGenericsPrepass();  // 泛型 (tB): 模板登记 + 使用点物化 (G2)
    // Interface (tB, B02): stage 2.7 建接口契约登记表 (名字/Extends 链/展平槽表)
    bool runInterfacePrepass();
    // 类继承 (tB, B07a): stage 2.8 建类继承链登记表 (基名解析/环/深度/v1 边界)
    bool runClassChainPrepass();
    // 虚方法 (tB, B08b): 2.8 内两步 —— 位置合法性 (只读 modules_) / 覆盖契约与 dynamicKeys (要链)
    void checkVirtualPlacement();
    void runVirtualContractChecks();
    // 类继承 (tB, B07b): stage 3.4 把祖先自有成员并进派生类 Class 符号 (必须早于 3.5 的逐字段跨模块拷贝)
    bool mergeInheritedMembers();
    // 类虚表 (tB, B08d): stage 3.4b 排每类的有序槽表 (要读 3.4 的 inhProcs 判"本类有无入口")
    bool buildVirtualSlotTables();
    // fixpoint 单轮物化: 消费 genericUses_ 中未物化项; freshOut 收特化副本
    bool materializeGenerics(std::vector<std::pair<Module*, Decl*>>* freshOut);
    // 泛型推断 fixpoint (G3): 收请求→物化→增量分析→再跨模块, 至收敛
    bool runGenericsFixpoint();
    bool runCrossModuleResolution();  // 跨模块符号链接
    bool runCodeGeneration(const CompileOptions& options, const std::string& outputDir);
    void writeErrorLog(const std::string& logPath, const std::string& stage);
    bool runLinker(const CompileOptions& options, const std::string& outputDir, const std::string& intermediatesDir, SessionManager& session);
};

} // namespace vb6c3
