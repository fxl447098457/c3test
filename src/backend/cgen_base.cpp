#include "backend/cgen.hpp"
#include <algorithm>
#include <cstdio>
#include <cctype>
#include <iostream>
#include <functional>
#include <unordered_set>

namespace vb6c3 {

// --- cgen_base.cpp: 辅助函数 + CodeEmitter + 构造 + generate（骨架，含 11 个函数体片段） ---
// 2026-09-17 两步法拆分：原 1753 行单文件（22 个顶层函数，主导 CCodeGen::generate 占 1067 行）
//   第 1 步（纯搬移）：cgen_base_type.cpp（类型映射/常量折叠）+ cgen_base_naming.cpp（默认值/标识符/常量/过程名）
//   第 2 步（函数体片段）：generate 函数体 91~1152 行切为 11 个 .inc，在 generate() 函数体内被 #include（C++ 允许）——
//     detail/cgen_base_generate_prologue.inc     入口初始化 + Fix 090q 返回 UDT 预扫（原 91~131 行）
//     detail/cgen_base_generate_state_scan.inc   P6.11 类成员 BSTR 扫描 + Fix 010n 模块级 UDT 持久化（原 132~255 行）
//     detail/cgen_base_generate_header_open.inc  .h 头文件开头：guard、include 顺序(P7)、标准宏取消(Fix 020)（原 256~314 行）
//     detail/cgen_base_generate_crossmod.inc     跨模块注册与定义顺序：010r-11 / 010r-12 / P8.7（原 315~384 行）
//     detail/cgen_base_generate_c_open.inc       .c 源文件头与类模块结构体定义（原 385~497 行）
//     detail/cgen_base_generate_decl_pass.inc    第一遍：声明（Const/变量/Declare/Event/过程前向声明）（原 498~596 行）
//     detail/cgen_base_generate_evt_decl.inc     事件处理器包装声明与 vtable source interface 前向声明（原 597~672 行）
//     detail/cgen_base_generate_body_pass.inc    第二遍：过程体 + 类模块工厂函数 + P6.5 事件处理器包装（原 673~795 行）
//     detail/cgen_base_generate_evt_impl.inc     COM 事件回调 + vtable 接收器实现 + 模块级变量延迟初始化（原 796~954 行）
//     detail/cgen_base_generate_entry.inc        入口点生成（EXE main / ActiveX DLL）（原 955~1101 行）
//     detail/cgen_base_generate_epilogue.inc     保存生成结果 + COM 接口 typedef 前向声明（原 1102~1152 行）
// 11 个 .inc 是「函数体片段」，不是独立编译单元（单独 include 会编译不过），片段内容逐行未改 → 零行为改动。

FrmControlType controlTypeFromName(const std::string& name) {
    static const std::unordered_map<std::string, FrmControlType> map = {
        {"commandbutton", FrmControlType::CommandButton},
        {"textbox", FrmControlType::TextBox},
        {"label", FrmControlType::Label},
        {"checkbox", FrmControlType::CheckBox},
        {"optionbutton", FrmControlType::OptionButton},
        {"listbox", FrmControlType::ListBox},
        {"combobox", FrmControlType::ComboBox},
        {"hscrollbar", FrmControlType::HScrollBar},
        {"vscrollbar", FrmControlType::VScrollBar},
        {"frame", FrmControlType::Frame},
        {"timer", FrmControlType::Timer},
        {"picturebox", FrmControlType::PictureBox},
        {"image", FrmControlType::Image},
    };
    std::string lower = name;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    auto it = map.find(lower);
    return (it != map.end()) ? it->second : FrmControlType::Unknown;
}


// ============================================================
// CodeEmitter
// ============================================================

void CodeEmitter::emitLine(const std::string& line) {
    flushPending();
    for (int i = 0; i < indentLevel_; i++) {
        oss_ << "    ";  // 4空格缩进
    }
    oss_ << line << "\n";
}

void CodeEmitter::emit(const std::string& text) {
    oss_ << text;
}

void CodeEmitter::emitBlank() {
    flushPending();
    oss_ << "\n";
}

// Fix 090q: 落地待输出声明行(带当前缩进). 供 As Any ByRef 实参的 UDT 临时
// 变量声明在表达式行之前落位 — 保证 "声明先于引用" (C 要求).
void CodeEmitter::flushPending() {
    for (auto& pl : pendingLines_) {
        for (int i = 0; i < indentLevel_; i++) {
            oss_ << "    ";  // 4空格缩进
        }
        oss_ << pl << "\n";
    }
    pendingLines_.clear();
}

// ============================================================
// CCodeGen 构造
// ============================================================

CCodeGen::CCodeGen(Diagnostics& diag, const SymbolTable& symTab,
                   const TypeSystem& typeSys,
                   const std::unordered_map<std::string, std::set<std::string>>* classVoidFieldMap,
                   const std::unordered_map<std::string, std::unordered_map<std::string, std::string>>* classTypedFieldMap,
                   const std::unordered_set<std::string>* variantReturnFuncs,
                   bool verbose)
    : diag_(diag), symTab_(symTab), typeSys_(typeSys),
      classVoidFieldMap_(classVoidFieldMap), classTypedFieldMap_(classTypedFieldMap),
      variantReturnFuncs_(variantReturnFuncs), verbose_(verbose) {}

// ============================================================
// 主入口: 生成 .h + .c
// ============================================================

bool CCodeGen::generate(Module& module, const std::string& baseName,
                         const std::unordered_set<std::string>& externalModules,
                         bool isDll, const std::string& dllProgId,
                         const FrmFormDesc* frmDesc) {
#include "backend/detail/base/cgen_base_generate_prologue.inc"
#include "backend/detail/base/cgen_base_generate_state_scan.inc"
#include "backend/detail/base/cgen_base_generate_header_open.inc"
#include "backend/detail/base/cgen_base_generate_crossmod.inc"
#include "backend/detail/base/cgen_base_generate_c_open.inc"
#include "backend/detail/base/cgen_base_generate_decl_pass.inc"
#include "backend/detail/base/cgen_base_generate_evt_decl.inc"
#include "backend/detail/base/cgen_base_generate_body_pass.inc"
#include "backend/detail/base/cgen_base_generate_evt_impl.inc"
#include "backend/detail/base/cgen_base_generate_entry.inc"
#include "backend/detail/base/cgen_base_generate_epilogue.inc"
}

// ============================================================
// 表达式求值 (累加器模式: lastExpr_接收结果)
// ============================================================


} // namespace vb6c3
