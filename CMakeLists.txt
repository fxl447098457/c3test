cmake_minimum_required(VERSION 3.20)
project(vb6c3 VERSION 0.10.2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# MSVC: use UTF-8 source charset (files are UTF-8 without BOM)
if(MSVC)
    add_compile_options(/utf-8)
endif()

# === LLVM backend (disabled by default, decision 006: C code gen + MSVC) ===
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
# Decision 008: Library-first architecture
# vb6c3-core = lexer/parser/AST/semantics/symbols/types/diagnostics
# c3.exe = thin CLI driver
# ============================================================

# --- Core library vb6c3-core ---
add_library(vb6c3-core STATIC
    src/common/diagnostics.cpp
    src/common/source_manager.cpp
    src/common/encoding.cpp
    src/lexer/lexer.cpp
    src/lexer/lexer_keywords.cpp
    src/lexer/lexer_number.cpp
    src/lexer/lexer_literal.cpp
    src/lexer/token.cpp
    src/preprocessor/preprocessor.cpp
    src/preprocessor/preprocessor_cond.cpp
    src/preprocessor/preprocessor_expr.cpp
    src/parser/parser.cpp
    src/parser/parser_helpers.cpp
    src/parser/parser_module.cpp
    src/parser/parser_expr.cpp
    src/parser/parser_expr_postfix.cpp
    src/parser/parser_decl.cpp
    src/parser/parser_decl_var.cpp
    # 语句解析家族
    src/parser/stmt/parser_stmt.cpp
    src/parser/stmt/parser_stmt_control.cpp
    src/parser/stmt/parser_stmt_jump.cpp
    src/parser/stmt/parser_stmt_assign.cpp
    src/parser/stmt/parser_stmt_io.cpp
    src/ast/ast.cpp
    src/ast/ast_visitor.cpp
    src/ast/ast_printer.cpp
    src/semantics/symbol_table.cpp
    src/semantics/type_system.cpp
    src/semantics/semantic_analyzer.cpp
    src/semantics/semantic_analyzer_decl.cpp
    src/semantics/semantic_analyzer_decl_type.cpp
    src/semantics/semantic_analyzer_stmt.cpp
    src/semantics/semantic_analyzer_expr.cpp
    src/semantics/semantic_analyzer_util.cpp
    src/semantics/semantic_analyzer_builtin.cpp
    src/project/vbp_parser.cpp
    src/project/frm_parser.cpp
    src/project/frm_parser_util.cpp
    src/project/frx_reader.cpp
    src/com/typelib_parser.cpp
    src/com/typelib_parser_parse.cpp
    src/com/typelib_parser_desc.cpp
    src/typelib/typelib_builder.cpp
    src/typelib/typelib_builder_coclass.cpp
)
target_compile_definitions(vb6c3-core PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
# TypeLib parser needs ole32/oleaut32 (COM API)
target_link_libraries(vb6c3-core PRIVATE ole32 oleaut32 uuid)

# --- C code generation backend ---
add_library(vb6c3-cgen STATIC
    src/backend/cgen_base.cpp
    # 表达式家族
    src/backend/expr/cgen_expr.cpp
    src/backend/expr/cgen_expr_ident.cpp
    src/backend/expr/cgen_expr_member.cpp
    src/backend/expr/cgen_expr_call.cpp
    src/backend/expr/cgen_expr_binary.cpp
    src/backend/expr/cgen_expr_binary_util.cpp
    src/backend/expr/cgen_expr_array.cpp
    src/backend/expr/cgen_expr_with.cpp
    # 语句家族
    src/backend/stmt/cgen_stmt.cpp
    src/backend/stmt/cgen_assign.cpp
    src/backend/stmt/cgen_setlet.cpp
    src/backend/stmt/cgen_control.cpp
    src/backend/stmt/cgen_loop.cpp
    src/backend/stmt/cgen_select.cpp
    src/backend/stmt/cgen_with.cpp
    src/backend/stmt/cgen_jumps.cpp
    src/backend/stmt/cgen_call.cpp
    src/backend/stmt/cgen_redim.cpp
    src/backend/stmt/cgen_file_io.cpp
    # 声明家族
    src/backend/decl/cgen_decl.cpp
    src/backend/decl/cgen_decl_proc.cpp
    src/backend/decl/cgen_decl_func.cpp
    src/backend/decl/cgen_decl_var.cpp
    src/backend/decl/cgen_decl_api.cpp
    src/backend/decl/cgen_decl_prop.cpp
    src/backend/decl/cgen_localdecl.cpp
    src/backend/decl/cgen_localdecl_hoist.cpp
    # 模块级产物（COM 服务器 / 窗体）
    src/backend/module/cgen_com.cpp
    src/backend/module/cgen_com_events.cpp
    src/backend/module/cgen_form.cpp
    src/backend/cgen_util.cpp
    src/backend/cgen_util_type.cpp
    src/backend/cgen_util_scan.cpp
    src/backend/cgen_util_com.cpp
    src/backend/cgen_util_comwrite.cpp
    src/backend/cgen_util_classcall.cpp
    src/backend/cgen_util_classtype.cpp
    src/backend/cgen_util_ctrl.cpp
    src/backend/msvc_driver.cpp
    src/backend/msvc_driver_discovery.cpp
    src/backend/msvc_driver_incremental.cpp
)
target_compile_definitions(vb6c3-cgen PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(vb6c3-cgen PRIVATE vb6c3-core)

# --- LLVM backend (optional) ---
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

# --- IR layer (preserved) ---
add_library(vb6c3-ir STATIC
    src/ir/ir.cpp
    src/ir/ir_emitter.cpp
)
target_compile_definitions(vb6c3-ir PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(vb6c3-ir PRIVATE vb6c3-core)

# --- Compiler CLI: C3.exe ---
# P10 (restored): c3rtl.rc embeds RTL .h/.c SOURCE files as RCDATA resources.
# RTL is compiled by MSVC at user-program build time (no pre-compiled .lib).
add_executable(c3
    src/driver/main.cpp
    src/driver/driver.cpp
    src/driver/rtl_embedded.cpp
    src/driver/c3rtl.rc
)
set_target_properties(c3 PROPERTIES OUTPUT_NAME C3)

# Fix 092z-2: CMake 只把 .rc 自身当依赖 —— 改了 src/rtl/core 下的嵌入源之后
# .res 不会重编, C3.exe 会继续嵌入旧文件。这里显式声明嵌入文件为 .rc 的依赖
# (39 个资源: 14 头 + 25 源码, 全部架构无关；与 c3rtl.rc 一一对应)。
set(C3RTL_EMBEDDED_FILES
    # vb6rtl
    ${CMAKE_CURRENT_SOURCE_DIR}/src/rtl/core/vb6rtl/vb6rtl.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/rtl/core/vb6rtl/vb6rtl.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/rtl/core/vb6rtl/vb6rtl_base.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/rtl/core/vb6rtl/vb6rtl_bstr.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/rtl/core/vb6rtl/vb6rtl_variant.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/rtl/core/vb6rtl/vb6rtl_builtin.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/rtl/core/vb6rtl/vb6rtl_array.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/rtl/core/vb6rtl/vb6rtl_class_com.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/rtl/core/vb6rtl/vb6rtl_runtime.h
    # vb6com
    ${CMAKE_CURRENT_SOURCE_DIR}/src/rtl/core/vb6com/vb6com.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/rtl/core/vb6com/vb6com.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/rtl/core/vb6com/vb6com_internal.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/rtl/core/vb6com/vb6com_invoke.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/rtl/core/vb6com/vb6com_pack.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/rtl/core/vb6com/vb6com_wrap.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/rtl/core/vb6com/vb6com_sink.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/rtl/core/vb6com/vb6com_foreach.c
    # vb6comserver
    ${CMAKE_CURRENT_SOURCE_DIR}/src/rtl/core/vb6comserver/vb6comserver.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/rtl/core/vb6comserver/vb6comserver.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/rtl/core/vb6comserver/vb6comserver_internal.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/rtl/core/vb6comserver/vb6comserver_obj.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/rtl/core/vb6comserver/vb6comserver_factory.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/rtl/core/vb6comserver/vb6comserver_cp.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/rtl/core/vb6comserver/vb6comserver_pci.c
    # vb6forms
    ${CMAKE_CURRENT_SOURCE_DIR}/src/rtl/core/vb6forms/vb6forms.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/rtl/core/vb6forms/vb6forms.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/rtl/core/vb6forms/vb6forms_internal.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/rtl/core/vb6forms/vb6forms_ctrl.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/rtl/core/vb6forms/vb6forms_list.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/rtl/core/vb6forms/vb6forms_style.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/rtl/core/vb6forms/vb6forms_scroll.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/rtl/core/vb6forms/vb6forms_picture.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/rtl/core/vb6forms/vb6forms_picture_prop.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/rtl/core/vb6forms/vb6forms_ctrlarr.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/rtl/core/vb6forms/vb6forms_webview.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/rtl/core/vb6forms/vb6forms_widget.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/rtl/core/vb6forms/vb6forms_widget_prop.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/rtl/core/vb6forms/vb6forms_shape.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/rtl/core/vb6forms/vb6forms_axsite.c
    # 平铺（不属于任何家族）
    ${CMAKE_CURRENT_SOURCE_DIR}/src/rtl/core/vb6_di_stubs.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/rtl/core/vb6_di_win32_stubs.c
)
set_source_files_properties(src/driver/c3rtl.rc PROPERTIES OBJECT_DEPENDS "${C3RTL_EMBEDDED_FILES}")
target_link_libraries(c3 PRIVATE vb6c3-core vb6c3-cgen)
if(WIN32)
    # 崩溃栈追踪 (C3_CRASH_TRACE=1) 需要 dbghelp
    target_link_libraries(c3 PRIVATE dbghelp)
endif()
if(VB6C3_ENABLE_LLVM)
    target_link_libraries(c3 PRIVATE vb6c3-llvm)
else()
    target_compile_definitions(c3 PRIVATE VB6C3_NO_LLVM)
endif()
target_compile_definitions(c3 PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
