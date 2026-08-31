#pragma once
// vb6c - Visual Basic 6.0 Compiler
// Copyright (c) 2026 vb6.pro project

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <unordered_map>
#include <optional>

namespace vb6c3 {

// VB6数据类型枚举
// 使用uint16_t以容纳Array(8192)和ByRef(16384)位标志
enum class Vb6Type : uint16_t {
    Empty = 0,
    Null = 1,
    Integer = 2,     // 16-bit
    Long = 3,        // 32-bit
    Single = 4,      // 32-bit float
    Double = 5,      // 64-bit float
    Currency = 6,    // 64-bit scaled integer
    Date = 7,        // 64-bit double (OLE Date)
    String = 8,      // BSTR
    Object = 9,      // IDispatch*
    Error = 10,      // SCODE
    Boolean = 11,    // 16-bit (0xFFFF = True, 0x0000 = False)
    Variant = 12,    // VARIANT
    DataObject = 13,
    Decimal = 14,    // 96-bit unsigned integer + scaling
    Byte = 17,       // 8-bit unsigned
    ULong = 19,      // unsigned Long (VB7+)
    LongPtr = 20,     // Fix 081e: LongPtr/LongLong - architecture-width integer (intptr_t)
    UserDefinedType = 36,
    Array = 8192,    // bit flag
    ByRef = 16384,   // bit flag

    // 编译器内部类型
    Void = 255,
    Unknown = 254,
};

// VB6调用约定
enum class CallConv : uint8_t {
    VBDefault,   // ByRef默认
    StdCall,     // Declare语句Windows API
    CDecl,       // Declare语句C语言
    FastCall,    // 保留
};

// VB6访问级别
enum class AccessLevel : uint8_t {
    Public = 0,
    Private = 1,
    Friend = 2,     // VB6无此关键字，保留
    Default = Public,
};

// VB6过程类型
enum class ProcKind : uint8_t {
    Sub,
    Function,
    PropertyGet,
    PropertyLet,
    PropertySet,
};

// VB6类Instancing属性
enum class VBInstancing : uint8_t {
    Private = 1,              // 仅本工程内可见（VB6默认）
    PublicNotCreatable = 2,   // 外部可用但不能New
    SingleUse = 3,            // 外部可New，每个客户独立实例
    GlobalSingleUse = 4,      // 同SingleUse，无需显式创建
    MultiUse = 5,             // 外部可New，多客户共享进程
    GlobalMultiUse = 6,       // 同MultiUse，无需显式创建
};

} // namespace vb6c3
