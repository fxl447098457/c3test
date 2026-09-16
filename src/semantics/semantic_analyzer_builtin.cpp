#include "semantics/semantic_analyzer.hpp"
#include <algorithm>
#include <cctype>
#include <tuple>
#include <initializer_list>
#include "semantics/semantic_analyzer_internal.h"

namespace vb6c3 {

// --- semantic_analyzer_builtin.cpp: 内置函数/常量注册 (registerBuiltins) ---


void SemanticAnalyzer::registerBuiltins() {
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
    addConst("vbWindowText", Vb6Type::Long, -2147483640LL);
    addConst("vbTitleBarText", Vb6Type::Long, -2147483639LL);
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
    addConst("vb3DFace", Vb6Type::Long, -2147483625LL);
    addConst("vb3DShadow", Vb6Type::Long, -2147483624LL);
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

    // 内置对象: Debug, Err, Screen, App, Printer
    auto addObj = [&](const char* name) {
        auto sym = std::make_unique<Symbol>(SymbolKind::Variable, name, Vb6Type::Object,
            SourceLocation{}, AccessLevel::Public);
        sym->isBuiltin = true;
        symTab_.define(std::move(sym));
    };
    addObj("Debug");
    addObj("Err");
    addObj("Screen");
    addObj("App");
    addObj("Printer");
    addObj("Forms");
    addObj("Clipboard");

    // 内置函数 (声明为 Function 符号, 避免未声明错误)
    auto addBuiltinFunc = [&](const char* name, Vb6Type retType) {
        auto sym = std::make_unique<Symbol>(SymbolKind::Function, name, retType,
            SourceLocation{}, AccessLevel::Public);
        sym->isBuiltin = true;
        symTab_.define(std::move(sym));
    };
    // Fix 030b: 带参数签名的内置函数注册 — 填充 sym->params, 让 IndexOrCallExpr 的
    // calleeParams 查找命中, 从而触发 Fix 024 P2 (ByVal Variant 正向包装) 和
    // Fix 029 (ByVal 具体类型反向提取). params: vector of (name, type, isByVal, isOptional).
    auto addBuiltinFuncWithParams = [&](const char* name, Vb6Type retType,
                                        std::initializer_list<std::tuple<const char*, Vb6Type, bool, bool>> params) {
        auto sym = std::make_unique<Symbol>(SymbolKind::Function, name, retType,
            SourceLocation{}, AccessLevel::Public);
        sym->isBuiltin = true;
        for (auto& p : params) {
            ParameterInfo pi;
            pi.name = std::get<0>(p);
            pi.type = std::get<1>(p);
            pi.isByVal = std::get<2>(p);
            pi.isOptional = std::get<3>(p);
            sym->params.push_back(std::move(pi));
        }
        symTab_.define(std::move(sym));
    };
    // Fix 034: Variant|Array 复合类型 (Variant 含数组), 供多个内置函数的数组参数
    // 声明使用 — 反向提取时 paramIsArray 分支会触发 vb6_VariantToSafeArray1D.
    const Vb6Type kVariantArray = static_cast<Vb6Type>(
        static_cast<uint16_t>(Vb6Type::Variant) | static_cast<uint16_t>(Vb6Type::Array));
    // Fix 034: 字符串/转换/数值/类型检查/数学/文件/日期等内置函数补全参数签名 —
    // 让 IndexOrCallExpr 的 calleeParams 查找命中, 触发 Fix 024 P2 (ByVal Variant 正向包装) 和
    // Fix 029 (ByVal 具体类型反向提取), 消除大量 C2440 (函数实参 → VARIANT/BSTR/double 等).
    // RTL C 签名参考 src/rtl/core/vb6rtl.h. VB6 Optional 参数标记 isOptional=true (语义保留),
    // 但 RTL C 函数不接受 Optional padding 和 IsMissing _has_ 尾叜 — cgen 用硬编码补默认值
    // (见 cgen_expr.cpp line 3140+ 和 line 3263+).
    // 字符串函数 (RTL 签名: vb6_Len/Left/Right/Mid/InStr/InStrRev 等均接受 BSTR)
    addBuiltinFuncWithParams("Len", Vb6Type::Long,
        {{"s", Vb6Type::String, true, false}});
    addBuiltinFuncWithParams("Left", Vb6Type::String,
        {{"s", Vb6Type::String, true, false}, {"n", Vb6Type::Long, true, false}});
    addBuiltinFuncWithParams("Right", Vb6Type::String,
        {{"s", Vb6Type::String, true, false}, {"n", Vb6Type::Long, true, false}});
    addBuiltinFuncWithParams("Mid", Vb6Type::String,
        {{"s", Vb6Type::String, true, false}, {"start", Vb6Type::Long, true, false},
         {"len", Vb6Type::Long, true, true}});
    // InStr: VB6 双形态 InStr(s1, s2) 和 InStr(start, s1, s2). cgen 用 argList="1, "+argList
    // 把 2-arg 形状调整为 3-arg (start=1). 为避免 2-arg 形态下 arg[0]=s1 被 calleeParams[0]
    // (start:Long) 错误包装 (Variant→vb6_VariantToLong 而非 vb6_VariantToString),
    // 不声明 calleeParams — 让 cgen 硬编码重排实参位置后直接调用.
    addBuiltinFunc("InStr", Vb6Type::Long);
    addBuiltinFuncWithParams("InStrRev", Vb6Type::Long,
        {{"string1", Vb6Type::String, true, false}, {"string2", Vb6Type::String, true, false},
         {"start", Vb6Type::Long, true, true}, {"compare", Vb6Type::Long, true, true}});
    addBuiltinFuncWithParams("LTrim", Vb6Type::String, {{"s", Vb6Type::String, true, false}});
    addBuiltinFuncWithParams("RTrim", Vb6Type::String, {{"s", Vb6Type::String, true, false}});
    addBuiltinFuncWithParams("Trim", Vb6Type::String, {{"s", Vb6Type::String, true, false}});
    addBuiltinFuncWithParams("LCase", Vb6Type::String, {{"s", Vb6Type::String, true, false}});
    addBuiltinFuncWithParams("UCase", Vb6Type::String, {{"s", Vb6Type::String, true, false}});
    addBuiltinFuncWithParams("Replace", Vb6Type::String,
        {{"expr", Vb6Type::String, true, false}, {"find", Vb6Type::String, true, false},
         {"rep", Vb6Type::String, true, false}, {"start", Vb6Type::Long, true, true},
         {"count", Vb6Type::Long, true, true}, {"compare", Vb6Type::Long, true, true}});
    addBuiltinFuncWithParams("Split", Vb6Type::Variant,
        {{"expr", Vb6Type::String, true, false}, {"delimiter", Vb6Type::String, true, true},
         {"limit", Vb6Type::Long, true, true}, {"compare", Vb6Type::Long, true, true}});
    addBuiltinFuncWithParams("Join", Vb6Type::String,
        {{"arr", kVariantArray, true, false}, {"delimiter", Vb6Type::String, true, true}});
    addBuiltinFuncWithParams("StrComp", Vb6Type::Long,
        {{"s1", Vb6Type::String, true, false}, {"s2", Vb6Type::String, true, false},
         {"compare", Vb6Type::Long, true, true}});
    addBuiltinFuncWithParams("StrReverse", Vb6Type::String, {{"s", Vb6Type::String, true, false}});
    addBuiltinFuncWithParams("Space", Vb6Type::String, {{"n", Vb6Type::Long, true, false}});
    // String(n, charCode) — charCode 在 VB6 可为 String 或 Integer; RTL 取 int32_t.
    addBuiltinFuncWithParams("String", Vb6Type::String,
        {{"n", Vb6Type::Long, true, false}, {"charCode", Vb6Type::Long, true, false}});
    addBuiltinFuncWithParams("Asc", Vb6Type::Long, {{"s", Vb6Type::String, true, false}});
    addBuiltinFuncWithParams("Chr", Vb6Type::String, {{"code", Vb6Type::Long, true, false}});
    addBuiltinFuncWithParams("Val", Vb6Type::Double, {{"s", Vb6Type::String, true, false}});
    addBuiltinFuncWithParams("Str", Vb6Type::String, {{"n", Vb6Type::Long, true, false}});
    // Format: 保留 addBuiltinFunc (无 calleeParams) — cgen_expr.cpp:3487 会按 inferExprType
    // 把 args[0] 包装成 vb6_VariantLong/Double/String/Int. 若此处加 ByVal Variant calleeParams,
    // Fix 024 P2 会先用 vb6_VariantFromValue 包装(返回 VARIANT), 然后 Format 特殊包装
    // vb6_VariantLong(VARIANT) 传 int32_t 参数 → C2440. 双重包装冲突, 故不注册参数.
    addBuiltinFunc("Format", Vb6Type::String);
    // 类型转换函数 — cgen 特殊处理决定参数策略.
    // CStr/CInt/CLng/CDbl: cgen_expr.cpp:3383/3428 有 callee-rewrite 特殊分支
    //   - CStr: 根据 arg 类型改写 callee 为 vb6_CStrLong/Dbl/Bool/Date (具体类型参数)
    //   - CInt/CLng/CDbl: 已知 Variant 变量改写 callee 为 vb6_CIntV/CLngV/CDblV (VARIANT 参数)
    // 若此处加 calleeParams, Fix 024 P2/Fix 029 会先 wrapping, 与 callee-rewrite 冲突 → C2440.
    // 故保持 addBuiltinFunc (无 params), 让特殊分支独立工作.
    addBuiltinFunc("CStr", Vb6Type::String);
    addBuiltinFunc("CInt", Vb6Type::Integer);
    addBuiltinFunc("CLng", Vb6Type::Long);
    addBuiltinFuncWithParams("CSng", Vb6Type::Single, {{"v", Vb6Type::Double, true, false}});
    addBuiltinFunc("CDbl", Vb6Type::Double);
    addBuiltinFuncWithParams("CBool", Vb6Type::Boolean, {{"v", Vb6Type::Double, true, false}});
    addBuiltinFuncWithParams("CDate", Vb6Type::Date, {{"v", Vb6Type::Variant, true, false}});
    addBuiltinFuncWithParams("CByte", Vb6Type::Byte, {{"v", Vb6Type::Double, true, false}});
    addBuiltinFuncWithParams("CCur", Vb6Type::Currency, {{"v", Vb6Type::Double, true, false}});
    addBuiltinFuncWithParams("CDec", Vb6Type::Variant, {{"v", Vb6Type::Variant, true, false}});
    addBuiltinFunc("CVar", Vb6Type::Variant);
    addBuiltinFuncWithParams("Hex", Vb6Type::String, {{"n", Vb6Type::Long, true, false}});
    addBuiltinFuncWithParams("Oct", Vb6Type::String, {{"n", Vb6Type::Long, true, false}});
    addBuiltinFuncWithParams("CVErr", Vb6Type::Variant, {{"errorNumber", Vb6Type::Long, true, false}});
    // 数值函数 — RTL C 签名: 所有数值函数接受 double.
    addBuiltinFuncWithParams("Abs", Vb6Type::Double, {{"x", Vb6Type::Double, true, false}});
    addBuiltinFuncWithParams("Int", Vb6Type::Double, {{"x", Vb6Type::Double, true, false}});
    addBuiltinFuncWithParams("Fix", Vb6Type::Double, {{"x", Vb6Type::Double, true, false}});
    // Fix 029: Sgn 返回 Long 但接受 double. Variant 实参 → vb6_VariantToDouble 提取.
    addBuiltinFuncWithParams("Sgn", Vb6Type::Long, {{"x", Vb6Type::Double, true, false}});
    addBuiltinFuncWithParams("Sqr", Vb6Type::Double, {{"x", Vb6Type::Double, true, false}});
    addBuiltinFuncWithParams("Round", Vb6Type::Double,
        {{"x", Vb6Type::Double, true, false}, {"decimals", Vb6Type::Long, true, true}});
    // Rnd/Randomize: Optional 参数. 不补 calleeParams — 现有 cgen 不补默认值会 C2198,
    // 但补了 calleeParams 又会被 !calleeIsBuiltin guard 跳过 padding. Fix 034 额外在
    // cgen_expr.cpp 中为这两个加硬编码 padding (无 calleeParams, 走老路径).
    addBuiltinFunc("Rnd", Vb6Type::Single);
    // 转换与类型检查 — RTL: IsXxx 接受 vb6_VARIANT by-value.
    addBuiltinFuncWithParams("IsNumeric", Vb6Type::Boolean, {{"v", Vb6Type::Variant, true, false}});
    addBuiltinFuncWithParams("IsDate", Vb6Type::Boolean, {{"v", Vb6Type::Variant, true, false}});
    addBuiltinFuncWithParams("IsEmpty", Vb6Type::Boolean, {{"v", Vb6Type::Variant, true, false}});
    addBuiltinFuncWithParams("IsNull", Vb6Type::Boolean, {{"v", Vb6Type::Variant, true, false}});
    addBuiltinFuncWithParams("IsObject", Vb6Type::Boolean, {{"v", Vb6Type::Variant, true, false}});
    addBuiltinFuncWithParams("IsArray", Vb6Type::Boolean, {{"v", Vb6Type::Variant, true, false}});
    addBuiltinFunc("IsNothing", Vb6Type::Boolean);
    addBuiltinFuncWithParams("IsError", Vb6Type::Boolean, {{"v", Vb6Type::Variant, true, false}});
    addBuiltinFuncWithParams("TypeName", Vb6Type::String, {{"v", Vb6Type::Variant, true, false}});
    addBuiltinFuncWithParams("VarType", Vb6Type::Long, {{"v", Vb6Type::Variant, true, false}});
    // 数组
    // Fix 030b: UBound/LBound 注册带参数签名 — 让 Fix 029 反向提取能在 Variant 实参上
    // 触发 vb6_VariantToSafeArray1D, 修正 C2440 (vb6_VARIANT → vb6_SafeArray1D*) 子类.
    // RTL: int32_t vb6_UBound(vb6_SafeArray1D* safeArray, int32_t dimension);
    // VB6: UBound(arrayname[, dimension]) — arrayname 接受 Variant 含数组或类型化数组.
    // 第 1 参数用 Variant|Array (不纯 Variant, 让反向提取 paramIsArray 分支命中).
    addBuiltinFuncWithParams("UBound", Vb6Type::Long,
        {{"array", kVariantArray, true, false}, {"dimension", Vb6Type::Long, true, true}});
    addBuiltinFuncWithParams("LBound", Vb6Type::Long,
        {{"array", kVariantArray, true, false}, {"dimension", Vb6Type::Long, true, true}});
    addBuiltinFunc("Array", Vb6Type::Variant);
    // 数学 — RTL: sin/cos/tan/atan/exp/log 均接受 double.
    addBuiltinFuncWithParams("Sin", Vb6Type::Double, {{"x", Vb6Type::Double, true, false}});
    addBuiltinFuncWithParams("Cos", Vb6Type::Double, {{"x", Vb6Type::Double, true, false}});
    addBuiltinFuncWithParams("Tan", Vb6Type::Double, {{"x", Vb6Type::Double, true, false}});
    addBuiltinFuncWithParams("Atn", Vb6Type::Double, {{"x", Vb6Type::Double, true, false}});
    addBuiltinFuncWithParams("Exp", Vb6Type::Double, {{"x", Vb6Type::Double, true, false}});
    addBuiltinFuncWithParams("Log", Vb6Type::Double, {{"x", Vb6Type::Double, true, false}});
    // 文件 — RTL: LOF/EOF/Loc/Seek 接受 int32_t filenumber; GetAttr/FileLen/FileDateTime/Environ 接受 BSTR.
    addBuiltinFunc("FreeFile", Vb6Type::Long);
    addBuiltinFuncWithParams("LOF", Vb6Type::Long, {{"filenumber", Vb6Type::Long, true, false}});
    addBuiltinFuncWithParams("EOF", Vb6Type::Boolean, {{"filenumber", Vb6Type::Long, true, false}});
    addBuiltinFuncWithParams("Loc", Vb6Type::Long, {{"filenumber", Vb6Type::Long, true, false}});
    // GetAttr 在 vbcrtl.h 中声明为接受 BSTR pathname.
    addBuiltinFuncWithParams("GetAttr", Vb6Type::Long, {{"pathname", Vb6Type::String, true, false}});
    addBuiltinFunc("SetAttr", Vb6Type::Void);
    addBuiltinFuncWithParams("Seek", Vb6Type::Long, {{"filenumber", Vb6Type::Long, true, false}});
    addBuiltinFuncWithParams("FileLen", Vb6Type::Long, {{"pathname", Vb6Type::String, true, false}});
    // FileAttr(filenumber, attribute) — 2 个 ByRefLong 参数
    addBuiltinFuncWithParams("FileAttr", Vb6Type::Long,
        {{"filenumber", Vb6Type::Long, true, false}, {"attribute", Vb6Type::Long, true, false}});
    addBuiltinFuncWithParams("FileDateTime", Vb6Type::Date, {{"pathname", Vb6Type::String, true, false}});
    addBuiltinFuncWithParams("Dir", Vb6Type::String,
        {{"pathname", Vb6Type::String, true, false}, {"attributes", Vb6Type::Long, true, true}});
    addBuiltinFuncWithParams("CurDir", Vb6Type::String, {{"drive", Vb6Type::String, true, true}});
    addBuiltinFuncWithParams("Shell", Vb6Type::Long,
        {{"pathname", Vb6Type::String, true, false}, {"windowstyle", Vb6Type::Long, true, true}});
    addBuiltinFuncWithParams("Environ", Vb6Type::String, {{"envstring", Vb6Type::String, true, false}});
    addBuiltinFunc("Command", Vb6Type::String);
    // 日期时间 — RTL: Year/Month/Day/Hour/Minute/Second 接受 double (Date),
    // DateAdd(interval, number, date), DateDiff(interval, d1, d2, ...), DateSerial(y,m,d),
    // DateValue(BSTR), TimeSerial(h,m,s), TimeValue(BSTR), Weekday(date[, firstDayOfWeek]).
    addBuiltinFunc("Now", Vb6Type::Date);
    addBuiltinFunc("Date", Vb6Type::Date);
    addBuiltinFunc("Time", Vb6Type::Date);
    addBuiltinFuncWithParams("DateAdd", Vb6Type::Date,
        {{"interval", Vb6Type::String, true, false}, {"number", Vb6Type::Double, true, false},
         {"date", Vb6Type::Double, true, false}});
    addBuiltinFuncWithParams("DateDiff", Vb6Type::Long,
        {{"interval", Vb6Type::String, true, false}, {"date1", Vb6Type::Double, true, false},
         {"date2", Vb6Type::Double, true, false}, {"firstDayOfWeek", Vb6Type::Long, true, true},
         {"firstWeekOfYear", Vb6Type::Long, true, true}});
    addBuiltinFuncWithParams("DatePart", Vb6Type::Long,
        {{"interval", Vb6Type::String, true, false}, {"date", Vb6Type::Double, true, false},
         {"firstDayOfWeek", Vb6Type::Long, true, true}, {"firstWeekOfYear", Vb6Type::Long, true, true}});
    addBuiltinFuncWithParams("DateSerial", Vb6Type::Double,
        {{"year", Vb6Type::Long, true, false}, {"month", Vb6Type::Long, true, false},
         {"day", Vb6Type::Long, true, false}});
    addBuiltinFuncWithParams("DateValue", Vb6Type::Date, {{"dateStr", Vb6Type::String, true, false}});
    addBuiltinFuncWithParams("TimeSerial", Vb6Type::Date,
        {{"hour", Vb6Type::Long, true, false}, {"minute", Vb6Type::Long, true, false},
         {"second", Vb6Type::Long, true, false}});
    addBuiltinFuncWithParams("TimeValue", Vb6Type::Date, {{"timeStr", Vb6Type::String, true, false}});
    addBuiltinFuncWithParams("Year", Vb6Type::Long, {{"date", Vb6Type::Double, true, false}});
    addBuiltinFuncWithParams("Month", Vb6Type::Long, {{"date", Vb6Type::Double, true, false}});
    addBuiltinFuncWithParams("Day", Vb6Type::Long, {{"date", Vb6Type::Double, true, false}});
    addBuiltinFuncWithParams("Hour", Vb6Type::Long, {{"time", Vb6Type::Double, true, false}});
    addBuiltinFuncWithParams("Minute", Vb6Type::Long, {{"time", Vb6Type::Double, true, false}});
    addBuiltinFuncWithParams("Second", Vb6Type::Long, {{"time", Vb6Type::Double, true, false}});
    addBuiltinFuncWithParams("Weekday", Vb6Type::Long,
        {{"date", Vb6Type::Double, true, false}, {"firstDayOfWeek", Vb6Type::Long, true, true}});

    // 交互
    addBuiltinFunc("MsgBox", Vb6Type::Long);
    addBuiltinFunc("InputBox", Vb6Type::String);
    // RTL: vb6_RGB(int32_t r, int32_t g, int32_t b); vb6_QBColor(int32_t n).
    addBuiltinFuncWithParams("RGB", Vb6Type::Long,
        {{"r", Vb6Type::Long, true, false}, {"g", Vb6Type::Long, true, false},
         {"b", Vb6Type::Long, true, false}});
    addBuiltinFuncWithParams("QBColor", Vb6Type::Long, {{"n", Vb6Type::Long, true, false}});
    // 杂项
    addBuiltinFunc("DoEvents", Vb6Type::Long);
    addBuiltinFunc("Erl", Vb6Type::Long);
    // Tab/Spc 用于 Print 语句, RTL 接受 int32_t.
    addBuiltinFuncWithParams("Tab", Vb6Type::String, {{"column", Vb6Type::Long, true, false}});
    addBuiltinFuncWithParams("Spc", Vb6Type::String, {{"count", Vb6Type::Long, true, false}});
    // RTL: vb6_CreateObject(const wchar_t* progId); vb6_GetObject(pathName, progId).
    addBuiltinFuncWithParams("CreateObject", Vb6Type::Object,
        {{"progId", Vb6Type::String, true, false}});
    addBuiltinFuncWithParams("GetObject", Vb6Type::Object,
        {{"pathName", Vb6Type::String, true, true}, {"progId", Vb6Type::String, true, true}});
    // LoadPicture(pathname) 返回 void* (Object). RTL: vb6_LoadPictureEx(BSTR).
    addBuiltinFuncWithParams("LoadPicture", Vb6Type::Object, {{"pathname", Vb6Type::String, true, false}});
    addBuiltinFunc("SavePicture", Vb6Type::Void);
    addBuiltinFunc("SaveSetting", Vb6Type::Void);
    addBuiltinFunc("GetSetting", Vb6Type::String);
    addBuiltinFunc("DeleteSetting", Vb6Type::Void);
    addBuiltinFunc("GetAllSettings", Vb6Type::Variant);
    addBuiltinFunc("Load", Vb6Type::Void);
    addBuiltinFunc("Unload", Vb6Type::Void);
    addBuiltinFunc("SendKeys", Vb6Type::Void);
    addBuiltinFunc("AppActivate", Vb6Type::Void);
    addBuiltinFunc("IsMissing", Vb6Type::Boolean);
    addBuiltinFunc("IIf", Vb6Type::Variant);
    addBuiltinFunc("Beep", Vb6Type::Void);
// P14.3.5: CallByName(obj, procName$, callType, [args...])
addBuiltinFunc("CallByName", Vb6Type::Variant);
    // P18-D: 兼容性填平新增内置函数 — RTL: AscW/AscB 接受 BSTR, ChrW/ChrB 接受 int32_t,
    // StrConv(text, conversion, localeID) 全 3 参.
    addBuiltinFuncWithParams("AscW", Vb6Type::Long, {{"s", Vb6Type::String, true, false}});
    addBuiltinFuncWithParams("ChrW", Vb6Type::String, {{"code", Vb6Type::Long, true, false}});
    addBuiltinFuncWithParams("AscB", Vb6Type::Long, {{"s", Vb6Type::String, true, false}});
    addBuiltinFuncWithParams("ChrB", Vb6Type::String, {{"code", Vb6Type::Long, true, false}});
    addBuiltinFunc("Timer", Vb6Type::Single);
    addBuiltinFuncWithParams("StrConv", Vb6Type::String,
        {{"text", Vb6Type::String, true, false}, {"conversion", Vb6Type::Long, true, false},
         {"localeID", Vb6Type::Long, true, true}});
    addBuiltinFunc("Filter", Vb6Type::Variant);
    addBuiltinFunc("VarPtr", Vb6Type::Long);
    addBuiltinFunc("StrPtr", Vb6Type::Long);
    addBuiltinFunc("ObjPtr", Vb6Type::Long);
    addBuiltinFunc("LSet", Vb6Type::String);
    addBuiltinFunc("RSet", Vb6Type::String);
    addBuiltinFunc("WeekdayName", Vb6Type::String);
    addBuiltinFunc("MonthName", Vb6Type::String);
    addBuiltinFunc("FormatCurrency", Vb6Type::String);
    addBuiltinFunc("FormatNumber", Vb6Type::String);
    addBuiltinFunc("FormatPercent", Vb6Type::String);
    addBuiltinFunc("FormatDateTime", Vb6Type::String);
    // P18-E: Financial / Choose / Switch / Lock / Reset / Partition
    addBuiltinFunc("Choose", Vb6Type::Variant);
    addBuiltinFunc("Switch", Vb6Type::Variant);
    addBuiltinFunc("SLN", Vb6Type::Double);
    addBuiltinFunc("SYD", Vb6Type::Double);
    addBuiltinFunc("DDB", Vb6Type::Double);
    addBuiltinFunc("FV", Vb6Type::Double);
    addBuiltinFunc("PV", Vb6Type::Double);
    addBuiltinFunc("Pmt", Vb6Type::Double);
    addBuiltinFunc("IPmt", Vb6Type::Double);
    addBuiltinFunc("PPmt", Vb6Type::Double);
    addBuiltinFunc("RATE", Vb6Type::Double);
    addBuiltinFunc("NPer", Vb6Type::Double);
    addBuiltinFunc("IRR", Vb6Type::Double);
    addBuiltinFunc("MIRR", Vb6Type::Double);
    addBuiltinFunc("NPV", Vb6Type::Double);
    addBuiltinFunc("Partition", Vb6Type::String);
}
} // namespace vb6c3
