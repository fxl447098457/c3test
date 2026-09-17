// cgen_form_create_controls.inc - emitFormFramework 函数体片段：CreateControls —— 控件树 CreateWindow
// 由 src/backend/module/cgen_form.cpp 拆出（2026-09-17），在函数体内被 #include，非独立编译单元。
// 内容 = 原文件第 1272~1475 行，逐行未改。

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

