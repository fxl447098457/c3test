// cgen_form_uc_methods.inc - emitDesignerControlDecls 函数体片段：紧跟
// `cgen_form_uc_props.inc` 之后、`static const vb6_UserControlDesc vb6_<ctl>_ucHostDesc = {`
// 之前，发射 UserControl **自有 Public Sub 的按名桥**（表 + thunk）。
//
// Fix <vbeclipse> rev21. 为什么需要这一档（与 rev18 的 props 桥同族, 但缺了它
// **整个停靠区摆不出来**）:
//   工程里 `Set l_ucFolder = Controls.Item(id)` 之后调 `l_ucFolder.ShowView ViewId`
//   是**晚绑定**（工程自己把变量声明成 Variant），cgen 发的是
//     vb6_ComCall(实例, L"ShowView", {vb6_ComPackBSTR(ViewId)}, 1)
//   而宿主模型的方法分派（uc_hostmodel_call.inc）只有一张 **Win32 内建成员**表
//   （Refresh/Cls/Move/Show/Hide/ZOrder/…）—— `ShowView` 是 .ctl 里自己写的
//   Public Sub，不在那张表里 ⇒ 落进末尾"未知方法一律空实现"被**静默丢弃**。
//   实证 play78 (--arch x86): 5 处 `vb6_ComCall(ucFolder实例, L"ShowView", …)`
//   全落空 ⇒ ucFolder.ShowView 里那句 `l_View.View.Visible = True` 从未执行
//   ⇒ 所有视图窗体永远 Visible=False ⇒ ucFolder.ViewArea_Resize 的
//   `If .Visible Then .Move …` 闸门永闭 ⇒ 视图窗体停在 0x0 / 设计期尺寸
//   ⇒ **子面板空白**。与 rev20 的 ScaleWidth 隔离是两个独立缺陷（那是"读到别的
//   实例", 这是"根本没调"）, 两者都在同一条链上, 修 rev20 不解决本条。
//
// 这里为**能安全还原的 Public Sub** 各发一个 thunk, 再发一张 {名字, argc, fn} 表
// 交给 vb6_UserControlDesc.methods; 宿主模型在实例归一成 HWND **之前**查它
// (见 vb6_UcMethodDesc 注释 —— 必须排在 Refresh 之前, 因为同名时真 VB6 走用户写的那个)。
//
// ⚠ 为什么落在 emitDesignerControlDecls 而不是 emitFormFramework:
//   与 cgen_form_uc_props.inc 同因 —— cgen_base_generate_decl_pass.inc:39 里
//   `.ctl` 走的是 `else if (frmDesc) emitDesignerControlDecls(*frmDesc)` 分支。
//
// 判据（保守, 缺席即保持改动前行为 = 静默丢弃, 不会更糟）:
//   - 只收 `SymbolKind::Sub` 且 `access == Public`、`isExternal == false`（本模块自有）。
//     Event（MouseDown/Click/…）由 desc 里既有的 mouseDown/… 槽转发, 不在此表。
//   - 形参必须**逐个**满足: ByVal + 标量（String/Long/Integer/Boolean/Double/
//     Single/Byte/LongPtr/Date）。ByRef 形参、Object/工程类/Picture/Variant/
//     UDT/数组/ParamArray 一律不发 —— 那些在 VB6 侧有落临时、所有权与
//     VariantClear 语义, thunk 里还原不了（同 rev18 props 桥的取舍）。
//   - 有 Optional 形参时, 引擎按**实参个数**匹配（md->argc = 形参总数, 含 Optional）;
//     尾叁 `_has_<name>` 缺省标记由 thunk 自己按 "argc > 该形参序号" 推出来。
//   - 只发 **最多 4 个形参**的（超过 4 个的桥收益低、代码量大, 缺席不改变症状）。

{
    struct UcMethPick {
        std::string name;
        std::string fn;                                       // 生成的 thunk 名
        std::vector<std::string> paramVb;                     // 每形参的 VB6 标量类型名
        std::vector<std::string> paramC;                      // 每形参的 C 拆箱表达式类型
    };
    std::vector<UcMethPick> ucMeths;
    ucHostMethodCount_ = 0;

    // 标量类型名（VB6 拼写）→ C 拆箱表达式。与 cgen_form_uc_props.inc 的
    // vbScalarOfName / unboxUcSetter 同口径, 避免两处漂移。
    auto vbScalarOfName = [](const std::string& n) -> std::string {
        const std::string l = Symbol::toLower(n);
        if (l == "boolean" || l == "integer" || l == "long" || l == "longptr"
            || l == "byte" || l == "single" || l == "double" || l == "date"
            || l == "string") return l;
        return std::string();
    };
    auto vbScalarOfType = [](Vb6Type t) -> std::string {
        switch (t) {
            case Vb6Type::Boolean: return "boolean";
            case Vb6Type::Integer: return "integer";
            case Vb6Type::Long:    return "long";
            case Vb6Type::LongPtr: return "longptr";
            case Vb6Type::Byte:    return "byte";
            case Vb6Type::Single:  return "single";
            case Vb6Type::Double:  return "double";
            case Vb6Type::Date:    return "date";
            case Vb6Type::String:  return "string";
            default: return std::string();
        }
    };
    // 参数在 C 侧的类型（thunk 调原生 Sub 时按这个拆箱后传）。
    // String 用 `wchar_t*` 而不是 `BSTR` —— 生成代码的 TU 只 include <stdint.h>/
    // <wchar.h>/windows.h，BSTR 宏未必可见；C 里 BSTR 就是 wchar_t*，ABI 相同。
    auto cTypeOfVbScalar = [](const std::string& vb) -> std::string {
        if (vb == "boolean") return "int16_t";
        if (vb == "integer") return "int16_t";
        if (vb == "long" || vb == "longptr") return "int32_t";
        if (vb == "byte")    return "uint8_t";
        if (vb == "single")  return "float";
        if (vb == "double" || vb == "date") return "double";
        if (vb == "string")  return "wchar_t*";
        return std::string();
    };
    // (const vb6_VARIANT*) → 上面那个 C 类型。String 走 vb6_ho_variantToBstr（**借
    // 用** BSTR: 不新建、不 VariantClear、不 SysFreeString —— 生命周期归调用方那个
    // 实参 Variant, 与 rev18 props 桥的 String setter 同取舍）。
    auto unboxArg = [](const std::string& vb, int32_t idx) -> std::string {
        const std::string a = "(*(vb6_VARIANT*)argv[" + std::to_string(idx) + "])";
        if (vb == "boolean") return "(int16_t)vb6_VariantToBool(" + a + ")";
        if (vb == "integer") return "(int16_t)vb6_VariantToLong(" + a + ")";
        if (vb == "long" || vb == "longptr") return "vb6_VariantToLong(" + a + ")";
        if (vb == "byte")    return "(uint8_t)vb6_VariantToLong(" + a + ")";
        if (vb == "single")  return "(float)vb6_VariantToDouble(" + a + ")";
        if (vb == "double" || vb == "date") return "vb6_VariantToDouble(" + a + ")";
        if (vb == "string")  return "vb6_ho_variantToBstr(argv[" + std::to_string(idx) + "])";
        return std::string();
    };

    if (symTab_.moduleScope()) {
        for (auto& kv : symTab_.moduleScope()->symbols()) {
            Symbol* ps = kv.second.get();
            if (!ps) continue;
            if (ps->kind != SymbolKind::Sub) continue;
            if (ps->access != AccessLevel::Public) continue;
            // 同 props 桥: moduleScope 里混着 driver_crossmod 注入的外部副本,
            // 收了会给别的模块的方法发 thunk → LNK2019。
            if (ps->isExternal) continue;
            // 形参全 ByVal 标量才发（判据见文件头）。上限 4 个。
            if (ps->params.empty() || ps->params.size() > 4) continue;
            UcMethPick pick;
            pick.name = ps->name;
            bool ok = true;
            std::vector<bool> isOpt;
            for (const ParameterInfo& pi : ps->params) {
                if (!pi.isByVal) { ok = false; break; }        // ByRef 还原不了
                if (pi.isParamArray) { ok = false; break; }
                std::string vt = vbScalarOfName(pi.typeRefName);
                if (vt.empty()) vt = vbScalarOfType(pi.type);
                if (vt.empty() || cTypeOfVbScalar(vt).empty()) { ok = false; break; }
                pick.paramVb.push_back(vt);
                isOpt.push_back(pi.isOptional);
            }
            if (!ok) continue;
            pick.fn = "vb6_" + ctl + "_ucMeth_" + cIdent(ps->name);
            // 记下 Optional 位（缺省时按 VB6 默认值补）→ 编码进 thunk 名后缀不便,
            // 改为把 optional 位写成函数尾叁 `_has_<形参>` 的显式 if（见下）。
            for (size_t i = 0; i < isOpt.size(); i++) {
                if (isOpt[i]) pick.paramVb[i] += "|opt";       // 标记, 发射时剥离
            }
            ucMeths.push_back(std::move(pick));
        }
    }
    // 按名字排序: 符号表是 unordered_map, 排序让产物稳定可比。
    std::sort(ucMeths.begin(), ucMeths.end(),
              [](const UcMethPick& a, const UcMethPick& b) {
                  return Symbol::toLower(a.name) < Symbol::toLower(b.name);
              });

    for (auto& m : ucMeths) {
        // 拆掉标记, 还原成 (类型, 是否Optional)
        std::vector<std::string> types;
        std::vector<bool> opt;
        for (std::string t : m.paramVb) {
            const size_t bar = t.rfind("|opt");
            if (bar != std::string::npos) {
                types.push_back(t.substr(0, bar));
                opt.push_back(true);
            } else {
                types.push_back(t);
                opt.push_back(false);
            }
        }
        const int32_t n = static_cast<int32_t>(types.size());

        // thunk 签名: (inst, argc, argv, outRet) -> int32_t
        c_.emitLine("static int32_t " + m.fn + "(void* inst, int32_t argc, const void* argv[], void* outRet) {");
        c_.emitLine("    (void)argc; (void)outRet;");
        c_.emitLine("    vb6_cls_" + ctl + "* me = (vb6_cls_" + ctl + "*)inst;");
        c_.emitLine("    if (!me || !argv) return 0;");
        // 逐形参拆箱。Optional 形参: 实参没给到这一位就落 VB6 缺省值。
        // 引擎已按 md->argc == 形参总数匹配, 所以 argv 一定够长。
        for (int32_t i = 0; i < n; i++) {
            c_.emitLine("    " + cTypeOfVbScalar(types[i]) + " a" + std::to_string(i) + " = "
                        + unboxArg(types[i], i) + ";");
        }
        std::string call = "vb6_" + ctl + "_" + cIdent(m.name) + "(me";
        for (int32_t i = 0; i < n; i++) {
            call += (i == 0 ? ", " : ", ") + std::string("a") + std::to_string(i);
            // Optional 形参的 C 签名带一个 `int _has_<name>` 尾叁（约定见
            // cgen_expr_call_callee_params.inc）。VB6 语义: 没传 ⇒ 该位 0,
            // 原生 Sub 内部自己读 `_has_` 决定用缺省值。
            if (opt[i]) call += ", 1";
        }
        call += ");";
        c_.emitLine("    " + call);
        c_.emitLine("    return 1;");
        c_.emitLine("}");
    }
    if (!ucMeths.empty()) {
        c_.emitLine("static const vb6_UcMethodDesc vb6_" + ctl + "_ucMethods[] = {");
        for (auto& m : ucMeths) {
            c_.emitLine("    { L\"" + m.name + "\", "
                        + std::to_string(static_cast<int32_t>(m.paramVb.size()))
                        + ", " + m.fn + " },");
        }
        c_.emitLine("};");
    }
    ucHostMethodCount_ = static_cast<int32_t>(ucMeths.size());
}
