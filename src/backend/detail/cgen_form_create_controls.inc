// cgen_form_create_controls.inc - emitFormFramework 函数体片段：CreateControls —— 控件树 CreateWindow
// 由 src/backend/module/cgen_form.cpp 拆出（2026-09-17），在函数体内被 #include，非独立编译单元。
// 内容 = 原文件第 1273~1772 行，逐行未改。

    // --- 控件创建函数 ---
    c_.emitLine("// === " + formName + " CreateControls ===");
    c_.emitLine("void " + createFn + "(void* hwnd, void* hInstance) {");
    c_.indent();
    c_.emitLine("(void)hwnd; (void)hInstance;");
    c_.emitLine("vb6_formLoading_" + cIdent(formName) + " = 1;  /* block events during form init */");
    c_.emitLine("vb6_ResetControlId();");
    c_.emitBlank();

    // P7.6: 初始化控件数组
    for (const auto& kv : knownControlArrays_) {
        // Find the original control name (need non-lowered version)
        for (const auto& ctrl : frmDesc.formControl.children) {
            std::string ctrlNameLower = ctrl.controlName;
            std::transform(ctrlNameLower.begin(), ctrlNameLower.end(), ctrlNameLower.begin(), ::tolower);
            if (ctrlNameLower == kv.first) {
                c_.emitLine("vb6_CtrlArr_Init(&vb6_arr_" + cIdent(ctrl.controlName) + ");");
                break;
            }
        }
    }
    c_.emitBlank();
    // P20-35: 注册Shape/Line自定义窗口类
    {
        bool hasShape = false, hasLine = false;
        for (const auto& ctrl : frmDesc.formControl.children) {
            if (ctrl.controlType == FrmControlType::Shape) hasShape = true;
            if (ctrl.controlType == FrmControlType::Line) hasLine = true;
        }
        if (hasShape || hasLine) {
            c_.emitLine("vb6_RegisterShapeLineClasses((void*)hInstance);");
        }
    }
    // 为每个控件生成CreateWindow调用
    ctrlId = 100;
    for (const auto& ctrl : frmDesc.formControl.children) {
        const char* win32Class = FrmParser::controlTypeToWin32Class(ctrl.controlType);
        if (!win32Class) {
            // P7.9: WebBrowser控件用vb6_CreateWebView创建
            if (ctrl.controlType == FrmControlType::WebBrowser) {
                int wbLeft = 0, wbTop = 0, wbWidth = 5000, wbHeight = 3000;
                auto pIt = ctrl.properties.find("Left");
                if (pIt != ctrl.properties.end()) wbLeft = (int)pIt->second.intValue;
                pIt = ctrl.properties.find("Top");
                if (pIt != ctrl.properties.end()) wbTop = (int)pIt->second.intValue;
                pIt = ctrl.properties.find("Width");
                if (pIt != ctrl.properties.end()) wbWidth = (int)pIt->second.intValue;
                pIt = ctrl.properties.find("Height");
                if (pIt != ctrl.properties.end()) wbHeight = (int)pIt->second.intValue;
                // 缇转像素: 1缇=1/15像素 (96DPI)
                c_.emitLine("{ void* vb6_tmp_hwnd = vb6_CreateWebView((void*)hwnd, " +
                    std::to_string(wbLeft / 15) + ", " + std::to_string(wbTop / 15) + ", " +
                    std::to_string(wbWidth / 15) + ", " + std::to_string(wbHeight / 15) + ", L\"" +
                    cIdent(ctrl.controlName) + "\");");
                c_.emitLine("vb6_hwnd_" + cIdent(ctrl.controlName) + " = vb6_tmp_hwnd; }");
            }
            // ActiveX控件: CoCreateInstance
            // P24-05: 优先用TypeLib中的CLSID直接生成代码, 避免ProgID不存在或错误
            // VB6 .frm的Begin行用CoClass名(如MSComctlLib.ImageList), 但真实ProgID可能不同(如MSComctlLib.ImageListCtrl)
            if (ctrl.controlType == FrmControlType::ImageList ||
                ctrl.controlType == FrmControlType::Toolbar ||
                ctrl.controlType == FrmControlType::StatusBar ||
                ctrl.controlType == FrmControlType::CommonDialog) {
                // 从符号表查找ComClass, 获取真实的CLSID或ProgID
                std::string comVarName = "vb6_com_" + cIdent(ctrl.controlName);
                Symbol* comSym = lookupDotted(ctrl.controlTypeName);
                if (!comSym || comSym->kind != SymbolKind::ComClass) {
                    // Try short name (e.g. "ImageList")
                    size_t dot = ctrl.controlTypeName.find('.');
                    if (dot != std::string::npos) {
                        comSym = symTab_.lookup(ctrl.controlTypeName.substr(dot + 1));
                    }
                }
                if (comSym && comSym->kind == SymbolKind::ComClass && !comSym->comClsidStr.empty()) {
                    // Use CLSID directly (most reliable, no runtime ProgID lookup)
                    // Convert "{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}" to C init form:
                    // {0xXXXXXXXX,0xXXXX,0xXXXX,{0xXX,0xXX,0xXX,0xXX,0xXX,0xXX,0xXX,0xXX}}
                    std::string raw = comSym->comClsidStr;  // e.g. "{2C247F23-8591-11D1-B16A-00C0F0283628}"
                    std::string cInit;
                    if (raw.size() >= 38 && raw.front() == '{' && raw.back() == '}') {
                        std::string h = raw.substr(1, raw.size()-2); // strip braces
                        // Split by '-'
                        std::vector<std::string> parts;
                        size_t pos = 0;
                        while (pos < h.size()) {
                            size_t dash = h.find('-', pos);
                            if (dash == std::string::npos) { parts.push_back(h.substr(pos)); break; }
                            parts.push_back(h.substr(pos, dash-pos));
                            pos = dash + 1;
                        }
                        if (parts.size() == 5) {
                            cInit = "0x" + parts[0] + ",0x" + parts[1] + ",0x" + parts[2] + ",{0x"
                                + parts[3].substr(0,2) + ",0x" + parts[3].substr(2,2) + ",0x"
                                + parts[4].substr(0,2) + ",0x" + parts[4].substr(2,2) + ",0x"
                                + parts[4].substr(4,2) + ",0x" + parts[4].substr(6,2) + ",0x"
                                + parts[4].substr(8,2) + ",0x" + parts[4].substr(10,2) + "}";
                        }
                    }
                    if (cInit.empty()) cInit = "0,0,0,{0,0,0,0,0,0,0,0}";  // fallback
                    c_.emitLine("{ static const CLSID vb6_clsid_" + cIdent(ctrl.controlName) + " = {" + cInit + "};");
                    c_.emitLine("  CoCreateInstance(&vb6_clsid_" + cIdent(ctrl.controlName) + ", NULL, 1/*CLSCTX_INPROC_SERVER*/, &IID_IDispatch, (void**)&" + comVarName + "); }");
                } else {
                    // Fallback: use ProgID from TypeLib if available, else controlTypeName
                    std::string progId = (comSym && !comSym->comProgId.empty()) ? comSym->comProgId : ctrl.controlTypeName;
                    c_.emitLine("{ CLSID vb6_clsid; CLSIDFromProgID(L\"" + progId + "\", &vb6_clsid);");
                    c_.emitLine("  CoCreateInstance(&vb6_clsid, NULL, 1/*CLSCTX_INPROC_SERVER*/, &IID_IDispatch, (void**)&" + comVarName + "); }");
                }

                // ImageList: set ImageWidth/ImageHeight and load ListImages from .frx
                if (ctrl.controlType == FrmControlType::ImageList && frxLoaded) {
                    std::string comVar = "vb6_com_" + cIdent(ctrl.controlName);
                    // Set ImageWidth/ImageHeight via COM (must be set before adding images)
                    auto iwIt = ctrl.properties.find("ImageWidth");
                    if (iwIt != ctrl.properties.end() && iwIt->second.intValue > 0) {
                        c_.emitLine("vb6_ComSetProp((void*)" + comVar + ", L\"ImageWidth\", vb6_ComPackInt(" + std::to_string((int)iwIt->second.intValue) + "));");
                    }
                    auto ihIt = ctrl.properties.find("ImageHeight");
                    if (ihIt != ctrl.properties.end() && ihIt->second.intValue > 0) {
                        c_.emitLine("vb6_ComSetProp((void*)" + comVar + ", L\"ImageHeight\", vb6_ComPackInt(" + std::to_string((int)ihIt->second.intValue) + "));");
                    }

                    // Find "Images" property block with nested ListImage entries
                    for (const auto& block : ctrl.propertyBlocks) {
                        if (block.blockName == "Images") {
                            int imgIdx = 0;
                            for (const auto& listImg : block.nestedBlocks) {
                                // Each nested block is like: blockName="ListImage1", has Picture/Key/Tag
                                auto picIt = listImg.properties.find("Picture");
                                if (picIt == listImg.properties.end() || picIt->second.type != FrmValueType::FrxReference) continue;

                                // Read image data from .frx (ImageList format: isImageList=true)
                                auto pic = FrxReader::readPicture(picIt->second.frxOffset, true);
                                if (pic.data.empty()) continue;

                                // Generate byte array in .h file
                                imgIdx++;
                                std::string varName = "vb6_frx_imglist_" + cIdent(ctrl.controlName) + "_" + std::to_string(imgIdx);
                                h_.emitLine(bytesToHexArray(pic.data.data(), pic.data.size(), varName));

                                // Get Key property (may be empty string)
                                std::string keyStr;
                                auto keyIt = listImg.properties.find("Key");
                                if (keyIt != listImg.properties.end() && keyIt->second.type == FrmValueType::String) {
                                    std::string raw = keyIt->second.rawText;
                                    if (raw.size() >= 2 && raw.front() == '"' && raw.back() == '"') keyStr = raw.substr(1, raw.size() - 2);
                                }

                                // ListImages.Add(index, key, picture) via COM
                                // Get ListImages collection, then call Add method
                                c_.emitLine("{ void* vb6_lstImgs = vb6_ComGetObjectProp((void*)" + comVar + ", L\"ListImages\");");
                                c_.emitLine("  if (vb6_lstImgs) {");
                                c_.emitLine("    void* vb6_picCom = vb6_LoadPictureAsCom(" + varName + ", " + varName + "_size);");
                                c_.emitLine("    if (vb6_picCom) {");
                                // args: [index, key, picture] -- VB6 positional parameter order
                                c_.emitLine("      void* vb6_args[] = { vb6_ComPackInt(" + std::to_string(imgIdx) + "), vb6_ComPackBSTR(L\"" + keyStr + "\"), vb6_ComPackObject(vb6_picCom) };");
                                c_.emitLine("      vb6_ComVarFree((void*)vb6_ComCall(vb6_lstImgs, L\"Add\", vb6_args, 3));");
                                c_.emitLine("      vb6_ReleaseObject(&vb6_picCom);");
                                c_.emitLine("    }");
                                c_.emitLine("    vb6_ReleaseObject(&vb6_lstImgs);");
                                c_.emitLine("  } }");
                            }
                            break;  // Only one "Images" block expected
                        }
                    }
                }

                ctrlId++;
                continue;
            }
            // 不可见控件 (Timer等) 跳过
            ctrlId++;
            continue;
        }

        // 提取控件属性
        int left = 0, top = 0, width = 1000, height = 300;
        std::string ctrlCaption = ctrl.controlName;  // 默认Caption=控件名
        std::string ctrlText;  // TextBox的Text属性

        auto propIt = ctrl.properties.find("Left");
        if (propIt != ctrl.properties.end()) left = (int)propIt->second.intValue;
        propIt = ctrl.properties.find("Top");
        if (propIt != ctrl.properties.end()) top = (int)propIt->second.intValue;
        propIt = ctrl.properties.find("Width");
        if (propIt != ctrl.properties.end()) width = (int)propIt->second.intValue;
        propIt = ctrl.properties.find("Height");
        if (propIt != ctrl.properties.end()) height = (int)propIt->second.intValue;

        propIt = ctrl.properties.find("Caption");
        if (propIt != ctrl.properties.end() && propIt->second.type == FrmValueType::String) {
            std::string raw = propIt->second.rawText;
            if (raw.size() >= 2 && raw.front() == '"' && raw.back() == '"') {
                ctrlCaption = raw.substr(1, raw.size() - 2);
            }
        }
        propIt = ctrl.properties.find("Text");
        if (propIt != ctrl.properties.end() && propIt->second.type == FrmValueType::String) {
            std::string raw = propIt->second.rawText;
            if (raw.size() >= 2 && raw.front() == '"' && raw.back() == '"') {
                ctrlText = raw.substr(1, raw.size() - 2);
            }
        }

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
                style |= kSsLeft;
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

