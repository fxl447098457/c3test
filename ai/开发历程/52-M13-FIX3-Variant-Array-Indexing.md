# M13-FIX3: Variant数组索引修复

> **历史说明**：本文涉及的 AIGC 零宽字符水印及其全部防御手段（`strip_zw` / `.temp/` 中转 / `CMakeLists_clean.txt` / `full_build.bat` / 强制 text-writer 纪律）已于 2026-09-14 撤销，注入源已不存在。以下为历史记录，请勿照做。

**日期**: 2026-07-05
**里程碑**: M13 (兼容性收尾)
**回归测试**: 74/74 零失败

## 问题

编译  = Array(10,20,30) : MsgBox a(i) (where a is Variant) 时出现两个bug:

1. **Array()赋值错误**: 生成  = vb6_VariantLong(_arr_0) — _arr_0 是 b6_SafeArray1D*，不是 int32_t
2. **Variant数组索引错误**: (i) 直接生成为C函数调用语法，C编译器报错  不是函数

## 根因分析

### Bug1: wrapVariantValue默认分支

cgen_util.cpp wrapVariantValue() 的switch-case逻辑:
- b6_VariantLong → 处理Long/Integer/Boolean等
- b6_VariantDouble → 处理Double/Single/Date等
- b6_VariantBSTR → 处理String
- b6_VariantObject → 处理Object
- b6_VariantFromComResult → 处理COM调用结果
- **default → vb6_VariantLong** ← 这里！

Array() 函数返回 Vb6Type::Variant 类型，不在switch中，走default。而 _arr_0 临时变量名不以 "vb6_Variant" 开头，也不匹配任何特殊前缀。

### Bug2: knownVariantVars_未参与数组索引判断

cgen_expr.cpp IndexOrCallExpr 数组索引检测:
1. 先查 knownArrays_ (Dim a() As Long 类型的数组)
2. 再查 symtab 的数组类型
3. 若 isArrayAccess=true → 生成 [i]
4. 否则 → 函数调用路径

Variant 变量在 semantic_analyzer 中注册到 knownVariantVars_ 而非 knownArrays_，所以 (i) 走了函数调用路径。

## 修复方案

### 修复1: wrapVariantValue添加_arr_检测 (cgen_util.cpp ~L1255)

``cpp
// 在default分支之前:
if (cExpr.find(""_arr_"") == 0) return ""vb6_VariantArray("" + cExpr + "")"";
``

当表达式以 _arr_ 开头时，说明是 Array() 创建的临时SafeArray变量，用 b6_VariantArray() 构造函数包装为Variant。

### 修复2: IndexOrCallExpr添加Variant数组检测 (cgen_expr.cpp ~L1234)

``cpp
if (!isArrayAccess && node.callee && node.callee->kind == ASTNodeKind::IdentifierExpr 
    && node.named.empty() && !node.positional.empty()) {
    auto& vIdent = static_cast<IdentifierExpr&>(*node.callee);
    std::string vLower = vIdent.name;
    std::transform(vLower.begin(), vLower.end(), vLower.begin(), ::tolower);
    if (knownVariantVars_.count(vLower)) {
        emitExpr(*node.positional[0]);
        std::string vIndex = std::move(lastExpr_);
        lastExpr_ = ""vb6_VariantArrayGet(&"" + cIdent(vIdent.name) + "", "" + vIndex + "")"";
        return;
    }
}
``

在 knownArrays_ 检查之后、if (isArrayAccess) 之前插入。检测 knownVariantVars_ 集合，生成 b6_VariantArrayGet() 调用。

### RTL新增 (vb6rtl.h + vb6rtl.c)

1. **vb6_vtArray = 0x2000**: Variant类型枚举新增数组标志
2. **vb6_VariantArray(void\*)**: 内联构造函数，设置vt=vb6_vtArray, parray=(SAFEARRAY*)ptr
3. **vb6_VariantArrayGet(vb6_VARIANT\*, int32_t)**: 按 t & vb6_vtArray 检测数组Variant，通过parray访问SafeArray，按elemType分发读取元素返回vb6_VARIANT
4. **vb6_VariantArraySet(vb6_VARIANT\*, int32_t, vb6_VARIANT)**: 写入元素到SafeArray

### 生成代码对比

**修复前:**
``c
vb6_VARIANT a = vb6_VariantEmpty();
vb6_SafeArray1D* _arr_0 = vb6_ArrayCreate(3);
vb6_ArraySetLong(_arr_0, 0, 10);
vb6_ArraySetLong(_arr_0, 1, 20);
vb6_ArraySetLong(_arr_0, 2, 30);
a = vb6_VariantLong(_arr_0);     // BUG: 类型不匹配
// ...
vb6_MsgBox1(a(i));                // BUG: C函数调用语法
``

**修复后:**
``c
vb6_VARIANT a = vb6_VariantEmpty();
vb6_SafeArray1D* _arr_0 = vb6_ArrayCreate(3);
vb6_ArraySetLong(_arr_0, 0, 10);
vb6_ArraySetLong(_arr_0, 1, 20);
vb6_ArraySetLong(_arr_0, 2, 30);
vb6_VariantClear(&a);
a = vb6_VariantArray(_arr_0);    // 正确: SafeArray→Variant包装
// ...
vb6_MsgBox1(vb6_VariantToString(vb6_VariantArrayGet(&a, i)));  // 正确: Variant数组索引
``

## RTL .lib重建

修改RTL源文件后需要重建预编译.lib:

1. 编译6个.obj: vb6rtl/vb6com/vb6forms/vb6comserver × (x64 + x86)
2. 创建6个.lib: vb6rtl.lib/vb6rtl_gui.lib/vb6rtl_dll.lib × (x64 + x86)
3. touch c3rtl.rc (强制RC重编译嵌入新.lib)
4. 重建c3.exe (full_build.bat)

注意: x86 obj需放在 src/rtl/lib/x86/ 目录。使用脚本 ebuild_rtl_full.bat 一次完成。

## 验证

- test_variant_array.bas: =Array(10,20,30): For i=0 To 2: MsgBox a(i): Next
  - 编译成功 (x86)
  - 运行弹出3个MsgBox: 10, 20, 30 ✓
- 74/74回归测试零失败 ✓

## 遗留项

- dic.Keys() 返回SAFEARRAY包在VARIANT中，需要类似的解包机制
- cgen_expr.cpp COM标记路径需要区分property get vs method call
- vb6_VariantArraySet 写入功能尚未有测试用例
