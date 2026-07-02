# 39-P22-VB6覆盖率综合分析与缝隙补全

> 日期: 2026-07-02
> 提交: 待提交

## 本轮目标

基于021覆盖率统计文档，对VB6语言覆盖率进行四维度综合分析（函数/语句/常量/VBP），识别并修复关键缝隙。

## 完成项

### P22-01: 综合覆盖率分析文档

创建 `ai/讨论记录/017-VB6语法成员覆盖率综合分析.md`，包含四维度详细分析：

| 维度 | 已实现 | 目标 | 覆盖率 |
|------|--------|------|--------|
| 内置函数 | 147 | ~143 | 102.8% |
| 语句 | 77完全+5部分 | 84 | 91.7%完全 |
| 内置常量 | 216 | ~350+ | ~60-65% |
| VBP解析 | ~12字段 | ~25+ | ~50% |

### P22-02: VBP VersionCompanyName 键名bug修复

**问题**: VB6 IDE在VBP文件中写入的版本信息键名带"Version"前缀（如`VersionCompanyName`），但解析器使用的是不带前缀的键名（如`CompanyName`），导致版本信息无法正确读取。

**修复**: `src/project/vbp_parser.cpp` 中7个键名修正：
- `CompanyName` → `VersionCompanyName`
- `FileDescription` → `VersionFileDescription`
- `LegalCopyright` → `VersionLegalCopyright`
- `ProductName` → `VersionProductName`
- `Comments` → `VersionComments`
- `LegalTrademarks` → `VersionLegalTrademarks`
- `OriginalFileName` → `VersionOriginalFileName`

### P22-03: 47个缺失内置常量

在 `src/semantics/semantic_analyzer.cpp` 中新增47个常量：

- **Shell常量(6个)**: vbHide(0), vbNormalFocus(1), vbMinimizedFocus(2), vbMaximizedFocus(3), vbNormalNoFocus(4), vbMinimizedNoFocus(6)
- **日期格式常量(5个)**: vbGeneralDate(0), vbLongDate(1), vbShortDate(2), vbLongTime(3), vbShortTime(4)
- **系统颜色常量(25个)**: vbScrollBars(-2147483648) ~ vb3DShadow(-2147483624)
- **IME状态常量(11个)**: vbIMEModeNoControl(0) ~ vbIMEModeHangul(10)

常量总数从216增加到263。

### P22-04: Date$/Time$ 语句实现

VB6中 `Date$ = "12-31-2025"` 和 `Time$ = "23:59:59"` 是设置系统日期/时间的语句。

**代码生成**: `src/backend/cgen_stmt.cpp` 的 `visit(AssignmentStmt&)` 开头新增拦截逻辑：
- 当赋值目标标识符为 `date`/`date$` → 生成 `vb6_DateSet(wrapToBSTR(value))`
- 当赋值目标标识符为 `time`/`time$` → 生成 `vb6_TimeSet(wrapToBSTR(value))`

**RTL实现**: `src/rtl/core/vb6rtl.h` + `vb6rtl.c`
- `vb6_DateSet(BSTR)`: 宽字符→ANSI转换，strtok_s按`-`或`/`分割为MM/DD/YYYY，SetLocalTime设置
- `vb6_TimeSet(BSTR)`: 宽字符→ANSI转换，strtok_s按`:`分割为HH:MM:SS，SetLocalTime设置

**词法分析配合**: lexer将 `Date$` 作为整体标识符（类型后缀$吸附到token中），cgen拦截时strip尾部$后比较。

## 修改文件

| 文件 | 修改内容 |
|------|---------|
| `src/project/vbp_parser.cpp` | 7个Version键名修正 |
| `src/semantics/semantic_analyzer.cpp` | +47个addConst常量 |
| `src/backend/cgen_stmt.cpp` | Date$/Time$赋值拦截 |
| `src/rtl/core/vb6rtl.h` | +vb6_DateSet/vb6_TimeSet声明 |
| `src/rtl/core/vb6rtl.c` | +vb6_DateSet/vb6_TimeSet实现 |
| `ai/讨论记录/017-VB6语法成员覆盖率综合分析.md` | 新建综合分析文档 |
| `ai/004-进度表.md` | +P22章节 |

## 回归测试

74/74 全部通过，零回归。

## 待推进

- DefType实际影响类型推断（P2优先级）
- LSet/RSet语句形式解析支持
- 更多常量补全（打印机常量等）
- VBP CompatibleMode/StartMode解析
- For Each COM集合(IEnumVARIANT)
