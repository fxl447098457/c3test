// driver_codegen_dll_entry.inc - runCodeGeneration 函数体片段：generateDllEntry 与 dll_entry.c 写出 + 返回（位于 if (options.isDll) 块内）
// 由 src/driver/driver.cpp 拆出（2026-09-17），在函数体内被 #include，非独立编译单元。
// 内容 = 原文件第 754~772 行，逐行未改。

        // P6.6.6: generateDllEntry() runs after TypeLib building so IIDs/CLSIDs are available
        std::string dllEntryCode = dllCgen.generateDllEntry(options.dllProgId, allSymTabs);

        std::string dllEntryPath = outputDir + "/dll_entry.c";
        {
            std::ofstream ofs(dllEntryPath, std::ios::out | std::ios::trunc);
            if (!ofs) {
                std::cerr << "C3: 无法写入文件: " << dllEntryPath << std::endl;
                return false;
            }
            ofs << dllEntryCode;
        }

        if (options.verbose) {
            std::cout << "C3: 生成 " << dllEntryPath << " (" << dllEntryCode.size() << " bytes)" << std::endl;
        }
    }

    return !diag_->hasErrors();
