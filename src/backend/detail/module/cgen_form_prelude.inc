// cgen_form_prelude.inc - emitFormFramework 函数体片段：初始化（frx 加载、菜单ID收集）与窗体属性提取
// 由 src/backend/module/cgen_form.cpp 拆出（2026-09-17），在函数体内被 #include，非独立编译单元。
// 内容 = 原文件第 54~144 行，逐行未改。

    std::string formName = frmDesc.formName;
    formName_ = formName;  // M22-Issue6: save for Form Print use

    // P24: Load .frx file if present
    bool frxLoaded = false;
    if (!frmDesc.frxFilePath.empty()) {
        frxLoaded = FrxReader::load(frmDesc.frxFilePath);
    }
    std::string clsName = "VB6_Form_" + cIdent(formName);    // Win32窗口类名
    std::string wndProc = "vb6_form_wndproc_" + cIdent(formName);  // WndProc函数名
    std::string createFn = "vb6_form_create_" + cIdent(formName);  // 控件创建函数名
    std::string showFn = "vb6_form_show_" + cIdent(formName);      // Show函数名
    int ctrlId = 0;  // P7.6: 控件ID计数器(在WM_COMMAND和CreateControls中复用)
    bool isMDIForm = (frmDesc.formControl.controlType == FrmControlType::MDIForm);  // P7.7
    bool isMDIChild = frmDesc.isMDIChild;  // P7.7

    // P20-36: 收集菜单项ID映射 (与emitMenuItem/emitMenuClickDispatch一致)
    {
        std::function<void(const FrmControl&, int&)> collectMenuIds;
        collectMenuIds = [&](const FrmControl& menuCtrl, int& mid) {
            for (const auto& child : menuCtrl.children) {
                std::string cap = child.controlName;
                auto capIt2 = child.properties.find("Caption");
                if (capIt2 != child.properties.end() && capIt2->second.type == FrmValueType::String) {
                    std::string raw = capIt2->second.rawText;
                    if (raw.size() >= 2 && raw.front() == '"' && raw.back() == '"') cap = raw.substr(1, raw.size() - 2);
                }
                if (cap == "-") continue;  // separator: no ID
                if (!child.children.empty()) {
                    collectMenuIds(child, mid);  // submenu: recurse, no ID for container
                } else {
                    std::string cLower = child.controlName;
                    std::transform(cLower.begin(), cLower.end(), cLower.begin(), ::tolower);
                    knownMenuIds_[cLower] = mid;
                    mid++;
                }
            }
        };
        int collId = 1000;
        knownMenuFormHwnd_ = "vb6_hwnd_" + cIdent(formName);
        for (const auto& ctrl : frmDesc.formControl.children) {
            if (ctrl.controlType == FrmControlType::Menu) collectMenuIds(ctrl, collId);
        }
    }

    // --- 提取窗体属性 ---
    std::string caption = formName;  // 默认标题=窗体名
    int clientHeight = 3000;  // 默认客户区高度 (缇)
    int clientWidth = 4680;  // 默认客户区宽度 (缇)
    int startupPos = 3;  // 默认: Windows Default

    auto it = frmDesc.formControl.properties.find("Caption");
    if (it != frmDesc.formControl.properties.end() && it->second.type == FrmValueType::String) {
        // 去掉引号
        std::string raw = it->second.rawText;
        if (raw.size() >= 2 && raw.front() == '"' && raw.back() == '"') {
            caption = raw.substr(1, raw.size() - 2);
        }
    }
    it = frmDesc.formControl.properties.find("ClientHeight");
    if (it != frmDesc.formControl.properties.end()) clientHeight = (int)it->second.intValue;
    it = frmDesc.formControl.properties.find("ClientWidth");
    if (it != frmDesc.formControl.properties.end()) clientWidth = (int)it->second.intValue;
    it = frmDesc.formControl.properties.find("StartUpPosition");
    if (it != frmDesc.formControl.properties.end()) startupPos = (int)it->second.intValue;

    // P24: Form.Icon from .frx
    {
        auto iconIt = frmDesc.formControl.properties.find("Icon");
        if (iconIt != frmDesc.formControl.properties.end() && iconIt->second.type == FrmValueType::FrxReference && frxLoaded) {
            auto pic = FrxReader::readPicture(iconIt->second.frxOffset, false);
            if (!pic.data.empty()) {
                std::string varName = "vb6_frx_icon_" + cIdent(formName);
                h_.emitLine(bytesToHexArray(pic.data.data(), pic.data.size(), varName));
                // Will be used in WM_CREATE to set window icon
                // Store a flag for later code generation
            }
        }
    }
    // P24: Form.Picture from .frx
    {
        auto picIt2 = frmDesc.formControl.properties.find("Picture");
        if (picIt2 != frmDesc.formControl.properties.end() && picIt2->second.type == FrmValueType::FrxReference && frxLoaded) {
            auto pic = FrxReader::readPicture(picIt2->second.frxOffset, false);
            if (!pic.data.empty()) {
                std::string varName = "vb6_frx_bgpic_" + cIdent(formName);
                h_.emitLine(bytesToHexArray(pic.data.data(), pic.data.size(), varName));
            }
        }
    }
