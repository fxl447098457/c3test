// driver_codegen_prelude.inc - runCodeGeneration 函数体片段：模块数与分析器数校验
// 由 src/driver/driver.cpp 拆出（2026-09-17），在函数体内被 #include，非独立编译单元。
// 内容 = 原文件第 95~99 行，逐行未改。

    if (modules_.size() != analyzers_.size()) {
        std::cerr << "C3: 内部错误: 模块数与分析器数不匹配" << std::endl;
        return false;
    }

    // ai/vb-asm-extension-spec: 本次发码收集的 Asm 过程 (runLinker 消费) —— 防重复调用累积
    asmProcs_.clear();

