// builtin_consts.inc - registerBuiltins 的基础常量注册段（函数体片段）
// 由 src/semantics/semantic_analyzer_builtin.cpp 拆出（2026-09-17），在函数体内被
// #include，非独立文件。内容 = 原文件第 14~297 行，逐行未改（含局部 lambda addConst /
// addStrConst 的定义）。

    // VB6 内置常量
    auto addConst = [&](const char* name, Vb6Type type, long long intVal) {
        auto sym = std::make_unique<Symbol>(SymbolKind::Constant, name, type,
            SourceLocation{}, AccessLevel::Public);
        sym->hasConstValue = true;
        sym->constType = type;
        sym->constIntValue = intVal;
        sym->isBuiltin = true;
        symTab_.define(std::move(sym));
    };

    // MsgBox 返回值常量
    addConst("vbOK", Vb6Type::Long, 1);
    addConst("vbCancel", Vb6Type::Long, 2);
    addConst("vbAbort", Vb6Type::Long, 3);
    addConst("vbRetry", Vb6Type::Long, 4);
    addConst("vbIgnore", Vb6Type::Long, 5);
    addConst("vbYes", Vb6Type::Long, 6);
    addConst("vbNo", Vb6Type::Long, 7);

    // MsgBox 按钮参数常量
    addConst("vbOKOnly", Vb6Type::Long, 0);
    addConst("vbOKCancel", Vb6Type::Long, 1);
    addConst("vbAbortRetryIgnore", Vb6Type::Long, 2);
    addConst("vbYesNoCancel", Vb6Type::Long, 3);
    addConst("vbYesNo", Vb6Type::Long, 4);
    addConst("vbRetryCancel", Vb6Type::Long, 5);
    addConst("vbCritical", Vb6Type::Long, 16);
    addConst("vbQuestion", Vb6Type::Long, 32);
    addConst("vbExclamation", Vb6Type::Long, 48);
    addConst("vbInformation", Vb6Type::Long, 64);
    addConst("vbDefaultButton1", Vb6Type::Long, 0);
    addConst("vbDefaultButton2", Vb6Type::Long, 256);
    addConst("vbDefaultButton3", Vb6Type::Long, 512);
    addConst("vbDefaultButton4", Vb6Type::Long, 768);
    addConst("vbApplicationModal", Vb6Type::Long, 0);
    addConst("vbSystemModal", Vb6Type::Long, 4096);

    // Form.Show 方法窗口模式常量
    addConst("vbModal", Vb6Type::Long, 1);
    addConst("vbModeless", Vb6Type::Long, 0);

    // 颜色常量
    addConst("vbBlack", Vb6Type::Long, 0);
    addConst("vbRed", Vb6Type::Long, 255);
    addConst("vbGreen", Vb6Type::Long, 65280);
    addConst("vbYellow", Vb6Type::Long, 65535);
    addConst("vbBlue", Vb6Type::Long, 16711680);
    addConst("vbMagenta", Vb6Type::Long, 16711935);
    addConst("vbCyan", Vb6Type::Long, 16776960);
    addConst("vbWhite", Vb6Type::Long, 16777215);

    // 字符串常量
    auto addStrConst = [&](const char* name, const char* val) {
        auto sym = std::make_unique<Symbol>(SymbolKind::Constant, name, Vb6Type::String,
            SourceLocation{}, AccessLevel::Public);
        sym->hasConstValue = true;
        sym->constType = Vb6Type::String;
        sym->constStringValue = val;
        sym->isBuiltin = true;
        symTab_.define(std::move(sym));
    };
    addStrConst("vbCr", "\r");
    addStrConst("vbLf", "\n");
    addStrConst("vbCrLf", "\r\n");
    addStrConst("vbNewLine", "\r\n");
    addStrConst("vbNullChar", std::string(1, '\0').c_str());
    addStrConst("vbTab", "\t");
    addStrConst("vbBack", "\b");
    addStrConst("vbFormFeed", "\f");
    addStrConst("vbVerticalTab", "\v");

    // 布尔常量 (虽然词法器已识别 True/False, 但也作为符号注册)
    addConst("vbTrue", Vb6Type::Boolean, -1);
    addConst("vbFalse", Vb6Type::Boolean, 0);

    // 杂项常量
    addConst("vbEmpty", Vb6Type::Long, 0);
    addConst("vbNull", Vb6Type::Long, 1);
    addConst("vbInteger", Vb6Type::Long, 2);
    addConst("vbLong", Vb6Type::Long, 3);
    addConst("vbSingle", Vb6Type::Long, 4);
    addConst("vbDouble", Vb6Type::Long, 5);
    addConst("vbCurrency", Vb6Type::Long, 6);
    addConst("vbDate", Vb6Type::Long, 7);
    addConst("vbString", Vb6Type::Long, 8);
    addConst("vbObject", Vb6Type::Long, 9);
    addConst("vbError", Vb6Type::Long, 10);
    addConst("vbBoolean", Vb6Type::Long, 11);
    addConst("vbVariant", Vb6Type::Long, 12);
    addConst("vbDBNull", Vb6Type::Long, 17);
    addConst("vbByte", Vb6Type::Long, 17);

    // P21-25: Missing VB6 built-in constants

    // Comparison constants
    addConst("vbBinaryCompare", Vb6Type::Long, 0);
    addConst("vbTextCompare", Vb6Type::Long, 1);
    addConst("vbDatabaseCompare", Vb6Type::Long, 2);

    // String conversion constants (StrConv)
    addConst("vbUpperCase", Vb6Type::Long, 1);
    addConst("vbLowerCase", Vb6Type::Long, 2);
    addConst("vbProperCase", Vb6Type::Long, 3);
    addConst("vbWide", Vb6Type::Long, 4);
    addConst("vbNarrow", Vb6Type::Long, 8);
    addConst("vbKatakana", Vb6Type::Long, 16);
    addConst("vbHiragana", Vb6Type::Long, 32);
    addConst("vbUnicode", Vb6Type::Long, 64);
    addConst("vbFromUnicode", Vb6Type::Long, 128);

    // File I/O constants
    addConst("vbNormal", Vb6Type::Long, 0);
    addConst("vbReadOnly", Vb6Type::Long, 1);
    addConst("vbHidden", Vb6Type::Long, 2);
    addConst("vbSystem", Vb6Type::Long, 4);
    addConst("vbVolume", Vb6Type::Long, 8);
    addConst("vbDirectory", Vb6Type::Long, 16);
    addConst("vbArchive", Vb6Type::Long, 32);
    addConst("vbAlias", Vb6Type::Long, 64);

    // File open mode constants
    addConst("vbInput", Vb6Type::Long, 0);
    addConst("vbOutput", Vb6Type::Long, 1);
    addConst("vbRandom", Vb6Type::Long, 4);
    addConst("vbAppend", Vb6Type::Long, 8);
    addConst("vbBinary", Vb6Type::Long, 32);

    // VarType constants (extended)
    addConst("vbDecimal", Vb6Type::Long, 14);
    addConst("vbUserDefinedType", Vb6Type::Long, 36);
    addConst("vbArray", Vb6Type::Long, 8192);
    addConst("vbDataObject", Vb6Type::Long, 13);

    // MsgBox additional constants
    addConst("vbMsgBoxSetForeground", Vb6Type::Long, 65536);
    addConst("vbMsgBoxRight", Vb6Type::Long, 524288);
    addConst("vbMsgBoxRtlReading", Vb6Type::Long, 1048576);
    addConst("vbDefaultButton5", Vb6Type::Long, 1024);

    // Date/time constants
    addConst("vbUseSystemDayOfWeek", Vb6Type::Long, 0);
    addConst("vbSunday", Vb6Type::Long, 1);
    addConst("vbMonday", Vb6Type::Long, 2);
    addConst("vbTuesday", Vb6Type::Long, 3);
    addConst("vbWednesday", Vb6Type::Long, 4);
    addConst("vbThursday", Vb6Type::Long, 5);
    addConst("vbFriday", Vb6Type::Long, 6);
    addConst("vbSaturday", Vb6Type::Long, 7);

    // FirstWeekOfYear constants
    addConst("vbFirstJan1", Vb6Type::Long, 1);
    addConst("vbFirstFourDays", Vb6Type::Long, 2);
    addConst("vbFirstFullWeek", Vb6Type::Long, 3);

    // Calendar constants
    addConst("vbCalGreg", Vb6Type::Long, 0);
    addConst("vbCalHijri", Vb6Type::Long, 1);

    // QueryClose constants (for forms)
    addConst("vbFormControlMenu", Vb6Type::Long, 0);
    addConst("vbFormCode", Vb6Type::Long, 1);
    addConst("vbAppWindows", Vb6Type::Long, 2);
    addConst("vbAppTaskManager", Vb6Type::Long, 3);
    addConst("vbFormMDIForm", Vb6Type::Long, 4);
    addConst("vbFormOwner", Vb6Type::Long, 5);

    // Print method constants (Tab, Spc handled as functions)
    addConst("vbObjectError", Vb6Type::Long, -2147221504);

    // Shift/Ctrl/Alt mask constants (for KeyDown/KeyUp)
    addConst("vbShiftMask", Vb6Type::Long, 1);
    addConst("vbCtrlMask", Vb6Type::Long, 2);
    addConst("vbAltMask", Vb6Type::Long, 4);

    // MouseButton constants
    addConst("vbLeftButton", Vb6Type::Long, 1);
    addConst("vbRightButton", Vb6Type::Long, 2);
    addConst("vbMiddleButton", Vb6Type::Long, 4);

    // Key code constants (commonly used)
    addConst("vbKeyLButton", Vb6Type::Long, 1);
    addConst("vbKeyRButton", Vb6Type::Long, 2);
    addConst("vbKeyCancel", Vb6Type::Long, 3);
    addConst("vbKeyMButton", Vb6Type::Long, 4);
    addConst("vbKeyBack", Vb6Type::Long, 8);
    addConst("vbKeyTab", Vb6Type::Long, 9);
    addConst("vbKeyClear", Vb6Type::Long, 12);
    addConst("vbKeyReturn", Vb6Type::Long, 13);
    addConst("vbKeyShift", Vb6Type::Long, 16);
    addConst("vbKeyControl", Vb6Type::Long, 17);
    addConst("vbKeyMenu", Vb6Type::Long, 18);
    addConst("vbKeyPause", Vb6Type::Long, 19);
    addConst("vbKeyCapital", Vb6Type::Long, 20);
    addConst("vbKeyEscape", Vb6Type::Long, 27);
    addConst("vbKeySpace", Vb6Type::Long, 32);
    addConst("vbKeyPageUp", Vb6Type::Long, 33);
    addConst("vbKeyPageDown", Vb6Type::Long, 34);
    addConst("vbKeyEnd", Vb6Type::Long, 35);
    addConst("vbKeyHome", Vb6Type::Long, 36);
    addConst("vbKeyLeft", Vb6Type::Long, 37);
    addConst("vbKeyUp", Vb6Type::Long, 38);
    addConst("vbKeyRight", Vb6Type::Long, 39);
    addConst("vbKeyDown", Vb6Type::Long, 40);
    addConst("vbKeySelect", Vb6Type::Long, 41);
    addConst("vbKeyPrint", Vb6Type::Long, 42);
    addConst("vbKeyExecute", Vb6Type::Long, 43);
    addConst("vbKeySnapshot", Vb6Type::Long, 44);
    addConst("vbKeyInsert", Vb6Type::Long, 45);
    addConst("vbKeyDelete", Vb6Type::Long, 46);
    addConst("vbKeyHelp", Vb6Type::Long, 47);
    addConst("vbKeyNumlock", Vb6Type::Long, 144);

    // Number key constants
    addConst("vbKey0", Vb6Type::Long, 48);
    addConst("vbKey1", Vb6Type::Long, 49);
    addConst("vbKey2", Vb6Type::Long, 50);
    addConst("vbKey3", Vb6Type::Long, 51);
    addConst("vbKey4", Vb6Type::Long, 52);
    addConst("vbKey5", Vb6Type::Long, 53);
    addConst("vbKey6", Vb6Type::Long, 54);
    addConst("vbKey7", Vb6Type::Long, 55);
    addConst("vbKey8", Vb6Type::Long, 56);
    addConst("vbKey9", Vb6Type::Long, 57);

    // Letter key constants
    addConst("vbKeyA", Vb6Type::Long, 65);
    addConst("vbKeyB", Vb6Type::Long, 66);
    addConst("vbKeyC", Vb6Type::Long, 67);
    addConst("vbKeyD", Vb6Type::Long, 68);
    addConst("vbKeyE", Vb6Type::Long, 69);
    addConst("vbKeyF", Vb6Type::Long, 70);
    addConst("vbKeyG", Vb6Type::Long, 71);
    addConst("vbKeyH", Vb6Type::Long, 72);
    addConst("vbKeyI", Vb6Type::Long, 73);
    addConst("vbKeyJ", Vb6Type::Long, 74);
    addConst("vbKeyK", Vb6Type::Long, 75);
    addConst("vbKeyL", Vb6Type::Long, 76);
    addConst("vbKeyM", Vb6Type::Long, 77);
    addConst("vbKeyN", Vb6Type::Long, 78);
    addConst("vbKeyO", Vb6Type::Long, 79);
    addConst("vbKeyP", Vb6Type::Long, 80);
    addConst("vbKeyQ", Vb6Type::Long, 81);
    addConst("vbKeyR", Vb6Type::Long, 82);
    addConst("vbKeyS", Vb6Type::Long, 83);
    addConst("vbKeyT", Vb6Type::Long, 84);
    addConst("vbKeyU", Vb6Type::Long, 85);
    addConst("vbKeyV", Vb6Type::Long, 86);
    addConst("vbKeyW", Vb6Type::Long, 87);
    addConst("vbKeyX", Vb6Type::Long, 88);
    addConst("vbKeyY", Vb6Type::Long, 89);
    addConst("vbKeyZ", Vb6Type::Long, 90);

    // Numpad key constants
    addConst("vbKeyNumpad0", Vb6Type::Long, 96);
    addConst("vbKeyNumpad1", Vb6Type::Long, 97);
    addConst("vbKeyNumpad2", Vb6Type::Long, 98);
    addConst("vbKeyNumpad3", Vb6Type::Long, 99);
    addConst("vbKeyNumpad4", Vb6Type::Long, 100);
    addConst("vbKeyNumpad5", Vb6Type::Long, 101);
    addConst("vbKeyNumpad6", Vb6Type::Long, 102);
    addConst("vbKeyNumpad7", Vb6Type::Long, 103);
    addConst("vbKeyNumpad8", Vb6Type::Long, 104);
    addConst("vbKeyNumpad9", Vb6Type::Long, 105);
    addConst("vbKeyMultiply", Vb6Type::Long, 106);
    addConst("vbKeyAdd", Vb6Type::Long, 107);
    addConst("vbKeySeparator", Vb6Type::Long, 108);
    addConst("vbKeySubtract", Vb6Type::Long, 109);
    addConst("vbKeyDecimal", Vb6Type::Long, 110);
    addConst("vbKeyDivide", Vb6Type::Long, 111);

    // Function key constants
    addConst("vbKeyF1", Vb6Type::Long, 112);
    addConst("vbKeyF2", Vb6Type::Long, 113);
    addConst("vbKeyF3", Vb6Type::Long, 114);
    addConst("vbKeyF4", Vb6Type::Long, 115);
    addConst("vbKeyF5", Vb6Type::Long, 116);
    addConst("vbKeyF6", Vb6Type::Long, 117);
    addConst("vbKeyF7", Vb6Type::Long, 118);
    addConst("vbKeyF8", Vb6Type::Long, 119);
    addConst("vbKeyF9", Vb6Type::Long, 120);
    addConst("vbKeyF10", Vb6Type::Long, 121);
    addConst("vbKeyF11", Vb6Type::Long, 122);
    addConst("vbKeyF12", Vb6Type::Long, 123);
    addConst("vbKeyF13", Vb6Type::Long, 124);
    addConst("vbKeyF14", Vb6Type::Long, 125);
    addConst("vbKeyF15", Vb6Type::Long, 126);
    addConst("vbKeyF16", Vb6Type::Long, 127);
