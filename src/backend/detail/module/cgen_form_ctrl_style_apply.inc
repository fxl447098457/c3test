// cgen_form_ctrl_style_apply.inc - emitFormFramework 函数体片段：CreateControls 之二 —— 样式常量 + CreateWindow + 属性应用
// 由 src/backend/detail/module/cgen_form_create_controls.inc 再拆出（2026-09-17），在函数体内被 #include，非独立编译单元。
// 内容 = 原文件第 1476~1771 行，逐行未改。

        // Win32样式常量（不依赖windows.h，使用WinUser.h固定数值）
        // WS_CHILD=0x40000000, WS_VISIBLE=0x10000000, WS_BORDER=0x00800000
        // BS_PUSHBUTTON=0, BS_AUTOCHECKBOX=3, BS_AUTORADIOBUTTON=9
        // BS_GROUPBOX=7, SS_LEFT=0, ES_AUTOHSCROLL=0x80
        // LBS_NOTIFY=1, CBS_DROPDOWN=2
        constexpr long kWsChild    = 0x40000000L;
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

        long style = kWsChild | kWsVisible;
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

        // P24: .frx resource loading for this control
        if (frxLoaded) {
            std::string ctrlHwndVar;
            {
                std::string ctrlNameLower = ctrl.controlName;
                std::transform(ctrlNameLower.begin(), ctrlNameLower.end(), ctrlNameLower.begin(), ::tolower);
                if (knownControlArrays_.count(ctrlNameLower)) {
                    ctrlHwndVar = "vb6_CtrlArr_GetAt(&vb6_arr_" + cIdent(ctrl.controlName) + ", " +
                                  std::to_string(ctrl.index >= 0 ? ctrl.index : 0) + ")";
                } else {
                    ctrlHwndVar = "vb6_hwnd_" + cIdent(ctrl.controlName);
                }
            }

            // P24: Control.Picture from .frx (CommandButton/OptionButton/CheckBox/PictureBox/Image)
            if (ctrl.controlType == FrmControlType::CommandButton ||
                ctrl.controlType == FrmControlType::OptionButton ||
                ctrl.controlType == FrmControlType::CheckBox ||
                ctrl.controlType == FrmControlType::PictureBox ||
                ctrl.controlType == FrmControlType::Image) {
                auto picIt = ctrl.properties.find("Picture");
                if (picIt != ctrl.properties.end() && picIt->second.type == FrmValueType::FrxReference) {
                    auto pic = FrxReader::readPicture(picIt->second.frxOffset, false);
                    if (!pic.data.empty()) {
                        std::string varName = "vb6_frx_pic_" + cIdent(ctrl.controlName);
                        if (ctrl.index >= 0) varName += "_" + std::to_string(ctrl.index);
                        h_.emitLine(bytesToHexArray(pic.data.data(), pic.data.size(), varName));
                        bool isGraphicalBtn = (ctrl.controlType == FrmControlType::CommandButton ||
                                               ctrl.controlType == FrmControlType::OptionButton ||
                                               ctrl.controlType == FrmControlType::CheckBox) &&
                                              ctrl.properties.count("Style") &&
                                              ctrl.properties.at("Style").intValue == 1;
                        if (isGraphicalBtn) {
                            // Graphical button: use BM_SETIMAGE with IMAGE_BITMAP
                            c_.emitLine("{ void* vb6_pic = vb6_LoadPictureFromMemory(" + varName + ", " + varName + "_size);");
                            c_.emitLine("  if (vb6_pic) vb6_GraphicalBtn_SetImage((void*)" + ctrlHwndVar + ", vb6_pic); }");
                        } else {
                            // PictureBox/Image: use vb6_SetControlPicture
                            c_.emitLine("{ void* vb6_pic = vb6_LoadPictureFromMemory(" + varName + ", " + varName + "_size);");
                            c_.emitLine("  if (vb6_pic) vb6_SetControlPicture((void*)" + ctrlHwndVar + ", vb6_pic); }");
                        }
                    }
                }
            }

            // P24: TextBox.Text from .frx (multiline text)
            if (ctrl.controlType == FrmControlType::TextBox) {
                auto txtIt = ctrl.properties.find("Text");
                if (txtIt != ctrl.properties.end() && txtIt->second.type == FrmValueType::FrxReference) {
                    auto txt = FrxReader::readText(txtIt->second.frxOffset);
                    if (!txt.text.empty()) {
                        c_.emitLine("{ wchar_t* vb6_wtxt = vb6_Utf8ToWide(\"" + escapeCString(txt.text) + "\");");
                        c_.emitLine("  if (vb6_wtxt) { SetWindowTextW((HWND)" + ctrlHwndVar + ", vb6_wtxt); free(vb6_wtxt); } }");
                    }
                }
            }

            // P24: ListBox.List + ItemData from .frx
            if (ctrl.controlType == FrmControlType::ListBox) {
                auto listIt = ctrl.properties.find("List");
                bool hasListFromFrx = (listIt != ctrl.properties.end() && listIt->second.type == FrmValueType::FrxReference);
                FrxListData listData;
                if (hasListFromFrx) {
                    listData = FrxReader::readStringList(listIt->second.frxOffset);
                    for (size_t i = 0; i < listData.items.size(); i++) {
                        c_.emitLine("{ wchar_t* vb6_witem = vb6_Utf8ToWide(\"" + escapeCString(listData.items[i]) + "\");");
                        c_.emitLine("  if (vb6_witem) { SendMessageW((HWND)" + ctrlHwndVar + ", LB_ADDSTRING, 0, (LPARAM)vb6_witem); free(vb6_witem); } }");
                    }
                }
                auto idataIt = ctrl.properties.find("ItemData");
                if (idataIt != ctrl.properties.end() && idataIt->second.type == FrmValueType::FrxReference) {
                    auto intData = FrxReader::readIntList(idataIt->second.frxOffset);
                    for (size_t i = 0; i < intData.items.size(); i++) {
                        c_.emitLine("SendMessageW((HWND)" + ctrlHwndVar + ", LB_SETITEMDATA, " +
                                    std::to_string((int)i) + ", (LPARAM)" + std::to_string(intData.items[i]) + ");");
                    }
                }
            }
        }

ctrlId++;
    }

