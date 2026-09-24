#pragma once
// asm_proc.hpp - ai/vb-asm-extension-spec: Asm 过程降级元数据
//
// codegen 侧收集 (CCodeGen::asmProcs_), driver 侧消费 (driver_link.cpp):
//   把每个「函数体 = 单个 Asm 块」的过程降级为一个独立 MASM 过程 (x64/ml64)。
// 只放纯数据, 不依赖 cgen/driver 任一方向, 两边都可 include。

#include <string>
#include <vector>

namespace vb6c3 {

struct AsmProcInfo {
    std::string cName;                 // C 符号名 (x64 无调用约定修饰 → 即 MASM PROC 名)
    std::string retCType;              // 返回 C 类型 ("void" 表示 Sub)
    std::vector<std::string> params;   // 逐参 "CType name" (与 makeParamList 同口径)
    std::vector<std::string> lines;    // 原始汇编行 (未做寄存器替换)
};

} // namespace vb6c3
