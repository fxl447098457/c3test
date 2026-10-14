#pragma once
// phase_timing.hpp - 编译相位计时 (ai/030 续, 2026-10-14)
//
// 为什么: "稍微大一点的项目编译太费时间" 这条只能靠读数收敛。C3 的墙钟拆成
// 若干相位 (vbp/lex/preprocess/parse/typelib/prepasses/semantic/crossmod/
// unpack-rtl/codegen/link + 逐模块 gen/io + 逐过程 proc), 每个 mark 打**距上一个
// mark** 的毫秒。
//
// 口径:
//   · C3_TIMING=1 才输出, 未设时除一次 getenv 外零成本、零输出 (不动任何既有读数);
//   · 行格式 "C3: TIMING <label> <ms> ms" —— ASCII 可断言 (与 OBJCACHE 行同族);
//   · 打 stderr; 多次编译共用一个进程时时钟连续, 看差值即可;
//   · c3PhaseMarkIf 的"未打印也推时钟": 逐过程这种高频 mark 不打小值, 但每次都要
//     把时钟前推, 否则下一个 mark 会把前面吞掉的时间全算进去。
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace vb6c3 {

namespace detail {
struct PhaseClock {
    bool on;
    std::chrono::steady_clock::time_point last;
    PhaseClock()
        : on(std::getenv("C3_TIMING") != nullptr),
          last(std::chrono::steady_clock::now()) {}
};
inline PhaseClock& phaseClock() { static PhaseClock c; return c; }
} // namespace detail

inline void c3PhaseMark(const std::string& label) {
    auto& clk = detail::phaseClock();
    if (!clk.on) return;
    auto now = std::chrono::steady_clock::now();
    double ms = std::chrono::duration<double, std::milli>(now - clk.last).count();
    std::fprintf(stderr, "C3: TIMING %s %.0f ms\n", label.c_str(), ms);
    std::fflush(stderr);
    clk.last = now;
}

// 只打 >= minMs 的 mark; 但无论打不打, 时钟都前推。
inline void c3PhaseMarkIf(const std::string& label, double minMs) {
    auto& clk = detail::phaseClock();
    if (!clk.on) return;
    auto now = std::chrono::steady_clock::now();
    double ms = std::chrono::duration<double, std::milli>(now - clk.last).count();
    if (ms >= minMs) {
        std::fprintf(stderr, "C3: TIMING %s %.0f ms\n", label.c_str(), ms);
        std::fflush(stderr);
    }
    clk.last = now;
}

// ---- 第二层: 热函数累计统计 (emitLine / inferExprType / emitExpr) ----
// 口径: C3_TIMING=1 时按模块 dump 一行 STATS 并清零 (c3StatDump); 高调用量函数里
// 用 RAII (C3ScopeTimer) 记账 —— 两次时钟读 ~60ns/次, 万级调用下忽略不计。
namespace detail {
struct PhaseStats {
    static const int kSlots = 4;
    double ms[kSlots] = {0, 0, 0, 0};
    unsigned long long n[kSlots] = {0, 0, 0, 0};
};
inline PhaseStats& phaseStats() { static PhaseStats s; return s; }
} // namespace detail

inline void c3StatAdd(int slot, double ms) {
    auto& s = detail::phaseStats();
    s.ms[slot] += ms;
    s.n[slot] += 1;
}

inline void c3StatDump(const std::string& tag) {
    auto& clk = detail::phaseClock();
    if (!clk.on) return;
    auto& s = detail::phaseStats();
    std::fprintf(stderr,
                 "C3: STATS %s emitLine=%.0fms/%llu inferExprType=%.0fms/%llu emitExpr=%.0fms/%llu\n",
                 tag.c_str(),
                 s.ms[0], s.n[0], s.ms[1], s.n[1], s.ms[2], s.n[2]);
    std::fflush(stderr);
    for (int i = 0; i < detail::PhaseStats::kSlots; i++) { s.ms[i] = 0; s.n[i] = 0; }
}

// RAII 记账器: 构造时(仅当开启)取时刻, 析构时把耗时累进 slot。
// 用在**多出口**的热函数顶部 (emitExpr/inferExprType 里到处 return)。
struct C3ScopeTimer {
    int slot;
    bool on;
    std::chrono::steady_clock::time_point t0;
    explicit C3ScopeTimer(int s) : slot(s), on(false) {
        static const bool c3t = (std::getenv("C3_TIMING") != nullptr);
        on = c3t;
        if (on) t0 = std::chrono::steady_clock::now();
    }
    ~C3ScopeTimer() {
        if (!on) return;
        c3StatAdd(slot, std::chrono::duration<double, std::milli>(
                            std::chrono::steady_clock::now() - t0).count());
    }
};

} // namespace vb6c3
