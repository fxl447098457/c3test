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

    // Fix 127/128: Property Set 型字体属性 —— 走 Fix 128 的"Get + 原地覆写字段"通道
    // (不调用 Set, 因其实现体末尾的 Refresh 会在 CreateWorks 阶段把图表置空)。
    // 环境变量 C3_FONT_SET=1 可临时打开旧的直接调用路径以便对照。
    const bool kApplyFontSet127 = ([] {
        const char* e = std::getenv("C3_FONT_SET");
        return e && *e && *e != '0';
    })();
    (void)kApplyFontSet127;

    // Fix 119: .frm 的 BeginProperty Font 块 → 构造 vb6_ComIface_Font 对象。
    // VB6 里每个控件实例都持有自己的 Font 对象, 且控件 InitProperties 常以
    // UserControl.Font 为基准 (如 TitleFont.Size = Font.Size + 8); 此前字体块被
    // 整体跳过, 所有控件都退回默认字体 → 字号/文字测量/整体布局全错。
    auto emitFrmFontObj = [&](const FrmPropertyBlock& blk, const std::string& varName) {
        std::string nm = "MS Sans Serif";
        double sz = 8.25;
        long weight = 400, charset = 0;
        long bBold = 0, bItalic = 0, bUnder = 0, bStrike = 0;
        auto it = blk.properties.find("Name");
        if (it != blk.properties.end()) {
            nm = it->second.rawText;
            if (nm.size() >= 2 && nm.front() == '"' && nm.back() == '"')
                nm = nm.substr(1, nm.size() - 2);
        }
        it = blk.properties.find("Size");
        if (it != blk.properties.end())
            sz = (it->second.type == FrmValueType::Float) ? it->second.floatValue
                                                          : (double)it->second.intValue;
        it = blk.properties.find("Weight");
        if (it != blk.properties.end()) weight = (long)it->second.intValue;
        it = blk.properties.find("Charset");
        if (it != blk.properties.end()) charset = (long)it->second.intValue;
        it = blk.properties.find("Bold");
        if (it != blk.properties.end()) bBold = (long)it->second.intValue;
        it = blk.properties.find("Italic");
        if (it != blk.properties.end()) bItalic = (long)it->second.intValue;
        it = blk.properties.find("Underline");
        if (it != blk.properties.end()) bUnder = (long)it->second.intValue;
        it = blk.properties.find("Strikethrough");
        if (it != blk.properties.end()) bStrike = (long)it->second.intValue;
        // 常规字重(400) 时 VB6 的 Bold 由 Weight>=700 推导
        if (weight >= 700) bBold = -1;

        c_.emitLine("{ vb6_ComIface_Font* " + varName
                  + " = (vb6_ComIface_Font*)vb6_UC_NewFont();");
        c_.emitLine("  if (" + varName + ") {");
        c_.emitLine("    " + varName + "->Name = vb6_BSTR_FromStr(L\"" + escapeWideCString(nm) + "\");");
        {
            char szBuf[64];
            snprintf(szBuf, sizeof(szBuf), "%.4g", sz);
            c_.emitLine("    " + varName + "->Size = " + std::string(szBuf) + "f;");
        }
        c_.emitLine("    " + varName + "->Weight = " + std::to_string(weight) + ";");
        c_.emitLine("    " + varName + "->Charset = " + std::to_string(charset) + ";");
        c_.emitLine("    " + varName + "->Bold = " + std::to_string(bBold) + ";");
        c_.emitLine("    " + varName + "->Italic = " + std::to_string(bItalic) + ";");
        c_.emitLine("    " + varName + "->Underline = " + std::to_string(bUnder) + ";");
        c_.emitLine("    " + varName + "->Strikethrough = " + std::to_string(bStrike) + ";");
    };

    for (const auto& ctrl : frmDesc.formControl.children) {
        // Fix 112: 工程内 UserControl (.ctl) 子控件 — 实例化到宿主子窗口.
        // 此前这类 controlTypeToWin32Class()==nullptr 的非 ActiveX 类型直接 continue,
        // Form2 上的 ucChartBar1 等从未创建 → vb6_ComCall(NULL,...) 被丢弃 → 白板窗体.
        if (findUserControlSpec(ctrl.controlTypeName)) {
            std::string ctlClass = knownUserControlCtrlVars_[Symbol::toLower(ctrl.controlName)];
            // 该 .ctl 模块的宿主描述必须先注册 (无 TU: extern 声明 + 直接调用)
            c_.emitLine("{ extern void vb6_" + cIdent(ctlClass) + "_RegisterHost(void);"
                      + " vb6_" + cIdent(ctlClass) + "_RegisterHost(); }");
            int ucL = 0, ucT = 0, ucW = 1000, ucH = 300;
            auto ucP = ctrl.properties.find("Left");
            if (ucP != ctrl.properties.end()) ucL = (int)ucP->second.intValue;
            ucP = ctrl.properties.find("Top");
            if (ucP != ctrl.properties.end()) ucT = (int)ucP->second.intValue;
            ucP = ctrl.properties.find("Width");
            if (ucP != ctrl.properties.end()) ucW = (int)ucP->second.intValue;
            ucP = ctrl.properties.find("Height");
            if (ucP != ctrl.properties.end()) ucH = (int)ucP->second.intValue;

            std::string ctlLower = Symbol::toLower(ctrl.controlName);
            // Fix 119: 实例字体 (BeginProperty Font) 必须在 HostCreate 之前设定 —
            // 控件的 InitProperties 会以 UserControl.Font 为默认字体基准。
            for (const auto& blk : ctrl.propertyBlocks) {
                if (_stricmp(blk.blockName.c_str(), "Font") != 0) continue;
                emitFrmFontObj(blk, "_vb6_f119");
                c_.emitLine("    vb6_UC_SetPendingFont(_vb6_f119);");
                c_.emitLine("  } }");
            }
            bool ucIsArr = knownControlArrays_.count(ctlLower) != 0;
            std::string hwndExpr;
            if (ucIsArr) {
                int idx = ctrl.index >= 0 ? ctrl.index : 0;
                c_.emitLine("{ void* vb6_uc_h = vb6_UC_HostCreate(\"" + ctlClass + "\", "
                    + std::to_string(ucL) + ", " + std::to_string(ucT) + ", "
                    + std::to_string(ucW) + ", " + std::to_string(ucH)
                    + ", hwnd, hInstance, \"" + cIdent(ctrl.controlName) + "_" + std::to_string(idx)
                    + "\", " + std::to_string(idx) + ");");
                c_.emitLine("  vb6_CtrlArr_SetAt(&vb6_arr_" + cIdent(ctrl.controlName) + ", "
                           + std::to_string(idx) + ", vb6_uc_h); }");
                hwndExpr = "vb6_CtrlArr_GetAt(&vb6_arr_" + cIdent(ctrl.controlName) + ", "
                         + std::to_string(idx) + ")";
            } else {
                c_.emitLine("vb6_hwnd_" + cIdent(ctrl.controlName) + " = vb6_UC_HostCreate(\""
                    + ctlClass + "\", " + std::to_string(ucL) + ", " + std::to_string(ucT) + ", "
                    + std::to_string(ucW) + ", " + std::to_string(ucH)
                    + ", hwnd, hInstance, \"" + ctrl.controlName + "\", -1);");
                hwndExpr = "vb6_hwnd_" + cIdent(ctrl.controlName);
            }

            // Fix 112/114: 设计期持久化的 UserControl 公共属性
            // (Caption=... / Value=30 / PB_Color1=... / Caption2="..." / ChartStyle=1 ...)
            // — 逐个调用该 .ctl 的 prop_let_ (跳过几何/Tab 等标准属性).
            // Fix 114: 原来用 symTab_.lookupModule(pn) 判断属性是否为 Property Let,
            // 但 lookupModule 只查**当前模块**(Form2)作用域, 而 Property Let 符号属于
            // 被引用 .ctl 模块 → 恒为 nullptr → 所有设计期属性一个都没发射, 导致
            // 三个 ucProgressCircular 只剩默认 50% 蓝环 / LabelPlus 无标题与底色 /
            // 饼图 ChartStyle、LabelsVisible 等全部丢失。改为跨模块查类写方向形参表。
            {
                static const char* kSkip[] = {"Left", "Top", "Width", "Height", "Index",
                    "TabIndex", "TabStop", "Visible", "Enabled", "Name", "_ExtentX",
                    "_ExtentY", "Tag", "ToolTipText", "WhatisThis", "HelpContextID",
                    // BeginProperty 复合块由 propertyBlocks 单独处理, 不应作为简单属性发射
                    "Font", "IconFont", "TitleFont", "Caption1_Font", "Caption2_Font",
                    "Images", "ListImages", "MouseIcon"};
                c_.emitLine("vb6_UC_Enter(" + hwndExpr + ");  /* Fix 112: 换入宿主状态 */");
                // Fix 119: 字体类复合属性块 (Caption1_Font / Caption2_Font / TitleFont /
                // IconFont / Font ...) — 若 .ctl 有同名属性写方向, 构造字体对象后调用之。
                // 例: 三个 ucProgressCircular 的 Caption1_Font=14.25 此前完全没生效。
                for (const auto& blk : ctrl.propertyBlocks) {
                    // Fix 119/125: 字体类块 → 构造字体对象后调用 .ctl 的同名写属性。
                    // · Property Let (ByVal StdFont)  → vb6_ComIface_Font*  直接传
                    // · Property Set (ByRef StdFont)  → vb6_ComIface_Font** 传 &变量
                    //   这类实现体是 `With m_X_Font: .Name = New_Font.Name ...`, 由
                    //   vb6_ComSetProp 落到"字体结构体直写字段"快路径 (Fix 125), 不再崩。
                    std::vector<ParameterInfo> fp119;
                    bool isSet119 = false;
                    if (findClassMemberWriteParams(ctlClass, blk.blockName, false, fp119)
                        && !fp119.empty()) {
                        isSet119 = false;
                    // Fix 130: Property **Set** 型字体属性 (ucProgressCircular.Caption1_Font /
                    // Caption2_Font, LabelPlus.Font) 走 vb6_UC_FontAssign 安全通道:
                    // 用该控件自己的 Property Get 拿到内部字体对象后**直写字段**,
                    // 不调用其 Set 实现体 (内含 Refresh, 会在 CreateControls 阶段
                    // 数据未就绪时刷新并把图表永久置空 —— Fix 127 实测)。
                    // 若不应用, InitProperties 的 `Set m_X_Font = Ambient.Font` 会把
                    // 宿主共享字体当成员用, 后续 COM 读写把它打坏 (实测 Size 变 1,
                    // 仪表 50%/30%/70% 与 Band/Visitors/Volume 缩成一点)。
                    } else if (findClassMemberWriteParams(ctlClass, blk.blockName, true, fp119)
                               && !fp119.empty()) {
                        isSet119 = true;
                    } else {
                        continue;
                    }
                    // 字体类块命名惯例: Font / TitleFont / Caption1_Font / IconFont ...
                    bool fontName119 = false;
                    {
                        std::string bn = Symbol::toLower(blk.blockName);
                        if (bn == "font" || bn.size() >= 4
                            && bn.compare(bn.size() - 4, 4, "font") == 0)
                            fontName119 = true;
                    }
                    if (!fontName119) continue;
                    std::string canonF = canonicalClassMemberName(ctlClass, blk.blockName);
                    if (canonF.empty()) canonF = blk.blockName;
                    std::string setterF = "vb6_" + cIdent(ctlClass)
                                        + (isSet119 ? "_prop_set_" : "_prop_let_") + cIdent(canonF);
                    emitFrmFontObj(blk, "_vb6_f119");
                    if (isSet119) {
                        // Fix 128: Property Set 型**不调用**其 Set 实现体 (内含 Refresh,
                        // 会在数据未就绪时刷新并把图表永久置空)。改用该控件自己的
                        // Property Get 取到内部字体对象, 原地覆写字段。
                        std::string getterF = "vb6_" + cIdent(ctlClass)
                                            + "_prop_get_" + cIdent(canonF);
                        c_.emitLine("    vb6_UC_FontAssign((void*)" + getterF
                                  + "((vb6_cls_" + cIdent(ctlClass) + "*)vb6_UC_InstanceOf("
                                  + hwndExpr + ")), _vb6_f119);");
                    } else {
                        c_.emitLine("    " + setterF + "((vb6_cls_" + cIdent(ctlClass)
                                  + "*)vb6_UC_InstanceOf(" + hwndExpr + "), _vb6_f119);");
                    }
                    c_.emitLine("  } }");
                }
                for (const auto& kv : ctrl.properties) {
                    const std::string& pn = kv.first;
                    bool skip = false;
                    for (const char* sk : kSkip) if (_stricmp(pn.c_str(), sk) == 0) { skip = true; break; }
                    if (skip) continue;
                    // Fix 114: 跨模块查询 .ctl 类的 Property Let 形参表 (driver 已注入
                    // 外部 Class 符号 → Phase B 可命中 memberLetParams).
                    std::vector<ParameterInfo> wp114;
                    if (!findClassMemberWriteParams(ctlClass, pn, false, wp114) || wp114.empty())
                        continue;
                    // 属性名拼写按 .ctl 内声明规范化 (VB6 大小写不敏感), 否则 LNK2019
                    std::string canonPn = canonicalClassMemberName(ctlClass, pn);
                    if (canonPn.empty()) canonPn = pn;
                    std::string setter = "vb6_" + cIdent(ctlClass) + "_prop_let_" + cIdent(canonPn);
                    const ParameterInfo& p114 = wp114.back();
                    std::string meExpr = "(vb6_cls_" + cIdent(ctlClass)
                                       + "*)vb6_UC_InstanceOf(" + hwndExpr + ")";

                    // 解析字符串值 (含 .frx 属性包持久化引用, 如 Caption="Form2.frx":006E)
                    std::string textVal;
                    bool isStr = false;
                    if (kv.second.type == FrmValueType::String) {
                        isStr = true;
                        textVal = kv.second.rawText;
                    } else if (kv.second.type == FrmValueType::FrxReference) {
                        if (frxLoaded) {
                            FrxTextData ds114 = FrxReader::readDocString(kv.second.frxOffset);
                            if (!ds114.text.empty()) { isStr = true; textVal = ds114.text; }
                        }
                        if (!isStr) continue;   // 无法解析则跳过, 避免发射非法字面量
                    }
                    if (isStr) {
                        if (textVal.size() >= 2 && textVal.front() == '"' && textVal.back() == '"')
                            textVal = textVal.substr(1, textVal.size() - 2);
                        std::string lit = "vb6_BSTR_FromStr(L\"" + escapeWideCString(textVal) + "\")";
                        if (p114.isByVal) {
                            c_.emitLine(setter + "(" + meExpr + ", " + lit + ");");
                        } else {
                            // ByRef New_X As String → C 形参为 BSTR*, 需可寻址临时量
                            c_.emitLine("{ BSTR _lp114 = " + lit + "; "
                                      + setter + "(" + meExpr + ", &_lp114); }");
                        }
                    } else {
                        std::string valExpr = kv.second.rawText.empty()
                                ? std::to_string(kv.second.intValue) : kv.second.rawText;
                        c_.emitLine(setter + "(" + meExpr + ", " + valExpr + ");");
                    }
                }

                // czUI fix: UC 事件接线 — thunk/sink 已在文件作用域 (prelude) 生成,
                // 这里按实例挂接 (attach 行由 prelude 存入 ucEventAttach_)。
                {
                    auto itAtt114 = ucEventAttach_.find(Symbol::toLower(ctrl.controlName));
                    if (itAtt114 != ucEventAttach_.end() && !getenv("C3_NO_EVENTS")) {
                        std::string shortCls114 = ctlClass;
                        size_t dotCls114 = shortCls114.rfind('.');
                        if (dotCls114 != std::string::npos) shortCls114 = shortCls114.substr(dotCls114 + 1);
                        c_.emitLine("{ /* UC events */");
                        for (const auto& al : itAtt114->second) c_.emitLine("  " + al);
                        c_.emitLine("  ((vb6_cls_" + cIdent(shortCls114) + "*)vb6_UC_InstanceOf(" + hwndExpr + "))->events = &_sink_" + cIdent(ctrl.controlName) + ";");
                        c_.emitLine("}");
                    }
                }

                // czUI fix: 设计期属性除 prop_let 直赋外, 再经轻量 PropertyBag 重放一次
                // UserControl_ReadProperties — 复现 .ctl 内部"读取后同步"逻辑
                // (czUI.ctl: `If m_Checked Then m_AnimPos = 1!` → toggle 初始 ON;
                //  这类语句只在 ReadProperties 里, prop_let 直赋路径会绕过它)。
                {
                    // ctlClass 可能是 "czFormDemo.czControl" 带命名空间 —
                    // 用设计器描述 + 代码段文本探测 .ctl 是否定义了
                    // UserControl_ReadProperties (CCodeGen 每模块重建, 不能用
                    // 跨模块 state; 类符号只存可调用成员, Private Sub 不在其中)
                    const FrmFile* ucSpec114 = findUserControlSpec(ctlClass);
                    bool hasReadProps114 = false;
                    if (ucSpec114) {
                        std::string lowerCode = Symbol::toLower(ucSpec114->codeSection);
                        hasReadProps114 =
                            lowerCode.find("sub usercontrol_readproperties") != std::string::npos;
                    }
                    static const char* noBagCls = getenv("C3_NO_BAG_CLS");
                    static const char* onlyBagCls = getenv("C3_ONLY_BAG_CLS");
                    bool bagBlocked = (noBagCls && strstr(noBagCls, Symbol::toLower(ctlClass).c_str()))
                        || (onlyBagCls && strstr(onlyBagCls, Symbol::toLower(ctlClass).c_str()) == nullptr);
                    if (hasReadProps114 && !getenv("C3_NO_BAG") && !bagBlocked) {
                        c_.emitLine("extern void vb6_" + cIdent(ctlClass) + "_UC_ReadProps(void* me, void** PropBag);");
                        c_.emitLine("{ void* _bag = vb6_UC_PropBagCreate();");
                        for (const auto& kvb : ctrl.properties) {
                            const std::string& pn114 = kvb.first;
                            bool skip114 = false;
                            for (const char* sk : kSkip) if (_stricmp(pn114.c_str(), sk) == 0) { skip114 = true; break; }
                            if (skip114) continue;
                            std::string litName = "L\"" + escapeWideCString(pn114) + "\"";
                            if (kvb.second.type == FrmValueType::Integer) {
                                c_.emitLine("vb6_UC_BagPutInt(_bag, " + litName + ", " + std::to_string((int)kvb.second.intValue) + ");");
                            } else if (kvb.second.type == FrmValueType::Float) {
                                c_.emitLine("vb6_UC_BagPutDbl(_bag, " + litName + ", " + std::to_string(kvb.second.floatValue) + ");");
                            } else if (kvb.second.type == FrmValueType::String) {
                                std::string tv = kvb.second.rawText;
                                if (tv.size() >= 2 && tv.front() == '"' && tv.back() == '"')
                                    tv = tv.substr(1, tv.size() - 2);
                                c_.emitLine("vb6_UC_BagPutStr(_bag, " + litName + ", L\"" + escapeWideCString(tv) + "\");");
                            } else if (kvb.second.type == FrmValueType::Identifier) {
                                std::string idl = Symbol::toLower(kvb.second.rawText);
                                if (idl == "true")  c_.emitLine("vb6_UC_BagPutBool(_bag, " + litName + ", -1);");
                                else if (idl == "false") c_.emitLine("vb6_UC_BagPutBool(_bag, " + litName + ", 0);");
                            } else if (kvb.second.type == FrmValueType::FrxReference && frxLoaded) {
                                // czUI fix: Caption 等字符串常存于 .frx (如 "Form2.frx":006E) —
                                // 跳过会让 ReadProperty 返回默认空串, 把 prop_let 设置好的
                                // 标题清空 (Charts2020 所有 LabelPlus 标题消失的根因)
                                FrxTextData dsB114 = FrxReader::readDocString(kvb.second.frxOffset);
                                if (!dsB114.text.empty()) {
                                    std::string tvB = dsB114.text;
                                    if (tvB.size() >= 2 && tvB.front() == '"' && tvB.back() == '"')
                                        tvB = tvB.substr(1, tvB.size() - 2);
                                    c_.emitLine("vb6_UC_BagPutStr(_bag, " + litName + ", L\"" + escapeWideCString(tvB) + "\");");
                                }
                            }
                        }
                        c_.emitLine("vb6_" + cIdent(ctlClass) + "_UC_ReadProps((vb6_cls_" + cIdent(ctlClass)
                                  + "*)vb6_UC_InstanceOf(" + hwndExpr + "), &_bag);");
                        c_.emitLine("vb6_UC_PropBagFree(_bag); }");
                    }
                }
            }
            ctrlId++;
            continue;
        }
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

