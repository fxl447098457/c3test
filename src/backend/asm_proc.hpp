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
        for (auto& s : subs) {
            std::string tokenLower = lower(s.first);
            std::string lowLine = lower(line);
            size_t pos = 0;
            while ((pos = lowLine.find(tokenLower, pos)) != std::string::npos) {
                line.replace(pos, s.first.size(), s.second);
                lowLine.replace(pos, s.first.size(), s.second);
                pos += s.second.size();
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

} // namespace vb6c3
