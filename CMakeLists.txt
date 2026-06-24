cmake_minimum_required(VERSION 3.20)
project(vb6c3 VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# MSVC: use UTF-8 source charset (files are UTF-8 without BOM)
if(MSVC)
    add_compile_options(/utf-8)
endif()

# === LLVM 后端选项 (默认关闭，006决策：现阶段走C代码生成+MSVC路线) ===
option(VB6C3_ENABLE_LLVM "Enable LLVM backend" OFF)
set(VB6C3_LLVM_DIR "" CACHE PATH "LLVM installation directory (for tool mode)")

if(VB6C3_ENABLE_LLVM)
    set(VB6C3_LLVM_API_MODE OFF)
    find_package(LLVM QUIET CONFIG)
    if(LLVM_FOUND)
        include_directories(${LLVM_INCLUDE_DIRS})
        separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
        add_definitions(${LLVM_DEFINITIONS_LIST})
        llvm_map_components_to_libnames(llvm_libs core support orcjit native)
        set(VB6C3_LLVM_API_MODE ON)
        message(STATUS "LLVM API mode: version ${LLVM_PACKAGE_VERSION}")
    else()
        if(VB6C3_LLVM_DIR)
            set(LLVM_CLANG "${VB6C3_LLVM_DIR}/bin/clang.exe")
        else()
            find_program(LLVM_CLANG clang)
        endif()
        if(EXISTS "${LLVM_CLANG}")
            get_filename_component(LLVM_BIN_DIR "${LLVM_CLANG}" DIRECTORY)
            get_filename_component(VB6C3_LLVM_DIR_CALC "${LLVM_BIN_DIR}" DIRECTORY)
            message(STATUS "LLVM tool mode: clang found at ${LLVM_CLANG}")
            if(NOT VB6C3_LLVM_DIR)
                set(VB6C3_LLVM_DIR "${VB6C3_LLVM_DIR_CALC}")
            endif()
        else()
            message(WARNING "LLVM enabled but neither CMake config nor clang found")
            set(VB6C3_ENABLE_LLVM OFF)
        endif()
    endif()
else()
    message(STATUS "LLVM backend disabled - using C code generation + MSVC")
endif()

include_directories(${CMAKE_SOURCE_DIR}/src)

# ============================================================
# 008决策：库优先架构
# vb6c3-core = 词法/语法/AST/语义/符号/类型/诊断/源码管理
# c3.exe = 薄壳CLI驱动
# ============================================================

# --- 核心库 vb6c3-core ---
add_library(vb6c3-core STATIC
    src/common/diagnostics.cpp
    src/common/source_manager.cpp
    src/common/encoding.cpp
    src/lexer/lexer.cpp
    src/lexer/token.cpp
    src/preprocessor/preprocessor.cpp
    src/parser/parser.cpp
    src/parser/parser_expr.cpp
    src/parser/parser_stmt.cpp
    src/parser/parser_decl.cpp
    src/ast/ast.cpp
    src/ast/ast_visitor.cpp
    src/ast/ast_printer.cpp
    src/semantics/symbol_table.cpp
    src/semantics/type_system.cpp
    src/semantics/semantic_analyzer.cpp
)
target_compile_definitions(vb6c3-core PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)

# --- C代码生成后端 ---
add_library(vb6c3-cgen STATIC
    src/backend/cgen.cpp
    src/backend/msvc_driver.cpp
)
target_compile_definitions(vb6c3-cgen PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(vb6c3-cgen PRIVATE vb6c3-core)

# --- LLVM后端 (可选) ---
if(VB6C3_ENABLE_LLVM)
    add_library(vb6c3-llvm STATIC
        src/backend/llvm_backend.cpp
        src/backend/pe_writer.cpp
    )
    if(VB6C3_LLVM_API_MODE)
        target_link_libraries(vb6c3-llvm PRIVATE ${llvm_libs})
    else()
        target_compile_definitions(vb6c3-llvm PRIVATE VB6C3_LLVM_TOOL_MODE VB6C3_LLVM_DIR="${VB6C3_LLVM_DIR}")
    endif()
    target_link_libraries(vb6c3-llvm PRIVATE vb6c3-core)
endif()

# --- IR层 (暂保留) ---
add_library(vb6c3-ir STATIC
    src/ir/ir.cpp
    src/ir/ir_emitter.cpp
)
target_compile_definitions(vb6c3-ir PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(vb6c3-ir PRIVATE vb6c3-core)

# --- 编译器CLI: c3.exe ---
add_executable(c3
    src/driver/main.cpp
    src/driver/driver.cpp
)
target_link_libraries(c3 PRIVATE vb6c3-core vb6c3-cgen)
if(VB6C3_ENABLE_LLVM)
    target_link_libraries(c3 PRIVATE vb6c3-llvm)
else()
    target_compile_definitions(c3 PRIVATE VB6C3_NO_LLVM)
endif()
target_compile_definitions(c3 PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)

# --- 运行时 RTL ---
add_library(vb6c3-rtl3-core STATIC
    src/rtl/core/bstring.cpp
    src/rtl/core/variant.cpp
    src/rtl/core/safearray.cpp
    src/rtl/core/fileio.cpp
    src/rtl/core/string_funcs.cpp
    src/rtl/core/math_funcs.cpp
    src/rtl/core/datetime.cpp
    src/rtl/core/conversion.cpp
    src/rtl/core/error_handler.cpp
)
target_compile_definitions(vb6c3-rtl3-core PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)

if(WIN32)
    add_library(vb6c3-rtl3-win32 STATIC
        src/rtl/platform/win32/com_interop.cpp
        src/rtl/platform/win32/registry.cpp
        src/rtl/platform/win32/forms.cpp
        src/rtl/platform/win32/controls.cpp
    )
    target_link_libraries(vb6c3-rtl3-win32 PRIVATE vb6c3-rtl3-core ole32 oleaut32 uuid)
endif()
