// cgen_base_generate_epilogue.inc - generate 函数体片段：保存生成结果与 COM 接口 typedef 声明
// 由 src/backend/cgen_base.cpp 拆出（2026-09-17），在函数体内被 #include，非独立编译单元。
// 内容 = 原文件第 1102~1152 行，逐行未改。

    // 保存生成结果
    header_ = h_.str();
    source_ = c_.str();

    // P6.3: 如果使用了COM接口类型, 在头文件中插入typedef前向声明
    // vb6_ComIface_<Name> 是不透明结构体, 仅用作类型化指针
    if (!usedComIfaceTypes_.empty() || !usedVb6IfaceTypes_.empty() || !usedClassTypes_.empty() || !usedUdtTypes_.empty()) {
        std::string typedefBlock;
        if (!usedComIfaceTypes_.empty()) {
            typedefBlock += "\n// P6.3: COM接口类型前向声明 (早期绑定)\n";
            for (const auto& ifaceName : usedComIfaceTypes_) {
                typedefBlock += "typedef struct vb6_ComIface_" + ifaceName + " vb6_ComIface_" + ifaceName + ";\n";
            }
        }
        if (!usedVb6IfaceTypes_.empty()) {
            typedefBlock += "\n// P6.4: VB6接口类型前向声明 (Implements)\n";
            for (const auto& ifaceName : usedVb6IfaceTypes_) {
                typedefBlock += "typedef struct vb6_vtbl_" + ifaceName + " vb6_vtbl_" + ifaceName + ";\n";
                typedefBlock += "typedef struct vb6_iface_" + ifaceName + " vb6_iface_" + ifaceName + ";\n";
            }
        }
        // Fix 010: VB6类类型前向声明
        // C11允许typedef重定义, 所以无条件发出所有使用到的类类型前向声明
        // 解决: (1) 当前模块类struct中使用UDT类型 (2) 循环#include导致的类型不可见
        if (!usedClassTypes_.empty()) {
            typedefBlock += "\n// VB6类类型前向声明\n";
            for (const auto& clsName : usedClassTypes_) {
                typedefBlock += "typedef struct vb6_cls_" + clsName + " vb6_cls_" + clsName + ";\n";
            }
        }
        // Fix 010: VB6 UDT类型前向声明
        // C11允许typedef重定义, 无条件发出 — 解决类struct中UDT类型先使用后定义的问题
        if (!usedUdtTypes_.empty()) {
            typedefBlock += "\n// VB6 UDT类型前向声明\n";
            for (const auto& udtName : usedUdtTypes_) {
                typedefBlock += "typedef struct vb6_type_" + udtName + " vb6_type_" + udtName + ";\n";
            }
        }
        // 在 #include "vb6rtl.h" 之后插入
        std::string marker = "#include \"vb6rtl.h\"";
        size_t pos = header_.find(marker);
        if (pos != std::string::npos) {
            pos = header_.find('\n', pos);  // 找到行尾
            if (pos != std::string::npos) {
                header_.insert(pos + 1, typedefBlock);
            }
        }
    }

    currentModule_ = nullptr;
    return diag_.errorCount() == 0;
