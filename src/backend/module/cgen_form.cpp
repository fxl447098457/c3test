#include "backend/cgen.hpp"
#include "project/frx_reader.hpp"
#include <algorithm>
#include <cctype>
#include <iostream>
#include <functional>
#include <sstream>
#include <iomanip>

namespace vb6c3 {

// P24: Convert binary data to C hex array string
static std::string bytesToHexArray(const uint8_t* data, size_t size, const std::string& varName) {
    std::ostringstream ss;
    ss << "static const unsigned char " << varName << "[] = {\n";
    for (size_t i = 0; i < size; i++) {
        if (i % 16 == 0) ss << "    ";
        ss << "0x" << std::setfill('0') << std::setw(2) << std::hex << (int)data[i];
        if (i + 1 < size) ss << ",";
        if (i % 16 == 15 || i + 1 == size) ss << "\n";
        else ss << " ";
    }
    ss << "};\n";
    ss << "static const int " << varName << "_size = " << std::dec << size << ";";
    return ss.str();
}

// P24: Escape string for C string literal (handles backslash, quote, newlines, tabs, etc.)
static std::string escapeCString(const std::string& s) {
    std::string result;
    for (char c : s) {
        switch (c) {
            case '\\': result += "\\\\"; break;
            case '"': result += "\\\""; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default:
                if ((unsigned char)c < 0x20) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\x%02x", (unsigned char)c);
                    result += buf;
                } else {
                    result += c;
                }
        }
    }
    return result;
}

// --- cgen_form.cpp: 窗体框架（emitFormFramework）---
// 2026-09-17 拆分：原 2222 行（emitFormFramework 单函数 1945 行）先纯搬移走 4 个成员函数
//   （emitMenuItem / emitMenuClickDispatch / escapeWideCString / CCodeGen::escapeCString
//     → backend/module/cgen_form_menu.cpp，已登记 CMakeLists），
//   余下函数体按既有分节注释切为 8 个「函数体片段」，在 emitFormFramework() 函数体内 #include：
//   detail/cgen_form_prelude.inc            —— 初始化（frx 加载、菜单ID收集）与窗体属性提取（原 54~144 行）
//   detail/cgen_form_ctrl_registry.inc      —— 控件名映射 / 控件数组检测 与 .h 声明（原 145~261 行）
//   detail/cgen_form_wndproc_subclass.inc   —— .c 实现开头：Form_Unload trampoline 与需子类化控件的 WndProc（原 262~597 行）
//   detail/cgen_form_wndproc_create.inc     —— WndProc 前半：WM_CREATE / 延迟 Form_Load / WM_COMMAND / WithEvents 派发（原 598~921 行）
//   detail/cgen_form_wndproc_dispatch.inc   —— WndProc 后半：焦点 / 滚动 / 键盘 / 关闭 / 清理 / default（原 922~1272 行）
//   detail/cgen_form_create_controls.inc    —— CreateControls：控件树 CreateWindow（原 1273~1772 行）
//   detail/cgen_form_frame_menu.inc         —— Frame 子控件 / 控件子类化安装 / Win32 菜单构建（原 1773~1937 行）
//   detail/cgen_form_show.inc               —— Show 函数（原 1938~1996 行）
// 八个 .inc 是「函数体片段」，在函数体内被 #include（C++ 允许），故不用 .cpp/.hpp 后缀 ——
// 它们不是独立编译单元，单独编译会报错，也不登记 CMakeLists。片段内局部 lambda 与块作用域原样不动，
// 逐行未改 → 零行为改动；切点全落在原函数体的分节注释处（相对花括号深度 0）。

void CCodeGen::emitFormFramework(const FrmFormDesc& frmDesc, Module& module) {
#include "backend/detail/module/cgen_form_prelude.inc"
#include "backend/detail/module/cgen_form_ctrl_registry.inc"
#include "backend/detail/module/cgen_form_wndproc_subclass.inc"
#include "backend/detail/module/cgen_form_wndproc_create.inc"
#include "backend/detail/module/cgen_form_wndproc_dispatch.inc"
#include "backend/detail/module/cgen_form_create_controls.inc"
#include "backend/detail/module/cgen_form_ctrl_style_apply.inc"
#include "backend/detail/module/cgen_form_frame_menu.inc"
#include "backend/detail/module/cgen_form_show.inc"
}

// ============================================================
// P6.6: 单独生成 DLL 入口文件 (dll_entry.c)
// 当DLL工程只有类模块(无标准模块)时使用
// ============================================================


} // namespace vb6c3

