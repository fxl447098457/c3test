// cgen_form_ctrl_style_apply.inc - emitFormFramework 函数体片段：CreateControls 之二 —— 样式常量 + CreateWindow + 属性应用
// 由 src/backend/detail/module/cgen_form_create_controls.inc 再拆出（2026-09-17），在函数体内被 #include，非独立编译单元。
// 内容 = 原文件第 1476~1771 行，逐行未改。

        // Win32样式常量（不依赖windows.h，使用WinUser.h固定数值）
        // WS_CHILD=0x40000000, WS_VISIBLE=0x10000000, WS_BORDER=0x00800000
        // BS_PUSHBUTTON=0, BS_AUTOCHECKBOX=3, BS_AUTORADIOBUTTON=9
        // BS_GROUPBOX=7, SS_LEFT=0, ES_AUTOHSCROLL=0x80
        // LBS_NOTIFY=1, CBS_DROPDOWN=2
        constexpr long kWsChild    = 0x40000000L;
        constexpr long kWsClipSib  = 0x04000000L;  // WS_CLIPSIBLINGS (Fix 124)
        constexpr long kWsVisible  = 0x10000000L;
        constexpr long kWsBorder   = 0x00800000L;
        constexpr long kBsPush     = 0x00000000L;
        constexpr long kBsAutoChk  = 0x00000003L;
        constexpr long kBsAutoRad  = 0x00000009L;
        constexpr long kBsGroupBox = 0x00000007L;
        constexpr long kSsLeft     = 0x00000000L;
        constexpr long kSsNotify   = 0x00000100L;  // SS_NOTIFY: Label 接收鼠标消息 (VB6 语义 + tooltip 需要)
        constexpr long kEsAutoH    = 0x00000080L;
        constexpr long kLbsNotify  = 0x00000001L;
        constexpr long kCbsDrop    = 0x00000002L;
        constexpr long kSsBitmap   = 0x0000000EL;  // SS_BITMAP for PictureBox/Image
        constexpr long kSsCenterImg= 0x00000200L;  // SS_CENTERIMAGE
        constexpr long kBsBitmap   = 0x00000080L;  // BS_BITMAP for Graphical buttons
        constexpr long kBsPushLike = 0x00001000L;  // BS_PUSHLIKE: button-style toggle
        constexpr long kEsMulti    = 0x00000004L;  // ES_MULTILINE for TextBox
        constexpr long kEsAutoV    = 0x00000040L;  // ES_AUTOVSCROLL
        constexpr long kWsHscroll  = 0x00100000L;  // WS_HSCROLL
        constexpr long kWsVscroll  = 0x00200000L;  // WS_VSCROLL

        // Fix 124: WS_CLIPSIBLINGS —— 标准控件与 UC 宿主同层叠放时,
        // 重绘不得画到上层兄弟控件上 (否则出现"控件被其他控件擦掉")。
        long style = kWsChild | kWsVisible | kWsClipSib;
        long exStyle = 0;
        std::string createCaption = ctrlCaption;

        switch (ctrl.controlType) {
            case FrmControlType::CommandButton:
                style |= kBsPush;
                {
                    auto stIt = ctrl.properties.find("Style");
                    if (stIt != ctrl.properties.end() && stIt->second.intValue == 1)
                        style |= kBsPushLike;  // Graphical: button-style with picture (no BS_BITMAP, we draw ourselves)
                }
                break;
            case FrmControlType::TextBox:
                style |= kWsBorder | kEsAutoH;
                // Bug3-Fix: 不在CreateControl中传初始文本，避免EN_CHANGE在句柄赋值前触发
                // createCaption will be set to "" below; text set after hwnd assignment
                {
                    auto mlIt = ctrl.properties.find("MultiLine");
                    if (mlIt != ctrl.properties.end() && mlIt->second.intValue != 0)
                        style |= kEsMulti | kEsAutoV;
                    auto sbIt = ctrl.properties.find("ScrollBars");
                    if (sbIt != ctrl.properties.end()) {
                        int sb = (int)sbIt->second.intValue;
                        if (sb == 1 || sb == 3) style |= kWsVscroll;  // Vertical
                        if (sb == 2 || sb == 3) style |= kWsHscroll;  // Horizontal
                    }
                }
                break;
            case FrmControlType::Label:
                // VB6 Label 有 Click/MouseDown 事件且是 tooltip 目标, 必须 SS_NOTIFY,
                // 否则 STATIC 对鼠标返回 HTTRANSPARENT, TTF_SUBCLASS 的 tooltip 永远不弹。
                style |= kSsLeft | kSsNotify;
                break;
            case FrmControlType::CheckBox:
                style |= kBsAutoChk;
                {
                    auto stIt = ctrl.properties.find("Style");
                    if (stIt != ctrl.properties.end() && stIt->second.intValue == 1)
                        style |= kBsPushLike;  // Graphical: button-style toggle with picture (no BS_BITMAP, we draw ourselves)
                }
                break;
            case FrmControlType::OptionButton:
                style |= kBsAutoRad;
                {
                    auto stIt = ctrl.properties.find("Style");
                    if (stIt != ctrl.properties.end() && stIt->second.intValue == 1)
                        style |= kBsPushLike;  // Graphical: button-style toggle with picture (no BS_BITMAP, we draw ourselves)
                }
                break;
            case FrmControlType::Frame:
                style |= kBsGroupBox;
                break;
            case FrmControlType::ListBox: {
                style |= kLbsNotify | kWsBorder | kWsVscroll;  // VB6 ListBox always shows vertical scrollbar
                // Sorted: LBS_SORT = 0x0002
                auto sortIt = ctrl.properties.find("Sorted");
                if (sortIt != ctrl.properties.end() && sortIt->second.intValue != 0) style |= 0x0002L;
                // MultiSelect: 1=LBS_MULTIPLESEL(0x8), 2=LBS_EXTENDEDSEL(0x800)
                auto msIt = ctrl.properties.find("MultiSelect");
                if (msIt != ctrl.properties.end()) {
                    if (msIt->second.intValue == 1) style |= 0x0008L;
                    else if (msIt->second.intValue == 2) style |= 0x0800L;
                }
                break;
            }
            case FrmControlType::ComboBox:
                // Bug3-Fix: ComboBox同TextBox，延迟设置初始文本
                // createCaption will be set to "" below
                {
                    // Style: 0=CBS_DROPDOWN(2), 1=CBS_SIMPLE(1), 2=CBS_DROPDOWNLIST(3)
                    auto stIt = ctrl.properties.find("Style");
                    if (stIt != ctrl.properties.end()) {
                        if (stIt->second.intValue == 1) style |= 0x0001L;  // CBS_SIMPLE
                        else if (stIt->second.intValue == 2) style |= 0x0003L;  // CBS_DROPDOWNLIST
                        else style |= kCbsDrop;  // default: CBS_DROPDOWN
                    } else {
                        style |= kCbsDrop;
                    }
                    // Fix 147: 同 cgen_util_ctrl.cpp — ComboBox 下拉列表需要
                    // WS_VSCROLL 才能滚动查看全部项 (否则只能看到前几项)
                    style |= kWsVscroll;
                    style |= kWsBorder;
                    // Sorted: CBS_SORT = 0x0100
                    auto sortIt = ctrl.properties.find("Sorted");
                    if (sortIt != ctrl.properties.end() && sortIt->second.intValue != 0) style |= 0x0100L;
                }
                break;
            case FrmControlType::PictureBox:
                // VB6 PictureBox: STATIC + SS_BITMAP + SS_CENTERIMAGE + border
                style |= kSsBitmap | kSsCenterImg;
                style |= kWsBorder;  // WS_BORDER for sunken border
                createCaption = "";   // No text for picture controls
                break;
            case FrmControlType::Image:
                // VB6 Image: STATIC + SS_BITMAP + SS_CENTERIMAGE (no border)
                style |= kSsBitmap | kSsCenterImg;
                createCaption = "";
                break;
            case FrmControlType::DriveListBox:
                // C29-1b: VB6 的 DriveListBox 外观就是"只能选、不能改"的组合框
                style |= 0x0003L | kWsVscroll | kWsBorder;  // CBS_DROPDOWNLIST
                createCaption = "";
                break;
            case FrmControlType::DirListBox:
            case FrmControlType::FileListBox:
                // 列表框同款: LBS_NOTIFY 才有 SELCHANGE / DBLCLK 通知可派
                style |= kLbsNotify | kWsBorder | kWsVscroll;
                createCaption = "";
                break;
            case FrmControlType::Shape:
            case FrmControlType::Line:
                // C29-1a: 自绘控件 (VB6_SHAPE / VB6_LINE)，无文本、无边框样式
                createCaption = "";
                break;
            case FrmControlType::CommonDialog:
                // D6 / C29-9: 这枚"控件"没有外观 —— 它是原生对话框的属性袋。
                // 窗口照建（要有句柄存属性），但必须**不可见**：基础样式里有 WS_VISIBLE，
                // 这里清掉，尺寸也由下面的 0x0 兜住。
                createCaption = "";
                style &= ~kWsVisible;
                width = 0;
                height = 0;
                break;
            case FrmControlType::Timer:
                // C29-T: 与 CommonDialog 同一手法 —— Timer 要有句柄（当计时器的身份），
                // 但绝不能有外观：不可见、0x0，且不吃 Tab 焦点。
                createCaption = "";
                style &= ~kWsVisible;
                width = 0;
                height = 0;
                break;
            default:
                break;
        }

        // Bug3-Fix: TextBox/ComboBox 延迟设置初始文本，避免 EN_CHANGE 在句柄赋值前触发
        bool needDelayedText = (ctrl.controlType == FrmControlType::TextBox ||
                                ctrl.controlType == FrmControlType::ComboBox);
        std::string delayedText = (ctrl.controlType == FrmControlType::TextBox) ? ctrlText : ctrlCaption;
        if (needDelayedText) createCaption = "";  // 创建时不传文本

        // C29-1a: Line 在 VB6 里没有 Left/Top/Width/Height —— 窗口矩形就是 X1/Y1/X2/Y2
        // (容器缇值) 的包围盒。不这样算, 控件会拿默认尺寸 1000x300 创建出来。
        if (ctrl.controlType == FrmControlType::Line) {
            lineRectFromEndpoints(ctrl, left, top, width, height);
        }

        // 生成vb6_CreateControl调用
        // 生成vb6_CreateControl调用 (P7.6: 数组控件用CtrlArr_SetAt)
        {
            std::string ctrlNameLower = ctrl.controlName;
            std::transform(ctrlNameLower.begin(), ctrlNameLower.end(), ctrlNameLower.begin(), ::tolower);
            c_.emitLine("{ void* vb6_tmp_hwnd = vb6_CreateControl(");
            c_.indent();
            c_.emitLine("\"" + std::string(win32Class) + "\", \"" + createCaption + "\",");
            c_.emitLine(std::to_string(style) + "L, " + std::to_string(exStyle) + "L,");
            c_.emitLine(std::to_string(left) + ", " + std::to_string(top) + ", "
                       + std::to_string(width) + ", " + std::to_string(height) + ",");
            c_.emitLine(std::to_string(ctrlId) + ", hwnd, hInstance);");
            c_.dedent();
            if (knownControlArrays_.count(ctrlNameLower)) {
                // 控件数组: 存入vb6_CtrlArr
                c_.emitLine("vb6_CtrlArr_SetAt(&vb6_arr_" + cIdent(ctrl.controlName) + ", "
                           + std::to_string(ctrl.index >= 0 ? ctrl.index : 0) + ", vb6_tmp_hwnd); }");
            } else {
                c_.emitLine("vb6_hwnd_" + cIdent(ctrl.controlName) + " = vb6_tmp_hwnd; }");
            }
        }

        // 设计期 BorderStyle / Alignment (顶层控件)
        {
            std::string dsNameLower = ctrl.controlName;
            std::transform(dsNameLower.begin(), dsNameLower.end(), dsNameLower.begin(), ::tolower);
            std::string dsHwnd = knownControlArrays_.count(dsNameLower) ?
                ("vb6_CtrlArr_GetAt(&vb6_arr_" + cIdent(ctrl.controlName) + ", " +
                 std::to_string(ctrl.index >= 0 ? ctrl.index : 0) + ")") :
                ("vb6_hwnd_" + cIdent(ctrl.controlName));
            emitDesignerStyleProps(ctrl, dsHwnd);
        }

        // Bug3-Fix: 延迟设置 TextBox/ComboBox 的初始文本（在句柄赋值之后）
        if (needDelayedText && !delayedText.empty()) {
            std::string ctrlNameLower2 = ctrl.controlName;
            std::transform(ctrlNameLower2.begin(), ctrlNameLower2.end(), ctrlNameLower2.begin(), ::tolower);
            std::string hwndVar = knownControlArrays_.count(ctrlNameLower2) ?
                ("vb6_CtrlArr_GetAt(&vb6_arr_" + cIdent(ctrl.controlName) + ", 0)") :
                ("vb6_hwnd_" + cIdent(ctrl.controlName));
            // 使用 SendMessage WM_SETTEXT 设置初始文本（句柄已赋值，EN_CHANGE 安全触发）
            c_.emitLine("SendMessageW((HWND)" + hwndVar + ", WM_SETTEXT, 0, (LPARAM)L\"" + delayedText + "\");");
        }

        // P20-12: CausesValidation property from .frm (default=True, only set when explicitly False)
        {
            auto cvIt = ctrl.properties.find("CausesValidation");
            if (cvIt != ctrl.properties.end() && cvIt->second.intValue == 0) {
                std::string cvNLower = ctrl.controlName; std::transform(cvNLower.begin(), cvNLower.end(), cvNLower.begin(), ::tolower);
                std::string cvHwnd = knownControlArrays_.count(cvNLower) ?
                    ("vb6_CtrlArr_GetAt(&vb6_arr_" + cIdent(ctrl.controlName) + ", 0)") :
                    ("vb6_hwnd_" + cIdent(ctrl.controlName));
                c_.emitLine("vb6_SetCausesValidation((void*)" + cvHwnd + ", 0);");
            }
        }

        
        // Image.Stretch from .frm (default=False; only set when explicitly True)
        if (ctrl.controlType == FrmControlType::Image) {
            auto stIt = ctrl.properties.find("Stretch");
            if (stIt != ctrl.properties.end() && stIt->second.intValue != 0) {
                std::string stNameLower = ctrl.controlName;
                std::transform(stNameLower.begin(), stNameLower.end(), stNameLower.begin(), ::tolower);
                std::string stHwnd = knownControlArrays_.count(stNameLower) ?
                    ("vb6_CtrlArr_GetAt(&vb6_arr_" + cIdent(ctrl.controlName) + ", " +
                     std::to_string(ctrl.index >= 0 ? ctrl.index : 0) + ")") :
                    ("vb6_hwnd_" + cIdent(ctrl.controlName));
                c_.emitLine("vb6_SetImageStretch((void*)" + stHwnd + ", 1);");
            }
        }

        // C29-1a: 设计期外观属性落到控件 (原来这里是一整段内联块; 容器子控件那条路共用同一个助手)
        emitShapeLineProps(ctrl, ctrlHwndExprForInit(ctrl));

        // P20-37 / C29-1b: 三控件的初值。设计期给了 Drive / Path / Pattern 就用它, 没给才
        // 退回当前目录 (FileListBox 的 Pattern 缺省时由 RTL 回 "*.*")。顺序上先填表再设值,
        // SetDrive / SetPath / SetPattern 自己会重刷列表。
        if (ctrl.controlType == FrmControlType::DriveListBox) {
            std::string p37Hwnd = "vb6_hwnd_" + cIdent(ctrl.controlName);
            c_.emitLine("vb6_DriveListBoxRefresh((void*)" + p37Hwnd + ");");
            auto dIt = ctrl.properties.find("Drive");
            if (dIt != ctrl.properties.end() && dIt->second.type == FrmValueType::String) {
                std::string dRaw = dIt->second.rawText;
                if (dRaw.size() >= 2 && dRaw.front() == '"' && dRaw.back() == '"')
                    dRaw = dRaw.substr(1, dRaw.size() - 2);
                if (!dRaw.empty())
                    c_.emitLine("vb6_DriveListBoxSetDrive((void*)" + p37Hwnd
                                + ", vb6_BSTR_FromStr(L\"" + escapeWideCString(dRaw) + "\"));");
            }
        } else if (ctrl.controlType == FrmControlType::DirListBox) {
            std::string p37Var = "_p" + cIdent(ctrl.controlName);
            std::string p37Hwnd2 = "vb6_hwnd_" + cIdent(ctrl.controlName);
            auto pIt = ctrl.properties.find("Path");
            bool hasPath = (pIt != ctrl.properties.end() && pIt->second.type == FrmValueType::String);
            std::string pRaw;
            if (hasPath) {
                pRaw = pIt->second.rawText;
                if (pRaw.size() >= 2 && pRaw.front() == '"' && pRaw.back() == '"')
                    pRaw = pRaw.substr(1, pRaw.size() - 2);
            }
            if (hasPath && !pRaw.empty()) {
                c_.emitLine("vb6_DirListBoxSetPath((void*)" + p37Hwnd2
                            + ", vb6_BSTR_FromStr(L\"" + escapeWideCString(pRaw) + "\"));");
            } else {
                c_.emitLine("{ wchar_t " + p37Var + "[MAX_PATH]; GetCurrentDirectoryW(MAX_PATH, " + p37Var + ");");
                c_.emitLine("  vb6_DirListBoxSetPath((void*)" + p37Hwnd2 + ", " + p37Var + "); }");
            }
        } else if (ctrl.controlType == FrmControlType::FileListBox) {
            std::string p37Var2 = "_p" + cIdent(ctrl.controlName);
            std::string p37Hwnd3 = "vb6_hwnd_" + cIdent(ctrl.controlName);
            auto qIt = ctrl.properties.find("Path");
            bool hasQ = (qIt != ctrl.properties.end() && qIt->second.type == FrmValueType::String);
            std::string qRaw;
            if (hasQ) {
                qRaw = qIt->second.rawText;
                if (qRaw.size() >= 2 && qRaw.front() == '"' && qRaw.back() == '"')
                    qRaw = qRaw.substr(1, qRaw.size() - 2);
            }
            if (hasQ && !qRaw.empty()) {
                c_.emitLine("vb6_FileListBoxSetPath((void*)" + p37Hwnd3
                            + ", vb6_BSTR_FromStr(L\"" + escapeWideCString(qRaw) + "\"));");
            } else {
                c_.emitLine("{ wchar_t " + p37Var2 + "[MAX_PATH]; GetCurrentDirectoryW(MAX_PATH, " + p37Var2 + ");");
                c_.emitLine("  vb6_FileListBoxSetPath((void*)" + p37Hwnd3 + ", " + p37Var2 + "); }");
            }
            auto kIt = ctrl.properties.find("Pattern");
            if (kIt != ctrl.properties.end() && kIt->second.type == FrmValueType::String) {
                std::string kRaw = kIt->second.rawText;
                if (kRaw.size() >= 2 && kRaw.front() == '"' && kRaw.back() == '"')
                    kRaw = kRaw.substr(1, kRaw.size() - 2);
                if (!kRaw.empty())
                    c_.emitLine("vb6_FileListBoxSetPattern((void*)" + p37Hwnd3
                                + ", vb6_BSTR_FromStr(L\"" + escapeWideCString(kRaw) + "\"));");
            }
        }

        // D6 / C29-9: CommonDialog 的设计期四行（VB6 会把用户填的属性原样存进 .frm）。
        // 走的是刚建好的那枚不可见句柄，所以顺序上必须排在 vb6_hwnd_<名> 赋值之后 ——
        // 这里就是那条不变式（同 C29-1a 的 emitShapeLineProps 一个位置）。
        if (ctrl.controlType == FrmControlType::CommonDialog) {
            std::string cdHwnd = "vb6_hwnd_" + cIdent(ctrl.controlName);
            auto cdStr = [&](const char* key, const char* fn) {
                auto it = ctrl.properties.find(key);
                if (it == ctrl.properties.end() || it->second.type != FrmValueType::String) return;
                std::string raw = it->second.rawText;
                if (raw.size() >= 2 && raw.front() == '"' && raw.back() == '"')
                    raw = raw.substr(1, raw.size() - 2);
                c_.emitLine(std::string(fn) + "((void*)" + cdHwnd + ", vb6_BSTR_FromStr(L\""
                            + escapeWideCString(raw) + "\"));");
            };
            auto cdInt = [&](const char* key, const char* fn) {
                auto it = ctrl.properties.find(key);
                if (it == ctrl.properties.end()) return;
                c_.emitLine(std::string(fn) + "((void*)" + cdHwnd + ", "
                            + std::to_string((int)it->second.intValue) + ");");
            };
            cdStr("Filter",      "vb6_CdSetFilter");
            cdStr("DialogTitle", "vb6_CdSetDialogTitle");
            cdStr("InitDir",     "vb6_CdSetInitDir");
            cdStr("DefaultExt",  "vb6_CdSetDefaultExt");
            cdStr("FileName",    "vb6_CdSetFileName");
            cdStr("FontName",    "vb6_CdSetFontName");
            cdInt("Flags",       "vb6_CdSetFlags");
            cdInt("CancelError", "vb6_CdSetCancelError");
            cdInt("Color",       "vb6_CdSetColor");
            cdInt("Min",         "vb6_CdSetMin");
            cdInt("Max",         "vb6_CdSetMax");
            cdInt("Copies",      "vb6_CdSetCopies");
            cdInt("FontSize",    "vb6_CdSetFontSize");
        }
        // P24 / Fix 182: .frx 设计期属性（Picture / TextBox.Text / ListBox.List）。
        // 实现已抽到 cgen_form_create_controls.inc 的 emitControlFrxProps, 与容器子控件共用。
        emitControlFrxProps(ctrl);

ctrlId++;
    }

