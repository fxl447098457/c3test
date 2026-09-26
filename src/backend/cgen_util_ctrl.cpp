#include "backend/cgen.hpp"
#include <algorithm>
#include <cctype>
#include <iostream>
#include <functional>

namespace vb6c3 {

// --- cgen_util_ctrl.cpp: 控件属性映射 (读/写函数名 + hWnd 参数 + 默认属性) + Variant 包装 ---


// ============================================================
// P7.5: 控件属性 → RTL读取函数名映射
// ============================================================

// ============================================================
// P20-42: 控件属性 → 返回类型
//
// 为什么需要这张表: inferExprType 对 `对象.成员` 的兜底是 lookupModule(memberName),
// 只按成员**裸名**查模块符号, 于是和模块级/内置符号同名的属性会被顶掉。
// 实测 `SSTab1.Tab` 撞上内置函数 `Tab` (vb6_Tab 返回 BSTR) → 判成 Vb6Type::String
// → 字符串拼接不套 vb6_CStr(vb6_VariantFromValue(...)) → int32_t 当 BSTR 解引用 → AV。
//
// 只对"会撞名"的属性登记即可: 其余属性继续走原有兜底 (lookupModule 找不到就返回
// Variant, 由 _Generic 安全包装)。这里登记的都按 Long —— RTL 侧这些 getter 就是
// int32_t, C 层口径一致。
// ============================================================
Vb6Type CCodeGen::controlPropType(FrmControlType ctrlType, const std::string& propName) const {
    std::string p = propName;
    std::transform(p.begin(), p.end(), p.begin(), ::tolower);

    // ---- 通用属性 (所有控件都适用) ----
    //
    // **必须先于** SSTab 那段, 而且必须存在 —— 这不是 SSTab 一个控件的问题:
    // `Left` / `Right` / `Mid` 全是 VB6 内置函数 (cgen_expr_ident_builtin.inc),
    // 其中 Left/Right/Mid 返回 **String**。于是 `lblPage0.Left` 被判成 String →
    // 生成码不套数值转换 → int 当 BSTR 解引用 → 0xC0000005。
    // 实测 SSTab 夹具: TS1 的 SSTab1.Tabs 正常, TS22 的 lblPage0.Left 崩。
    // 这几个 RTL getter 的 C 返回类型都是 int, 故一律 Long。
    if (p == "left" || p == "top" || p == "width" || p == "height"
        || p == "visible" || p == "enabled") {
        return Vb6Type::Long;
    }

    if (ctrlType == FrmControlType::SSTab) {
        // VB6 里 WordWrap 是 Boolean, 但 RTL 的 getter 给的是 VB6 的 True/-1,
        // 与 Tabs/Tab 同为 int32_t —— 一律按 Long, C 层才对得上。
        if (p == "tabs" || p == "tab" || p == "taborientation" || p == "tabstyle"
            || p == "tabsperrow" || p == "wordwrap") {
            return Vb6Type::Long;
        }
    }
    // C29-OLE: OLEType/OLETypeAllowed/SizeMode/AutoActivate 是 Long 语义 (枚举/布尔);
    // Class/SourceDoc/SourceItem 是 String (RTL getter 返回 wchar_t*, 包装层会转 BSTR)。
    if (ctrlType == FrmControlType::OLE) {
        if (p == "oletype" || p == "oletypeallowed" || p == "sizemode"
            || p == "autoactivate" || p == "displayasicon" || p == "autoverbmenu"
            || p == "borderstyle") {
            return Vb6Type::Long;
        }
        if (p == "class" || p == "sourcedoc" || p == "sourceitem") {
            return Vb6Type::String;
        }
    }
    if (ctrlType == FrmControlType::TreeView) {
        // C29-8a: 同理 —— vb6_TreeView_Get* 全是 int32_t (布尔按 VB6 的 -1/0 给)。
        // 不登记就走 inferExprType 的兜底, 那条按成员裸名查符号, 判成 Variant/String
        // 都不匹配 C 侧的 int32_t (SSTab1.Tab 撞内置 Tab 那次 AV 的同族)。
        if (p == "linestyle" || p == "indentation" || p == "checkboxes"
            || p == "hottracking" || p == "hideselection") {
            return Vb6Type::Long;
        }
    }
    if (ctrlType == FrmControlType::Toolbar) {
        // C29-5a: 同一口径 —— 这四条的 RTL getter 都是 int32_t, 判成 Variant/String
        // 就跟 C 层不匹配 (SSTab1.Tab 那次 AV 的同族)。
        if (p == "showtips" || p == "textstyle" || p == "allowcustomize" || p == "align") {
            return Vb6Type::Long;
        }
        // `tb1.Buttons.Count` 的读函数在 cgen_util_com.cpp 里改道, 那条路不查控件属性表,
        // 于是成员名 "count" 单独进来 —— 不登记成 Long 就被装箱, `Count = 3` 恒假
        // (实测: 值是对的 3, 打出来却是空串, 比较也全 N)。
        if (ctrlType == FrmControlType::Toolbar && p == "count") return Vb6Type::Long;
    }
    return Vb6Type::Unknown;
}

std::string CCodeGen::getControlPropReadFn(FrmControlType ctrlType, const std::string& propName) const {
    std::string propLower = propName;
    std::transform(propLower.begin(), propLower.end(), propLower.begin(), ::tolower);

        // P11.8: Common properties for all visible controls (checked before switch)
    if (propLower == "left") return "vb6_GetControlLeft";
    if (propLower == "top") return "vb6_GetControlTop";
    if (propLower == "width") return "vb6_GetControlWidth";
    if (propLower == "height") return "vb6_GetControlHeight";
    if (propLower == "hwnd") return "vb6_GetControlHwnd";
    // P13.1: Font properties (all visible controls with text)
    // D6 / C29-9: CommonDialog 的 FontName / FontSize 是**对话框字段**（ChooseFont 的 LOGFONT），
    // 不是控件字体 —— 这枚控件没有外观。通用那一组查在类型 switch **之前**，不挡就把
    // `CD1.FontName = "Consolas"` 静默落到字体属性上（C29-1a 那条 borderstyle 被抢走同一类碰撞）。
    if (propLower == "fontname" && ctrlType != FrmControlType::CommonDialog) return "vb6_GetControlFontName";
    if (propLower == "fontsize" && ctrlType != FrmControlType::CommonDialog) return "vb6_GetControlFontSize";
    if (propLower == "fontbold") return "vb6_GetControlFontBold";
    if (propLower == "fontitalic") return "vb6_GetControlFontItalic";
    if (propLower == "fontunderline") return "vb6_GetControlFontUnderline";
    if (propLower == "fontstrikethrough") return "vb6_GetControlFontStrikethrough";
    // P13.2: Color properties (all visible controls)
    if (propLower == "forecolor") return "vb6_GetControlForeColor";
    if (propLower == "backcolor") return "vb6_GetControlBackColor";
    // P13.5: Alignment (all text controls)
    if (propLower == "alignment") return "vb6_GetAlignment";
    // P13.6: TabIndex/TabStop (all visible controls)
    if (propLower == "tabindex") return "vb6_GetTabIndex";
    if (propLower == "tabstop") return "vb6_GetTabStop";
    if (propLower == "causesvalidation") return "vb6_GetCausesValidation";
    // P13.8: ToolTipText (all visible controls)
    if (propLower == "tooltiptext") return "vb6_GetToolTipText";
    // P13.9: Tag (all controls)
    if (propLower == "tag") return "vb6_GetControlTag";
    // P13.7: MousePointer/MouseIcon (all visible controls)
    if (propLower == "mousepointer") return "vb6_GetMousePointer";
    if (propLower == "mouseicon") return "vb6_GetMouseIcon";
    // P13.10: BorderStyle (all visible controls)
    // C29-1a: Shape/Line 的 BorderStyle 是"画笔线型"(0..6), 与窗口边框样式(0/1)同名
    // 不同物 —— 让这两个类型走下面各自的 case, 否则读回来的永远是窗口那套。
    if (propLower == "borderstyle" && ctrlType != FrmControlType::Shape
        && ctrlType != FrmControlType::Line) return "vb6_GetBorderStyle";

    switch (ctrlType) {
    case FrmControlType::TextBox:
        if (propLower == "text") return "vb6_GetControlText";
        if (propLower == "multiline") return "vb6_GetMultiLine";
        if (propLower == "scrollbars") return "vb6_GetScrollBars";
        if (propLower == "maxlength") return "vb6_GetMaxLength";
        if (propLower == "passwordchar") return "vb6_GetPasswordChar";
        if (propLower == "locked") return "vb6_GetLocked";
        if (propLower == "selstart") return "vb6_GetSelStart";
        if (propLower == "sellength") return "vb6_GetSelLength";
        if (propLower == "seltext") return "vb6_GetSelText";
        if (propLower == "visible") return "vb6_GetControlVisible";
        if (propLower == "enabled") return "vb6_GetControlEnabled";
        break;
    case FrmControlType::ListBox:
    case FrmControlType::ComboBox:
        if (propLower == "text") return "vb6_GetControlText";
        if (propLower == "listcount") return "vb6_GetListCount";
        if (propLower == "listindex") return "vb6_GetListIndex";
        if (propLower == "list") return "vb6_GetListItem";
        if (propLower == "selected") return "vb6_GetSelected";
        if (propLower == "itemdata") return "vb6_GetItemData";
        if (propLower == "newindex") return "vb6_GetNewIndex";
        if (propLower == "visible") return "vb6_GetControlVisible";
        if (propLower == "enabled") return "vb6_GetControlEnabled";
        break;
    case FrmControlType::Frame:
        if (propLower == "caption") return "vb6_GetControlText";
        if (propLower == "visible") return "vb6_GetControlVisible";
        if (propLower == "enabled") return "vb6_GetControlEnabled";
        if (propLower == "align") return "vb6_GetControlAlign";  // P13.5b: 停靠
        break;
    case FrmControlType::Label:
        if (propLower == "caption") return "vb6_GetControlText";
        if (propLower == "visible") return "vb6_GetControlVisible";
        if (propLower == "enabled") return "vb6_GetControlEnabled";
        break;
    case FrmControlType::CommandButton:
        if (propLower == "caption") return "vb6_GetControlText";
        if (propLower == "default") return "vb6_GetDefaultButton";
        if (propLower == "cancel") return "vb6_GetCancelButton";
        if (propLower == "visible") return "vb6_GetControlVisible";
        if (propLower == "enabled") return "vb6_GetControlEnabled";
        break;
    case FrmControlType::CheckBox:
    case FrmControlType::OptionButton:
        if (propLower == "value") return "vb6_GetCheckValue";
        if (propLower == "caption") return "vb6_GetControlText";
        if (propLower == "visible") return "vb6_GetControlVisible";
        if (propLower == "enabled") return "vb6_GetControlEnabled";
        break;
    case FrmControlType::Form:
        if (propLower == "caption") return "vb6_GetControlText";
        if (propLower == "visible") return "vb6_GetControlVisible";
        if (propLower == "enabled") return "vb6_GetControlEnabled";
        // Fix 056: Form-specific properties
        if (propLower == "windowstate") return "vb6_GetWindowState";
        if (propLower == "scalewidth") return "vb6_GetScaleWidth";
        if (propLower == "scaleheight") return "vb6_GetScaleHeight";
        break;
    case FrmControlType::WebBrowser:
        if (propLower == "url" || propLower == "locationurl") return "vb6_WebViewGetUrl";
        if (propLower == "visible") return "vb6_GetControlVisible";
        if (propLower == "enabled") return "vb6_GetControlEnabled";
        break;
    case FrmControlType::HScrollBar:
    case FrmControlType::VScrollBar:
        if (propLower == "value") return "vb6_GetScrollValue";
        if (propLower == "min") return "vb6_GetScrollMin";
        if (propLower == "max") return "vb6_GetScrollMax";
        if (propLower == "largechange") return "vb6_GetLargeChange";
        if (propLower == "smallchange") return "vb6_GetSmallChange";
        if (propLower == "visible") return "vb6_GetControlVisible";
        if (propLower == "enabled") return "vb6_GetControlEnabled";
        break;
    case FrmControlType::Timer:
        if (propLower == "interval") return "vb6_GetTimerInterval";
        if (propLower == "enabled") return "vb6_GetTimerEnabled";
        break;
    case FrmControlType::PictureBox:
        if (propLower == "caption") return "vb6_GetControlText";
        if (propLower == "picture") return "vb6_GetControlPicture";
        if (propLower == "autosize") return "vb6_GetPictureAutoSize";
        if (propLower == "align") return "vb6_GetControlAlign";  // P13.5b: 停靠
        if (propLower == "visible") return "vb6_GetControlVisible";
        if (propLower == "enabled") return "vb6_GetControlEnabled";
        break;
    case FrmControlType::Image:
        if (propLower == "picture") return "vb6_GetControlPicture";
        if (propLower == "stretch") return "vb6_GetImageStretch";  // P17.2
        if (propLower == "visible") return "vb6_GetControlVisible";
        if (propLower == "enabled") return "vb6_GetControlEnabled";
        break;
    case FrmControlType::ProgressBar:  // P20-38: 只读回来必须走 RTL,
        // 否则会落到 vb6_ComGetStringProp 这条 COM 占位路径上, 静默返回空。
        if (propLower == "min") return "vb6_GetProgressBarMin";
        if (propLower == "max") return "vb6_GetProgressBarMax";
        if (propLower == "value") return "vb6_GetProgressBarValue";
        if (propLower == "orientation") return "vb6_GetProgressBarOrientation";
        if (propLower == "scrolling") return "vb6_GetProgressBarScrolling";
        if (propLower == "visible") return "vb6_GetControlVisible";
        if (propLower == "enabled") return "vb6_GetControlEnabled";
        break;
    case FrmControlType::ImageList:  // P20-39: 读写都走原生 RTL, 不落 COM 占位路径
        // ListImages 集合本身 (Count/Add/Remove/Item) 由 cgen_util_com.cpp 的
        // resolveComValue 拦截改道, 这里只管 ImageList 自身的标量属性。
        if (propLower == "imagewidth") return "vb6_GetImageListImageWidth";
        if (propLower == "imageheight") return "vb6_GetImageListImageHeight";
        break;
    case FrmControlType::StatusBar:  // P20-40: 只读回来必须走 RTL, 否则落 COM 占位路径静默答错
        if (propLower == "align") return "vb6_StatusBar_GetAlign";
        if (propLower == "style") return "vb6_StatusBar_GetStyle";
        if (propLower == "simpletext") return "vb6_StatusBar_GetSimpleText";
        if (propLower == "visible") return "vb6_GetControlVisible";
        if (propLower == "enabled") return "vb6_GetControlEnabled";
        break;
    case FrmControlType::SSTab:  // P20-42
        // TabCaption(i)/TabVisible(i) 是**带下标的索引属性**, 不是标量;
        // 走不了这张表 (表的入参只有 propLower), 见 cgen_expr_member_form_builtin.inc 的 TODO。
        if (propLower == "tabs")           return "vb6_SSTab_GetTabs";
        if (propLower == "tab")            return "vb6_SSTab_GetTab";
        if (propLower == "taborientation") return "vb6_SSTab_GetTabOrientation";
        if (propLower == "tabstyle")       return "vb6_SSTab_GetTabStyle";
        if (propLower == "tabsperrow")     return "vb6_SSTab_GetTabsPerRow";
        if (propLower == "wordwrap")       return "vb6_SSTab_GetWordWrap";
        if (propLower == "visible")        return "vb6_GetControlVisible";
        if (propLower == "enabled")        return "vb6_GetControlEnabled";
        break;
    case FrmControlType::ListView:  // C29-7
        // ListItems / ColumnHeaders 是**集合**, 走成员对象那条路
        // (cgen_util_com.cpp 的 resolveComValue / resolveComMarkerForPack 拦截改道);
        // 这张表只管 ListView 自身的标量属性。
        // 不登记就会落到 vb6_ComGetStringProp 这个 COM 占位路径上静默答空 ——
        // 与 ProgressBar / StatusBar 那两批同一条纪律。
        if (propLower == "view")               return "vb6_ListView_GetView";
        if (propLower == "gridlines")          return "vb6_ListView_GetGridLines";
        if (propLower == "fullrowselect")      return "vb6_ListView_GetFullRowSelect";
        if (propLower == "multiselect")        return "vb6_ListView_GetMultiSelect";
        if (propLower == "checkboxes")         return "vb6_ListView_GetCheckBoxes";
        if (propLower == "hidecolumnheaders")  return "vb6_ListView_GetHideColumnHeaders";
        if (propLower == "allowcolumnreorder") return "vb6_ListView_GetAllowColumnReorder";
        if (propLower == "labeledit")          return "vb6_ListView_GetLabelEdit";
        if (propLower == "sorted")             return "vb6_ListView_GetSorted";
        if (propLower == "sortkey")            return "vb6_ListView_GetSortKey";
        if (propLower == "sortorder")          return "vb6_ListView_GetSortOrder";
        if (propLower == "visible")            return "vb6_GetControlVisible";
        if (propLower == "enabled")            return "vb6_GetControlEnabled";
        break;
    case FrmControlType::OLE:  // C29-OLE: OLEType/Class/SizeMode 等走 RTL (真 OLE 容器)
        // Object 属性走晚绑定 (cgen_util_com.cpp 那条), 不在这张表。
        if (propLower == "class")           return "vb6_OleCon_GetClass";
        if (propLower == "oletype")         return "vb6_OleCon_GetOleType";
        if (propLower == "oletypeallowed")  return "vb6_OleCon_GetOLETypeAllowed";
        if (propLower == "sizemode")        return "vb6_OleCon_GetSizeMode";
        if (propLower == "displayasicon")   return "vb6_OleCon_GetDisplayAsIcon";
        if (propLower == "autoactivate")    return "vb6_OleCon_GetAutoActivate";
        if (propLower == "autoverbmenu")    return "vb6_OleCon_GetAutoVerbMenu";
        if (propLower == "borderstyle")     return "vb6_OleCon_GetBorderStyle";
        if (propLower == "sourcedoc")       return "vb6_OleCon_GetSourceDoc";
        if (propLower == "sourceitem")      return "vb6_OleCon_GetSourceItem";
        if (propLower == "visible")         return "vb6_GetControlVisible";
        if (propLower == "enabled")         return "vb6_GetControlEnabled";
        break;
    case FrmControlType::Menu:  // P20-36
        if (propLower == "caption") return "vb6_GetMenuCaption";
        if (propLower == "checked") return "vb6_GetMenuChecked";
        if (propLower == "enabled") return "vb6_GetMenuEnabled";
        if (propLower == "visible") return "vb6_GetMenuVisible";
        break;
    // C29-1b: 文件系统三控件的专有成员。列表成员走 ListBox/ComboBox 那一族 helper
    // (它们内部按窗口类分流 LB_* / CB_*), 所以 Drive 的组合框与 Dir/File 的列表框
    // 共用同一批读函数, 不需要为这三类另写一套。
    case FrmControlType::DriveListBox:
        if (propLower == "drive") return "vb6_DriveListBoxDrive";
        if (propLower == "text") return "vb6_GetControlText";
        if (propLower == "listcount") return "vb6_GetListCount";
        if (propLower == "listindex") return "vb6_GetListIndex";
        if (propLower == "list") return "vb6_GetListItem";
        if (propLower == "visible") return "vb6_GetControlVisible";
        if (propLower == "enabled") return "vb6_GetControlEnabled";
        break;
    case FrmControlType::DirListBox:
        if (propLower == "path") return "vb6_DirListBoxPath";
        if (propLower == "listcount") return "vb6_GetListCount";
        if (propLower == "listindex") return "vb6_GetListIndex";
        if (propLower == "list") return "vb6_GetListItem";
        if (propLower == "visible") return "vb6_GetControlVisible";
        if (propLower == "enabled") return "vb6_GetControlEnabled";
        break;
    case FrmControlType::FileListBox:
        if (propLower == "path") return "vb6_FileListBoxPath";
        if (propLower == "pattern") return "vb6_FileListBoxPattern";
        if (propLower == "filename") return "vb6_FileListBoxFileName";
        if (propLower == "listcount") return "vb6_GetListCount";
        if (propLower == "listindex") return "vb6_GetListIndex";
        if (propLower == "list") return "vb6_GetListItem";
        if (propLower == "visible") return "vb6_GetControlVisible";
        if (propLower == "enabled") return "vb6_GetControlEnabled";
        break;
    // C29-8a: TreeView 的标量属性面。之前这类零格 ⇒ 全部落到"未知属性"的通用兜底
    // vb6_ComGetStringProp(裸 HWND, L"CheckBoxes") —— 读回空串、写进去静默丢 (029 §九)。
    // 这四条的真值在窗口样式位上 (RTL 读 GWL_STYLE)，Indentation 的缇值存窗口属性。
    // Style / LabelEdit / Sorted / PathSeparator / Nodes 一族留 C29-8b (等成员对象机制)。
    case FrmControlType::TreeView:
        if (propLower == "linestyle") return "vb6_TreeView_GetLineStyle";
        if (propLower == "indentation") return "vb6_TreeView_GetIndentation";
        if (propLower == "checkboxes") return "vb6_TreeView_GetCheckBoxes";
        if (propLower == "hottracking") return "vb6_TreeView_GetHotTracking";
        if (propLower == "hideselection") return "vb6_TreeView_GetHideSelection";
        if (propLower == "visible") return "vb6_GetControlVisible";
        if (propLower == "enabled") return "vb6_GetControlEnabled";
        break;
    // C29-5a: Toolbar 的标量属性面。改之前这枚控件连窗口都没有 (被"ImageList || Toolbar
    // 走 CoCreateInstance"那一组扣住)，读一个 tb1.Visible 就是 C2065: vb6_hwnd_tb1 未声明。
    // ShowTips / TextStyle / AllowCustomize 的真值在 GWL_STYLE 上，Align 存窗口属性。
    case FrmControlType::Toolbar:
        if (propLower == "showtips") return "vb6_Toolbar_GetShowTips";
        if (propLower == "textstyle") return "vb6_Toolbar_GetTextStyle";
        if (propLower == "allowcustomize") return "vb6_Toolbar_GetAllowCustomize";
        if (propLower == "align") return "vb6_Toolbar_GetAlign";
        if (propLower == "visible") return "vb6_GetControlVisible";
        if (propLower == "enabled") return "vb6_GetControlEnabled";
        break;
    // D6 / C29-9: CommonDialog —— 属性袋挂在那枚自注册的不可见窗口上，
    // 读写口全部走 vb6_Cd* （原生 comdlg32，不再经 MSComDlg.OCX）。
    case FrmControlType::CommonDialog:
        if (propLower == "filter") return "vb6_CdGetFilter";
        if (propLower == "filename") return "vb6_CdGetFileName";
        if (propLower == "filetitle") return "vb6_CdGetFileTitle";
        if (propLower == "dialogtitle") return "vb6_CdGetDialogTitle";
        if (propLower == "initdir") return "vb6_CdGetInitDir";
        if (propLower == "defaultext") return "vb6_CdGetDefaultExt";
        if (propLower == "fontname") return "vb6_CdGetFontName";
        if (propLower == "flags") return "vb6_CdGetFlags";
        if (propLower == "cancelerror") return "vb6_CdGetCancelError";
        if (propLower == "color") return "vb6_CdGetColor";
        if (propLower == "min") return "vb6_CdGetMin";
        if (propLower == "max") return "vb6_CdGetMax";
        if (propLower == "copies") return "vb6_CdGetCopies";
        if (propLower == "fontsize") return "vb6_CdGetFontSize";
        break;
    case FrmControlType::Shape:  // P20-35
        if (propLower == "shape") return "vb6_GetShapeType";        if (propLower == "borderwidth") return "vb6_GetShapeBorderWidth";
        if (propLower == "borderstyle") return "vb6_GetShapeBorderStyle";
        if (propLower == "fillstyle") return "vb6_GetShapeFillStyle";
        if (propLower == "bordercolor") return "vb6_GetShapeBorderColor";
        if (propLower == "fillcolor") return "vb6_GetShapeFillColor";
        if (propLower == "visible") return "vb6_GetControlVisible";
        break;
    case FrmControlType::Line:  // P20-35
        if (propLower == "x1") return "vb6_GetLineX1";
        if (propLower == "y1") return "vb6_GetLineY1";
        if (propLower == "x2") return "vb6_GetLineX2";
        if (propLower == "y2") return "vb6_GetLineY2";
        if (propLower == "borderwidth") return "vb6_GetLineBorderWidth";
        if (propLower == "borderstyle") return "vb6_GetLineBorderStyle";
        if (propLower == "bordercolor") return "vb6_GetLineColor";
        if (propLower == "visible") return "vb6_GetControlVisible";
        break;
    default:
        // 所有可见控件通用属性
        if (propLower == "visible") return "vb6_GetControlVisible";
        if (propLower == "enabled") return "vb6_GetControlEnabled";
        break;
    }
    return "";  // 未知属性
}


std::string CCodeGen::getControlPropWriteFn(FrmControlType ctrlType, const std::string& propName) const {
    std::string propLower = propName;
    std::transform(propLower.begin(), propLower.end(), propLower.begin(), ::tolower);

        // P11.8: Common properties for all visible controls (checked before switch)
    if (propLower == "left") return "vb6_SetControlLeft";
    if (propLower == "top") return "vb6_SetControlTop";
    if (propLower == "width") return "vb6_SetControlWidth";
    if (propLower == "height") return "vb6_SetControlHeight";
    // P13.1: Font properties (all visible controls with text)
    // D6 / C29-9: 同上 —— CommonDialog 的 Font* 走它自己的属性袋（写侧）。
    if (propLower == "fontname" && ctrlType != FrmControlType::CommonDialog) return "vb6_SetControlFontName";
    if (propLower == "fontsize" && ctrlType != FrmControlType::CommonDialog) return "vb6_SetControlFontSize";
    if (propLower == "fontbold") return "vb6_SetControlFontBold";
    if (propLower == "fontitalic") return "vb6_SetControlFontItalic";
    if (propLower == "fontunderline") return "vb6_SetControlFontUnderline";
    if (propLower == "fontstrikethrough") return "vb6_SetControlFontStrikethrough";
    // P13.2: Color properties (all visible controls)
    if (propLower == "forecolor") return "vb6_SetControlForeColor";
    if (propLower == "backcolor") return "vb6_SetControlBackColor";
    // P13.5: Alignment (all text controls)
    if (propLower == "alignment") return "vb6_SetAlignment";
    // P13.6: TabIndex/TabStop (all visible controls)
    if (propLower == "tabindex") return "vb6_SetTabIndex";
    if (propLower == "tabstop") return "vb6_SetTabStop";
    if (propLower == "causesvalidation") return "vb6_SetCausesValidation";
    // P13.8: ToolTipText (all visible controls)
    if (propLower == "tooltiptext") return "vb6_SetToolTipText";
    // P13.9: Tag (all controls)
    if (propLower == "tag") return "vb6_SetControlTag";
    // P13.7: MousePointer/MouseIcon (all visible controls)
    if (propLower == "mousepointer") return "vb6_SetMousePointer";
    if (propLower == "mouseicon") return "vb6_SetMouseIcon";
    // P13.10: BorderStyle (all visible controls)
    // C29-1a: Shape/Line 的 BorderStyle 是"画笔线型"(0..6), 与窗口边框样式(0/1)同名
    // 不同物 —— 让这两个类型走下面各自的 case, 否则读回来的永远是窗口那套。
    if (propLower == "borderstyle" && ctrlType != FrmControlType::Shape
        && ctrlType != FrmControlType::Line) return "vb6_SetBorderStyle";

    switch (ctrlType) {
    case FrmControlType::TextBox:
        if (propLower == "text") return "vb6_SetControlText";
        if (propLower == "multiline") return "vb6_SetMultiLine";
        if (propLower == "scrollbars") return "vb6_SetScrollBars";
        if (propLower == "maxlength") return "vb6_SetMaxLength";
        if (propLower == "passwordchar") return "vb6_SetPasswordChar";
        if (propLower == "locked") return "vb6_SetLocked";
        if (propLower == "selstart") return "vb6_SetSelStart";
        if (propLower == "sellength") return "vb6_SetSelLength";
        if (propLower == "seltext") return "vb6_SetSelText";
        if (propLower == "visible") return "vb6_SetControlVisible";
        if (propLower == "enabled") return "vb6_SetControlEnabled";
        break;
    case FrmControlType::ListBox:
    case FrmControlType::ComboBox:
        if (propLower == "text") return "vb6_SetControlText";
        if (propLower == "listindex") return "vb6_SetListIndex";
        if (propLower == "list") return "vb6_SetListItem";
        if (propLower == "selected") return "vb6_SetSelected";
        if (propLower == "itemdata") return "vb6_SetItemData";
        if (propLower == "visible") return "vb6_SetControlVisible";
        if (propLower == "enabled") return "vb6_SetControlEnabled";
        break;
    case FrmControlType::Frame:
        if (propLower == "caption") return "vb6_SetControlText";
        if (propLower == "visible") return "vb6_SetControlVisible";
        if (propLower == "enabled") return "vb6_SetControlEnabled";
        if (propLower == "align") return "vb6_SetControlAlign";  // P13.5b: 停靠
        break;
    case FrmControlType::Label:
        if (propLower == "caption") return "vb6_SetControlText";
        if (propLower == "visible") return "vb6_SetControlVisible";
        if (propLower == "enabled") return "vb6_SetControlEnabled";
        break;
    case FrmControlType::CommandButton:
        if (propLower == "caption") return "vb6_SetControlText";
        if (propLower == "default") return "vb6_SetDefaultButton";
        if (propLower == "cancel") return "vb6_SetCancelButton";
        if (propLower == "visible") return "vb6_SetControlVisible";
        if (propLower == "enabled") return "vb6_SetControlEnabled";
        break;
    case FrmControlType::CheckBox:
    case FrmControlType::OptionButton:
        if (propLower == "value") return "vb6_SetCheckValue";
        if (propLower == "caption") return "vb6_SetControlText";
        if (propLower == "visible") return "vb6_SetControlVisible";
        if (propLower == "enabled") return "vb6_SetControlEnabled";
        break;
    case FrmControlType::Form:
        if (propLower == "caption") return "vb6_SetControlText";
        if (propLower == "visible") return "vb6_SetControlVisible";
        if (propLower == "enabled") return "vb6_SetControlEnabled";
        break;
    case FrmControlType::WebBrowser:
        if (propLower == "visible") return "vb6_SetControlVisible";
        if (propLower == "enabled") return "vb6_SetControlEnabled";
        break;
    case FrmControlType::HScrollBar:
    case FrmControlType::VScrollBar:
        if (propLower == "value") return "vb6_SetScrollValue";
        if (propLower == "min") return "vb6_SetScrollMin";
        if (propLower == "max") return "vb6_SetScrollMax";
        if (propLower == "largechange") return "vb6_SetLargeChange";
        if (propLower == "smallchange") return "vb6_SetSmallChange";
        if (propLower == "visible") return "vb6_SetControlVisible";
        if (propLower == "enabled") return "vb6_SetControlEnabled";
        break;
    case FrmControlType::ProgressBar:
        if (propLower == "min") return "vb6_SetProgressBarMin";
        if (propLower == "max") return "vb6_SetProgressBarMax";
        if (propLower == "value") return "vb6_SetProgressBarValue";
        if (propLower == "orientation") return "vb6_SetProgressBarOrientation";
        if (propLower == "scrolling") return "vb6_SetProgressBarScrolling";
        if (propLower == "visible") return "vb6_SetControlVisible";
        if (propLower == "enabled") return "vb6_SetControlEnabled";
        break;
    case FrmControlType::ImageList:  // P20-39
        if (propLower == "imagewidth") return "vb6_SetImageListImageWidth";
        if (propLower == "imageheight") return "vb6_SetImageListImageHeight";
        break;
    case FrmControlType::StatusBar:  // P20-40
        if (propLower == "align") return "vb6_StatusBar_SetAlign";
        if (propLower == "style") return "vb6_StatusBar_SetStyle";
        if (propLower == "simpletext") return "vb6_StatusBar_SetSimpleText";
        if (propLower == "visible") return "vb6_SetControlVisible";
        if (propLower == "enabled") return "vb6_SetControlEnabled";
        break;
    case FrmControlType::SSTab:  // P20-42
        if (propLower == "tabs")           return "vb6_SSTab_SetTabs";
        if (propLower == "tab")            return "vb6_SSTab_SetTab";
        if (propLower == "taborientation") return "vb6_SSTab_SetTabOrientation";
        if (propLower == "tabstyle")       return "vb6_SSTab_SetTabStyle";
        if (propLower == "tabsperrow")     return "vb6_SSTab_SetTabsPerRow";
        if (propLower == "wordwrap")       return "vb6_SSTab_SetWordWrap";
        if (propLower == "visible")        return "vb6_SetControlVisible";
        if (propLower == "enabled")        return "vb6_SetControlEnabled";
        break;
    case FrmControlType::ListView:  // C29-7
        if (propLower == "view")               return "vb6_ListView_SetView";
        if (propLower == "gridlines")          return "vb6_ListView_SetGridLines";
        if (propLower == "fullrowselect")      return "vb6_ListView_SetFullRowSelect";
        if (propLower == "multiselect")        return "vb6_ListView_SetMultiSelect";
        if (propLower == "checkboxes")         return "vb6_ListView_SetCheckBoxes";
        if (propLower == "hidecolumnheaders")  return "vb6_ListView_SetHideColumnHeaders";
        if (propLower == "allowcolumnreorder") return "vb6_ListView_SetAllowColumnReorder";
        if (propLower == "labeledit")          return "vb6_ListView_SetLabelEdit";
        if (propLower == "sorted")             return "vb6_ListView_SetSorted";
        if (propLower == "sortkey")            return "vb6_ListView_SetSortKey";
        if (propLower == "sortorder")          return "vb6_ListView_SetSortOrder";
        if (propLower == "visible")            return "vb6_SetControlVisible";
        if (propLower == "enabled")            return "vb6_SetControlEnabled";
        break;
    case FrmControlType::OLE:  // C29-OLE 写表
        if (propLower == "oletypeallowed") return "vb6_OleCon_SetOLETypeAllowed";
        if (propLower == "sizemode")       return "vb6_OleCon_SetSizeMode";
        if (propLower == "displayasicon")  return "vb6_OleCon_SetDisplayAsIcon";
        if (propLower == "autoactivate")   return "vb6_OleCon_SetAutoActivate";
        if (propLower == "autoverbmenu")   return "vb6_OleCon_SetAutoVerbMenu";
        if (propLower == "borderstyle")    return "vb6_OleCon_SetBorderStyle";
        break;
    case FrmControlType::Timer:
        if (propLower == "interval") return "vb6_SetTimerInterval";
        if (propLower == "enabled") return "vb6_SetTimerEnabled";
        break;
    case FrmControlType::PictureBox:
        if (propLower == "caption") return "vb6_SetControlText";
        if (propLower == "picture") return "vb6_SetControlPicture";
        if (propLower == "autosize") return "vb6_SetPictureAutoSize";
        if (propLower == "align") return "vb6_SetControlAlign";  // P13.5b: Picture1.Align 停靠
        if (propLower == "visible") return "vb6_SetControlVisible";
        if (propLower == "enabled") return "vb6_SetControlEnabled";
        break;
    case FrmControlType::Image:
        if (propLower == "picture") return "vb6_SetControlPicture";
        if (propLower == "stretch") return "vb6_SetImageStretch";  // P17.2
        if (propLower == "visible") return "vb6_SetControlVisible";
        if (propLower == "enabled") return "vb6_SetControlEnabled";
        break;
    case FrmControlType::Menu:  // P20-36
        if (propLower == "caption") return "vb6_SetMenuCaption";
        if (propLower == "checked") return "vb6_SetMenuChecked";
        if (propLower == "enabled") return "vb6_SetMenuEnabled";
        if (propLower == "visible") return "vb6_SetMenuVisible";
        break;
    // C29-1b: 写侧同理 —— Path / Pattern / Drive 一赋就重刷列表 (RTL setter 内部
    // 调 Refresh), 这正是 VB6 三控件联动的机制。
    case FrmControlType::DriveListBox:
        if (propLower == "drive") return "vb6_DriveListBoxSetDrive";
        if (propLower == "text") return "vb6_SetControlText";
        if (propLower == "listindex") return "vb6_SetListIndex";
        break;
    case FrmControlType::DirListBox:
        if (propLower == "path") return "vb6_DirListBoxSetPath";
        if (propLower == "listindex") return "vb6_SetListIndex";
        break;
    case FrmControlType::FileListBox:
        if (propLower == "path") return "vb6_FileListBoxSetPath";
        if (propLower == "pattern") return "vb6_FileListBoxSetPattern";
        if (propLower == "filename") return "vb6_FileListBoxSetFileName";
        if (propLower == "listindex") return "vb6_SetListIndex";
        break;
    // C29-8a: TreeView 写侧。四条样式位的 setter 会连带 SWP_FRAMECHANGED + 重绘
    // (复选框位改的是每个节点的度量)，所以运行期赋值立刻见效，不是"只改了张表"。
    case FrmControlType::TreeView:
        if (propLower == "linestyle") return "vb6_TreeView_SetLineStyle";
        if (propLower == "indentation") return "vb6_TreeView_SetIndentation";
        if (propLower == "checkboxes") return "vb6_TreeView_SetCheckBoxes";
        if (propLower == "hottracking") return "vb6_TreeView_SetHotTracking";
        if (propLower == "hideselection") return "vb6_TreeView_SetHideSelection";
        if (propLower == "visible") return "vb6_SetControlVisible";
        if (propLower == "enabled") return "vb6_SetControlEnabled";
        break;
    // C29-5a: Toolbar 写侧 (与读侧同一批四条 + 通用两条)。
    case FrmControlType::Toolbar:
        if (propLower == "showtips") return "vb6_Toolbar_SetShowTips";
        if (propLower == "textstyle") return "vb6_Toolbar_SetTextStyle";
        if (propLower == "allowcustomize") return "vb6_Toolbar_SetAllowCustomize";
        if (propLower == "align") return "vb6_Toolbar_SetAlign";
        if (propLower == "visible") return "vb6_SetControlVisible";
        if (propLower == "enabled") return "vb6_SetControlEnabled";
        break;
    // D6 / C29-9: CommonDialog 写侧（取消由 RTL 按 CancelError 决定报不报 32755）。
    case FrmControlType::CommonDialog:
        if (propLower == "filter") return "vb6_CdSetFilter";
        if (propLower == "filename") return "vb6_CdSetFileName";
        if (propLower == "filetitle") return "vb6_CdSetFileTitle";
        if (propLower == "dialogtitle") return "vb6_CdSetDialogTitle";
        if (propLower == "initdir") return "vb6_CdSetInitDir";
        if (propLower == "defaultext") return "vb6_CdSetDefaultExt";
        if (propLower == "fontname") return "vb6_CdSetFontName";
        if (propLower == "flags") return "vb6_CdSetFlags";
        if (propLower == "cancelerror") return "vb6_CdSetCancelError";
        if (propLower == "color") return "vb6_CdSetColor";
        if (propLower == "min") return "vb6_CdSetMin";
        if (propLower == "max") return "vb6_CdSetMax";
        if (propLower == "copies") return "vb6_CdSetCopies";
        if (propLower == "fontsize") return "vb6_CdSetFontSize";
        break;
    case FrmControlType::Shape:  // P20-35
        if (propLower == "shape") return "vb6_SetShapeType";
        if (propLower == "borderwidth") return "vb6_SetShapeBorderWidth";
        if (propLower == "borderstyle") return "vb6_SetShapeBorderStyle";
        if (propLower == "fillstyle") return "vb6_SetShapeFillStyle";
        if (propLower == "bordercolor") return "vb6_SetShapeBorderColor";
        if (propLower == "fillcolor") return "vb6_SetShapeFillColor";
        if (propLower == "visible") return "vb6_SetControlVisible";
        break;
    case FrmControlType::Line:  // P20-35
        if (propLower == "x1") return "vb6_SetLineX1";
        if (propLower == "y1") return "vb6_SetLineY1";
        if (propLower == "x2") return "vb6_SetLineX2";
        if (propLower == "y2") return "vb6_SetLineY2";
        if (propLower == "borderwidth") return "vb6_SetLineBorderWidth";
        if (propLower == "borderstyle") return "vb6_SetLineBorderStyle";
        if (propLower == "bordercolor") return "vb6_SetLineColor";
        if (propLower == "visible") return "vb6_SetControlVisible";
        break;
    default:
        if (propLower == "visible") return "vb6_SetControlVisible";
        if (propLower == "enabled") return "vb6_SetControlEnabled";
        break;
    }
    return "";  // 未知属性
}

// 应用 .frm 设计期样式属性 (BorderStyle / Alignment)。
// 说明: 这些属性不会改变窗口类默认外观，必须在控件创建后调用对应 RTL setter；
//       顶层控件与容器(Frame/PictureBox)子控件都需要应用。
void CCodeGen::emitDesignerStyleProps(const FrmControl& ctrl, const std::string& hwndExpr) {
    std::string hw = "(void*)" + hwndExpr;

    auto bsIt = ctrl.properties.find("BorderStyle");
    if (bsIt != ctrl.properties.end()) {
        switch (ctrl.controlType) {
        case FrmControlType::Label:
        case FrmControlType::Frame:
        case FrmControlType::PictureBox:
        case FrmControlType::Image:
        case FrmControlType::TextBox:
            c_.emitLine("vb6_SetBorderStyle(" + hw + ", " + std::to_string((int)bsIt->second.intValue) + ");");
            break;
        default:
            break;
        }
    }

    auto alIt = ctrl.properties.find("Alignment");
    if (alIt != ctrl.properties.end() &&
        (ctrl.controlType == FrmControlType::Label || ctrl.controlType == FrmControlType::TextBox)) {
        c_.emitLine("vb6_SetAlignment(" + hw + ", " + std::to_string((int)alIt->second.intValue) + ");");
    }
}

// 控件类型的 Win32 样式位。取值与 cgen_form_ctrl_style_apply.inc 保持一致。
// 顶层控件与容器子控件共用，避免容器内子控件缺失类型样式。
long CCodeGen::controlTypeStyleBits(const FrmControl& ctrl) const {
    constexpr long kWsBorder   = 0x00800000L;
    constexpr long kBsPush     = 0x00000000L;
    constexpr long kBsAutoChk  = 0x00000003L;
    constexpr long kBsAutoRad  = 0x00000009L;
    constexpr long kBsGroupBox = 0x00000007L;
    constexpr long kSsLeft     = 0x00000000L;
    constexpr long kSsNotify   = 0x00000100L;  // SS_NOTIFY: Label 接收鼠标消息 (tooltip/Click)
    constexpr long kEsAutoH    = 0x00000080L;
    constexpr long kLbsNotify  = 0x00000001L;
    constexpr long kCbsDrop    = 0x00000002L;
    constexpr long kSsBitmap   = 0x0000000EL;
    constexpr long kSsCenterImg= 0x00000200L;
    constexpr long kBsPushLike = 0x00001000L;
    constexpr long kEsMulti    = 0x00000004L;
    constexpr long kEsAutoV    = 0x00000040L;
    constexpr long kWsHscroll  = 0x00100000L;
    constexpr long kWsVscroll  = 0x00200000L;

    long style = 0;
    switch (ctrl.controlType) {
        case FrmControlType::CommandButton: {
            style |= kBsPush;
            auto it = ctrl.properties.find("Style");
            if (it != ctrl.properties.end() && it->second.intValue == 1) style |= kBsPushLike;
            break;
        }
        case FrmControlType::TextBox: {
            style |= kWsBorder | kEsAutoH;
            auto mlIt = ctrl.properties.find("MultiLine");
            if (mlIt != ctrl.properties.end() && mlIt->second.intValue != 0) style |= kEsMulti | kEsAutoV;
            auto sbIt = ctrl.properties.find("ScrollBars");
            if (sbIt != ctrl.properties.end()) {
                int sb = (int)sbIt->second.intValue;
                if (sb == 1 || sb == 3) style |= kWsVscroll;
                if (sb == 2 || sb == 3) style |= kWsHscroll;
            }
            break;
        }
        case FrmControlType::Label:
            style |= kSsLeft | kSsNotify;
            break;
        case FrmControlType::CheckBox: {
            style |= kBsAutoChk;
            auto it = ctrl.properties.find("Style");
            if (it != ctrl.properties.end() && it->second.intValue == 1) style |= kBsPushLike;
            break;
        }
        case FrmControlType::OptionButton: {
            style |= kBsAutoRad;
            auto it = ctrl.properties.find("Style");
            if (it != ctrl.properties.end() && it->second.intValue == 1) style |= kBsPushLike;
            break;
        }
        case FrmControlType::Frame:
            style |= kBsGroupBox;
            break;
        case FrmControlType::ListBox: {
            style |= kLbsNotify | kWsBorder | kWsVscroll;
            auto sortIt = ctrl.properties.find("Sorted");
            if (sortIt != ctrl.properties.end() && sortIt->second.intValue != 0) style |= 0x0002L;
            auto msIt = ctrl.properties.find("MultiSelect");
            if (msIt != ctrl.properties.end()) {
                if (msIt->second.intValue == 1) style |= 0x0008L;
                else if (msIt->second.intValue == 2) style |= 0x0800L;
            }
            break;
        }
        case FrmControlType::ComboBox: {
            auto stIt = ctrl.properties.find("Style");
            if (stIt != ctrl.properties.end()) {
                if (stIt->second.intValue == 1) style |= 0x0001L;       // CBS_SIMPLE
                else if (stIt->second.intValue == 2) style |= 0x0003L;  // CBS_DROPDOWNLIST
                else style |= kCbsDrop;
            } else {
                style |= kCbsDrop;
            }
            // Fix 147: VB6 的 ComboBox 下拉列表带垂直滚动条 — 项数超过可见区时
            // 靠它滚动查看全部项. 此前漏了 WS_VSCROLL, 下拉只能看到前几项,
            // 用户以为"选项不全" (实测 32 个主题只能看到 9 个且无法滚动).
            style |= kWsVscroll;
            style |= kWsBorder;
            auto sortIt = ctrl.properties.find("Sorted");
            if (sortIt != ctrl.properties.end() && sortIt->second.intValue != 0) style |= 0x0100L;
            break;
        }
        case FrmControlType::PictureBox:
            style |= kSsBitmap | kSsCenterImg | kWsBorder;
            break;
        case FrmControlType::Image:
            style |= kSsBitmap | kSsCenterImg;
            break;
        default:
            break;
    }
    return style;
}

bool CCodeGen::controlTypeClearsCaption(const FrmControl& ctrl) const {
    return ctrl.controlType == FrmControlType::PictureBox ||
           ctrl.controlType == FrmControlType::Image ||
           // C29-1a: Shape / Line 是自绘控件, 窗口文字没有任何视觉效果, 但留着控件名
           // 当 caption 会让子类化/工具提示那几条路把它当有文本的控件看待。
           ctrl.controlType == FrmControlType::Shape ||
           ctrl.controlType == FrmControlType::Line;
}

// C29-1a: Line 的窗口矩形就是四个端点的包围盒 (单位 = 容器缇值, 与 .frm 存的一致)。
// 两条创建路 (顶层 / 容器子控件) 都调这里, 免得一边算对一边算成默认的 2000x300。
void CCodeGen::lineRectFromEndpoints(const FrmControl& ctrl,
                                     int& l, int& t, int& w, int& h) {
    auto getTw = [&ctrl](const char* key) -> int {
        auto it = ctrl.properties.find(key);
        return (it != ctrl.properties.end()) ? (int)it->second.intValue : 0;
    };
    int x1 = getTw("X1"), y1 = getTw("Y1");
    int x2 = getTw("X2"), y2 = getTw("Y2");
    l = (x1 < x2) ? x1 : x2;
    t = (y1 < y2) ? y1 : y2;
    w = (x1 < x2) ? x2 - x1 : x1 - x2;
    h = (y1 < y2) ? y2 - y1 : y1 - y2;
    if (w < 1) w = 1;
    if (h < 1) h = 1;
}

// C29-1a: 设计期外观属性落到控件上。值为 0 的那些 (Shape=0、FillStyle=0、BorderStyle=0)
// 也照发 —— RTL 侧把这类枚举存成 val+1, 所以 0 不再等于"没设过"。
// C29-1a: 设计期初始化用的句柄表达式 —— 控件数组 (如 ShapeLamp(0)/ShapeLamp(1)) 的
// 句柄在 vb6_arr_<名> 里, 硬写 vb6_hwnd_<名> 会打到空句柄上 (SetProp 静默失败)。
std::string CCodeGen::ctrlHwndExprForInit(const FrmControl& ctrl) const {
    std::string lower = Symbol::toLower(ctrl.controlName);
    if (knownControlArrays_.count(lower)) {
        return "vb6_CtrlArr_GetAt(&vb6_arr_" + cIdent(ctrl.controlName) + ", "
             + std::to_string(ctrl.index >= 0 ? ctrl.index : 0) + ")";
    }
    return "vb6_hwnd_" + cIdent(ctrl.controlName);
}

void CCodeGen::emitShapeLineProps(const FrmControl& ctrl, const std::string& hwndExpr) {
    auto emitInt = [&](const char* prop, const char* fn, int skipWhen) {
        auto it = ctrl.properties.find(prop);
        if (it == ctrl.properties.end()) return;
        int v = (int)it->second.intValue;
        if (v == skipWhen) return;
        c_.emitLine(std::string(fn) + "((void*)" + hwndExpr + ", " + std::to_string(v) + ");");
    };
    if (ctrl.controlType == FrmControlType::Shape) {
        emitInt("Shape", "vb6_SetShapeType", -1);
        emitInt("BorderWidth", "vb6_SetShapeBorderWidth", 1);
        emitInt("BorderStyle", "vb6_SetShapeBorderStyle", 1);
        emitInt("FillStyle", "vb6_SetShapeFillStyle", 1);
        emitInt("FillColor", "vb6_SetShapeFillColor", 0);
        emitInt("BorderColor", "vb6_SetShapeBorderColor", 0);
    } else if (ctrl.controlType == FrmControlType::Line) {
        emitInt("X1", "vb6_SetLineX1", -1);
        emitInt("Y1", "vb6_SetLineY1", -1);
        emitInt("X2", "vb6_SetLineX2", -1);
        emitInt("Y2", "vb6_SetLineY2", -1);
        emitInt("BorderWidth", "vb6_SetLineBorderWidth", 1);
        emitInt("BorderStyle", "vb6_SetLineBorderStyle", 1);
        emitInt("BorderColor", "vb6_SetLineColor", 0);
    }
}

// P20-36: 生成控件属性访问的HWND参数 (Menu控件用GetMenu+menuId)
std::string CCodeGen::makeCtrlHwndArg(const std::string& ctrlNameLower, FrmControlType ctrlType) const {
    // P20-39: ImageList 走原生复刻, 槽里是复刻实例指针不是 HWND, 必须用 vb6_com_<Name>
    // (emitControlHandleDecls 也是这么声明的) —— 发 vb6_hwnd_<Name> 就是 C2065。
    if (ctrlType == FrmControlType::ImageList) {
        auto it0 = knownFormControlOriginalNames_.find(ctrlNameLower);
        std::string n0 = (it0 != knownFormControlOriginalNames_.end()) ? it0->second : ctrlNameLower;
        return "vb6_com_" + cIdent(n0);
    }
    if (ctrlType == FrmControlType::Menu) {
        auto menuIt = knownMenuIds_.find(ctrlNameLower);
        if (menuIt != knownMenuIds_.end()) {
            return "(void*)GetMenu((HWND)" + knownMenuFormHwnd_ + "), " + std::to_string(menuIt->second);
        }
    }
    auto origIt = knownFormControlOriginalNames_.find(ctrlNameLower);
    std::string origName = (origIt != knownFormControlOriginalNames_.end()) ? origIt->second : ctrlNameLower;
    return "vb6_hwnd_" + cIdent(origName);
}

const char* CCodeGen::getDefaultPropertyName(FrmControlType ctrlType) {
    switch (ctrlType) {
    case FrmControlType::TextBox:      return "Text";
    case FrmControlType::Label:        return "Caption";
    case FrmControlType::CommandButton: return "Caption";
    case FrmControlType::CheckBox:     return "Value";
    case FrmControlType::OptionButton: return "Value";
    case FrmControlType::ListBox:      return "Text";
    case FrmControlType::ComboBox:     return "Text";
    case FrmControlType::Frame:        return "Caption";
    case FrmControlType::Form:         return "Caption";
    case FrmControlType::MDIForm:      return "Caption";
    case FrmControlType::PictureBox:   return "Picture";  // P17.2
    case FrmControlType::Image:        return "Picture";  // P17.2
    case FrmControlType::HScrollBar:   return "Value";  // P20-41
    case FrmControlType::VScrollBar:   return "Value";  // P20-41
    default:                           return nullptr;
    }
}




// ============================================================
// P8.4: Variant值包装 - 根据表达式类型推断Variant构造函数
// ============================================================

std::string CCodeGen::wrapVariantValue(ASTNode* valueNode, const std::string& cExpr) const {
    if (!valueNode) return "vb6_VariantEmpty()";
    
    // 特殊情况: Null字面量
    if (valueNode->kind == ASTNodeKind::LiteralExpr) {
        auto& lit = static_cast<LiteralExpr&>(*valueNode);
        if (lit.literalKind == LiteralKind::Null) return "vb6_VariantNull()";
        if (lit.literalKind == LiteralKind::Empty) return "vb6_VariantEmpty()";
        if (lit.literalKind == LiteralKind::Boolean) {
            return std::string("vb6_VariantBool(") + cExpr + ")";
        }
    }

    // Fix 090d2: C 表达式顶层已是 vb6_VARIANT 时直接返回, 不再包装 —
    // 原 2998 行的检查在 inferExprType switch 之后, 对符号表/推断层
    // 认为是具体标量但实际生成 vb6_VARIANT 的表达式 (如 COM 属性
    // fileInfo.Size 被推断为 Long, 实际生成
    // vb6_VariantFromComResult(vb6_ComCall(...))) 会被 switch 抢先包装
    // 成 vb6_VariantLong(vb6_VARIANT) → C2440 (cHttpServerResponse File
    // 内 fileSize = fileInfo.Size, fileSize As Variant).
    if (cExprIsVariant(cExpr)) {
        return cExpr;
    }

    // Fix 170: 右侧是 VB6 整体数组引用 `A()` (空括号) → 装箱成持有数组的 Variant。
    // 必须先于下面的 inferExprType switch: 它对 `A()` 可能给回**元素**类型 (Byte/Long),
    // 于是 vb6_VariantLong(载体指针) → C 侧只是警告, 指针被截断成 int32, 运行期才炸。
    if (valueNode->kind == ASTNodeKind::IndexOrCallExpr
        && isWholeArrayRef(static_cast<const Expr*>(valueNode))) {
        return "vb6_VariantArray((void*)" + cExpr + ")";
    }
    
    // 使用inferExprType推断表达式类型
    if (valueNode->kind == ASTNodeKind::BinaryExpr ||
        valueNode->kind == ASTNodeKind::UnaryExpr ||
        valueNode->kind == ASTNodeKind::LiteralExpr ||
        valueNode->kind == ASTNodeKind::IdentifierExpr ||
        valueNode->kind == ASTNodeKind::IndexOrCallExpr ||
        valueNode->kind == ASTNodeKind::MemberAccessExpr) {
        Vb6Type vtype = inferExprType(static_cast<Expr&>(*valueNode));
        switch (vtype) {
            case Vb6Type::String:    return "vb6_VariantString(" + cExpr + ")";
            case Vb6Type::Long:
            case Vb6Type::Integer:   return "vb6_VariantLong(" + cExpr + ")";
            case Vb6Type::Double:
            case Vb6Type::Single:    return "vb6_VariantDouble(" + cExpr + ")";
            case Vb6Type::Boolean:   return "vb6_VariantBool(" + cExpr + ")";
            case Vb6Type::Byte:      return "vb6_VariantLong(" + cExpr + ")";
            case Vb6Type::Date:      return "vb6_VariantDouble(" + cExpr + ")";
            case Vb6Type::Currency:  return "vb6_VariantDouble(" + cExpr + ")";
            default: break;
        }
    }
    
    // 如果右侧已经是vb6_VARIANT类型(如函数返回Variant), 直接赋值
    // Fix 051: 排除 vb6_VariantTo* 函数 (如 vb6_VariantToObjectVal 返回 void*,
    // vb6_VariantToString 返回 BSTR), 这些不是 vb6_VARIANT 类型, 需要包装.
    if ((cExpr.find("vb6_Variant") == 0 && cExpr.find("vb6_VariantTo") != 0)
        || cExpr.find("vb6_CStr") == 0) {
        return cExpr;
    }
    
    // COM后期绑定调用: vb6_ComCall返回VARIANT*, 需转为vb6_VARIANT
    if (cExpr.find("vb6_ComCall(") == 0) {
        return "vb6_VariantFromComResult(" + cExpr + ")";
    }
    
    // Array()临时变量: _arr_N is vb6_SafeArray1D*, 包装为Variant持有数组
    if (cExpr.find("_arr_") == 0) {
        return "vb6_VariantArray(" + cExpr + ")";
    }
    
    // Fix 025: 默认改用 _Generic 多态宏 vb6_VariantFromValue, 让编译器按实参 C 类型
    // 自动选择 Variant 构造函数。覆盖标量/BSTR/void*/class ptr/vb6_SafeArray1D* 等所有
    // 已注册的 _Generic 选择器, 不再粗暴回退到 VariantLong (会把指针/BSTR 当 int 截断)。
    return "vb6_VariantFromValue(" + cExpr + ")";
}

// Fix 198: 见 cgen_helpers.inc 声明处注释 —— 装箱点的布尔口径修正.
std::string CCodeGen::boxToVariant(Expr* expr, const std::string& cExpr) const {
    // 非布尔表达式必须逐字节退回原样 (vb6_VariantFromValue): 早退式的
    // "已是 VARIANT 就不包" 看着更干净, 但它会把存量码也一起改了 (护栏实测
    // VbQRCodegen 的 VB6_SA_AT 实参少了那层恒等包装) —— 本批只许动布尔。
    if (expr && inferExprType(*expr) == Vb6Type::Boolean && !cExprIsVariant(cExpr)) {
        // 形参是 int16_t: 显式收窄, 兼容 _Bool/int 两种 C 侧布尔表示.
        return "vb6_VariantBool((int16_t)(" + cExpr + "))";
    }
    return "vb6_VariantFromValue(" + cExpr + ")";
}
} // namespace vb6c3
