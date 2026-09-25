// driver_staticlib.cpp — 静态库引用校验 (ai/024, 批次 T01)
//
// 职责单一: 校验工程里**全部**静态链接引用, 全是编译期诊断, **不发一个字节的码**
// (发码是 T02)。两类引用, 走同一条寻址路径 (LibSearchPaths):
//
//   A) `Declare ... Lib "xxx.lib"`  —— 每个 Declare 自带的那一个库
//   B) vbp `ExtraLib=xxx.lib` / CLI `--extra-lib xxx.lib` —— 没有对应 Declare 的
//      纯链接依赖 (典型: 静态库之间互引)
//
// 每个引用做三件事:
//   1) 寻址      — 真找到那个归档文件没有 (找不到就报"搜过哪些根")
//   2) 格式匹配  — 后端吃不吃这个后缀 (§六-1)
//   3) 参数形态  — x86 下 ByVal Variant 会栈失衡 (§六-8, 仅 A 类)
//
// A 与 B 的**失败策略刻意不同**:
//   A: 找不到 = 错误。Declare 的 Lib 指的是工程里一个具体文件。
//   B: 裸 `.lib` 名找不到 = 放行给链接器 (链接器还会按 LIB 环境变量找, 而
//      `ws2_32.lib` 这类系统导入库本来就不在工程目录里)。但"按路径找不到"、
//      或"找的是 .obj/.o"(目标文件不会出现在 SDK 的 LIB 目录) = 错误。
//
// 动态形态 (裸名 / .dll) 在这里**一路 continue**, 保证 E3 护栏: 动态路径的字节
// 输出与基线全同 —— 本文件的存在不改变任何既有行为。

#include "driver/driver.hpp"
#include "driver/coff_archive.hpp"
#include "common/diagnostics.hpp"
#include "common/encoding.hpp"
#include "ast/ast.hpp"

#include <algorithm>
#include <iostream>

namespace vb6c3 {

namespace {

std::string lowerAscii(std::string s) {
    for (char& c : s) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    return s;
}

// 该参数在 x86 下是否是"按值传 Variant"
// (§六-8: `vb6_VARIANT` x86 = 24 字节, Win32 `VARIANT` x86 = 16 字节 →
//  按值传参压 24 而 callee 只消费 16 → 栈失衡。ByRef 与 x64 都安全。)
bool isByValVariant(const ParameterDecl& p) {
    if (!p.isByVal) return false;               // ByRef → 指针, 安全
    if (!p.asType) return true;                 // 无 `As` 子句 = Variant (VB6 语义)
    if (p.asType->kind != ASTNodeKind::SimpleTypeRef) return false;  // 数组/定长串
    return lowerAscii(static_cast<const SimpleTypeRef&>(*p.asType).name) == "variant";
}

// 串里有没有目录分隔符 (决定"路径"还是"裸名")
bool hasSeparator(const std::string& s) {
    return s.find('/') != std::string::npos || s.find('\\') != std::string::npos;
}

std::string buildNotFoundMsg(const std::string& what, const std::string& raw,
                             const LibResolution& res) {
    std::string msg = what + " '" + raw + "' not found";
    if (!res.searchedRoots.empty()) {
        msg += "  (searched:";
        for (const auto& r : res.searchedRoots) msg += " " + r + ";";
        msg += "  -- add a LibDir= entry or pass --libdir)";
    } else {
        msg += "  (no search roots configured; add LibDir= or pass --libdir)";
    }
    return msg;
}

bool equalNoCase(const std::string& a, const std::string& b) {
    return lowerAscii(a) == lowerAscii(b);
}

// L2 (§五): 链接器将引用的确切符号名。空串 = 形态不确定, 不做符号比对。
//   x64            → 无修饰 (C 源里的标识符就是链接符号)
//   x86 __cdecl    → "_name"
//   x86 __stdcall  → "_name@N", N = 参数栈字节数总和
// 拿不准的形态 (Variant / UDT / 数组 / Optional / ParamArray / 序号导出)
// 一律返回空串 —— 诊断绝不能有假阳性。
// ai/024 E4 (T04b): Alias 字面含 '@' 的形态不再跳过 —— 用户字面写的就是链接符号
// (落点已实测: /alternatename 桥接, link.exe 14.29 → RUN_EXIT=24), 直接拿字面值
// 当引用名参与归档比对: 精确命中静默, 库里有近似形态 (差个 @N) 则报 5008 指出
// 真名, 这正是逃生舱最需要的诊断。
std::string referenceSymbol(const DeclareDecl& d, bool isX86) {
    std::string exportName = libStripQuotes(d.aliasName);
    if (exportName.empty()) exportName = d.name;
    if (exportName.empty()) return "";
    if (exportName[0] == '#') return "";                       // 序号导出
    if (exportName.find('@') != std::string::npos) return exportName;  // E4: 字面即符号

    if (!isX86) return exportName;  // x64: 无修饰

    // x86 __stdcall: 逐参累加栈字节数。整数/指针/字符串参数都占 4 字节栈槽
    // (int16_t 也提升到 4); Double/Currency 占 8。
    if (d.callingConv == CallConv::CDecl) return "_" + exportName;  // cdecl 无 @N
    long bytes = 0;
    for (const auto& p : d.params) {
        if (!p || p->isOptional || p->isParamArray) return "";
        if (!p->isByVal) { bytes += 4; continue; }  // ByRef = 4 字节指针
        if (!p->asType || p->asType->kind != ASTNodeKind::SimpleTypeRef) return "";
        const std::string t =
            lowerAscii(static_cast<const SimpleTypeRef&>(*p->asType).name);
        if (t == "byte" || t == "boolean" || t == "integer" || t == "long" ||
            t == "single" || t == "string") {
            bytes += 4;
        } else if (t == "double" || t == "currency") {
            bytes += 8;
        } else {
            return "";  // Variant / UDT / 对象 / 别名 ... 不猜
        }
    }
    return "_" + exportName + "@" + std::to_string(bytes);
}

// ai/024 E4 (批次 T04b): 为 `Alias "_foo@12"` 这类字面含 '@' 的静态 Declare 生成
// /alternatename 指令体 ("<internal>=<real>", 无 directive 前缀; 空串 = 不加)。
//
// 背景 (§四 E4 补充 / SR10): '@' 不是合法 C 标识符, 发码侧 (cgen_decl_api.cpp)
// 只能清洗成 '_' 后再走平常的 extern —— 但清洗后的修饰名与归档里的真实符号对不上,
// 之前实测 LNK2019 `_vb6__sl_mul3_12@12`。落点实验 (link.exe 14.29.30159, x86):
//   #pragma/link /alternatename:__c3alias_mul3@12=_sl_mul3@12  → 链接通过, 运行得 24
// 结论: pragma 指令与命令行 /alternatename 都行, 但**必须写完整修饰名**
// (x86 stdcall = '_' + 标识符 + "@N"; 实验里少写前导 '_' 就 LNK2019)。
//
// 因此本函数必须**逐字镜像发码侧的两件事**:
//   1) cgen_decl_api.cpp 的 sanitizer: 非字母数字/下划线 → '_', 清洗结果以
//      数字或 '_' 开头 → 前缀 "vb6_" (实测 `_sl_mul3@12` → `vb6__sl_mul3_12`);
//   2) extern 的调用约定: x86 stdcall 的 @N 与 referenceSymbol 同一套口径,
//      CDecl 是 "_名", x64 无修饰。
// N 算不出 (Variant/UDT/Optional...) → 返回空串不加, 保持原生 LNK2019
// (与"逃生舱保留字面语义"一致, 也绝不让链接输入出错)。
std::string alternatenameDirective(const DeclareDecl& d, bool isX86) {
    const std::string exportName = libStripQuotes(d.aliasName);
    if (exportName.empty()) return "";
    if (exportName.find('@') == std::string::npos) return "";  // 不需要桥接
    if (exportName[0] == '#') return "";                       // 序号导出不碰

    // 镜像 1): 发码侧 sanitizer
    std::string san;
    for (const char c : exportName) {
        const bool ok = (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') ||
                        (c >= 'a' && c <= 'z') || c == '_';
        san.push_back(ok ? c : '_');
    }
    if (!san.empty() && (san[0] == '_' || (san[0] >= '0' && san[0] <= '9')))
        san = "vb6_" + san;

    // 镜像 2): extern 的链接期修饰名
    std::string internal;
    if (!isX86) {
        internal = san;  // x64: 无修饰
    } else if (d.callingConv == CallConv::CDecl) {
        internal = "_" + san;
    } else {
        // x86 __stdcall: 与 referenceSymbol 同一套参栈字节口径
        long bytes = 0;
        for (const auto& p : d.params) {
            if (!p || p->isOptional || p->isParamArray) return "";
            if (!p->isByVal) { bytes += 4; continue; }
            if (!p->asType || p->asType->kind != ASTNodeKind::SimpleTypeRef) return "";
            const std::string t =
                lowerAscii(static_cast<const SimpleTypeRef&>(*p->asType).name);
            if (t == "byte" || t == "boolean" || t == "integer" || t == "long" ||
                t == "single" || t == "string") {
                bytes += 4;
            } else if (t == "double" || t == "currency") {
                bytes += 8;
            } else {
                return "";  // 不猜 → 不加, 原生诊断兜底
            }
        }
        internal = "_" + san + "@" + std::to_string(bytes);
    }
    return internal + "=" + exportName;
}

// L2 主比对 (§五): 对已寻址成功的 .lib 做归一化符号匹配。
//   唯一命中且与引用名一致 → 静默;
//   多个候选且无一精确等于引用名 → 报多义并列出候选 (5007);
//   唯一命中但与引用名不同 → 报出库里实际有的名字 (5008, 验收正例);
//   零命中 → 原样交链接器 (保留原生错误)。
// .obj 单目标文件不比 (符号表命名口径有出入, 只对 link.exe 消费的 linker
// member 索引做精确比对)。解析失败静默跳过 (SR4: L2 是诊断不是前提)。
void checkArchiveSymbols(const DeclareDecl& d, const std::string& rawLib,
                         const std::string& absPath, bool isX86,
                         bool verbose, Diagnostics* diag, bool* hadError) {
    const std::string ref = referenceSymbol(d, isX86);
    if (ref.empty()) return;
    if (libLowerExt(rawLib) != ".lib") return;  // .obj 不比 (见上)

    const CoffSymbols syms = readCoffSymbols(absPath);
    if (!syms.ok) return;  // SR4: 静默

    const std::string norm = normalizeCoffSymbol(ref);
    std::vector<std::string> hits;
    for (const auto& s : syms.names) {
        if (normalizeCoffSymbol(s) == norm) hits.push_back(s);
    }
    if (hits.empty()) return;  // 零命中 → 交链接器

    bool exact = false;
    for (const auto& h : hits) {
        if (equalNoCase(h, ref)) { exact = true; break; }
    }
    if (exact) return;  // 引用名就在库里 → 一切正常

    std::string list;
    for (const auto& h : hits) list += " '" + h + "'";

    if (hits.size() > 1) {
        diag->error(DiagnosticID::LinkStaticLibAmbiguous, d.loc,
                    "static Declare '" + d.name + "': " + std::to_string(hits.size()) +
                    " archive members match '" + ref + "' in '" + absPath + "': " + list +
                    " -- rename one of them so the linker can pick");
    } else {
        diag->error(DiagnosticID::LinkStaticLibSymbol, d.loc,
                    "static Declare '" + d.name + "': linker will reference '" + ref +
                    "' but archive '" + absPath + "' provides '" + hits[0] +
                    "' -- check the C prototype (parameter count/types must match; "
                    "__stdcall decorates the total byte count)");
    }
    if (verbose) {
        std::cout << "C3: archive '" << absPath << "' symbols: " << syms.names.size()
                  << ", normalized candidates for '" << ref << "':" << list << std::endl;
    }
    *hadError = true;
}

} // namespace

bool Driver::validateStaticLibRefs(const CompileOptions& options) {
    // 后端判定: v1 只有 MSVC 走通 (T06 才做 MinGW 的 .a 归档), 而当前所有工程都经
    // MSVC 工具链, 故恒为 Msvc。MinGW 分支与 --target 的联动留给 T06。
    const LibBackend backend = LibBackend::Msvc;
    const bool isX86 = (options.arch == "x86");

    bool anyStatic = false;
    bool hadError = false;

    // 寻址 + 格式匹配。返回 0=失败(已报错) / 1=成功(outAbs 给出绝对路径) /
    // 2=不适用 (动态串, 或裸 .lib/.a 名找不到而放行给链接器 —— 此时 outAbs 给出裸名)
    auto resolveOne = [&](const std::string& raw, const SourceLocation& loc,
                          const std::string& what, bool allowLinkerFallback,
                          std::string* outAbs) -> int {
        if (classifyLib(raw) == LibKind::Dynamic) return 2;

        const LibResolution res = staticLibPaths_.resolve(raw, backend);
        if (res.rejected) {
            diag_->error(DiagnosticID::LinkStaticLibFormat, loc,
                         what + " '" + raw + "': " + res.reason);
            return 0;
        }
        if (!res.ok) {
            const std::string ext = libLowerExt(raw);
            const bool linkerMayFind = allowLinkerFallback && !hasSeparator(raw) &&
                                       (ext == ".lib" || ext == ".a");
            if (linkerMayFind) {
                if (options.verbose) {
                    std::cout << "C3: " << what << " '" << raw
                              << "' not in project -- left to the linker" << std::endl;
                }
                // 放行: 把**裸名**当链接输入交给 link.exe (它还会按 LIB 环境变量找;
                // `ws2_32.lib` 这类系统导入库本来就不在工程目录里)。
                if (outAbs) *outAbs = raw;
                return 2;
            }
            diag_->error(DiagnosticID::LinkStaticLibPath, loc,
                         buildNotFoundMsg(what, raw, res));
            return 0;
        }
        if (outAbs) *outAbs = res.absPath;
        if (options.verbose) {
            std::cout << "C3: " << what << " '" << raw << "' -> " << res.absPath << std::endl;
        }
        return 1;
    };

    // === A) Declare 的 Lib 串 ===
    for (const auto& module : modules_) {
        if (!module) continue;

        for (const auto& decl : module->declarations) {
            if (!decl || decl->kind != ASTNodeKind::DeclareDecl) continue;
            auto& d = static_cast<DeclareDecl&>(*decl);

            const std::string raw = libStripQuotes(d.libName);
            if (classifyLib(raw) == LibKind::Dynamic) {
                continue;   // ← 动态路径: 今天的行为, 一个字节都不碰
            }
            anyStatic = true;

            std::string abs;
            const int r = resolveOne(raw, d.loc, "static library", /*allowLinkerFallback=*/false, &abs);
            if (r == 0) {
                hadError = true;
            } else if (r == 1) {
                // Declare 的 Lib 找不到就是错误 (allowLinkerFallback=false), 所以这里
                // 只会拿到绝对路径 → 记下来当链接输入 (T02)。
                staticLibResolved_[raw] = abs;
                // L2 (§五, T04): 查归档符号表, 算错时告诉用户"库里其实有什么"。
                checkArchiveSymbols(d, raw, abs, isX86, options.verbose,
                                    diag_.get(), &hadError);
                // E4 (T04b): Alias 字面含 '@' → 生成 /alternatename 桥接指令,
                // 把发码侧清洗出的内部修饰名桥到真实归档符号 (详见函数注释)。
                const std::string alt = alternatenameDirective(d, isX86);
                if (!alt.empty()) staticLibAlternatenames_.push_back(alt);
            }

            // 参数形态: x86 ByVal Variant
            if (!isX86) continue;
            for (const auto& p : d.params) {
                if (!p || !isByValVariant(*p)) continue;
                diag_->error(DiagnosticID::LinkStaticLibParam, p->loc,
                             "static Declare '" + d.name + "': ByVal Variant parameter '" +
                             p->name + "' is not supported on x86 -- vb6_VARIANT is 24 bytes "
                             "while Win32 VARIANT is 16, so passing it by value unbalances the "
                             "stack; pass it ByRef (or build for x64)");
                hadError = true;
                break;      // 一个函数报一次就够
            }
        }
    }

    // === B) ExtraLib= / --extra-lib (没有对应 Declare 的链接依赖) ===
    // 它是工程级链接配置, 不属于任何一行源码 → 报在 SourceLocation{} (与 vbp 的
    // ComLib= 路径缺失提示同款).
    for (const auto& rawRaw : extraLibRaw_) {
        const std::string raw = libStripQuotes(rawRaw);
        if (raw.empty()) continue;

        if (classifyLib(raw) == LibKind::Dynamic) {
            diag_->error(DiagnosticID::LinkStaticLibFormat, SourceLocation{},
                         "ExtraLib '" + raw + "': only .lib/.obj archives belong here "
                         "(a DLL is not a link input -- declare it instead)");
            hadError = true;
            continue;
        }
        anyStatic = true;
        std::string abs;
        const int r = resolveOne(raw, SourceLocation{}, "ExtraLib", /*allowLinkerFallback=*/true, &abs);
        if (r == 0) {
            hadError = true;
        } else {
            // r=1 绝对路径 / r=2 裸名(放行给链接器) —— 两种都是链接输入, 原样透传。
            // **必须按 extraLibRaw_ 的顺序逐条 push** —— 与 raw 是 1:1 的,
            // 中间 continue 掉的那条 (空串 / 动态形态报错) 不产生输入。
            extraLibResolved_.push_back(abs);
        }
    }

    if (options.verbose && anyStatic) {
        std::cout << "C3: static library references: "
                  << staticLibResolved_.size() << " resolved, "
                  << extraLibResolved_.size() << " ExtraLib input(s)" << std::endl;
    }
    return !hadError;
}

} // namespace vb6c3
