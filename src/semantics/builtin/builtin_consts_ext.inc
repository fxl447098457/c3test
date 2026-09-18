// builtin_consts_ext.inc - registerBuiltins 的扩展常量注册段（函数体片段）
// 由 src/semantics/semantic_analyzer_builtin.cpp 拆出（2026-09-17），在函数体内被
// #include，非独立文件。内容 = 原文件第 298~503 行，逐行未改（P21-25 / P22 / P23-04）。
// 依赖上一片段定义的 addConst / addStrConst，必须排在它之后 include。


    // P22: Shell constants (for Shell function window style)
    addConst("vbHide", Vb6Type::Long, 0);
    addConst("vbNormalFocus", Vb6Type::Long, 1);
    addConst("vbMinimizedFocus", Vb6Type::Long, 2);
    addConst("vbMaximizedFocus", Vb6Type::Long, 3);
    addConst("vbNormalNoFocus", Vb6Type::Long, 4);
    addConst("vbMinimizedNoFocus", Vb6Type::Long, 6);
    // P22: Date format constants (for FormatDateTime)
    addConst("vbGeneralDate", Vb6Type::Long, 0);
    addConst("vbLongDate", Vb6Type::Long, 1);
    addConst("vbShortDate", Vb6Type::Long, 2);
    addConst("vbLongTime", Vb6Type::Long, 3);
    addConst("vbShortTime", Vb6Type::Long, 4);
    // P22: System color constants (OLE system colors)
    addConst("vbScrollBars", Vb6Type::Long, -2147483648LL);
    addConst("vbDesktop", Vb6Type::Long, -2147483647LL);
    addConst("vbActiveTitleBar", Vb6Type::Long, -2147483646LL);
    addConst("vbInactiveTitleBar", Vb6Type::Long, -2147483645LL);
    addConst("vbMenu", Vb6Type::Long, -2147483644LL);
    addConst("vbWindowBackground", Vb6Type::Long, -2147483643LL);
    addConst("vbWindowFrame", Vb6Type::Long, -2147483642LL);
    addConst("vbMenuText", Vb6Type::Long, -2147483641LL);
    addConst("vbTitleBarText", Vb6Type::Long, -2147483639LL);
    // Fix 131: vbWindowTitle (0x80000008) 缺失, vbWindowText 原误为 ...640
    addConst("vbWindowTitle", Vb6Type::Long, -2147483640LL);
    addConst("vbWindowText", Vb6Type::Long, -2147483639LL);
    addConst("vbActiveBorder", Vb6Type::Long, -2147483638LL);
    addConst("vbInactiveBorder", Vb6Type::Long, -2147483637LL);
    addConst("vbApplicationWorkspace", Vb6Type::Long, -2147483636LL);
    addConst("vbHighlight", Vb6Type::Long, -2147483635LL);
    addConst("vbHighlightText", Vb6Type::Long, -2147483634LL);
    addConst("vbButtonFace", Vb6Type::Long, -2147483633LL);
    addConst("vbButtonShadow", Vb6Type::Long, -2147483632LL);
    addConst("vbGrayText", Vb6Type::Long, -2147483631LL);
    addConst("vbButtonText", Vb6Type::Long, -2147483630LL);
    addConst("vbInactiveCaptionText", Vb6Type::Long, -2147483629LL);
    addConst("vb3DHighlight", Vb6Type::Long, -2147483628LL);
    addConst("vb3DDKShadow", Vb6Type::Long, -2147483627LL);
    addConst("vb3DLight", Vb6Type::Long, -2147483626LL);
    // Fix 131: 原表 vb3DFace=-2147483625 / vb3DShadow=-2147483624 整体错位 ——
    // 那两个值其实是 vbInfoText/vbInfoBackground (淡黄), 导致仪表 Caption2
    // "Band/Visitors/Volume" 渲染成淡黄色而非 vb3DShadow 深灰。
    addConst("vbInfoText", Vb6Type::Long, -2147483625LL);       // 0x80000017
    addConst("vbInfoBackground", Vb6Type::Long, -2147483624LL); // 0x80000018
    addConst("vb3DFace", Vb6Type::Long, -2147483633LL);         // 0x8000000F = vbButtonFace
    addConst("vb3DShadow", Vb6Type::Long, -2147483632LL);       // 0x80000010 = vbButtonShadow
    // P22: IME status constants
    addConst("vbIMEModeNoControl", Vb6Type::Long, 0);
    addConst("vbIMEModeOn", Vb6Type::Long, 1);
    addConst("vbIMEModeOff", Vb6Type::Long, 2);
    addConst("vbIMEModeDisable", Vb6Type::Long, 3);
    addConst("vbIMEModeHiragana", Vb6Type::Long, 4);
    addConst("vbIMEModeKatakana", Vb6Type::Long, 5);
    addConst("vbIMEModeKatakanaHalf", Vb6Type::Long, 6);
    addConst("vbIMEModeAlphaFull", Vb6Type::Long, 7);
    addConst("vbIMEModeAlpha", Vb6Type::Long, 8);
    addConst("vbIMEModeHangulFull", Vb6Type::Long, 9);
    addConst("vbIMEModeHangul", Vb6Type::Long, 10);

    // P22-09: Printer constants
    addConst("vbPRORPortrait", Vb6Type::Long, 1);        // Printer orientation: Portrait
    addConst("vbPRORLandscape", Vb6Type::Long, 2);       // Printer orientation: Landscape
    addConst("vbPRPQDraft", Vb6Type::Long, -1);          // Print quality: Draft
    addConst("vbPRPQLow", Vb6Type::Long, -2);            // Print quality: Low
    addConst("vbPRPQMedium", Vb6Type::Long, -3);         // Print quality: Medium
    addConst("vbPRPQHigh", Vb6Type::Long, -4);           // Print quality: High
    addConst("vbPRCMMillimeters", Vb6Type::Long, 1);     // Page scale: Millimeters
    addConst("vbPRCMCentimeters", Vb6Type::Long, 2);     // Page scale: Centimeters
    addConst("vbPRCMInches", Vb6Type::Long, 3);          // Page scale: Inches
    addConst("vbPRCMCharacters", Vb6Type::Long, 4);      // Page scale: Characters
    addConst("vbPRBPSingle", Vb6Type::Long, 1);          // Binary performation: Single
    addConst("vbPRBPSDouble", Vb6Type::Long, 2);         // Binary performation: Double
    addConst("vbPRBPTriple", Vb6Type::Long, 3);          // Binary performation: Triple
    addConst("vbPRDPHorizontal", Vb6Type::Long, 1);      // Duplex: Horizontal
    addConst("vbPRDPVertical", Vb6Type::Long, 2);        // Duplex: Vertical

    // P22-09: Other missing constants
    addConst("vbUseSystem", Vb6Type::Long, -1);           // Use system setting
    addConst("vbUseCompareOption", Vb6Type::Long, -1);   // Use Option Compare setting
    addConst("Win16", Vb6Type::Long, 0);                  // Obsolete: always False
    addConst("Win32", Vb6Type::Long, -1);                 // True on 32-bit Windows
    addConst("vbDot", Vb6Type::Long, 46);                 // "." character code


    // P22-09: MsgBox additional constants
    addConst("vbMsgBoxHelpButton", Vb6Type::Long, 16384); // &H4000


    // --- P23-04: Low-frequency constants (85 additions) ---

    // VbCallType (CallType)
    addConst("vbMethod", Vb6Type::Long, 2);
    addConst("vbGet", Vb6Type::Long, 3);
    addConst("vbLet", Vb6Type::Long, 1);
    addConst("vbSet", Vb6Type::Long, 4);

    // VbCalendar

    // VbQueryClose

    // Clipboard format constants
    addConst("vbCFText", Vb6Type::Long, 1);
    addConst("vbCFBitmap", Vb6Type::Long, 2);
    addConst("vbCFMetafile", Vb6Type::Long, 3);
    addConst("vbCFDIB", Vb6Type::Long, 8);
    addConst("vbCFPalette", Vb6Type::Long, 9);
    addConst("vbCFRTF", Vb6Type::Long, -16639);       // &HFFFFBF01
    addConst("vbCFEMetafile", Vb6Type::Long, 14);

    // DriveType constants
    addConst("vbDriveTypeRemovable", Vb6Type::Long, 1);
    addConst("vbDriveTypeFixed", Vb6Type::Long, 2);
    addConst("vbDriveTypeNetwork", Vb6Type::Long, 3);
    addConst("vbDriveTypeCDRom", Vb6Type::Long, 4);
    addConst("vbDriveTypeRAMDisk", Vb6Type::Long, 5);

    // VbAppWinStyle
    addConst("vbAppWinStyleNormal", Vb6Type::Long, 1);
    addConst("vbAppWinStyleMinimize", Vb6Type::Long, 2);
    addConst("vbAppWinStyleMaximize", Vb6Type::Long, 3);

    // VbIMEStatus (distinct from IMEMode)
    addConst("vbIMEOn", Vb6Type::Long, 1);
    addConst("vbIMEOff", Vb6Type::Long, 0);
    addConst("vbIMEDisable", Vb6Type::Long, 2);
    addConst("vbIMEHiragana", Vb6Type::Long, 4);
    addConst("vbIMEKatakana", Vb6Type::Long, 5);
    addConst("vbIMEKatakanaHalf", Vb6Type::Long, 6);
    addConst("vbIMEAlphaFull", Vb6Type::Long, 7);
    addConst("vbIMEAlpha", Vb6Type::Long, 8);

    // Shape control constants
    addConst("vbShapeRectangle", Vb6Type::Long, 0);
    addConst("vbShapeSquare", Vb6Type::Long, 1);
    addConst("vbShapeOval", Vb6Type::Long, 2);
    addConst("vbShapeCircle", Vb6Type::Long, 3);
    addConst("vbShapeRoundedRectangle", Vb6Type::Long, 4);
    addConst("vbShapeRoundedSquare", Vb6Type::Long, 5);

    // BorderStyle (Shape/Line)
    addConst("vbTransparent", Vb6Type::Long, 0);
    addConst("vbBSSolid", Vb6Type::Long, 1);
    addConst("vbBSDash", Vb6Type::Long, 2);
    addConst("vbBSDot", Vb6Type::Long, 3);
    addConst("vbBSDashDot", Vb6Type::Long, 4);
    addConst("vbBSDashDotDot", Vb6Type::Long, 5);
    addConst("vbBSInsideSolid", Vb6Type::Long, 6);

    // FillStyle constants
    addConst("vbFSSolid", Vb6Type::Long, 0);
    addConst("vbFSTransparent", Vb6Type::Long, 1);
    addConst("vbFSHorizontalLine", Vb6Type::Long, 2);
    addConst("vbFSVerticalLine", Vb6Type::Long, 3);
    addConst("vbFSUpwardDiagonal", Vb6Type::Long, 4);
    addConst("vbFSDownwardDiagonal", Vb6Type::Long, 5);
    addConst("vbFSCross", Vb6Type::Long, 6);
    addConst("vbFSDiagonalCross", Vb6Type::Long, 7);

    // MousePointer constants
    addConst("vbDefault", Vb6Type::Long, 0);
    addConst("vbArrow", Vb6Type::Long, 1);
    addConst("vbCrosshair", Vb6Type::Long, 2);
    addConst("vbIbeam", Vb6Type::Long, 3);
    addConst("vbIconPointer", Vb6Type::Long, 4);
    addConst("vbSizePointer", Vb6Type::Long, 5);
    addConst("vbSizeNESW", Vb6Type::Long, 6);
    addConst("vbSizeNS", Vb6Type::Long, 7);
    addConst("vbSizeNWSE", Vb6Type::Long, 8);
    addConst("vbSizeEW", Vb6Type::Long, 9);
    addConst("vbUpArrow", Vb6Type::Long, 10);
    addConst("vbHourglass", Vb6Type::Long, 11);
    addConst("vbNoDrop", Vb6Type::Long, 12);
    addConst("vbArrowHourglass", Vb6Type::Long, 13);
    addConst("vbArrowQuestion", Vb6Type::Long, 14);
    addConst("vbSizeAll", Vb6Type::Long, 15);
    addConst("vbCustom", Vb6Type::Long, 99);

    // Alignment constants
    addConst("vbLeftJustify", Vb6Type::Long, 0);
    addConst("vbRightJustify", Vb6Type::Long, 1);
    addConst("vbCenter", Vb6Type::Long, 2);

    // ScrollBar constants
    addConst("vbSBNone", Vb6Type::Long, 0);
    addConst("vbSBHorizontal", Vb6Type::Long, 1);
    addConst("vbSBVertical", Vb6Type::Long, 2);
    addConst("vbSBBoth", Vb6Type::Long, 3);

    // ScaleMode constants
    addConst("vbTwips", Vb6Type::Long, 1);
    addConst("vbPoints", Vb6Type::Long, 2);
    addConst("vbPixels", Vb6Type::Long, 3);
    addConst("vbCharacters", Vb6Type::Long, 4);
    addConst("vbInches", Vb6Type::Long, 5);
    addConst("vbMillimeters", Vb6Type::Long, 6);
    addConst("vbCentimeters", Vb6Type::Long, 7);
    addConst("vbHimetric", Vb6Type::Long, 8);
    addConst("vbContainerPosition", Vb6Type::Long, 9);
    addConst("vbContainerSize", Vb6Type::Long, 10);
    addConst("vbUser", Vb6Type::Long, 0);

    // WindowState constants
    addConst("vbMinimized", Vb6Type::Long, 1);
    addConst("vbMaximized", Vb6Type::Long, 2);

    // Fix 056: CheckBox constants
    addConst("vbUnchecked", Vb6Type::Long, 0);
    addConst("vbChecked", Vb6Type::Long, 1);
    addConst("vbGrayed", Vb6Type::Long, 2);

    // String constants (additional)
    addStrConst("vbNullString", "");
