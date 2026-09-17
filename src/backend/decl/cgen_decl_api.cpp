#include "backend/cgen.hpp"
#include <algorithm>
#include <cctype>
#include <iostream>
#include <functional>

namespace vb6c3 {

// --- cgen_decl_api.cpp: 外部 API 声明生成（DeclareDecl） ---


void CCodeGen::visit(DeclareDecl& node) {
    // 外部函数声明 (Declare Sub/Function ... Lib "xxx" [Alias "yyy"] [CDecl])
    // Fix 081e: Declare函数返回Long在x64下应映射为intptr_t
    // VB6 Long (32-bit) 在Declare中常用于返回句柄/指针 (如CreateEnhMetaFileW返回HDC),
    // 在x64下需要intptr_t (8字节) 才能容纳指针值
    std::string retType = (node.procKind == ProcKind::Function)
        ? mapDeclareType(node.returnType.get()) : "void";

    std::string params = makeParamList(node.params, true);

    // 调用约定
    std::string callConv = (node.callingConv == CallConv::CDecl) ? "__cdecl" : "__stdcall";

    // 去除字符串两端引号 (词法器保留引号)
    auto stripQuotes = [](const std::string& s) -> std::string {
        if (s.size() >= 2 && s.front() == '"' && s.back() == '"')
            return s.substr(1, s.size() - 2);
        return s;
    };

    // Lib名: 去引号、去.dll后缀
    std::string libName = stripQuotes(node.libName);
    if (libName.size() > 4 &&
        (libName.compare(libName.size()-4, 4, ".dll") == 0 ||
         libName.compare(libName.size()-4, 4, ".DLL") == 0)) {
        libName = libName.substr(0, libName.size()-4);
    }

    // Alias: 去引号, 保持原始导出名 (大小写敏感, 可能含#序号前缀)
    std::string aliasName = stripQuotes(node.aliasName);

    // VB6函数名→C标识符 (用于调用点)
    std::string cFuncIdent = cIdent(node.name);

    // 导出名: Alias优先, 否则用VB6函数名
    // 注意: Windows API函数名是大小写敏感的, 需要保持原始大小写
    std::string exportedName = aliasName.empty() ? node.name : aliasName;

    // M22: 检测A版Declare函数 (函数名或Alias以'A'结尾)
    // A版API需要BSTR->ANSI转换: vb6_BSTR_ToANSI/vb6_FreeANSI
    bool isAnsiDeclare = false;
    if (!aliasName.empty()) {
        // Alias "SomeFuncA" - check if alias ends with 'A'
        std::string aliasStr = stripQuotes(node.aliasName);
        if (aliasStr.size() >= 2 && aliasStr.back() == 'A' && std::isupper(static_cast<unsigned char>(aliasStr[aliasStr.size()-1]))) {
            // Also check that the char before 'A' is lowercase (to avoid false positives like "Data")
            if (std::isalpha(static_cast<unsigned char>(aliasStr[aliasStr.size()-2])) &&
                std::islower(static_cast<unsigned char>(aliasStr[aliasStr.size()-2]))) {
                isAnsiDeclare = true;
            }
        }
    } else {
        // No Alias - check if function name ends with 'A'
        if (node.name.size() >= 2 && node.name.back() == 'A' &&
            std::islower(static_cast<unsigned char>(node.name[node.name.size()-2]))) {
            isAnsiDeclare = true;
        }
    }
    if (isAnsiDeclare) {
        std::string funcLower = node.name;
        std::transform(funcLower.begin(), funcLower.end(), funcLower.begin(), ::tolower);
        knownDeclareAnsi_.insert(funcLower);
    }

    // Fix 092z-2: VB6/VBA 运行时库 (msvbvm60 等) 不生成 #pragma comment(lib, ...)
    //
    // 原因:
    //  1. 这些库只随 VB6/VBA 发行, Windows SDK / VS 均不携带 → 链接器报 fatal
    //     LNK1104 "无法打开文件 msvbvm60.lib" (本机 SysWOW64 只有 32 位
    //     msvbvm60.dll, x64 无导入库可链); VB6 运行时也只有 32 位。
    //  2. Fix 076 之后 Declare 一律生成 `extern <ret> __stdcall vb6_di_<name>(...)`
    //     + `#define <VB名> vb6_di_<name>`, 由 RTL 转发桩提供实现 —— 导入库这条路
    //     对 Declare 早已废弃。且生成物引用的符号名是 C3 内部名, 即使补上真库也
    //     解析不了 (MSVBVM60.DLL 导出的是 VarPtr / __vbaObjSetAddref / 序号)。
    //  3. 这些符号的实现见 src/rtl/core/vb6_di_stubs.c (原生实现, 不做 LoadLibrary 转发)。
    std::string libLower = libName;
    std::transform(libLower.begin(), libLower.end(), libLower.begin(), ::tolower);
    bool isVb6RuntimeLib = (libLower == "msvbvm60" || libLower == "msvbvm50" ||
                            libLower == "vbe7" || libLower == "vbe6" ||
                            libLower == "vba7" || libLower == "vba6");
    // 另一类: Windows SDK 不提供导入库的 DLL。实测 cryptdlg.dll 只有 DLL,
    // SDK 10.0.26100.0\um\x64 无 cryptdlg.lib → 同样不生成 pragma, 对应符号
    // 由 RTL 动态加载实现 (LoadLibrary + GetProcAddress), 见 vb6_di_stubs.c
    // 的 vb6_di_CertSelectCertificateW。
    bool isNoImportLib = (libLower == "cryptdlg");
    if (!isVb6RuntimeLib && !isNoImportLib) {
        c_.emitLine("#pragma comment(lib, \"" + libName + ".lib\")");
    }

    // Fix 010a: 避免与Windows SDK (windows.h) 声明冲突
    //
    // 问题: vb6rtl.h 已 #include <windows.h>, 即所有Windows API函数已被声明。
    // C3生成的 __declspec(dllimport) 声明与SDK声明签名不同 (如 int32_t vs HANDLE/void*),
    // 导致 C2371 "redefinition; different basic types" 等错误。
    //
    // 解决方案: 使用C3内部唯一名称 vb6_di_<ExportedName> 作为 __declspec(dllimport) 的函数名,
    // 然后用 #define 将VB6函数名映射到该内部名称。
    //
    // 这样:
    // 1. SDK已#define的宏 (如 CopyMemory → RtlMoveMemory → memmove):
    //    #ifndef CopyMemory 为false → 不生成C3的#define → 调用点使用SDK的宏展开 → 正确
    //    (vb6_di_CopyMemory 声明存在但永远不会被调用 → 无害)
    // 2. SDK已声明为函数 (如 GetCurrentProcess):
    //    #ifndef GetCurrentProcess 为true → 生成 #define GetCurrentProcess vb6_di_GetCurrentProcess
    //    → 调用点 GetCurrentProcess() 被宏展开为 vb6_di_GetCurrentProcess() → 使用C3的导入版本
    //    (SDK的 GetCurrentProcess 声明仍在, 但不会被调用 → 无冲突, 因为名字不同)
    // 3. SDK未声明的函数 (如 archive_read_new):
    //    #ifndef 为true → #define 映射生效 → 调用 vb6_di_archive_read_new() → 正确

    // C3内部导入名: 使用导出名构造唯一标识符
    // Fix 010b: 序号导出名 (如 "#644") 含非法C标识符字符, 需清洗
    // '#' → 'ord_', 其他非字母数字/下划线字符 → '_'
    std::string sanitizedExport = exportedName;
    for (size_t i = 0; i < sanitizedExport.size(); i++) {
        char c = sanitizedExport[i];
        if (c == '#') {
            sanitizedExport[i] = '_';
        } else if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_') {
            sanitizedExport[i] = '_';
        }
    }
    // 确保不以数字开头 (合法C标识符要求)
    if (!sanitizedExport.empty() && (std::isdigit(static_cast<unsigned char>(sanitizedExport[0])) || sanitizedExport[0] == '_')) {
        // 对序号导出(#nnn清洗后为_nnn), 加ord前缀使名称更清晰
        if (!exportedName.empty() && exportedName[0] == '#') {
            sanitizedExport = "ord" + sanitizedExport;  // _644 → ord_644
        } else {
            sanitizedExport = "vb6_" + sanitizedExport;
        }
    }
    std::string cExportedIdent = "vb6_di_" + sanitizedExport;

    // Fix 010i: 同一Declare函数可能出现在多个VB6模块中 (如CoTaskMemFree)
    // 用#ifndef guard防止__declspec(dllimport)声明重定义 (C2371)
    std::string diGuard = "VB6_DI_" + sanitizedExport + "_DEFINED";
    h_.emitLine("#ifndef " + diGuard);
    h_.emitLine("#define " + diGuard);
    // Fix 076: Changed from __declspec(dllimport) to extern declaration.
    // __declspec(dllimport) creates import symbols named vb6_di_Xxx that can't be
    // resolved by Windows import libraries (which export the real API names like
    // CloseEnhMetaFile, not vb6_di_CloseEnhMetaFile). Instead, we declare them as
    // extern and provide forwarding stubs in vb6rtl.c that bridge vb6_di_Xxx → real API.
    h_.emitLine("extern " + retType + " " + callConv + " " + cExportedIdent + "(" + params + ");");
    h_.emitLine("#endif");

    // 生成: #define <VB6名> → <内部导入名> (仅当VB6名未被SDK定义为宏时)
    // #ifndef 检查处理两种情况:
    //   - SDK宏 (CopyMemory等): #ifndef为false, 跳过 → 调用使用SDK宏
    //   - SDK函数声明: #ifndef为true, 生成 → 调用重定向到C3导入版本
    //   - 无SDK定义: #ifndef为true, 生成 → 正常
    h_.emitLine("#ifndef " + cFuncIdent);
    h_.emitLine("#define " + cFuncIdent + " " + cExportedIdent);
    h_.emitLine("#endif");
}

} // namespace vb6c3
