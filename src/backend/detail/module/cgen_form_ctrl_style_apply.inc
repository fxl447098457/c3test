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
        // P20-40: StatusBar — SBARS_SIZEGRIP/SBARS_TOOLTIPS 是 commctrl.h 的类专属样式
        // (不是 WS_*, 但仍要进 CreateWindow 的 style 参数)。RTL 自己算面板文本,
        // 故这里只给 sizegrip (与 VB6 默认观感一致), tooltip 由 RTL 存着不激活。
        constexpr long kSbarsGrip  = 0x00000100L;  // SBARS_SIZEGRIP
        constexpr long kSbarsTip   = 0x00000800L;  // SBARS_TOOLTIPS

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
            case FrmControlType::ProgressBar:
                // VB6 ProgressBar 无标题; 外观属性 (Orientation/Scrolling) 由
                // vb6_InitProgressBar 在窗口创建后改写 GWL_STYLE, 这里只给默认水平。
                createCaption = "";
                break;
            case FrmControlType::StatusBar:  // P20-40: msctls_status32
                style |= kSbarsGrip;
                createCaption = "";   // 面板文本走 SB_SETTEXTW, 创建时不给标题
                break;
            case FrmControlType::SSTab:      // P20-42: SysTabControl32
                // 位置/外观类 TCS_* 由 vb6_SSTab_Init 在窗口创建后改写 GWL_STYLE
                // (创建时读设计期属性容易与"未写即默认"的口径打架), 这里只给基类样式。
                createCaption = "";   // 页标题走 TCM_INSERTITEM, 创建时不给标题
                break;
            default:
                break;
        }

        // Bug3-Fix: TextBox/ComboBox 延迟设置初始文本，避免 EN_CHANGE 在句柄赋值前触发
        bool needDelayedText = (ctrl.controlType == FrmControlType::TextBox ||
                                ctrl.controlType == FrmControlType::ComboBox);
        std::string delayedText = (ctrl.controlType == FrmControlType::TextBox) ? ctrlText : ctrlCaption;
        if (needDelayedText) createCaption = "";  // 创建时不传文本

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

        // P20-35: Shape/Line属性初始化 (从.frm属性设置)
        if (ctrl.controlType == FrmControlType::Shape) {
            std::string shw = "vb6_hwnd_" + cIdent(ctrl.controlName);
            auto shIt = ctrl.properties.find("Shape"); if (shIt != ctrl.properties.end() && shIt->second.intValue != 0)
                c_.emitLine("vb6_SetShapeType((void*)" + shw + ", " + std::to_string((int)shIt->second.intValue) + ");");
            shIt = ctrl.properties.find("BorderWidth"); if (shIt != ctrl.properties.end() && shIt->second.intValue != 1)
                c_.emitLine("vb6_SetShapeBorderWidth((void*)" + shw + ", " + std::to_string((int)shIt->second.intValue) + ");");
            shIt = ctrl.properties.find("BorderStyle"); if (shIt != ctrl.properties.end() && shIt->second.intValue != 1)
                c_.emitLine("vb6_SetShapeBorderStyle((void*)" + shw + ", " + std::to_string((int)shIt->second.intValue) + ");");
            shIt = ctrl.properties.find("FillStyle"); if (shIt != ctrl.properties.end() && shIt->second.intValue != 1)
                c_.emitLine("vb6_SetShapeFillStyle((void*)" + shw + ", " + std::to_string((int)shIt->second.intValue) + ");");
        } else if (ctrl.controlType == FrmControlType::Line) {
            std::string lhw = "vb6_hwnd_" + cIdent(ctrl.controlName);
            auto lIt2 = ctrl.properties.find("X1"); if (lIt2 != ctrl.properties.end() && lIt2->second.intValue != 0)
                c_.emitLine("vb6_SetLineX1((void*)" + lhw + ", " + std::to_string((int)lIt2->second.intValue) + ");");
            lIt2 = ctrl.properties.find("Y1"); if (lIt2 != ctrl.properties.end() && lIt2->second.intValue != 0)
                c_.emitLine("vb6_SetLineY1((void*)" + lhw + ", " + std::to_string((int)lIt2->second.intValue) + ");");
            lIt2 = ctrl.properties.find("X2"); if (lIt2 != ctrl.properties.end())
                c_.emitLine("vb6_SetLineX2((void*)" + lhw + ", " + std::to_string((int)lIt2->second.intValue) + ");");
            lIt2 = ctrl.properties.find("Y2"); if (lIt2 != ctrl.properties.end())
                c_.emitLine("vb6_SetLineY2((void*)" + lhw + ", " + std::to_string((int)lIt2->second.intValue) + ");");
            lIt2 = ctrl.properties.find("BorderWidth"); if (lIt2 != ctrl.properties.end() && lIt2->second.intValue != 1)
                c_.emitLine("vb6_SetLineBorderWidth((void*)" + lhw + ", " + std::to_string((int)lIt2->second.intValue) + ");");
            lIt2 = ctrl.properties.find("BorderStyle"); if (lIt2 != ctrl.properties.end() && lIt2->second.intValue != 1)
                c_.emitLine("vb6_SetLineBorderStyle((void*)" + lhw + ", " + std::to_string((int)lIt2->second.intValue) + ");");
        }

        // P20-37: DriveListBox/DirListBox/FileListBox initial population
        if (ctrl.controlType == FrmControlType::DriveListBox) {
            c_.emitLine("vb6_DriveListBoxRefresh((void*)vb6_hwnd_" + cIdent(ctrl.controlName) + ");");
        } else if (ctrl.controlType == FrmControlType::DirListBox) {
            c_.emitLine("{ wchar_t _p" + cIdent(ctrl.controlName) + "[MAX_PATH]; GetCurrentDirectoryW(MAX_PATH, _p" + cIdent(ctrl.controlName) + ");");
            c_.emitLine("  vb6_DirListBoxSetPath((void*)vb6_hwnd_" + cIdent(ctrl.controlName) + ", _p" + cIdent(ctrl.controlName) + "); }");
        } else if (ctrl.controlType == FrmControlType::FileListBox) {
            c_.emitLine("{ wchar_t _p" + cIdent(ctrl.controlName) + "[MAX_PATH]; GetCurrentDirectoryW(MAX_PATH, _p" + cIdent(ctrl.controlName) + ");");
            c_.emitLine("  vb6_FileListBoxSetPath((void*)vb6_hwnd_" + cIdent(ctrl.controlName) + ", _p" + cIdent(ctrl.controlName) + "); }");
        }

        // ProgressBar 设计期属性 (Min/Max/Value/Orientation/Scrolling)。
        // 未写进 .frm 的传 -1 表示"用 VB6 默认" (Min=0 / Max=100 / Value=0 / 水平 / 分块)。
        // 注意 VB6 序列化 ProgressBar 时常见 `Value = -10` (控件默认值的内部表示),
        // 这里一并当作未指定处理, 由 RTL 钳位到 0 —— 与 VB6 运行时的实际读数一致。
        if (ctrl.controlType == FrmControlType::ProgressBar) {
            std::string pbNLower = ctrl.controlName;
            std::transform(pbNLower.begin(), pbNLower.end(), pbNLower.begin(), ::tolower);
            std::string pbHwnd = knownControlArrays_.count(pbNLower) ?
                ("vb6_CtrlArr_GetAt(&vb6_arr_" + cIdent(ctrl.controlName) + ", " +
                 std::to_string(ctrl.index >= 0 ? ctrl.index : 0) + ")") :
                ("vb6_hwnd_" + cIdent(ctrl.controlName));
            auto pbIt = ctrl.properties.find("Min");
            int pbgMin = (pbIt != ctrl.properties.end() && pbIt->second.intValue != -1)
                       ? (int)pbIt->second.intValue : -1;
            pbIt = ctrl.properties.find("Max");
            int pbgMax = (pbIt != ctrl.properties.end() && pbIt->second.intValue != -1)
                       ? (int)pbIt->second.intValue : -1;
            pbIt = ctrl.properties.find("Value");
            int pbgVal = (pbIt != ctrl.properties.end() && pbIt->second.intValue != -1
                          && pbIt->second.intValue > -10)
                       ? (int)pbIt->second.intValue : -1;
            pbIt = ctrl.properties.find("Orientation");
            int pbgOri = (pbIt != ctrl.properties.end() && pbIt->second.intValue != -1)
                       ? (int)pbIt->second.intValue : -1;
            pbIt = ctrl.properties.find("Scrolling");
            int pbgScr = (pbIt != ctrl.properties.end() && pbIt->second.intValue != -1)
                       ? (int)pbIt->second.intValue : -1;
            c_.emitLine("vb6_InitProgressBar((void*)" + pbHwnd + ", "
                      + std::to_string(pbgMin) + ", " + std::to_string(pbgMax) + ", "
                      + std::to_string(pbgVal) + ", " + std::to_string(pbgOri) + ", "
                      + std::to_string(pbgScr) + ");");
        }

        // P20-40: StatusBar 设计期 —— Init 一次, 再按 Panels 集合逐项灌进去。
        // 未写进 .frm 的属性走 VB6 默认 (Align=2 靠下 / Style=0 多面板 / 无面板)。
        if (ctrl.controlType == FrmControlType::StatusBar) {
            // 登记裸名: 表达式侧的 statusBarNameOfExpr 是把 `vb6_ComGetObjectProp(vb6_com_X,
            // L"Panels")` 抠成裸名 X 之后比对的, 存槽名或带前缀的名字都匹配不上。
            statusBarVars_.insert(ctrl.controlName);
            std::string sbNLower = ctrl.controlName;
            std::transform(sbNLower.begin(), sbNLower.end(), sbNLower.begin(), ::tolower);
            std::string sbHwnd = knownControlArrays_.count(sbNLower) ?
                ("vb6_CtrlArr_GetAt(&vb6_arr_" + cIdent(ctrl.controlName) + ", " +
                 std::to_string(ctrl.index >= 0 ? ctrl.index : 0) + ")") :
                ("vb6_hwnd_" + cIdent(ctrl.controlName));

            // 取字符串字面量: FrmValue::rawText 带引号, 剥掉后转义即可直接拼进 C 源码。
            auto litOf = [](const std::map<std::string, FrmValue>& mp, const std::string& k) {
                auto it = mp.find(k);
                if (it == mp.end() || it->second.type != FrmValueType::String) return std::string();
                std::string raw = it->second.rawText;
                if (raw.size() >= 2 && raw.front() == '"' && raw.back() == '"')
                    raw = raw.substr(1, raw.size() - 2);
                for (char& ch : raw) if (ch == '"') ch = '\'';
                return raw;
            };
            auto intOf = [](const std::map<std::string, FrmValue>& mp, const std::string& k) -> int {
                auto it = mp.find(k);
                return (it != mp.end()) ? (int)it->second.intValue : -1;
            };

            auto alIt = ctrl.properties.find("Align");
            int sbAlign = (alIt != ctrl.properties.end() && alIt->second.intValue >= 0)
                        ? (int)alIt->second.intValue : 2;
            int sbStyle = (intOf(ctrl.properties, "Style") == 1) ? 1 : 0;
            c_.emitLine("vb6_StatusBar_Init((void*)" + sbHwnd + ", "
                      + std::to_string(sbStyle) + ", " + std::to_string(sbAlign) + ");");

            if (sbStyle == 1) {   // sbrSimple: 只有一格, 内容走 SimpleText
                std::string stx = litOf(ctrl.properties, "SimpleText");
                c_.emitLine("vb6_StatusBar_SetSimpleText((void*)" + sbHwnd + ", "
                          + (stx.empty() ? std::string("L\"\"") : ("L\"" + stx + "\"")) + ");");
            }

            for (const auto& block : ctrl.propertyBlocks) {
                // 两种块名都认: 有裸标题行时取标题, 没有(StatusBar 的 Panels 就是这种)
                // 时解析器兜底成 "Items"。
                if (block.blockName != "Panels" && block.blockName != "Items") continue;
                int pOrd = 0;
                for (const auto& p : block.nestedBlocks) {
                    pOrd++;
                    // 下标没写就按出现顺序 (VB6 的 `Panels(1)/(2)` 一般都会写)。
                    int idx = intOf(p.properties, "Index");
                    if (idx <= 0) idx = pOrd;
                    std::string key = litOf(p.properties, "Key");
                    std::string txt = litOf(p.properties, "Text");
                    c_.emitLine("vb6_StatusBar_AddPanel((void*)" + sbHwnd + ", "
                              + std::to_string(idx) + ", "
                              + (key.empty() ? std::string("NULL") : ("L\"" + key + "\"")) + ", "
                              + "L\"" + txt + "\");");
                    // 子属性只在偏离 VB6 默认值时才发, 省掉一堆无意义的调用。
                    int pv = intOf(p.properties, "Style");
                    if (pv > 0) c_.emitLine("vb6_StatusBar_SetPanelStyle((void*)" + sbHwnd
                                          + ", " + std::to_string(idx) + ", " + std::to_string(pv) + ");");
                    pv = intOf(p.properties, "AutoSize");
                    if (pv > 0) c_.emitLine("vb6_StatusBar_SetPanelAutoSize((void*)" + sbHwnd
                                          + ", " + std::to_string(idx) + ", " + std::to_string(pv) + ");");
                    // Width/MinWidth 的 VB6 默认就是 0, 只在**显式给了正数**时才发;
                    // 用 `!= 0` 会把"没写"误判成 -1 灌进去。
                    pv = intOf(p.properties, "Width");
                    if (pv > 0) c_.emitLine("vb6_StatusBar_SetPanelWidth((void*)" + sbHwnd
                                          + ", " + std::to_string(idx) + ", " + std::to_string(pv) + ");");
                    pv = intOf(p.properties, "MinWidth");
                    if (pv > 0) c_.emitLine("vb6_StatusBar_SetPanelMinWidth((void*)" + sbHwnd
                                          + ", " + std::to_string(idx) + ", " + std::to_string(pv) + ");");
                    std::string tip = litOf(p.properties, "ToolTipText");
                    if (!tip.empty()) c_.emitLine("vb6_StatusBar_SetPanelToolTip((void*)" + sbHwnd
                                          + ", " + std::to_string(idx) + ", L\"" + tip + "\");");
                }
                // **不要 break**: 一个控件里可以有多个同名的集合块 (StatusBar 的 Panels
                // 一项一个块, 块名都是解析器兜底的 "Items"), break 会把第二项之后的
                // 面板整个丢掉, 且不报任何错。
            }
        }

        // P20-42: SSTab 设计期 —— 一次 Init 灌进 Tabs/Tab/TabOrientation/TabStyle/
        // TabsPerRow/WordWrap, 再逐页灌 Caption。
        // 页标题的 .frm 形态支持两种: ①VB6 实际写的 BeginProperty Tabs {…} 块里
        // 逐个 BeginProperty TabN {…} 的 Caption; ②手写的 `TabCaption(0) = "…"` 行。
        if (ctrl.controlType == FrmControlType::SSTab) {
            std::string tsNLower = ctrl.controlName;
            std::transform(tsNLower.begin(), tsNLower.end(), tsNLower.begin(), ::tolower);
            std::string tsHwnd = knownControlArrays_.count(tsNLower) ?
                ("vb6_CtrlArr_GetAt(&vb6_arr_" + cIdent(ctrl.controlName) + ", " +
                 std::to_string(ctrl.index >= 0 ? ctrl.index : 0) + ")") :
                ("vb6_hwnd_" + cIdent(ctrl.controlName));

            auto tsInt = [](const std::map<std::string, FrmValue>& mp, const std::string& k, int defv) -> int {
                auto it = mp.find(k);
                if (it == mp.end()) return defv;
                return (int)it->second.intValue;
            };
            auto tsLit = [](const std::map<std::string, FrmValue>& mp, const std::string& k) -> std::string {
                auto it = mp.find(k);
                if (it == mp.end() || it->second.type != FrmValueType::String) return std::string();
                std::string raw = it->second.rawText;
                if (raw.size() >= 2 && raw.front() == '"' && raw.back() == '"')
                    raw = raw.substr(1, raw.size() - 2);
                for (char& ch : raw) if (ch == '"') ch = '\'';
                return raw;
            };

            int tsTabs     = tsInt(ctrl.properties, "Tabs", 0);
            int tsCur      = tsInt(ctrl.properties, "Tab", 0);
            int tsOrient   = tsInt(ctrl.properties, "TabOrientation", 0);
            int tsStyle    = tsInt(ctrl.properties, "TabStyle", 0);
            int tsPerRow   = tsInt(ctrl.properties, "TabsPerRow", 0);
            int tsWrap     = tsInt(ctrl.properties, "WordWrap", 0);
            if (tsTabs < 0) tsTabs = 0;
            c_.emitLine("vb6_SSTab_Init((void*)" + tsHwnd + ", "
                      + std::to_string(tsTabs) + ", " + std::to_string(tsCur) + ", "
                      + std::to_string(tsOrient) + ", " + std::to_string(tsStyle) + ", "
                      + std::to_string(tsPerRow) + ", " + std::to_string(tsWrap) + ");");

            // ①BeginProperty Tabs {…} → Tab1..TabN 的 Caption (VB6 实际形态)
            for (const auto& tb : ctrl.propertyBlocks) {
                if (tb.blockName != "Tabs") continue;
                int ord = 0;
                for (const auto& page : tb.nestedBlocks) {
                    ord++;
                    // TabN 的 N 是 1 基; 没写就按出现顺序 (VB6 一般都会写)。
                    int idx = tsInt(page.properties, "Index", ord - 1);
                    if (idx < 0) idx = 0;
                    std::string cap = tsLit(page.properties, "Caption");
                    if (cap.empty()) cap = tsLit(page.properties, "Text");
                    c_.emitLine("vb6_SSTab_SetTabCaption((void*)" + tsHwnd + ", "
                              + std::to_string(idx) + ", L\"" + cap + "\");");
                }
            }
            // ②`TabCaption(n) = "…"` 直接写在属性行上 (手写 .frm 的形态)
            for (const auto& kv : ctrl.properties) {
                const std::string& k = kv.first;
                if (k.size() < 12) continue;
                if (_strnicmp(k.c_str(), "TabCaption", 10) != 0) continue;
                size_t lp = k.find('(');
                size_t rp = k.find(')');
                if (lp == std::string::npos || rp == std::string::npos || rp <= lp + 1) continue;
                int idx = atoi(k.substr(lp + 1, rp - lp - 1).c_str());
                std::string raw = kv.second.type == FrmValueType::String ? kv.second.rawText : std::string();
                if (raw.size() >= 2 && raw.front() == '"' && raw.back() == '"')
                    raw = raw.substr(1, raw.size() - 2);
                for (char& ch : raw) if (ch == '"') ch = '\'';
                c_.emitLine("vb6_SSTab_SetTabCaption((void*)" + tsHwnd + ", "
                          + std::to_string(idx) + ", L\"" + raw + "\");");
            }
        }

        // P24 / Fix 182: .frx 设计期属性（Picture / TextBox.Text / ListBox.List）。
        // 实现已抽到 cgen_form_create_controls.inc 的 emitControlFrxProps, 与容器子控件共用。
        emitControlFrxProps(ctrl);

ctrlId++;
    }

