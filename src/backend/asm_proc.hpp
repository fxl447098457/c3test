#pragma once
// asm_proc.hpp - ai/vb-asm-extension-spec: Asm 过程降级元数据
//
// codegen 侧收集 (CCodeGen::asmProcs_), driver 侧消费 (driver_link.cpp):
//   把每个「函数体 = 单个 Asm 块」的过程降级为一个独立 MASM 过程 (x64/ml64)。
// 只放纯数据, 不依赖 cgen/driver 任一方向, 两边都可 include。

#include <string>
#include <vector>
#include <utility>
#include <cctype>
#include <cstdlib>

namespace vb6c3 {

struct AsmProcInfo {
    std::string cName;                 // C 符号名 (x64 无调用约定修饰 → 即 MASM PROC 名)
    std::string retCType;              // 返回 C 类型 ("void" 表示 Sub)
    std::vector<std::string> params;   // 逐参 "CType name" (与 makeParamList 同口径)
    std::vector<std::string> lines;    // 原始汇编行 (未做寄存器替换)
    // ai/vb-asm-extension-spec §7: `<Naked>` —— 不生成任何保存/对齐代码, 用户自管 prologue/ret。
    bool naked = false;
    // `Asm Clobber("rbx","memory")` 声明 (小写)。与块内静态扫描出的寄存器取并集,
    // 决定过程要 push/pop 哪些 callee-saved 寄存器。
    std::vector<std::string> clobbers;
};

// ============================================================
// 参数 ABI 分类 (spec §6 ABI 表 / §5 绑定模型)
//   x64 (Win64): 整型/指针走 RCX,RDX,R8,R9 (独立计数); float/double 走 XMM0–3 (独立计数);
//   两类**各自计数**, 第 5 个起压栈 ([rsp+40+8k], 40 = 8 返回地址 + 32 shadow space)。
//   x86 (stdcall/cdecl): 全部右→左压栈, 第 n 个 (0 基) 在 [ebp+8+4n]; 浮点按值也压栈。
//   float (Single) 在栈上是 4 字节, double 8 字节 —— 占位宽度按类型计。
// ============================================================
enum class AsmParamClass { Int, Float, Stack };

inline bool asmIsFloatCType(const std::string& t) { return t == "float" || t == "double"; }

struct AsmParamSlot {
    std::string ctype;          // C 类型
    std::string name;           // 参数名
    AsmParamClass cls = AsmParamClass::Int;
    int regIndex = -1;          // Int: GPR 槽 (0-3); Float: xmm 槽 (0-3); Stack: -1
    int stackOffset = 0;        // Stack: 相对 rsp/ebp 的字节偏移 (x64: 从 rsp; x86: 从 ebp)
};

// 把 "CType name" 解析成槽位表。abi == "x64" / "x86"。
inline std::vector<AsmParamSlot> asmClassifyParams(const std::vector<std::string>& params,
                                                   const std::string& abi) {
    std::vector<AsmParamSlot> out;
    int gpr = 0, xmm = 0, stackBy = 0;
    for (auto& ps : params) {
        size_t sp = ps.find(' ');
        if (sp == std::string::npos) continue;
        AsmParamSlot s;
        s.ctype = ps.substr(0, sp);
        s.name = ps.substr(sp + 1);
        while (!s.name.empty() && s.name.back() == ' ') s.name.pop_back();
        const bool isFloat = asmIsFloatCType(s.ctype);
        const bool isPtr = s.ctype.find('*') != std::string::npos;
        if (abi == "x64") {
            if (isFloat && xmm < 4) {
                s.cls = AsmParamClass::Float; s.regIndex = xmm++;
            } else if (!isFloat && !isPtr && gpr < 4) {
                s.cls = AsmParamClass::Int; s.regIndex = gpr++;
            } else if (!isFloat && isPtr && gpr < 4) {
                s.cls = AsmParamClass::Int; s.regIndex = gpr++;
            } else {
                // 压栈: 8 返回地址 + 32 shadow space = 40; 之后每个栈参占 8 字节槽。
                s.cls = AsmParamClass::Stack;
                s.stackOffset = 40 + stackBy;
                stackBy += 8;      // Win64 栈槽一律 8 字节对齐
            }
        } else {
            s.cls = AsmParamClass::Stack;
            s.stackOffset = 8 + stackBy;   // x86: [ebp+8] 起
            stackBy += isFloat && s.ctype == "float" ? 4 : (isFloat ? 8 : 4);
            stackBy = (stackBy + 3) & ~3;  // 4 字节对齐
        }
        out.push_back(std::move(s));
    }
    return out;
}

// ============================================================
// callee-saved 寄存器识别 (spec §7)
//   三处口径一致: x64 MASM 过程 / x86 内联块 / (将来) 其它后端。
//   做法是**文本级 token 扫描**: 把每行按"标识符"切分, 命中别名表即算用到。
//   已知取舍: 不做数据流分析 —— 只出现在注释里的名字不算 (先剥注释), 但出现在
//   一个永不执行的分支里也算。宁多保存一次, 不可少保存 (少保存 = 毁调用者寄存器)。
//
//   别名表覆盖同一寄存器的全部书写宽度 (rbx/ebx/bx/bl/bh …) —— 写 `ebx` 在 x64 下
//   同样改到了 RBX (低 32 位写零扩展), 所以两种写法都算踩到。clobber 声明也走这张表,
//   于是同一份源码在 x86/x64 下写同一个名字即可 (`Clobber("rbx")` 在 x86 下落到 ebx)。
//   R12–R15 是 x64 独有, 在 x86 目标下被忽略。
// ============================================================

struct AsmSavedReg {
    const char* x64;   // x64 拼写 (同时是规范名)
    const char* x86;   // x86 拼写 (nullptr = 该架构没有这个寄存器)
    const char* aliases[5];
};

inline const std::vector<AsmSavedReg>& asmSavedRegTable() {
    static const std::vector<AsmSavedReg> kTable = {
        { "rbx", "ebx", { "rbx", "ebx", "bx", "bl", "bh" } },
        { "rbp", "ebp", { "rbp", "ebp", "bp", "bpl", nullptr } },
        { "rdi", "edi", { "rdi", "edi", "di",  "dil", nullptr } },
        { "rsi", "esi", { "rsi", "esi", "si",  "sil", nullptr } },
        { "r12", nullptr, { "r12", "r12d", "r12w", "r12b", nullptr } },
        { "r13", nullptr, { "r13", "r13d", "r13w", "r13b", nullptr } },
        { "r14", nullptr, { "r14", "r14d", "r14w", "r14b", nullptr } },
        { "r15", nullptr, { "r15", "r15d", "r15w", "r15b", nullptr } },
    };
    return kTable;
}

// 扫描块内用到的 callee-saved 寄存器 ∪ clobber 声明, 返回**目标架构的拼写**
// (x86: ebx/esi/edi/ebp; x64: rbx/rsi/rdi/rbp/r12–r15)。顺序由表固定, push/pop 稳定可读。
inline std::vector<std::string> asmSavedRegsForArch(const std::vector<std::string>& lines,
                                                    const std::vector<std::string>& clobbers,
                                                    bool x64) {
    const auto& table = asmSavedRegTable();
    std::vector<bool> hit(table.size(), false);

    auto markToken = [&](const std::string& tok) {
        for (size_t i = 0; i < table.size(); i++) {
            if (!x64 && !table[i].x86) continue;      // x86 没有该寄存器
            for (const char* alias : table[i].aliases) {
                if (alias && tok == alias) { hit[i] = true; return; }
            }
        }
    };

    auto scanOne = [&](std::string s) {
        size_t q = s.find('\'');                      // `'` 之后是注释, 不算使用
        if (q != std::string::npos) s = s.substr(0, q);
        std::string tok;
        for (char c : s) {
            unsigned char u = static_cast<unsigned char>(c);
            if (std::isalnum(u) || c == '_') {
                tok += static_cast<char>(::tolower(u));
            } else {
                if (!tok.empty()) { markToken(tok); tok.clear(); }
            }
        }
        if (!tok.empty()) markToken(tok);
    };

    for (auto& l : lines) scanOne(l);
    for (auto& c : clobbers) scanOne(c);

    std::vector<std::string> out;
    for (size_t i = 0; i < table.size(); i++)
        if (hit[i]) out.push_back(x64 ? table[i].x64 : table[i].x86);
    return out;
}

// ============================================================
// 行重写 (x86 内联块与 x64 MASM 过程共用一份, 免得两条路各写一遍走偏)
//   ① 注释: VB/FB 风格 `'` → `;` (两种后端都认 `;`)
//   ② 括号内空白归一: `[ num ]` → `[num]` —— 后面才能做朴素 token 替换
//   ③ 局部标签: `.name` → `<labelPrefix>_name` (MASM PROC 内无 proc 局部标签, 且一个
//      .asm 里多个过程会撞名; MSVC 内联汇编的标签也在函数作用域)
//   ④ subs 表代入 (`[param]` / `[function]`, 大小写不敏感)
//   ⑤ 自赋值消除: 代入后 dst==src 的 mov 退化成注释 (典型是 `mov [Function], eax` →
//      `mov eax, eax`, 值本来就在返回寄存器里)
// ============================================================
inline std::vector<std::string> asmRewriteLines(
        const std::vector<std::string>& lines,
        const std::vector<std::pair<std::string, std::string>>& subs,
        const std::string& labelPrefix) {
    auto lower = [](std::string s) {
        for (auto& c : s) c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));
        return s;
    };
    auto squeeze = [](std::string& s, const std::string& a, const std::string& b) {
        size_t pos = 0;
        while ((pos = s.find(a, pos)) != std::string::npos) s.replace(pos, a.size(), b);
    };

    // 第一遍: 收集 `.name:` 局部标签
    std::vector<std::string> labels;
    for (auto& raw : lines) {
        size_t i = 0;
        while (i < raw.size() && (raw[i] == ' ' || raw[i] == '\t')) i++;
        if (i >= raw.size() || raw[i] != '.') continue;
        size_t j = i + 1;
        while (j < raw.size() && (isalnum(static_cast<unsigned char>(raw[j])) || raw[j] == '_')) j++;
        if (j > i + 1 && j < raw.size() && raw[j] == ':') labels.push_back(raw.substr(i + 1, j - i - 1));
    }

    std::vector<std::string> out;
    for (auto& raw : lines) {
        std::string line = raw;
        for (auto& ch : line) if (ch == '\'') ch = ';';
        squeeze(line, "[ ", "["); squeeze(line, " ]", "]");
        squeeze(line, ",\t", ","); squeeze(line, "\t", " ");
        // ③ 局部标签
        for (auto& lb : labels) {
            std::string from = "." + lb, to = labelPrefix + "_" + lb;
            std::string lowLine = lower(line), fromLower = lower(from);
            size_t pos = 0;
            while ((pos = lowLine.find(fromLower, pos)) != std::string::npos) {
                size_t after = pos + fromLower.size();
                if (after < lowLine.size() &&
                    (isalnum(static_cast<unsigned char>(lowLine[after])) || lowLine[after] == '_')) {
                    pos = after; continue;
                }
                line.replace(pos, from.size(), to);
                lowLine.replace(pos, from.size(), to);
                pos += to.size();
            }
        }
        // ④ subs 代入
        //   两种形态, 都要**连整对方括号一起吃掉** (与 [name] → val 的口径一致):
        //     a) 精确形态 `[name]`              → val
        //     b) 带常量偏移 `[name+4]` / `[name-8]` → val+4 / val-8
        //   为什么 b 必须整体处理 (实测踩过两次, 两个方向都错):
        //     只吃前缀 `[name+` → val+ 会留下孤立的 `]` → cl 报 C2400「找到 ]」;
        //     只吃前缀但补上方括号 `[name+` → [val+ 会变成「按 val 当指针再偏 4」的
        //     内存引用 —— 语法合法、编译零报错, 但语义完全不对, 运行静默取到垃圾。
        //   偏移只限十进制数字 (+/- 可选); [name] 与 [name+...] 由长度/字符双重区分,
        //   不会互相误吞 (`[abc]` 不会被 `[ab` 规则命中, 因为要求紧随 `+`/`-` 且
        //   再往后全是数字并以 `]` 收尾)。
        for (auto& s : subs) {
            // 扫 "[" + bare (不含尾部 ']'), 从而同时覆盖精确与带偏移两种形态。
            //   注意: 不能拿完整的 "[name]" 去 find —— "[name+4]" 里并不含 "[name]"
            //   这个子串, find 直接 npos, 偏移分支永远进不去 (踩过)。
            std::string bare = s.first.substr(1, s.first.size() - 2);
            std::string openLower = lower("[" + bare);
            std::string lowLine = lower(line);
            size_t pos = 0;
            while (pos < lowLine.size() &&
                   (pos = lowLine.find(openLower, pos)) != std::string::npos) {
                size_t k = pos + openLower.size();
                // 形态 b: [name±N]  →  val±N   (连尾 ']' 一起吃)
                if (k < lowLine.size() && (lowLine[k] == '+' || lowLine[k] == '-')) {
                    char sign = lowLine[k];
                    size_t d = k + 1, e = d;
                    while (e < lowLine.size() && std::isdigit(static_cast<unsigned char>(lowLine[e]))) e++;
                    if (e > d && e < lowLine.size() && lowLine[e] == ']') {
                        std::string off = line.substr(d, e - d);
                        std::string repl = s.second + sign + off;
                        size_t whole = e + 1 - pos;      // "[name±N]" 整体长度
                        line.replace(pos, whole, repl);
                        lowLine.replace(pos, whole, lower(repl));
                        pos += repl.size();
                        continue;
                    }
                }
                // 形态 a: [name]  →  val
                if (k < lowLine.size() && lowLine[k] == ']') {
                    line.replace(pos, openLower.size() + 1, s.second);
                    lowLine.replace(pos, openLower.size() + 1, s.second);
                    pos += s.second.size();
                    continue;
                }
                pos += 1;   // 不是我们的引用形态 (如 [namex]), 继续往后找
            }
        }
        // ⑤ 自赋值消除
        {
            std::string t = lower(line);
            size_t a = t.find_first_not_of(" \t");
            if (a != std::string::npos && t.compare(a, 3, "mov") == 0) {
                size_t c = t.find(',', a);
                if (c != std::string::npos) {
                    auto grab = [&](size_t b, size_t e) {
                        while (b < e && (t[b] == ' ' || t[b] == '\t')) b++;
                        while (e > b && (t[e - 1] == ' ' || t[e - 1] == '\t')) e--;
                        return t.substr(b, e - b);
                    };
                    std::string dst = grab(a + 3, c);
                    size_t semi = t.find(';', c + 1);
                    std::string src = grab(c + 1, semi == std::string::npos ? t.size() : semi);
                    if (!dst.empty() && dst == src)
                        line = "; [Function] -> " + dst + " (值已在返回寄存器, 自赋值省略)";
                }
            }
        }
        out.push_back(line);
    }
    return out;
}

// ============================================================
// `[param]` / `[Function]` 替换表构建 (x64 驱动 emitMasmProc 与 codegen 宽度校验共用;
// 两份实现迟早走偏, 所以只留这一份)
//   x64: 32 位整型参数 (int8/16/32, BOOL, unsigned) → 低 32 位寄存器; 指针/64 位 → 整寄存器;
//        [Function] → 与返回类型同宽的返回寄存器 (MASM 不容许宽度不等的 mov)。
//   x86: [param] → C 参数名 (MSVC 内联汇编按名解析); [Function] → 返回变量名
//        (naked 下传空串 → eax)。
// ============================================================
inline std::vector<std::pair<std::string, std::string>> asmBuildX64Subs(const AsmProcInfo& p) {
    static const char* kReg64[4] = {"rcx", "rdx", "r8", "r9"};
    static const char* kReg32[4] = {"ecx", "edx", "r8d", "r9d"};
    std::vector<std::pair<std::string, std::string>> subs;
    // 按 Win64 ABI 分类: 整型/指针 GPR 独立计数, float/double XMM 独立计数, 溢出压栈。
    auto slots = asmClassifyParams(p.params, "x64");
    for (auto& s : slots) {
        if (s.cls == AsmParamClass::Int) {
            bool is32 = (s.ctype == "int32_t" || s.ctype == "int16_t" || s.ctype == "int8_t" ||
                         s.ctype == "VBABOOL" || s.ctype == "unsigned");
            subs.push_back({ "[" + s.name + "]", is32 ? kReg32[s.regIndex] : kReg64[s.regIndex] });
        } else if (s.cls == AsmParamClass::Float) {
            // float/double 在 xmm0–3; 大小标注由用户写 (`movss`/`movsd` 或 dword/qword ptr)。
            subs.push_back({ "[" + s.name + "]", "xmm" + std::to_string(s.regIndex) });
        } else {
            // 栈参数 (第 5 个起): 用户写 `[name]` 表示"该槽的**地址表达式**", 代入后为
            // 裸 `[rsp+40+8k]` —— 宽度由用户的 `dword ptr`/`qword ptr` 标注决定
            // (Win64 栈槽一律 8 字节, 但参数本身可能只占低 4 字节)。
            // 布局: [rsp+0] 返回地址; [rsp+8..39] 调用方 shadow space (32B);
            //       [rsp+40] 第 5 参, [rsp+48] 第 6 参 …
            subs.push_back({ "[" + s.name + "]",
                             "[rsp+" + std::to_string(s.stackOffset) + "]" });
        }
    }
    // [Function] → 与返回类型同宽的返回寄存器。浮点返回走 xmm0 (spec §6)。
    std::string ret;
    if (asmIsFloatCType(p.retCType)) ret = "xmm0";
    else {
        bool retIs32 = (p.retCType == "int8_t" || p.retCType == "int16_t" ||
                        p.retCType == "int32_t" || p.retCType == "VBABOOL" ||
                        p.retCType == "unsigned");
        ret = retIs32 ? "eax" : "rax";
    }
    subs.push_back({ "[function]", ret });
    return subs;
}

inline std::vector<std::pair<std::string, std::string>> asmBuildX86Subs(
        const AsmProcInfo& p, const std::string& retVarName) {
    std::vector<std::pair<std::string, std::string>> subs;
    // x86: 全部参数 (含 >4 个 / 浮点按值) 由 MSVC 内联汇编按名解析 —— 名字天然指向
    // 正确的栈槽或寄存器, 无需我们算偏移。这里只需保留 [name] → name 的替换。
    // 带偏移的形态 ([Function+4]) 由 asmRewriteLines 的通用规则展开, subs 只表达
    // 「[name] → 内容」这一件事, 不在这里拼前缀变体 (拼过, 两种写法都错, 见该处注释)。
    for (auto& ps : p.params) {
        size_t sp = ps.find(' ');
        if (sp == std::string::npos) continue;
        std::string name = ps.substr(sp + 1);
        while (!name.empty() && name.back() == ' ') name.pop_back();
        subs.push_back({ "[" + name + "]", name });
    }
    subs.push_back({ "[function]", retVarName.empty() ? std::string("eax") : retVarName });
    return subs;
}

// ============================================================
// 操作数宽度校验 (spec §2.2): 把 ml64 的 A2022 / cl 的 C2443 前移成 VB 诊断。
//   起因 (实测踩过): `mov rbx, ecx` (64←32) / `mov eax, rax` (32←64) 这类宽度不等
//   的寄存器搬运, 要等到汇编阶段才报一句 A2022, 用户完全对不上 VB 源码行。
//   只对「两个操作数都是纯寄存器且宽度已知」的行判定 —— 内存操作数 (dword ptr [x])
//   不解析、交给汇编器; movzx/movsx/movsxd/lea 本来就是变宽/取址, 排除。
//   传入的是**重写后**的 body (asmRewriteLines 不增删行, 下标与用户原始行一一对应)。
// ============================================================
inline int asmRegWidthBits(const std::string& r) {
    static const char* k64[] = {"rax","rbx","rcx","rdx","rsi","rdi","rbp","rsp",
                                "r8","r9","r10","r11","r12","r13","r14","r15"};
    static const char* k32[] = {"eax","ebx","ecx","edx","esi","edi","ebp","esp",
                                "r8d","r9d","r10d","r11d","r12d","r13d","r14d","r15d"};
    static const char* k16[] = {"ax","bx","cx","dx","si","di","bp","sp",
                                "r8w","r9w","r10w","r11w","r12w","r13w","r14w","r15w"};
    static const char* k8[]  = {"al","ah","bl","bh","cl","ch","dl","dh","sil","dil",
                                "bpl","spl","r8b","r9b","r10b","r11b","r12b","r13b","r14b","r15b"};
    for (const char* k : k64) if (r == k) return 64;
    for (const char* k : k32) if (r == k) return 32;
    for (const char* k : k16) if (r == k) return 16;
    for (const char* k : k8)  if (r == k) return 8;
    return 0;   // 内存操作数 / 立即数 / C 变量名 (x86 按名引用) / xmm 等 → 不判定
}

template <typename OnError>
inline void asmCheckRegWidths(const std::vector<std::string>& body, OnError onError) {
    auto isIdentCh = [](char c) {
        return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
    };
    auto pureReg = [&](const std::string& t) -> std::string {
        if (t.empty() || t.find('[') != std::string::npos ||
            t.find('(') != std::string::npos || t.find(',') != std::string::npos)
            return "";
        for (char c : t) if (!isIdentCh(c)) return "";
        return t;   // 全标识符 → 可能是寄存器 (宽度表查不到就当变量名跳过)
    };
    for (size_t i = 0; i < body.size(); i++) {
        std::string s = body[i];
        size_t q = s.find(';');                       // 重写后注释是 `;`
        if (q != std::string::npos) s = s.substr(0, q);
        size_t a = s.find_first_not_of(" \t");
        if (a == std::string::npos) continue;
        s = s.substr(a);
        static const char* kPrefixes[] = {"lock ", "rep ", "repe ", "repne ", "repz ", "repnz "};
        for (const char* p : kPrefixes)
            if (s.rfind(p, 0) == 0) { s = s.substr(std::strlen(p)); break; }
        // 助记符
        size_t e = 0;
        while (e < s.size() && isIdentCh(s[e])) e++;
        std::string mn = s.substr(0, e);
        for (auto& c : mn) c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));
        if (mn == "movzx" || mn == "movsx" || mn == "movsxd" || mn == "lea" ||
            mn == "xlat" || mn == "imul" || mn == "shrd" || mn == "shld")
            continue;   // 变宽 / 多形 / 三操作数, 不做双寄存器等宽判定
        size_t comma = s.find(',', e);
        if (comma == std::string::npos) continue;
        auto grab = [&](size_t b, size_t end) {
            while (b < end && (s[b] == ' ' || s[b] == '\t')) b++;
            while (end > b && (s[end - 1] == ' ' || s[end - 1] == '\t')) end--;
            return s.substr(b, end - b);
        };
        std::string dst = pureReg(grab(e, comma));
        std::string src = pureReg(grab(comma + 1, s.size()));
        if (dst.empty() || src.empty()) continue;
        int dw = asmRegWidthBits(dst), sw = asmRegWidthBits(src);
        if (dw == 0 || sw == 0 || dw == sw) continue;
        onError(i, dst, src, dw, sw);
    }
}

} // namespace vb6c3
