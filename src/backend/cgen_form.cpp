#include "backend/cgen.hpp"
#include <algorithm>
#include <cctype>
#include <iostream>
#include <functional>

namespace vb6c3 {

// --- cgen_form.cpp: 窗体框架 + 菜单 + 转义 ---

void CCodeGen::emitFormFramework(const FrmFormDesc& frmDesc, Module& module) {
    std::string formName = frmDesc.formName;
    formName_ = formName;  // M22-Issue6: save for Form Print use
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

    // --- P7.5+P7.6: 填充已知控件名映射 + 检测控件数组 ---
    {
        std::string formNameLower = formName;
        std::transform(formNameLower.begin(), formNameLower.end(), formNameLower.begin(), ::tolower);
        knownFormName_ = formNameLower;
        knownFormControls_[formNameLower] = FrmControlType::Form;
        knownFormControlOriginalNames_[formNameLower] = formName;

        // P7.6: 先扫描控件数组 (同名控件出现多次 = 数组)
        std::unordered_map<std::string, int> ctrlNameCount;
        for (const auto& ctrl : frmDesc.formControl.children) {
            std::string ctrlNameLower = ctrl.controlName;
            std::transform(ctrlNameLower.begin(), ctrlNameLower.end(), ctrlNameLower.begin(), ::tolower);
            ctrlNameCount[ctrlNameLower]++;
            knownFormControls_[ctrlNameLower] = ctrl.controlType;
            knownFormControlOriginalNames_[ctrlNameLower] = ctrl.controlName;
        }
        for (const auto& kv : ctrlNameCount) {
            if (kv.second > 1) {
                knownControlArrays_[kv.first] = true;
            }
        }

        // P7.6: 构建控件ID→Index映射 (WM_COMMAND事件分发用)
        {
            int tempId = 100;
            for (const auto& ctrl : frmDesc.formControl.children) {
                std::string ctrlNameLower = ctrl.controlName;
                std::transform(ctrlNameLower.begin(), ctrlNameLower.end(), ctrlNameLower.begin(), ::tolower);
                if (knownControlArrays_.count(ctrlNameLower) && ctrl.index >= 0) {
                    auto& idxMap = controlIdToIndexMap_[ctrlNameLower];
                    int idSlot = tempId - 100;
                    while ((int)idxMap.size() <= idSlot) idxMap.push_back(-1);
                    idxMap[idSlot] = ctrl.index;
                }
                tempId++;
            }
        }
    }

    // --- .h文件: 声明 ---
    h_.emitLine("// P7: Win32 Form - " + formName);
    h_.emitBlank();

    // 窗体句柄变量
    h_.emitLine("static void* vb6_hwnd_" + cIdent(formName) + " = NULL;");

    // 控件句柄变量 (P7.6: 数组控件使用vb6_CtrlArr, 非数组使用void*)
    {
        std::unordered_set<std::string> emitted;
        for (const auto& ctrl : frmDesc.formControl.children) {
            std::string ctrlNameLower = ctrl.controlName;
            std::transform(ctrlNameLower.begin(), ctrlNameLower.end(), ctrlNameLower.begin(), ::tolower);
            if (emitted.count(ctrlNameLower)) continue;
            if (!FrmParser::controlTypeToWin32Class(ctrl.controlType)) {
                // P7.9: WebBrowser needs HWND declaration though no Win32 class
                if (ctrl.controlType == FrmControlType::WebBrowser) {
                    h_.emitLine("static void* vb6_hwnd_" + cIdent(ctrl.controlName) + " = NULL;");
                    emitted.insert(ctrlNameLower);
                }
                continue;
            }
            if (knownControlArrays_.count(ctrlNameLower)) {
                h_.emitLine("static vb6_CtrlArr vb6_arr_" + cIdent(ctrl.controlName) + ";");
            } else {
                h_.emitLine("static void* vb6_hwnd_" + cIdent(ctrl.controlName) + " = NULL;");
            }
            emitted.insert(ctrlNameLower);
        }
    }

    h_.emitBlank();

    // WndProc前向声明
    h_.emitLine("LRESULT CALLBACK " + wndProc + "(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);");
    // 创建函数前向声明
    h_.emitLine("void " + createFn + "(void* hwnd, void* hInstance);");
    // Show函数前向声明
    h_.emitLine("void " + showFn + "(void* hMDIClient);");
    h_.emitBlank();

    // --- .c文件: 实现 ---

    // Form_Unload trampoline (如果存在Form_Unload)
    {
        std::string formUnloadFn = cProcName("Form_Unload", AccessLevel::Private);
        auto* unloadSym = symTab_.lookup("Form_Unload");
        if (unloadSym) {
            c_.emitLine("// Form_Unload trampoline: bridge callback int(*)(void) to void(int16_t*)");
            c_.emitLine("static int " + formUnloadFn + "_trampoline(void) {");
            c_.indent();
            c_.emitLine("int16_t vb6_cancel = 0;");
            c_.emitLine("extern void " + formUnloadFn + "(int16_t*);");
            c_.emitLine(formUnloadFn + "(&vb6_cancel);");
            c_.emitLine("return (int)vb6_cancel;");
            c_.dedent();
            c_.emitLine("}");
            c_.emitBlank();
        }
    }
    // P18-F: 生成需要子类化的控件的 subclass WndProc
    // 需要子类化的条件: 控件有MouseEnter/MouseLeave/MouseDown/MouseUp/MouseMove/KeyPress/KeyDown/KeyUp事件处理器
    // 或者 PictureBox/Frame/Label 有 GotFocus/LostFocus 事件处理器(这些控件不通过WM_COMMAND发送焦点通知)
    {
        struct SubclassInfo {
            std::string ctrlName;      // 控件名(原始大小写)
            std::string hwndVar;       // HWND变量名
            bool hasGotFocus = false;
            bool hasLostFocus = false;
            bool hasMouseEnter = false;
            bool hasMouseLeave = false;
            bool hasMouseDown = false;
            bool hasMouseUp = false;
            bool hasMouseMove = false;
            bool hasKeyPress = false;
            bool hasKeyDown = false;
            bool hasKeyUp = false;
            bool hasDblClick = false;
            bool hasMouseHover = false;
            bool hasValidate = false;
            FrmControlType ctrlType;
        };
        std::vector<SubclassInfo> subclassCtrls;
        std::unordered_set<std::string> emitted;
        for (const auto& ctrl : frmDesc.formControl.children) {
            std::string ctrlNameLower = ctrl.controlName;
            std::transform(ctrlNameLower.begin(), ctrlNameLower.end(), ctrlNameLower.begin(), ::tolower);
            if (emitted.count(ctrlNameLower)) continue;
            emitted.insert(ctrlNameLower);

            // 跳过不可见控件(Timer/Menu等)和没有Win32窗口类的控件
            if (ctrl.controlType == FrmControlType::Timer ||
                ctrl.controlType == FrmControlType::Menu ||
                ctrl.controlType == FrmControlType::CommonDialog ||
                ctrl.controlType == FrmControlType::WebBrowser) continue;

            SubclassInfo info;
            info.ctrlName = ctrl.controlName;
            info.ctrlType = ctrl.controlType;
            if (knownControlArrays_.count(ctrlNameLower)) {
                info.hwndVar = "(void*)vb6_arr_" + cIdent(ctrl.controlName) + ".hwnds[0]";
            } else {
                info.hwndVar = "vb6_hwnd_" + cIdent(ctrl.controlName);
            }

            // PictureBox/Frame/Label 的 GotFocus/LostFocus 需要子类化
            // (STATIC/BUTTON类控件不通过WM_COMMAND发送焦点通知)
            if (ctrl.controlType == FrmControlType::PictureBox ||
                ctrl.controlType == FrmControlType::Frame ||
                ctrl.controlType == FrmControlType::Label ||
                ctrl.controlType == FrmControlType::Image) {
                info.hasGotFocus = symTab_.lookup(ctrl.controlName + "_GotFocus") != nullptr;
                info.hasLostFocus = symTab_.lookup(ctrl.controlName + "_LostFocus") != nullptr;
            }
            // P20-12: Validate event (all control types)
            info.hasValidate = symTab_.lookup(ctrl.controlName + "_Validate") != nullptr;

            // MouseEnter/MouseLeave 总是需要子类化(TrackMouseEvent)
            info.hasMouseEnter = symTab_.lookup(ctrl.controlName + "_MouseEnter") != nullptr;
            info.hasMouseLeave = symTab_.lookup(ctrl.controlName + "_MouseLeave") != nullptr;
            // VB6中MouseHover对应WM_MOUSEHOVER, 较少见, 暂不实现

            // 控件级鼠标事件(所有控件都可以有)需要子类化
            info.hasMouseDown = symTab_.lookup(ctrl.controlName + "_MouseDown") != nullptr;
            info.hasMouseUp = symTab_.lookup(ctrl.controlName + "_MouseUp") != nullptr;
            info.hasMouseMove = symTab_.lookup(ctrl.controlName + "_MouseMove") != nullptr;

            // 控件级键盘事件需要子类化
            info.hasKeyPress = symTab_.lookup(ctrl.controlName + "_KeyPress") != nullptr;
            info.hasKeyDown = symTab_.lookup(ctrl.controlName + "_KeyDown") != nullptr;
            info.hasKeyUp = symTab_.lookup(ctrl.controlName + "_KeyUp") != nullptr;
            info.hasDblClick = symTab_.lookup(ctrl.controlName + "_DblClick") != nullptr;
            info.hasMouseHover = symTab_.lookup(ctrl.controlName + "_MouseHover") != nullptr;

            // 只要有任一子类化事件需求就加入列表
            if (info.hasGotFocus || info.hasLostFocus || info.hasMouseEnter || info.hasMouseLeave ||
                info.hasMouseDown || info.hasMouseUp || info.hasMouseMove ||
                info.hasKeyPress || info.hasKeyDown || info.hasKeyUp || info.hasDblClick || info.hasValidate || info.hasMouseHover) {
                subclassCtrls.push_back(std::move(info));
            }
        }

        // P0-5修复: 扫描Frame容器内的子控件
        for (const auto& ctrl : frmDesc.formControl.children) {
            if (ctrl.controlType != FrmControlType::Frame) continue;
            for (const auto& child : ctrl.children) {
                std::string childNameLower = child.controlName;
                std::transform(childNameLower.begin(), childNameLower.end(), childNameLower.begin(), ::tolower);
                if (emitted.count(childNameLower)) continue;
                emitted.insert(childNameLower);
                if (child.controlType == FrmControlType::Timer ||
                    child.controlType == FrmControlType::Menu ||
                    child.controlType == FrmControlType::CommonDialog ||
                    child.controlType == FrmControlType::WebBrowser) continue;
                SubclassInfo info;
                info.ctrlName = child.controlName;
                info.ctrlType = child.controlType;
                info.hwndVar = "vb6_hwnd_" + cIdent(child.controlName);
                if (child.controlType == FrmControlType::PictureBox ||
                    child.controlType == FrmControlType::Frame ||
                    child.controlType == FrmControlType::Label ||
                    child.controlType == FrmControlType::Image) {
                    info.hasGotFocus = symTab_.lookup(child.controlName + "_GotFocus") != nullptr;
                    info.hasLostFocus = symTab_.lookup(child.controlName + "_LostFocus") != nullptr;
                }
                // P20-12: Validate event (all control types)
                info.hasValidate = symTab_.lookup(child.controlName + "_Validate") != nullptr;
                info.hasMouseEnter = symTab_.lookup(child.controlName + "_MouseEnter") != nullptr;
                info.hasMouseLeave = symTab_.lookup(child.controlName + "_MouseLeave") != nullptr;
                info.hasMouseDown = symTab_.lookup(child.controlName + "_MouseDown") != nullptr;
                info.hasMouseUp = symTab_.lookup(child.controlName + "_MouseUp") != nullptr;
                info.hasMouseMove = symTab_.lookup(child.controlName + "_MouseMove") != nullptr;
                info.hasKeyPress = symTab_.lookup(child.controlName + "_KeyPress") != nullptr;
                info.hasKeyDown = symTab_.lookup(child.controlName + "_KeyDown") != nullptr;
                info.hasKeyUp = symTab_.lookup(child.controlName + "_KeyUp") != nullptr;
                info.hasDblClick = symTab_.lookup(child.controlName + "_DblClick") != nullptr;
                info.hasMouseHover = symTab_.lookup(child.controlName + "_MouseHover") != nullptr;
                if (info.hasGotFocus || info.hasLostFocus || info.hasMouseEnter || info.hasMouseLeave ||
                    info.hasMouseDown || info.hasMouseUp || info.hasMouseMove ||
                    info.hasKeyPress || info.hasKeyDown || info.hasKeyUp || info.hasDblClick || info.hasValidate || info.hasMouseHover) {
                    subclassCtrls.push_back(std::move(info));
                }
            }
        }
        // 为每个需要子类化的控件生成 subclass WndProc
        for (const auto& info : subclassCtrls) {
            std::string subProcName = "vb6_ctrl_subproc_" + cIdent(info.ctrlName);
            c_.emitLine("// P18-F: Subclass WndProc for " + info.ctrlName);
            c_.emitLine("static LRESULT CALLBACK " + subProcName + "(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {");
            c_.indent();

            // GotFocus (WM_SETFOCUS)
            if (info.hasGotFocus) {
                std::string fn = cProcName(info.ctrlName + "_GotFocus", AccessLevel::Private);
                c_.emitLine("if (msg == WM_SETFOCUS) { extern void " + fn + "(); " + fn + "(); }");
            }
            // LostFocus (WM_KILLFOCUS)
            if (info.hasLostFocus) {
                std::string fn = cProcName(info.ctrlName + "_LostFocus", AccessLevel::Private);
                if (info.hasValidate) {
                c_.emitLine("if (msg == WM_KILLFOCUS && !GetPropW(hwnd, L\"VB6_ValidateCancel\")) { extern void " + fn + "(); " + fn + "(); }");
            } else {
                c_.emitLine("if (msg == WM_KILLFOCUS) { extern void " + fn + "(); " + fn + "(); }");
            }
            }
            // P20-12: Validate event (WM_KILLFOCUS with CausesValidation + Cancel)
            // Validate fires BEFORE LostFocus when the gaining focus control has CausesValidation=True
            if (info.hasValidate) {
                std::string fn = cProcName(info.ctrlName + "_Validate", AccessLevel::Private);
                c_.emitLine("// Validate(Cancel As Boolean) - fires before LostFocus");
                c_.emitLine("if (msg == WM_KILLFOCUS) {");
                c_.indent();
                c_.emitLine("HWND vb6_gaining = (HWND)wp;");
                c_.emitLine("if (vb6_GetCausesValidation((void*)vb6_gaining)) {");
                c_.indent();
                c_.emitLine("int16_t vb6_vcancel = 0;");
                c_.emitLine("extern void " + fn + "(int16_t*);");
                c_.emitLine(fn + "(&vb6_vcancel);");
                c_.emitLine("if (vb6_vcancel) {");
                c_.indent();
                c_.emitLine("// Cancel=True: prevent focus transfer, restore focus");
                c_.emitLine("SetPropW(hwnd, L\"VB6_ValidateCancel\", (HANDLE)1);");
                c_.emitLine("PostMessage(hwnd, 0x7FFF, 0, 0); /* WM_VB6_RESTOREFOCUS */");
                c_.dedent();
                c_.emitLine("}");
                c_.dedent();
                c_.emitLine("}");
                c_.dedent();
                c_.emitLine("}");
            }

            // MouseEnter/MouseLeave via TrackMouseEvent
            if (info.hasMouseEnter || info.hasMouseLeave || info.hasMouseHover) {
                c_.emitLine("if (msg == WM_MOUSEMOVE) {");
                c_.indent();
                c_.emitLine("if (!GetPropW(hwnd, L\"VB6_MouseTracked\")) {");
                c_.indent();
                c_.emitLine("SetPropW(hwnd, L\"VB6_MouseTracked\", (HANDLE)1);");
                c_.emitLine("vb6_StartMouseTracking((void*)hwnd);");
                if (info.hasMouseEnter) {
                    std::string fn = cProcName(info.ctrlName + "_MouseEnter", AccessLevel::Private);
                    c_.emitLine("{ extern void " + fn + "(); " + fn + "(); }");
                }
                c_.dedent();
                c_.emitLine("}");
                // 如果同时有MouseMove事件，也在这里分发
                if (info.hasMouseMove) {
                    std::string fn = cProcName(info.ctrlName + "_MouseMove", AccessLevel::Private);
                    c_.emitLine("{ int16_t vb6_button = 0; int16_t vb6_shift = 0;");
                    c_.emitLine("if (wp & MK_LBUTTON) vb6_button |= 1; if (wp & MK_RBUTTON) vb6_button |= 2; if (wp & MK_MBUTTON) vb6_button |= 4;");
                    c_.emitLine("if (wp & MK_SHIFT) vb6_shift |= 1; if (wp & MK_CONTROL) vb6_shift |= 2; if (GetKeyState(VK_MENU) & 0x8000) vb6_shift |= 4;");
                    c_.emitLine("float vb6_x = (float)(int16_t)LOWORD(lp); float vb6_y = (float)(int16_t)HIWORD(lp);");
                    c_.emitLine("extern void " + fn + "(int16_t*, int16_t*, float*, float*); " + fn + "(&vb6_button, &vb6_shift, &vb6_x, &vb6_y); }");
                }
                c_.dedent();
                c_.emitLine("}");
                c_.emitLine("if (msg == WM_MOUSELEAVE) {");
                c_.indent();
                c_.emitLine("RemovePropW(hwnd, L\"VB6_MouseTracked\");");
                if (info.hasMouseLeave) {
                    std::string fn = cProcName(info.ctrlName + "_MouseLeave", AccessLevel::Private);
                    c_.emitLine("{ extern void " + fn + "(); " + fn + "(); }");
                }
                c_.dedent();
                c_.emitLine("}");
                if (info.hasMouseHover) {
                    c_.emitLine("if (msg == WM_MOUSEHOVER) {");
                    c_.indent();
                    std::string hfn = cProcName(info.ctrlName + "_MouseHover", AccessLevel::Private);
                    c_.emitLine("{ extern void " + hfn + "(); " + hfn + "(); }");
                    c_.dedent();
                    c_.emitLine("}");
                }
            } else if (info.hasMouseMove) {
                // 只有MouseMove没有MouseEnter/Leave
                c_.emitLine("if (msg == WM_MOUSEMOVE) {");
                c_.indent();
                std::string fn = cProcName(info.ctrlName + "_MouseMove", AccessLevel::Private);
                c_.emitLine("{ int16_t vb6_button = 0; int16_t vb6_shift = 0;");
                c_.emitLine("if (wp & MK_LBUTTON) vb6_button |= 1; if (wp & MK_RBUTTON) vb6_button |= 2; if (wp & MK_MBUTTON) vb6_button |= 4;");
                c_.emitLine("if (wp & MK_SHIFT) vb6_shift |= 1; if (wp & MK_CONTROL) vb6_shift |= 2; if (GetKeyState(VK_MENU) & 0x8000) vb6_shift |= 4;");
                c_.emitLine("float vb6_x = (float)(int16_t)LOWORD(lp); float vb6_y = (float)(int16_t)HIWORD(lp);");
                c_.emitLine("extern void " + fn + "(int16_t*, int16_t*, float*, float*); " + fn + "(&vb6_button, &vb6_shift, &vb6_x, &vb6_y); }");
                c_.dedent();
                c_.emitLine("}");
            }

            // MouseDown
            if (info.hasMouseDown) {
                std::string fn = cProcName(info.ctrlName + "_MouseDown", AccessLevel::Private);
                c_.emitLine("if (msg == WM_LBUTTONDOWN || msg == WM_RBUTTONDOWN || msg == WM_MBUTTONDOWN) {");
                c_.indent();
                c_.emitLine("{ int16_t vb6_button = 0; int16_t vb6_shift = 0;");
                c_.emitLine("if (msg == WM_LBUTTONDOWN) vb6_button = 1;");
                c_.emitLine("else if (msg == WM_RBUTTONDOWN) vb6_button = 2;");
                c_.emitLine("else if (msg == WM_MBUTTONDOWN) vb6_button = 4;");
                c_.emitLine("if (wp & MK_SHIFT) vb6_shift |= 1; if (wp & MK_CONTROL) vb6_shift |= 2; if (GetKeyState(VK_MENU) & 0x8000) vb6_shift |= 4;");
                c_.emitLine("float vb6_x = (float)(int16_t)LOWORD(lp); float vb6_y = (float)(int16_t)HIWORD(lp);");
                c_.emitLine("extern void " + fn + "(int16_t*, int16_t*, float*, float*); " + fn + "(&vb6_button, &vb6_shift, &vb6_x, &vb6_y); }");
                c_.dedent();
                c_.emitLine("}");
            }
            // MouseUp
            if (info.hasMouseUp) {
                std::string fn = cProcName(info.ctrlName + "_MouseUp", AccessLevel::Private);
                c_.emitLine("if (msg == WM_LBUTTONUP || msg == WM_RBUTTONUP || msg == WM_MBUTTONUP) {");
                c_.indent();
                c_.emitLine("{ int16_t vb6_button = 0; int16_t vb6_shift = 0;");
                c_.emitLine("if (msg == WM_LBUTTONUP) vb6_button = 1;");
                c_.emitLine("else if (msg == WM_RBUTTONUP) vb6_button = 2;");
                c_.emitLine("else if (msg == WM_MBUTTONUP) vb6_button = 4;");
                c_.emitLine("if (wp & MK_SHIFT) vb6_shift |= 1; if (wp & MK_CONTROL) vb6_shift |= 2; if (GetKeyState(VK_MENU) & 0x8000) vb6_shift |= 4;");
                c_.emitLine("float vb6_x = (float)(int16_t)LOWORD(lp); float vb6_y = (float)(int16_t)HIWORD(lp);");
                c_.emitLine("extern void " + fn + "(int16_t*, int16_t*, float*, float*); " + fn + "(&vb6_button, &vb6_shift, &vb6_x, &vb6_y); }");
                c_.dedent();
                c_.emitLine("}");
            }

            // KeyPress (WM_CHAR)
            if (info.hasKeyPress) {
                std::string fn = cProcName(info.ctrlName + "_KeyPress", AccessLevel::Private);
                c_.emitLine("if (msg == WM_CHAR) {");
                c_.indent();
                c_.emitLine("{ int16_t vb6_keyascii = (int16_t)(unsigned char)wp;");
                c_.emitLine("extern void " + fn + "(int16_t*); " + fn + "(&vb6_keyascii); }");
                c_.dedent();
                c_.emitLine("}");
            }
            // KeyDown (WM_KEYDOWN)
            if (info.hasKeyDown) {
                std::string fn = cProcName(info.ctrlName + "_KeyDown", AccessLevel::Private);
                c_.emitLine("if (msg == WM_KEYDOWN) {");
                c_.indent();
                c_.emitLine("{ int16_t vb6_keycode = (int16_t)wp; int16_t vb6_shift = 0;");
                c_.emitLine("if (GetKeyState(VK_SHIFT) & 0x8000) vb6_shift |= 1; if (GetKeyState(VK_CONTROL) & 0x8000) vb6_shift |= 2; if (GetKeyState(VK_MENU) & 0x8000) vb6_shift |= 4;");
                c_.emitLine("extern void " + fn + "(int16_t*, int16_t*); " + fn + "(&vb6_keycode, &vb6_shift); }");
                c_.dedent();
                c_.emitLine("}");
            }
            // KeyUp (WM_KEYUP)
            if (info.hasKeyUp) {
                std::string fn = cProcName(info.ctrlName + "_KeyUp", AccessLevel::Private);
                c_.emitLine("if (msg == WM_KEYUP) {");
                c_.indent();
                c_.emitLine("{ int16_t vb6_keycode = (int16_t)wp; int16_t vb6_shift = 0;");
                c_.emitLine("if (GetKeyState(VK_SHIFT) & 0x8000) vb6_shift |= 1; if (GetKeyState(VK_CONTROL) & 0x8000) vb6_shift |= 2; if (GetKeyState(VK_MENU) & 0x8000) vb6_shift |= 4;");
                c_.emitLine("extern void " + fn + "(int16_t*, int16_t*); " + fn + "(&vb6_keycode, &vb6_shift); }");
                c_.dedent();
                c_.emitLine("}");
            }
            // P1-13修复: DblClick (WM_LBUTTONDBLCLK)
            if (info.hasDblClick) {
                std::string fn = cProcName(info.ctrlName + "_DblClick", AccessLevel::Private);
                c_.emitLine("if (msg == WM_LBUTTONDBLCLK) { extern void " + fn + "(); " + fn + "(); }");
            }

            // P20-12: WM_VB6_RESTOREFOCUS - restore focus after Validate cancel
            if (info.hasValidate) {
                c_.emitLine("if (msg == 0x7FFF) { RemovePropW(hwnd, L\"VB6_ValidateCancel\"); SetFocus(hwnd); return 0; }");
            }

            // WM_DESTROY: 移除子类化 + 清理ValidateCancel属性
            c_.emitLine("if (msg == WM_DESTROY) { RemovePropW(hwnd, L\"VB6_ValidateCancel\"); vb6_RemoveControlSubclass((void*)hwnd); }");

            // Call original WndProc for unhandled messages
            c_.emitLine("WNDPROC vb6_orig = (WNDPROC)vb6_GetOriginalWndProc((void*)hwnd);");
            c_.emitLine("if (vb6_orig) return CallWindowProcW(vb6_orig, hwnd, msg, wp, lp);");
            c_.emitLine("return DefWindowProcW(hwnd, msg, wp, lp);");
            c_.dedent();
            c_.emitLine("}");
            c_.emitBlank();
        }

        // Store subclass controls for WM_CREATE installation (used later in CreateControls)
        // We'll emit the install calls in the createFn, after all controls are created
    }

    // WndProc
    c_.emitLine("// === " + formName + " WndProc ===");
    c_.emitLine("LRESULT CALLBACK " + wndProc + "(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {");
    c_.indent();
    c_.emitLine("switch (msg) {");

    // WM_CREATE: 创建控件
    c_.indent();
    c_.emitLine("case WM_CREATE: {");
    c_.indent();
    c_.emitLine("CREATESTRUCTA* cs = (CREATESTRUCTA*)lParam;");
    c_.emitLine(createFn + "((void*)hwnd, (void*)cs->hInstance);");
    c_.emitLine("vb6_Forms_Register((void*)hwnd);");

    // P20-40: 从.frm属性初始化Form属性
    {
        auto kpIt = frmDesc.formControl.properties.find("KeyPreview");
        if (kpIt != frmDesc.formControl.properties.end() && kpIt->second.intValue != 0) {
            c_.emitLine("vb6_SetKeyPreview((void*)hwnd, -1);");
        }
        auto cbIt = frmDesc.formControl.properties.find("ControlBox");
        if (cbIt != frmDesc.formControl.properties.end() && cbIt->second.intValue == 0) {
            c_.emitLine("vb6_SetControlBox((void*)hwnd, 0);");
        }
        auto mbIt = frmDesc.formControl.properties.find("MaxButton");
        if (mbIt != frmDesc.formControl.properties.end() && mbIt->second.intValue == 0) {
            c_.emitLine("vb6_SetMaxButton((void*)hwnd, 0);");
        }
        auto mnIt = frmDesc.formControl.properties.find("MinButton");
        if (mnIt != frmDesc.formControl.properties.end() && mnIt->second.intValue == 0) {
            c_.emitLine("vb6_SetMinButton((void*)hwnd, 0);");
        }
        auto wsIt = frmDesc.formControl.properties.find("WindowState");
        if (wsIt != frmDesc.formControl.properties.end() && wsIt->second.intValue != 0) {
            c_.emitLine("vb6_SetWindowState((void*)hwnd, " + std::to_string((int)wsIt->second.intValue) + ");");
        }
    }

    // M22-fix: 在Form_Load之前先赋值vb6_hwnd_<Form>，这样Form_Load中
    // Me.Caption / Me.Text 等才能通过vb6_hwnd_引用到正确的HWND
    // CreateFormWindow还未返回，vb6_hwnd_尚未被赋值
    c_.emitLine("vb6_hwnd_" + cIdent(formName) + " = (void*)hwnd;  /* early assign for Form_Load */");

    // 调用VB6 Form_Load事件 (仅当存在时调用)
    std::string formLoadFn = cProcName("Form_Load", AccessLevel::Private);
    auto* formLoadSym = symTab_.lookup("Form_Load");
    if (formLoadSym) {
        c_.emitLine("{ /* Form_Load */ extern void " + formLoadFn + "(); " + formLoadFn + "(); }");
    }
    c_.emitLine("break;");
    c_.dedent();
    c_.emitLine("}");

    // WM_COMMAND: 按钮点击等 (P7.6: 支持控件数组Index参数)
    c_.emitLine("case WM_COMMAND: {");
    c_.indent();
    c_.emitLine("int id = LOWORD(wParam);");
    c_.emitLine("int code = HIWORD(wParam);");

    // 为每个CommandButton/CheckBox/OptionButton生成WM_COMMAND处理
    ctrlId = 100;
    for (const auto& ctrl : frmDesc.formControl.children) {
        std::string ctrlNameLower = ctrl.controlName;
        std::transform(ctrlNameLower.begin(), ctrlNameLower.end(), ctrlNameLower.begin(), ::tolower);
        bool isArrayCtrl = knownControlArrays_.count(ctrlNameLower) > 0;

        if (ctrl.controlType == FrmControlType::CommandButton ||
            ctrl.controlType == FrmControlType::CheckBox ||
            ctrl.controlType == FrmControlType::OptionButton) {
            if (symTab_.lookup(ctrl.controlName + "_Click")) {
                std::string clickFn = cProcName(ctrl.controlName + "_Click", AccessLevel::Private);
                c_.emitLine("if (id == " + std::to_string(ctrlId) + ") {");
                c_.indent();
                if (isArrayCtrl) {
                    // 控件数组: 事件处理带Index参数
                    std::string idxVar = "vb6_idx_" + std::to_string(ctrl.index >= 0 ? ctrl.index : 0);
                    c_.emitLine("{ int16_t " + idxVar + " = " + std::to_string(ctrl.index >= 0 ? ctrl.index : 0) + "; extern void " + clickFn + "(int16_t*); " + clickFn + "(&" + idxVar + "); }");
                } else {
                    c_.emitLine("{ extern void " + clickFn + "(); " + clickFn + "(); }");
                }
                c_.dedent();
                c_.emitLine("}");
            }
        }
        else if (ctrl.controlType == FrmControlType::TextBox) {
            // P14.4.1: EN_CHANGE (code=0x0300=768) -> _Change() (仅当处理器存在时)
            if (symTab_.lookup(ctrl.controlName + "_Change")) {
                std::string changeFn = cProcName(ctrl.controlName + "_Change", AccessLevel::Private);
                c_.emitLine("if (id == " + std::to_string(ctrlId) + " && code == 768) {");
                c_.indent();
                c_.emitLine("{ extern void " + changeFn + "(); " + changeFn + "(); }");
                c_.dedent();
                c_.emitLine("}");
            }
        } else if (ctrl.controlType == FrmControlType::ListBox) {
            // P14.4.1: LBN_SELCHANGE (code=1) -> _Click() (仅当处理器存在时)
            if (symTab_.lookup(ctrl.controlName + "_Click")) {
                std::string clickFn = cProcName(ctrl.controlName + "_Click", AccessLevel::Private);
                c_.emitLine("if (id == " + std::to_string(ctrlId) + " && code == 1) {");
                c_.indent();
                c_.emitLine("{ extern void " + clickFn + "(); " + clickFn + "(); }");
                c_.dedent();
                c_.emitLine("}");
            }
            // LBN_DBLCLK (code=2) -> _DblClick() (仅当处理器存在时)
            if (symTab_.lookup(ctrl.controlName + "_DblClick")) {
                std::string dblClickFn = cProcName(ctrl.controlName + "_DblClick", AccessLevel::Private);
                c_.emitLine("if (id == " + std::to_string(ctrlId) + " && code == 2) {");
                c_.indent();
                c_.emitLine("{ extern void " + dblClickFn + "(); " + dblClickFn + "(); }");
                c_.dedent();
                c_.emitLine("}");
            }
        } else if (ctrl.controlType == FrmControlType::ComboBox) {
            // P14.4.1: CBN_SELCHANGE (code=1) -> _Click() (仅当处理器存在时)
            if (symTab_.lookup(ctrl.controlName + "_Click")) {
                std::string clickFn = cProcName(ctrl.controlName + "_Click", AccessLevel::Private);
                c_.emitLine("if (id == " + std::to_string(ctrlId) + " && code == 1) {");
                c_.indent();
                c_.emitLine("{ extern void " + clickFn + "(); " + clickFn + "(); }");
                c_.dedent();
                c_.emitLine("}");
            }
            // CBN_EDITCHANGE (code=5) -> _Change() (仅当处理器存在时)
            if (symTab_.lookup(ctrl.controlName + "_Change")) {
                std::string changeFn = cProcName(ctrl.controlName + "_Change", AccessLevel::Private);
                c_.emitLine("if (id == " + std::to_string(ctrlId) + " && code == 5) {");
                c_.indent();
                c_.emitLine("{ extern void " + changeFn + "(); " + changeFn + "(); }");
                c_.dedent();
                c_.emitLine("}");
            }
            // CBN_DROPDOWN (code=7) -> _DropDown() (仅当处理器存在时)
            if (symTab_.lookup(ctrl.controlName + "_DropDown")) {
                std::string dropDownFn = cProcName(ctrl.controlName + "_DropDown", AccessLevel::Private);
                c_.emitLine("if (id == " + std::to_string(ctrlId) + " && code == 7) {");
                c_.indent();
                c_.emitLine("{ extern void " + dropDownFn + "(); " + dropDownFn + "(); }");
                c_.dedent();
                c_.emitLine("}");
            }
            // CBN_CLOSEUP (code=8) -> _CloseUp() (仅当处理器存在时)
            if (symTab_.lookup(ctrl.controlName + "_CloseUp")) {
                std::string closeUpFn = cProcName(ctrl.controlName + "_CloseUp", AccessLevel::Private);
                c_.emitLine("if (id == " + std::to_string(ctrlId) + " && code == 8) {");
                c_.indent();
                c_.emitLine("{ extern void " + closeUpFn + "(); " + closeUpFn + "(); }");
                c_.dedent();
                c_.emitLine("}");
            }
        }
        ctrlId++;
    }

    // P16: WithEvents控件事件分发 (按HWND匹配, 在标准命名handler之后)
    // 对于每个WithEvents控件变量, 检查其HWND是否匹配lParam, 是则调用对应handler
    if (!knownWithEventsCtrlVars_.empty()) {
        c_.emitLine("if ((HWND)lParam != NULL) {");
        c_.indent();
        c_.emitLine("HWND hCtrlWE = (HWND)lParam;");
        for (const auto& we : knownWithEventsCtrlVars_) {
            std::string varLower = we.first;
            FrmControlType ctrlType = we.second;
            std::string varName = knownWithEventsCtrlOrigNames_.count(varLower) ? knownWithEventsCtrlOrigNames_[varLower] : cIdent(varLower);  // 用原始大小写变量名
            // 根据控件类型生成不同的事件分发
            if (ctrlType == FrmControlType::CommandButton ||
                ctrlType == FrmControlType::CheckBox ||
                ctrlType == FrmControlType::OptionButton) {
                // BN_CLICKED (code=0) → _Click()
                auto* clickSym = symTab_.lookup(varLower + "_click");
                if (clickSym) {
                    std::string clickFn = cProcName(clickSym->name, AccessLevel::Private);
                    c_.emitLine("if (code == 0 && " + varName + " != NULL && " + varName + " == hCtrlWE) {");
                    c_.indent();
                    c_.emitLine("{ extern void " + clickFn + "(); " + clickFn + "(); }");
                    c_.dedent();
                    c_.emitLine("}");
                }
            } else if (ctrlType == FrmControlType::TextBox) {
                // EN_CHANGE (code=768) → _Change()
                auto* changeSym = symTab_.lookup(varLower + "_change");
                if (changeSym) {
                    std::string changeFn = cProcName(changeSym->name, AccessLevel::Private);
                    c_.emitLine("if (code == 768 && " + varName + " != NULL && " + varName + " == hCtrlWE) {");
                    c_.indent();
                    c_.emitLine("{ extern void " + changeFn + "(); " + changeFn + "(); }");
                    c_.dedent();
                    c_.emitLine("}");
                }
            } else if (ctrlType == FrmControlType::ListBox) {
                // LBN_SELCHANGE (code=1) → _Click()
                auto* clickSym = symTab_.lookup(varLower + "_click");
                if (clickSym) {
                    std::string clickFn = cProcName(clickSym->name, AccessLevel::Private);
                    c_.emitLine("if (code == 1 && " + varName + " != NULL && " + varName + " == hCtrlWE) {");
                    c_.indent();
                    c_.emitLine("{ extern void " + clickFn + "(); " + clickFn + "(); }");
                    c_.dedent();
                    c_.emitLine("}");
                }
                // LBN_DBLCLK (code=2) → _DblClick()
                auto* dblClickSym = symTab_.lookup(varLower + "_dblclick");
                if (dblClickSym) {
                    std::string dblClickFn = cProcName(dblClickSym->name, AccessLevel::Private);
                    c_.emitLine("if (code == 2 && " + varName + " != NULL && " + varName + " == hCtrlWE) {");
                    c_.indent();
                    c_.emitLine("{ extern void " + dblClickFn + "(); " + dblClickFn + "(); }");
                    c_.dedent();
                    c_.emitLine("}");
                }
            } else if (ctrlType == FrmControlType::ComboBox) {
                // CBN_SELCHANGE (code=1) → _Click()
                auto* clickSym = symTab_.lookup(varLower + "_click");
                if (clickSym) {
                    std::string clickFn = cProcName(clickSym->name, AccessLevel::Private);
                    c_.emitLine("if (code == 1 && " + varName + " != NULL && " + varName + " == hCtrlWE) {");
                    c_.indent();
                    c_.emitLine("{ extern void " + clickFn + "(); " + clickFn + "(); }");
                    c_.dedent();
                    c_.emitLine("}");
                }
                // CBN_EDITCHANGE (code=5) → _Change()
                auto* changeSym = symTab_.lookup(varLower + "_change");
                if (changeSym) {
                    std::string changeFn = cProcName(changeSym->name, AccessLevel::Private);
                    c_.emitLine("if (code == 5 && " + varName + " != NULL && " + varName + " == hCtrlWE) {");
                    c_.indent();
                    c_.emitLine("{ extern void " + changeFn + "(); " + changeFn + "(); }");
                    c_.dedent();
                    c_.emitLine("}");
                }
            }
            // Timer: 已通过SetTimer回调处理, 无需WndProc分发
            // HScrollBar/VScrollBar: 在WM_HSCROLL/WM_VSCROLL中处理(下方)
        }
        c_.dedent();
        c_.emitLine("}");  /* close if ((HWND)lParam != NULL) */
    }
    // P7.8: 菜单项点击事件派发 (Menu控件ID从1000开始)
    {
        int menuId = 1000;
        for (const auto& ctrl : frmDesc.formControl.children) {
            if (ctrl.controlType == FrmControlType::Menu) {
                emitMenuClickDispatch(ctrl, menuId);
            }
        }
    }

    // P18-B: GotFocus/LostFocus events (EN_SETFOCUS/EN_KILLFOCUS/CBN_SETFOCUS/CBN_KILLFOCUS)
    {
        ctrlId = 100;
        for (const auto& ctrl : frmDesc.formControl.children) {
            std::string gotFn = cProcName(ctrl.controlName + "_GotFocus", AccessLevel::Private);
            std::string lostFn = cProcName(ctrl.controlName + "_LostFocus", AccessLevel::Private);
            bool hasGot = symTab_.lookup(ctrl.controlName + "_GotFocus") != nullptr;
            bool hasLost = symTab_.lookup(ctrl.controlName + "_LostFocus") != nullptr;
            bool hasValidate = symTab_.lookup(ctrl.controlName + "_Validate") != nullptr;
            if (hasGot || hasLost) {
                if (ctrl.controlType == FrmControlType::TextBox) {
                    // EN_SETFOCUS=256, EN_KILLFOCUS=512
                    if (hasGot) c_.emitLine("if (id == " + std::to_string(ctrlId) + " && code == 256) { extern void " + gotFn + "(); " + gotFn + "(); }");
                    if (hasLost) { if (hasValidate) c_.emitLine("if (id == " + std::to_string(ctrlId) + " && code == 512 && !GetPropW((HWND)lParam, L\"VB6_ValidateCancel\")) { extern void " + lostFn + "(); " + lostFn + "(); }"); else c_.emitLine("if (id == " + std::to_string(ctrlId) + " && code == 512) { extern void " + lostFn + "(); " + lostFn + "(); }"); }
                } else if (ctrl.controlType == FrmControlType::ComboBox) {
                    // CBN_SETFOCUS=1024, CBN_KILLFOCUS=2048
                    if (hasGot) c_.emitLine("if (id == " + std::to_string(ctrlId) + " && code == 1024) { extern void " + gotFn + "(); " + gotFn + "(); }");
                    if (hasLost) { if (hasValidate) c_.emitLine("if (id == " + std::to_string(ctrlId) + " && code == 2048 && !GetPropW((HWND)lParam, L\"VB6_ValidateCancel\")) { extern void " + lostFn + "(); " + lostFn + "(); }"); else c_.emitLine("if (id == " + std::to_string(ctrlId) + " && code == 2048) { extern void " + lostFn + "(); " + lostFn + "(); }"); }
                } else if (ctrl.controlType == FrmControlType::ListBox) {
                    // LBN_SETFOCUS=4, LBN_KILLFOCUS=5
                    if (hasGot) c_.emitLine("if (id == " + std::to_string(ctrlId) + " && code == 4) { extern void " + gotFn + "(); " + gotFn + "(); }");
                    if (hasLost) { if (hasValidate) c_.emitLine("if (id == " + std::to_string(ctrlId) + " && code == 5 && !GetPropW((HWND)lParam, L\"VB6_ValidateCancel\")) { extern void " + lostFn + "(); " + lostFn + "(); }"); else c_.emitLine("if (id == " + std::to_string(ctrlId) + " && code == 5) { extern void " + lostFn + "(); " + lostFn + "(); }"); }
                } else if (ctrl.controlType == FrmControlType::CommandButton ||
                           ctrl.controlType == FrmControlType::CheckBox ||
                           ctrl.controlType == FrmControlType::OptionButton) {
                    // BN_SETFOCUS=6, BN_KILLFOCUS=7 (button class notifications via WM_COMMAND)
                    if (hasGot) c_.emitLine("if (id == " + std::to_string(ctrlId) + " && code == 6) { extern void " + gotFn + "(); " + gotFn + "(); }");
                    if (hasLost) { if (hasValidate) c_.emitLine("if (id == " + std::to_string(ctrlId) + " && code == 7 && !GetPropW((HWND)lParam, L\"VB6_ValidateCancel\")) { extern void " + lostFn + "(); " + lostFn + "(); }"); else c_.emitLine("if (id == " + std::to_string(ctrlId) + " && code == 7) { extern void " + lostFn + "(); " + lostFn + "(); }"); }
                }
                // Label: no GotFocus/LostFocus in VB6 (windowless control)
                // PictureBox: GotFocus/LostFocus handled via subclassing (P18-F)
            }
            ctrlId++;
        }
    }
    c_.emitLine("break;");
    c_.dedent();
    c_.emitLine("}");


    // WM_CLOSE: 调用Form_Unload判断是否允许关闭
    c_.emitLine("case WM_CLOSE: {");
    c_.indent();
    {
        std::string formUnloadFn = cProcName("Form_Unload", AccessLevel::Private);
        auto* formUnloadSym2 = symTab_.lookup("Form_Unload");
        if (formUnloadSym2) {
            // 生成trampoline: 桥接 int(*)(void) 回调和 void(int16_t*) VB6签名
            std::string trampoline = formUnloadFn + "_trampoline";
            // P18-B: Form_QueryUnload - check if close is allowed before Unload
            {
                std::string queryUnloadFn = cProcName("Form_QueryUnload", AccessLevel::Private);
                auto* quSym = symTab_.lookup("Form_QueryUnload");
                if (quSym) {
                    c_.emitLine("{ int16_t vb6_cancel = 0; int16_t vb6_unloadmode = 0; /* 0=vbFormControlMenu */");
                    c_.emitLine("extern void " + queryUnloadFn + "(int16_t*, int16_t*); " + queryUnloadFn + "(&vb6_cancel, &vb6_unloadmode);");
                    c_.emitLine("if (vb6_cancel != 0) return 0; /* Cancel close */ }");
                }
            }
            c_.emitLine("vb6_SetFormUnloadCallback((void*)" + trampoline + ");");
            c_.emitLine("if (vb6_QueryFormUnload() == 0) {");
            c_.indent();
            c_.emitLine("DestroyWindow(hwnd);");
            c_.dedent();
            c_.emitLine("}");
        } else {
            c_.emitLine("DestroyWindow(hwnd);");
        }
    }
    c_.emitLine("break;");
    c_.dedent();
    c_.emitLine("}");
    // WM_DESTROY: PostQuitMessage (如果是主窗体)
    c_.emitLine("case WM_DESTROY: {");
    c_.indent();
    c_.emitLine("vb6_Forms_Unregister((void*)hwnd);");
    // P18-F: 移除所有控件子类化
    {
        auto needsSubclass = [&](const FrmControl& ctrl) -> bool {
            return (ctrl.controlType == FrmControlType::PictureBox ||
                    ctrl.controlType == FrmControlType::Frame ||
                    ctrl.controlType == FrmControlType::Label ||
                    ctrl.controlType == FrmControlType::Image)
                   && (symTab_.lookup(ctrl.controlName + "_GotFocus") ||
                       symTab_.lookup(ctrl.controlName + "_LostFocus"))
                || symTab_.lookup(ctrl.controlName + "_MouseEnter")
                || symTab_.lookup(ctrl.controlName + "_MouseLeave")
                || symTab_.lookup(ctrl.controlName + "_MouseDown")
                || symTab_.lookup(ctrl.controlName + "_MouseUp")
                || symTab_.lookup(ctrl.controlName + "_MouseMove")
                || symTab_.lookup(ctrl.controlName + "_KeyPress")
                || symTab_.lookup(ctrl.controlName + "_KeyDown")
                || symTab_.lookup(ctrl.controlName + "_KeyUp")
              || symTab_.lookup(ctrl.controlName + "_Validate")
                || symTab_.lookup(ctrl.controlName + "_MouseHover");
        };
        std::unordered_set<std::string> subEmitted;
        for (const auto& ctrl : frmDesc.formControl.children) {
            std::string ctrlNameLower = ctrl.controlName;
            std::transform(ctrlNameLower.begin(), ctrlNameLower.end(), ctrlNameLower.begin(), ::tolower);
            if (subEmitted.count(ctrlNameLower)) continue;
            subEmitted.insert(ctrlNameLower);
            if (!needsSubclass(ctrl)) continue;
            if (knownControlArrays_.count(ctrlNameLower)) {
                c_.emitLine("{ int vb6_si; for (vb6_si = 0; vb6_si < VB6_CTRLARR_MAX; vb6_si++) {");
                c_.emitLine("  void* vb6_sub_hwnd = vb6_CtrlArr_GetAt(&vb6_arr_" + cIdent(ctrl.controlName) + ", vb6_si);");
                c_.emitLine("  if (vb6_sub_hwnd) vb6_RemoveControlSubclass(vb6_sub_hwnd); } }");
            } else {
                c_.emitLine("vb6_RemoveControlSubclass((void*)vb6_hwnd_" + cIdent(ctrl.controlName) + ");");
            }
        }
        // P0-5修复: Frame容器内子控件
        for (const auto& ctrl : frmDesc.formControl.children) {
            if (ctrl.controlType != FrmControlType::Frame) continue;
            for (const auto& child : ctrl.children) {
                std::string childNameLower = child.controlName;
                std::transform(childNameLower.begin(), childNameLower.end(), childNameLower.begin(), ::tolower);
                if (subEmitted.count(childNameLower)) continue;
                subEmitted.insert(childNameLower);
                if (!needsSubclass(child)) continue;
                c_.emitLine("vb6_RemoveControlSubclass((void*)vb6_hwnd_" + cIdent(child.controlName) + ");");
            }
        }
    }
    c_.emitLine("PostQuitMessage(0);");
    c_.emitLine("break;");
    c_.dedent();
    c_.emitLine("}");

    // P14.4.1b: WM_HSCROLL / WM_VSCROLL - ScrollBar事件
    c_.emitLine("case WM_HSCROLL:");
    c_.emitLine("case WM_VSCROLL: {");
    c_.indent();
    bool hasScrollBar = false;
    for (const auto& ctrl : frmDesc.formControl.children) {
        if (ctrl.controlType == FrmControlType::HScrollBar || ctrl.controlType == FrmControlType::VScrollBar) { hasScrollBar = true; break; }
    }
    if (hasScrollBar) {
    c_.emitLine("{ void* scrollHwnd = (void*)lParam; int scrollCode = LOWORD(wParam);");
    for (const auto& ctrl : frmDesc.formControl.children) {
        if (ctrl.controlType == FrmControlType::HScrollBar || ctrl.controlType == FrmControlType::VScrollBar) {
            std::string ctrlHwnd = "vb6_hwnd_" + cIdent(ctrl.controlName);
            std::string scrollFn = cProcName(ctrl.controlName + "_Scroll", AccessLevel::Private);
            std::string changeFn = cProcName(ctrl.controlName + "_Change", AccessLevel::Private);
            bool hasScroll = symTab_.lookup(ctrl.controlName + "_Scroll") != nullptr;
            bool hasChange = symTab_.lookup(ctrl.controlName + "_Change") != nullptr;
            c_.emitLine("if (scrollHwnd == (void*)" + ctrlHwnd + ") {");
            c_.indent();
            if (hasScroll) c_.emitLine("if (scrollCode == 5 || scrollCode == 4) { extern void " + scrollFn + "(); " + scrollFn + "(); }");
            if (hasChange) c_.emitLine("else { extern void " + changeFn + "(); " + changeFn + "(); }");
            c_.dedent();
            c_.emitLine("}");
        }
    }
    c_.emitLine("}");
    }
    // P16: WithEvents HScrollBar/VScrollBar事件分发
    for (const auto& we : knownWithEventsCtrlVars_) {
        if (we.second == FrmControlType::HScrollBar || we.second == FrmControlType::VScrollBar) {
            std::string varName = knownWithEventsCtrlOrigNames_.count(we.first) ? knownWithEventsCtrlOrigNames_[we.first] : cIdent(we.first);
            auto* scrollSym = symTab_.lookup(we.first + "_scroll");
            auto* changeSym = symTab_.lookup(we.first + "_change");
            c_.emitLine("if (" + varName + " != NULL && (void*)" + varName + " == scrollHwnd) {");
            c_.indent();
            if (scrollSym) {
                std::string scrollFn = cProcName(scrollSym->name, AccessLevel::Private);
                c_.emitLine("if (scrollCode == 5 || scrollCode == 4) { extern void " + scrollFn + "(); " + scrollFn + "(); }");
            }
            if (changeSym) {
                std::string changeFn = cProcName(changeSym->name, AccessLevel::Private);
                c_.emitLine("else { extern void " + changeFn + "(); " + changeFn + "(); }");
            }
            c_.dedent();
            c_.emitLine("}");
        }
    }    c_.emitLine("break;");
    c_.dedent();
    c_.emitLine("}");

    // P14.4.1d: WM_SIZE -> Form_Resize() (仅当处理器存在时生成)
    {
        std::string resizeFn = cProcName(formName + "_Resize", AccessLevel::Private);
        if (symTab_.lookup(formName + "_Resize") || symTab_.lookup("Form_Resize")) {
            c_.emitLine("case WM_SIZE: {");
            c_.indent();
            c_.emitLine("{ extern void " + resizeFn + "(); " + resizeFn + "(); }");
            c_.emitLine("break;");
            c_.dedent();
            c_.emitLine("}");
        }
    }

    // P14.4.1e: WM_KEYDOWN/WM_CHAR -> Form_KeyDown/KeyPress (仅当处理器存在时)
    {
        std::string keyDownFn = cProcName(formName + "_KeyDown", AccessLevel::Private);
        if (symTab_.lookup(formName + "_KeyDown") || symTab_.lookup("Form_KeyDown")) {
            c_.emitLine("case WM_KEYDOWN: {");
            c_.indent();
            c_.emitLine("{ int16_t vb6_keyCode = (int16_t)wParam; int16_t vb6_shift = 0; if (GetKeyState(VK_SHIFT) & 0x8000) vb6_shift |= 1; if (GetKeyState(VK_CONTROL) & 0x8000) vb6_shift |= 2; if (GetKeyState(VK_MENU) & 0x8000) vb6_shift |= 4;");
            c_.emitLine("extern void " + keyDownFn + "(int16_t*, int16_t*); " + keyDownFn + "(&vb6_keyCode, &vb6_shift); }");
            c_.emitLine("break;");
            c_.dedent();
            c_.emitLine("}");
        }
    }
    {
        std::string keyPressFn = cProcName(formName + "_KeyPress", AccessLevel::Private);
        if (symTab_.lookup(formName + "_KeyPress") || symTab_.lookup("Form_KeyPress")) {
            c_.emitLine("case WM_CHAR: {");
            c_.indent();
            c_.emitLine("{ int16_t vb6_keyAscii = (int16_t)wParam;");
            c_.emitLine("extern void " + keyPressFn + "(int16_t*); " + keyPressFn + "(&vb6_keyAscii); }");
            c_.emitLine("break;");
            c_.dedent();
            c_.emitLine("}");
        }
    }
    // P18-B: Form_KeyUp (WM_KEYUP)
    {
        std::string keyUpFn = cProcName(formName + "_KeyUp", AccessLevel::Private);
        if (symTab_.lookup(formName + "_KeyUp") || symTab_.lookup("Form_KeyUp")) {
            c_.emitLine("case WM_KEYUP: {");
            c_.indent();
            c_.emitLine("{ int16_t vb6_keyCode = (int16_t)wParam; int16_t vb6_shift = 0; if (GetKeyState(VK_SHIFT) & 0x8000) vb6_shift |= 1; if (GetKeyState(VK_CONTROL) & 0x8000) vb6_shift |= 2; if (GetKeyState(VK_MENU) & 0x8000) vb6_shift |= 4;");
            c_.emitLine("extern void " + keyUpFn + "(int16_t*, int16_t*); " + keyUpFn + "(&vb6_keyCode, &vb6_shift); }");
            c_.emitLine("break;");
            c_.dedent();
            c_.emitLine("}");
        }
    }
    // P18-B: Form_MouseDown/MouseUp/MouseMove (WM_LBUTTONDOWN/UP/MOVE)
    {
        std::string mouseDownFn = cProcName(formName + "_MouseDown", AccessLevel::Private);
        std::string mouseUpFn = cProcName(formName + "_MouseUp", AccessLevel::Private);
        std::string mouseMoveFn = cProcName(formName + "_MouseMove", AccessLevel::Private);
        bool hasMouseDown = symTab_.lookup(formName + "_MouseDown") || symTab_.lookup("Form_MouseDown");
        bool hasMouseUp = symTab_.lookup(formName + "_MouseUp") || symTab_.lookup("Form_MouseUp");
        bool hasMouseMove = symTab_.lookup(formName + "_MouseMove") || symTab_.lookup("Form_MouseMove");
        if (hasMouseDown) {
            c_.emitLine("case WM_LBUTTONDOWN: case WM_RBUTTONDOWN: case WM_MBUTTONDOWN: {");
            c_.indent();
            c_.emitLine("{ int16_t vb6_button = 0; int16_t vb6_shift = 0;");
            c_.emitLine("if (msg == WM_LBUTTONDOWN) vb6_button = 1;");
            c_.emitLine("else if (msg == WM_RBUTTONDOWN) vb6_button = 2;");
            c_.emitLine("else if (msg == WM_MBUTTONDOWN) vb6_button = 4;");
            c_.emitLine("if (wParam & MK_SHIFT) vb6_shift |= 1; if (wParam & MK_CONTROL) vb6_shift |= 2; if (GetKeyState(VK_MENU) & 0x8000) vb6_shift |= 4;");
            c_.emitLine("float vb6_x = (float)(int16_t)LOWORD(lParam); float vb6_y = (float)(int16_t)HIWORD(lParam);");
            c_.emitLine("extern void " + mouseDownFn + "(int16_t*, int16_t*, float*, float*); " + mouseDownFn + "(&vb6_button, &vb6_shift, &vb6_x, &vb6_y); }");
            c_.emitLine("break;");
            c_.dedent();
            c_.emitLine("}");
        }
        if (hasMouseUp) {
            c_.emitLine("case WM_LBUTTONUP: case WM_RBUTTONUP: case WM_MBUTTONUP: {");
            c_.indent();
            c_.emitLine("{ int16_t vb6_button = 0; int16_t vb6_shift = 0;");
            c_.emitLine("if (msg == WM_LBUTTONUP) vb6_button = 1;");
            c_.emitLine("else if (msg == WM_RBUTTONUP) vb6_button = 2;");
            c_.emitLine("else if (msg == WM_MBUTTONUP) vb6_button = 4;");
            c_.emitLine("if (wParam & MK_SHIFT) vb6_shift |= 1; if (wParam & MK_CONTROL) vb6_shift |= 2; if (GetKeyState(VK_MENU) & 0x8000) vb6_shift |= 4;");
            c_.emitLine("float vb6_x = (float)(int16_t)LOWORD(lParam); float vb6_y = (float)(int16_t)HIWORD(lParam);");
            c_.emitLine("extern void " + mouseUpFn + "(int16_t*, int16_t*, float*, float*); " + mouseUpFn + "(&vb6_button, &vb6_shift, &vb6_x, &vb6_y); }");
            c_.emitLine("break;");
            c_.dedent();
            c_.emitLine("}");
        }
        if (hasMouseMove) {
            c_.emitLine("case WM_MOUSEMOVE: {");
            c_.indent();
            c_.emitLine("{ int16_t vb6_button = 0; int16_t vb6_shift = 0;");
            c_.emitLine("if (wParam & MK_LBUTTON) vb6_button |= 1; if (wParam & MK_RBUTTON) vb6_button |= 2; if (wParam & MK_MBUTTON) vb6_button |= 4;");
            c_.emitLine("if (wParam & MK_SHIFT) vb6_shift |= 1; if (wParam & MK_CONTROL) vb6_shift |= 2; if (GetKeyState(VK_MENU) & 0x8000) vb6_shift |= 4;");
            c_.emitLine("float vb6_x = (float)(int16_t)LOWORD(lParam); float vb6_y = (float)(int16_t)HIWORD(lParam);");
            c_.emitLine("extern void " + mouseMoveFn + "(int16_t*, int16_t*, float*, float*); " + mouseMoveFn + "(&vb6_button, &vb6_shift, &vb6_x, &vb6_y); }");
            c_.emitLine("break;");
            c_.dedent();
            c_.emitLine("}");
        }
    }
    // P18-B: Form_DblClick (WM_LBUTTONDBLCLK)
    {
        std::string dblClickFn = cProcName(formName + "_DblClick", AccessLevel::Private);
        if (symTab_.lookup(formName + "_DblClick") || symTab_.lookup("Form_DblClick")) {
            c_.emitLine("case WM_LBUTTONDBLCLK: {");
            c_.indent();
            c_.emitLine("extern void " + dblClickFn + "(); " + dblClickFn + "();");
            c_.emitLine("break;");
            c_.dedent();
            c_.emitLine("}");
        }
    }
    // P18-B: WM_ACTIVATE -> Form_Activate/Deactivate
    {
        std::string activateFn = cProcName(formName + "_Activate", AccessLevel::Private);
        std::string deactivateFn = cProcName(formName + "_Deactivate", AccessLevel::Private);
        bool hasActivate = symTab_.lookup(formName + "_Activate") || symTab_.lookup("Form_Activate");
        bool hasDeactivate = symTab_.lookup(formName + "_Deactivate") || symTab_.lookup("Form_Deactivate");
        if (hasActivate || hasDeactivate) {
            c_.emitLine("case WM_ACTIVATE: {");
            c_.indent();
            if (hasActivate) {
                c_.emitLine("if (LOWORD(wParam) != WA_INACTIVE) { extern void " + activateFn + "(); " + activateFn + "(); }");
            }
            if (hasDeactivate) {
                c_.emitLine("if (LOWORD(wParam) == WA_INACTIVE) { extern void " + deactivateFn + "(); " + deactivateFn + "(); }");
            }
            c_.emitLine("break;");
            c_.dedent();
            c_.emitLine("}");
        }
    }
    // default: DefWindowProc (P7.7: MDI窗体使用DefFrameProc/DefMDIChildProc)
    c_.emitLine("default:");
    c_.indent();
    if (isMDIForm) {
        c_.emitLine("return DefFrameProcA(hwnd, (HWND)vb6_GetMDIClient(hwnd), msg, wParam, lParam);");
    } else if (isMDIChild) {
        c_.emitLine("return DefMDIChildProcA(hwnd, msg, wParam, lParam);");
    } else {
        c_.emitLine("return DefWindowProcA(hwnd, msg, wParam, lParam);");
    }
    c_.dedent();

    c_.dedent();
    c_.emitLine("}");  // switch
    c_.emitLine("return 0;");
    c_.dedent();
    c_.emitLine("}");  // WndProc
    c_.emitBlank();

    // --- 控件创建函数 ---
    c_.emitLine("// === " + formName + " CreateControls ===");
    c_.emitLine("void " + createFn + "(void* hwnd, void* hInstance) {");
    c_.indent();
    c_.emitLine("(void)hwnd; (void)hInstance;");
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

        long style = kWsChild | kWsVisible;
        long exStyle = 0;
        std::string createCaption = ctrlCaption;

        switch (ctrl.controlType) {
            case FrmControlType::CommandButton:
                style |= kBsPush;
                break;
            case FrmControlType::TextBox:
                style |= kWsBorder | kEsAutoH;
                createCaption = ctrlText;  // TextBox用Text而非Caption
                break;
            case FrmControlType::Label:
                style |= kSsLeft;
                break;
            case FrmControlType::CheckBox:
                style |= kBsAutoChk;
                break;
            case FrmControlType::OptionButton:
                style |= kBsAutoRad;
                break;
            case FrmControlType::Frame:
                style |= kBsGroupBox;
                break;
            case FrmControlType::ListBox: {
                style |= kLbsNotify | kWsBorder;
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
            case FrmControlType::ComboBox: {
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
                break;
            }
            default:
                break;
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
ctrlId++;
    }


    // P14.4.2: Frame容器 - 创建Frame的子控件(以Frame HWND为父窗口)
    for (const auto& ctrl : frmDesc.formControl.children) {
        if (ctrl.controlType == FrmControlType::Frame && !ctrl.children.empty()) {
            int frameLeft = 0, frameTop = 0;
            auto lIt = ctrl.properties.find("Left");
            if (lIt != ctrl.properties.end()) frameLeft = (int)lIt->second.intValue;
            auto tIt = ctrl.properties.find("Top");
            if (tIt != ctrl.properties.end()) frameTop = (int)tIt->second.intValue;
            std::string frameHwnd = "vb6_hwnd_" + cIdent(ctrl.controlName);
            int childCtrlId = 200;  // Frame子控件ID起始值(避免与顶层冲突)
            for (const auto& child : ctrl.children) {
                const char* childClass = FrmParser::controlTypeToWin32Class(child.controlType);
                if (!childClass) continue;
                int cLeft = 0, cTop = 0, cWidth = 2000, cHeight = 300;
                auto clIt = child.properties.find("Left");
                if (clIt != child.properties.end()) cLeft = (int)clIt->second.intValue;
                auto ctIt = child.properties.find("Top");
                if (ctIt != child.properties.end()) cTop = (int)ctIt->second.intValue;
                auto cwIt = child.properties.find("Width");
                if (cwIt != child.properties.end()) cWidth = (int)cwIt->second.intValue;
                auto chIt = child.properties.find("Height");
                if (chIt != child.properties.end()) cHeight = (int)chIt->second.intValue;
                // 位置相对于Frame(缇→像素, 减去Frame偏移)
                int pxLeft = (cLeft - frameLeft) * 15 / 10;
                int pxTop = (cTop - frameTop) * 15 / 10;
                int pxWidth = cWidth * 15 / 10;
                int pxHeight = cHeight * 15 / 10;
                long childStyle = 0x40000000L | 0x10000000L;  // WS_CHILD | WS_VISIBLE
                std::string childCaption = child.controlName;
                auto ccIt = child.properties.find("Caption");
                if (ccIt != child.properties.end() && ccIt->second.type == FrmValueType::String) {
                    std::string raw = ccIt->second.rawText;
                    if (raw.size() >= 2 && raw.front() == 0x22 && raw.back() == 0x22) childCaption = raw.substr(1, raw.size() - 2);
                }
                auto ctIt2 = child.properties.find("Text");
                if (ctIt2 != child.properties.end() && ctIt2->second.type == FrmValueType::String) {
                    std::string raw = ctIt2->second.rawText;
                    if (raw.size() >= 2 && raw.front() == 0x22 && raw.back() == 0x22) childCaption = raw.substr(1, raw.size() - 2);
                }
                c_.emitLine("vb6_hwnd_" + cIdent(child.controlName) + " = vb6_CreateControl("
                    + "\"" + std::string(childClass) + "\", \"" + childCaption + "\","
                    + std::to_string(childStyle) + "L, 0L,"
                    + std::to_string(pxLeft) + ", " + std::to_string(pxTop) + ", "
                    + std::to_string(pxWidth) + ", " + std::to_string(pxHeight) + ","
                    + std::to_string(childCtrlId) + ", " + frameHwnd + ", hInstance);");
                // 注册控件名到knownFormControls_(供属性访问)
                {
                    std::string childNameLower = child.controlName;
                    std::transform(childNameLower.begin(), childNameLower.end(), childNameLower.begin(), ::tolower);
                    knownFormControls_[childNameLower] = child.controlType;
                    knownFormControlOriginalNames_[childNameLower] = child.controlName;
                }
                childCtrlId++;
            }
        }
    }
    // P14.4.1c: Timer回调注册
    for (const auto& ctrl : frmDesc.formControl.children) {
        if (ctrl.controlType == FrmControlType::Timer) {
            int interval = 1000;
            auto pIt = ctrl.properties.find("Interval");
            if (pIt != ctrl.properties.end()) interval = (int)pIt->second.intValue;
            int enabled = 1;
            pIt = ctrl.properties.find("Enabled");
            if (pIt != ctrl.properties.end()) enabled = (int)pIt->second.intValue;
            std::string timerFn = cProcName(ctrl.controlName + "_Timer", AccessLevel::Private);
            if (enabled) {
                c_.emitLine("vb6_SetTimer(" + std::to_string(interval) + ", (void*)" + timerFn + ");");
            }
        }
    }
    // P18-F: 安装控件子类化 (在所有控件创建之后)
    {
        // 收集需要子类化的控件名(去重)
        auto needsSubclass = [&](const FrmControl& ctrl) -> bool {
            return (ctrl.controlType == FrmControlType::PictureBox ||
                    ctrl.controlType == FrmControlType::Frame ||
                    ctrl.controlType == FrmControlType::Label ||
                    ctrl.controlType == FrmControlType::Image)
                   && (symTab_.lookup(ctrl.controlName + "_GotFocus") ||
                       symTab_.lookup(ctrl.controlName + "_LostFocus"))
                || symTab_.lookup(ctrl.controlName + "_MouseEnter")
                || symTab_.lookup(ctrl.controlName + "_MouseLeave")
                || symTab_.lookup(ctrl.controlName + "_MouseDown")
                || symTab_.lookup(ctrl.controlName + "_MouseUp")
                || symTab_.lookup(ctrl.controlName + "_MouseMove")
                || symTab_.lookup(ctrl.controlName + "_KeyPress")
                || symTab_.lookup(ctrl.controlName + "_KeyDown")
                || symTab_.lookup(ctrl.controlName + "_KeyUp")
              || symTab_.lookup(ctrl.controlName + "_Validate")
                || symTab_.lookup(ctrl.controlName + "_MouseHover");
        };
        std::unordered_set<std::string> subEmitted;
        // 顶层控件
        for (const auto& ctrl : frmDesc.formControl.children) {
            std::string ctrlNameLower = ctrl.controlName;
            std::transform(ctrlNameLower.begin(), ctrlNameLower.end(), ctrlNameLower.begin(), ::tolower);
            if (subEmitted.count(ctrlNameLower)) continue;
            subEmitted.insert(ctrlNameLower);
            if (!needsSubclass(ctrl)) continue;
            std::string subProcName = "vb6_ctrl_subproc_" + cIdent(ctrl.controlName);
            if (knownControlArrays_.count(ctrlNameLower)) {
                // P0-4修复: 控件数组遍历所有元素安装子类化
                c_.emitLine("{ int vb6_si; for (vb6_si = 0; vb6_si < VB6_CTRLARR_MAX; vb6_si++) {");
                c_.emitLine("  void* vb6_sub_hwnd = vb6_CtrlArr_GetAt(&vb6_arr_" + cIdent(ctrl.controlName) + ", vb6_si);");
                c_.emitLine("  if (vb6_sub_hwnd) vb6_InstallControlSubclass(vb6_sub_hwnd, (void*)" + subProcName + "); } }");
            } else {
                c_.emitLine("vb6_InstallControlSubclass((void*)vb6_hwnd_" + cIdent(ctrl.controlName) + ", (void*)" + subProcName + ");");
            }
        }
        // P0-5修复: Frame容器内的子控件
        for (const auto& ctrl : frmDesc.formControl.children) {
            if (ctrl.controlType == FrmControlType::Frame) {
                for (const auto& child : ctrl.children) {
                    std::string childNameLower = child.controlName;
                    std::transform(childNameLower.begin(), childNameLower.end(), childNameLower.begin(), ::tolower);
                    if (subEmitted.count(childNameLower)) continue;
                    subEmitted.insert(childNameLower);
                    if (!needsSubclass(child)) continue;
                    std::string subProcName = "vb6_ctrl_subproc_" + cIdent(child.controlName);
                    c_.emitLine("vb6_InstallControlSubclass((void*)vb6_hwnd_" + cIdent(child.controlName) + ", (void*)" + subProcName + ");");
                }
            }
        }
    }

    // P7.8: 菜单构建 (VB.Menu控件不创建窗口, 用Win32菜单API)
    {
        int menuId = 1000;  // 菜单项ID起始值 (控件ID用100-999)
        bool hasMenu = false;
        for (const auto& ctrl : frmDesc.formControl.children) {
            if (ctrl.controlType == FrmControlType::Menu) {
                hasMenu = true;
                break;
            }
        }
        if (hasMenu) {
            c_.emitBlank();
            c_.emitLine("// P7.8: Menu construction");
            c_.emitLine("HMENU hMenuBar = CreateMenu();");

            for (const auto& ctrl : frmDesc.formControl.children) {
                if (ctrl.controlType != FrmControlType::Menu) continue;

                // Top-level menu item -> popup menu
                std::string caption = ctrl.controlName;
                auto capIt = ctrl.properties.find("Caption");
                if (capIt != ctrl.properties.end() && capIt->second.type == FrmValueType::String) {
                    std::string raw = capIt->second.rawText;
                    if (raw.size() >= 2 && raw.front() == '"' && raw.back() == '"') {
                        caption = raw.substr(1, raw.size() - 2);
                    }
                }

                if (ctrl.children.empty()) {
                    c_.emitLine("AppendMenuW(hMenuBar, MF_STRING, " + std::to_string(menuId) + ", L\"" + escapeWideCString(caption) + "\");");
                    menuId++;
                } else {
                    std::string popupVar = "hPopup_" + cIdent(ctrl.controlName);
                    c_.emitLine("HMENU " + popupVar + " = CreatePopupMenu();");

                    for (const auto& child : ctrl.children) {
                        emitMenuItem(popupVar, child, menuId);
                    }

                    c_.emitLine("AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR)" + popupVar + ", L\"" + escapeWideCString(caption) + "\");");
                }
            }

            c_.emitLine("SetMenu((HWND)hwnd, hMenuBar);");
            c_.emitBlank();
        }
    }

    c_.dedent();
    c_.emitLine("}");  // CreateControls
    c_.emitBlank();

    // --- Show函数 (P7.7: 支持MDIForm/MDIChild) ---
    c_.emitLine("// === " + formName + " Show ===");
    c_.emitLine("void " + showFn + "(void* hMDIClient) {");
    c_.indent();
    c_.emitLine("(void)hMDIClient;");
    c_.emitLine("void* hInst = vb6_GetAppInstance();");
    if (isMDIForm) {
        c_.emitLine("vb6_RegisterMDIFormClass(\"" + clsName + "\", (void*)" + wndProc + ", hInst, 0);");
        c_.emitLine("vb6_hwnd_" + cIdent(formName) + " = vb6_CreateMDIFormWindow(");
        c_.indent();
        c_.emitLine("\"" + clsName + "\", \"" + caption + "\",");
        if (startupPos == 2) {
            // M22-Issue2: StartUpPosition=2 CenterScreen
            c_.emitLine("-1, -1,  /* M22-Issue2: CenterScreen, RTL handles pixel calc */");
        } else if (startupPos == 3) {
            c_.emitLine("CW_USEDEFAULT, CW_USEDEFAULT,");
        } else {
            c_.emitLine("0, 0,");
        }
        c_.emitLine(std::to_string(clientWidth) + ", " + std::to_string(clientHeight) + ",");
        c_.emitLine("hInst);");
        c_.dedent();
    } else if (isMDIChild) {
        c_.emitLine("vb6_RegisterFormClass(\"" + clsName + "\", (void*)" + wndProc + ", hInst, 0);");
        c_.emitLine("vb6_hwnd_" + cIdent(formName) + " = vb6_CreateMDIChildWindow(");
        c_.indent();
        c_.emitLine("\"" + clsName + "\", \"" + caption + "\",");
        if (startupPos == 2) {
            // M22-Issue2: StartUpPosition=2 CenterScreen
            c_.emitLine("-1, -1,  /* M22-Issue2: CenterScreen, RTL handles pixel calc */");
        } else if (startupPos == 3) {
            c_.emitLine("CW_USEDEFAULT, CW_USEDEFAULT,");
        } else {
            c_.emitLine("0, 0,");
        }
        c_.emitLine(std::to_string(clientWidth) + ", " + std::to_string(clientHeight) + ",");
        c_.emitLine("hMDIClient, hInst);");
        c_.dedent();
    } else {
        c_.emitLine("vb6_RegisterFormClass(\"" + clsName + "\", (void*)" + wndProc + ", hInst, 0);");
        c_.emitLine("vb6_hwnd_" + cIdent(formName) + " = vb6_CreateFormWindow(");
        c_.indent();
        c_.emitLine("\"" + clsName + "\", \"" + caption + "\",");
        if (startupPos == 2) {
            // M22-Issue2: StartUpPosition=2 CenterScreen
            c_.emitLine("-1, -1,  /* M22-Issue2: CenterScreen, RTL handles pixel calc */");
        } else if (startupPos == 3) {
            c_.emitLine("CW_USEDEFAULT, CW_USEDEFAULT,");
        } else {
            c_.emitLine("0, 0,");
        }
        c_.emitLine(std::to_string(clientWidth) + ", " + std::to_string(clientHeight) + ",");
        c_.emitLine("hInst, NULL);");
        c_.dedent();
    }
    c_.emitLine("vb6_ShowForm(vb6_hwnd_" + cIdent(formName) + ", 0);");
    c_.dedent();
    c_.emitLine("}");  // Show
    c_.emitBlank();
}

// P7.8: 递归生成菜单项
void CCodeGen::emitMenuItem(const std::string& parentVar, const FrmControl& menuCtrl, int& menuId) {
    std::string caption = menuCtrl.controlName;
    auto capIt = menuCtrl.properties.find("Caption");
    if (capIt != menuCtrl.properties.end() && capIt->second.type == FrmValueType::String) {
        std::string raw = capIt->second.rawText;
        if (raw.size() >= 2 && raw.front() == '"' && raw.back() == '"') {
            caption = raw.substr(1, raw.size() - 2);
        }
    }

    // Check for separator: Caption = "-"
    if (caption == "-") {
        c_.emitLine("AppendMenuW(" + parentVar + ", MF_SEPARATOR, 0, NULL);");
        return;
    }

    // Extract menu properties
    bool checked = false;
    bool enabled = true;
    bool visible = true;

    auto chkIt = menuCtrl.properties.find("Checked");
    if (chkIt != menuCtrl.properties.end() && chkIt->second.type == FrmValueType::Integer) {
        checked = (chkIt->second.intValue != 0);
    }
    auto enIt = menuCtrl.properties.find("Enabled");
    if (enIt != menuCtrl.properties.end() && enIt->second.type == FrmValueType::Identifier) {
        if (enIt->second.rawText == "0" || enIt->second.rawText == "False") enabled = false;
    }
    // Also check integer Enabled = 0
    if (enIt != menuCtrl.properties.end() && enIt->second.type == FrmValueType::Integer) {
        if (enIt->second.intValue == 0) enabled = false;
    }
    auto visIt = menuCtrl.properties.find("Visible");
    if (visIt != menuCtrl.properties.end() && visIt->second.type == FrmValueType::Identifier) {
        if (visIt->second.rawText == "0" || visIt->second.rawText == "False") visible = false;
    }
    if (visIt != menuCtrl.properties.end() && visIt->second.type == FrmValueType::Integer) {
        if (visIt->second.intValue == 0) visible = false;
    }

    if (!visible) {
        // Invisible menu item: skip entirely, but still assign ID
        menuId++;
        return;
    }

    long flags = 0;  // MF_STRING = 0
    if (checked) flags |= 0x0008;  // MF_CHECKED
    if (!enabled) flags |= 0x0002;  // MF_GRAYED

    if (!menuCtrl.children.empty()) {
        // Submenu: create a popup
        std::string popupVar = "hPopup_" + cIdent(menuCtrl.controlName);
        c_.emitLine("HMENU " + popupVar + " = CreatePopupMenu();");

        for (const auto& child : menuCtrl.children) {
            emitMenuItem(popupVar, child, menuId);
        }

        c_.emitLine("AppendMenuW(" + parentVar + ", 0x0010L | " + std::to_string(flags) + ", (UINT_PTR)" + popupVar + ", L\"" + escapeWideCString(caption) + "\");");
        // 0x0010 = MF_POPUP
    } else {
        // Leaf menu item
        c_.emitLine("AppendMenuW(" + parentVar + ", " + std::to_string(flags) + ", " + std::to_string(menuId) + ", L\"" + escapeWideCString(caption) + "\");");
        menuId++;
    }
}

// P7.8: 递归生成菜单点击事件派发 (WM_COMMAND中)
void CCodeGen::emitMenuClickDispatch(const FrmControl& menuCtrl, int& menuId) {
    // M22-Issue3: 如果顶层Menu控件没有children, 它自身就是叶菜单项, 需要生成Click处理
    if (menuCtrl.children.empty()) {
        std::string caption = menuCtrl.controlName;
        auto capIt = menuCtrl.properties.find("Caption");
        if (capIt != menuCtrl.properties.end() && capIt->second.type == FrmValueType::String) {
            std::string raw = capIt->second.rawText;
            if (raw.size() >= 2 && raw.front() == '"' && raw.back() == '"') {
                caption = raw.substr(1, raw.size() - 2);
            }
        }
        if (caption != "-") {
            bool visible = true;
            auto visIt = menuCtrl.properties.find("Visible");
            if (visIt != menuCtrl.properties.end()) {
                if (visIt->second.type == FrmValueType::Identifier &&
                    (visIt->second.rawText == "0" || visIt->second.rawText == "False")) {
                    visible = false;
                }
                if (visIt->second.type == FrmValueType::Integer && visIt->second.intValue == 0) {
                    visible = false;
                }
            }
            if (visible) {
                std::string clickFn = cProcName(menuCtrl.controlName + "_Click", AccessLevel::Private);
                c_.emitLine("if (id == " + std::to_string(menuId) + ") {");
                c_.indent();
                c_.emitLine("{ extern void " + clickFn + "(); " + clickFn + "(); }");
                c_.dedent();
                c_.emitLine("}");
            }
            menuId++;
        }
        return;
    }
    // 顶层Menu控件 (如mnuFile) 只是popup容器, 只对叶子菜单项和子菜单项生成WM_COMMAND派发
    for (const auto& child : menuCtrl.children) {
        std::string caption = child.controlName;
        auto capIt = child.properties.find("Caption");
        if (capIt != child.properties.end() && capIt->second.type == FrmValueType::String) {
            std::string raw = capIt->second.rawText;
            if (raw.size() >= 2 && raw.front() == '"' && raw.back() == '"') {
                caption = raw.substr(1, raw.size() - 2);
            }
        }

        if (caption == "-") {
            // 分隔线没有点击事件，不分配ID
            continue;
        }

        // 检查Visible属性
        bool visible = true;
        auto visIt = child.properties.find("Visible");
        if (visIt != child.properties.end()) {
            if (visIt->second.type == FrmValueType::Identifier &&
                (visIt->second.rawText == "0" || visIt->second.rawText == "False")) {
                visible = false;
            }
            if (visIt->second.type == FrmValueType::Integer && visIt->second.intValue == 0) {
                visible = false;
            }
        }

        if (!child.children.empty()) {
            // 子菜单: 递归处理子项
            emitMenuClickDispatch(child, menuId);
        } else {
            // 叶子菜单项
            if (visible) {
                std::string clickFn = cProcName(child.controlName + "_Click", AccessLevel::Private);
                c_.emitLine("if (id == " + std::to_string(menuId) + ") {");
                c_.indent();
                c_.emitLine("{ extern void " + clickFn + "(); " + clickFn + "(); }");
                c_.dedent();
                c_.emitLine("}");
            }
            menuId++;
        }
    }
}
// M22: 转义宽C字符串（UTF-8多字节→Unicode \xNNNN转义）
std::string CCodeGen::escapeWideCString(const std::string& s) {
    std::string result;
    result.reserve(s.size() + 16);
    for (size_t j = 0; j < s.size(); ) {
        unsigned char ch = (unsigned char)s[j];
        if (ch == '"') {
            result += "\\\""; j++;
        } else if (ch == '\\') {
            result += "\\\\"; j++;
        } else if (ch == '\n') {
            result += "\\n"; j++;
        } else if (ch == '\r') {
            result += "\\r"; j++;
        } else if (ch == '\t') {
            result += "\\t"; j++;
        } else if (ch < 0x80) {
            result += (char)ch; j++;
        } else {
            uint32_t cp = 0;
            int bytes = 0;
            if ((ch & 0xE0) == 0xC0) { cp = ch & 0x1F; bytes = 2; }
            else if ((ch & 0xF0) == 0xE0) { cp = ch & 0x0F; bytes = 3; }
            else if ((ch & 0xF8) == 0xF8) { cp = ch & 0x07; bytes = 4; }
            else { cp = ch; bytes = 1; }
            for (int b = 1; b < bytes && j + b < s.size(); b++) {
                cp = (cp << 6) | ((unsigned char)s[j + b] & 0x3F);
            }
            j += bytes;
            char hex[8];
            snprintf(hex, sizeof(hex), "\\x%04X", cp);
            result += hex;
        }
    }
    return result;
}

// P7.8: 转义C字符串
std::string CCodeGen::escapeCString(const std::string& s) {
    std::string result;
    for (char c : s) {
        switch (c) {
            case '"':  result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default:   result += c; break;
        }
    }
    return result;
}

// ============================================================
// P6.6: 单独生成 DLL 入口文件 (dll_entry.c)
// 当DLL工程只有类模块(无标准模块)时使用
// ============================================================


} // namespace vb6c3
