# BugFix - VB6字符串语义修复 + Format函数完整实现

**日期**: 2026-07-14
**里程碑**: M28
**回归**: 82/82 PASS
**提交**: 763541e

## 问题背景

使用AsyncTest2（VBMAN cHttpClient异步下载测试）验证P26的事件Sink功能时，发现日志输出存在两个VB6字符串语义Bug：

1. **vbCrLf输出异常**：`vbCrLf`（应为`\r\n`即CRLF）在日志文件中表现为CR（0x0D），实际hex为`0D 0D 0A`（CR CR LF）
2. **双引号转义未处理**：VB6中`""`表示一个双引号字符，但PostData常量输出为两个双引号

同时发现`Format`函数对格式字符串完全无效：`Format(0.870336, "0.0")`输出`0.870336`而非`0.9`。

## Bug1: vbCrLf换行符问题

### 根因（两层）

**第一层：RTL运行时文本模式陷阱**

`vb6_Open`用文本模式打开文件（`fopen("...", "a")`）。Windows C运行时文本模式下，`fputc('\n')`自动扩展为`\r\n`。当`vb6_Print`写BSTR中的`\r\n`时：

- `fputc('\r')` → 0x0D
- `fputc('\n')` → 0x0D 0x0A（文本模式自动加`\r`）

结果：文件中是`0D 0D 0A`而非`0D 0A`。

**第二层：.res资源缓存陷阱**

初版修复后仍不生效，hex dump仍为`0D 0D 0A`。排查发现`c3rtl.rc.res`（编译后的RC资源）未被CMake增量构建检测到`.lib`变化，C3.exe仍嵌入旧版RTL。手动删除`.res`文件后重建，修复才真正生效。

### 修复

**vb6_Open**（`src/rtl/core/vb6rtl.c`）：
- Sequential文件模式改为binary：`"r"` → `"rb"`、`"w"` → `"wb"`、`"a"` → `"ab"`
- Random/Binary模式本身已用`"r+b"`，不受影响

**vb6_Print**（`src/rtl/core/vb6rtl.c`）：
- 字符串内容中的`\r`/`\n`原样写出（binary模式无C运行时转换）
- Print语句末尾换行从`fputc('\n')`改为`fputc('\r'); fputc('\n')`显式写CRLF
- Width越界换行同样改为显式CRLF

### 验证

修复后hex dump：
```
...=== 0D 0A Time:... 0D 0A
```
第1行`=== Async ... ===`后正确CRLF，第2行`Time: ...`后正确CRLF。

## Bug2: 双引号转义问题

### 根因（三个层次）

**第一层：词法分析保留原始`""`**

`Lexer::scanString()`（`lexer.cpp:609-639`）在扫描VB6字符串时，保留了`""`原始两个`"`字符到token的rawText中，不做折叠。这是正确行为——词法层应保留源码原貌。

**第二层：代码生成未折叠就直接C转义**

`cgen_expr.cpp:64-113`字符串字面量代码生成中，剥离外层引号后，每个`"`独立C转义为`\"`。VB6的`""`变成`\"\"`（两个双引号）而非`\"`（一个双引号）。

示例：`Const PostData = "{""orderFields"":[]}"` → 生成 `L"{\"\"orderFields\"\":[]}"` 而非 `L"{\"orderFields\":[]}"`

**第三层：符号表constStringValue含引号且无C转义**

- `semantic_analyzer.cpp:405`：用户字符串常量的`constStringValue`被赋值为`lit->rawText`（含外层引号和未折叠`""`），与内置常量路径（如`addStrConst("vbCr", "\r")`，存的是不带引号的实际字符）不一致
- `semantic_analyzer.cpp:2353`：inline const求值直接将`constStringValue`拼入C字符串字面量，无任何C转义处理

### 修复

1. **cgen_expr.cpp**：String literal代码生成中，在C转义前新增`""` → `"`折叠步骤：
```cpp
// VB6的""转义折叠: 字符串内""表示一个双引号 → 折叠为单个"
std::string folded;
for (size_t k = 0; k < inner.size(); k++) {
    if (inner[k] == '"' && k + 1 < inner.size() && inner[k + 1] == '"') {
        folded += '"';  // "" → "
        k++;            // skip second "
    } else {
        folded += inner[k];
    }
}
```

2. **semantic_analyzer.cpp:405**：`constStringValue`存储时，剥离外层引号+折叠`""`

3. **semantic_analyzer.cpp:2353**：inline const C代码生成，对`constStringValue`做C转义（`\`、`"`、`\n`、`\r`、`\t`），不再直接拼接

### 验证

修复后生成的Form1.h：
```c
#define PostData (vb6_BSTR_FromStr(L"{\"orderFields\":[],\"orderDirects\":[],...}"))
```
每个键名只有一对转义双引号`\"`，日志输出`{"orderFields":[],...}`正确。

## Format函数完整实现

### 原始状态

旧版`vb6_Format`（`vb6rtl.c:208-254`）完全忽略`fmt`参数（`(void)fmt;`），数值一律`swprintf(buf, 64, L"%g", val)`原样输出。格式字符串"0.0"等完全无效。

### VB6 Format规范

VB6 Format支持两大类格式：

**命名格式**：
- `General Number` / `Currency` / `Fixed` / `Standard` / `Percent` / `Scientific`
- `Yes/No` / `True/False` / `On/Off`
- `Long Date` / `Short Date` / `Long Time` / `Short Time`

**用户自定义数字格式**：
| 字符 | 含义 |
|------|------|
| `0` | 数字占位符，不足补0 |
| `#` | 数字占位符，不足不补 |
| `.` | 小数点（按位数四舍五入） |
| `%` | 数值×100追加% |
| `,` | 千位分隔符 / 缩放因子(÷1000) |
| `E-`/`E+`/`e-`/`e+` | 科学计数法 |
| `\` | 转义下一个字符 |
| `"xxx"` | 原样输出引号内文本 |
| `;` | 多节格式(正;负;零;Null) |

**用户自定义字符串格式**：`<` 强制小写 / `>` 强制大写 / `@` 占位补空格 / `&` 占位不补 / `!` 左→右填充

### 实现架构

```
vb6_Format(expr, fmt)
  ├── 提取数值/字符串/Null
  ├── fmt为空 → 默认转换（兼容旧行为）
  ├── 命名格式匹配 → 委托FormatCurrency等
  ├── 解析分节符(;) → 选择正/负/零/Null节
  ├── 检测格式类型(数字 vs 字符串)
  ├── 字符串格式 → <>=&!处理
  └── 数字格式
       ├── 预扫描: %缩放 + ,缩放 + E科学计数
       ├── 解析占位符: beforeDot0/afterDot0/#
       ├── 四舍五入到小数位数
       ├── swprintf格式化
       ├── 千位分隔符插入
       └── 尾零裁剪 + %追加
```

### 关键设计决策

1. **四舍五入在snprintf之前**：先用`round(val * 10^n) / 10^n`精确舍入，再`swprintf("%.Nf")`输出，避免浮点精度问题
2. **千位分隔符**：先格式化无分隔符的整数部分，再从右往左每3位插入`,`
3. **尾零裁剪**：`afterDot0`决定最少保留几位小数（`0`占位符），`afterDotHash`部分可裁剪尾零
4. **分节符语义**：1节=所有值, 2节=非负/负, 3节=正/负/零, 4节=正/负/零/Null

### 验证

`Format(0.870336, "0.0")` → 按afterDot0=1四舍五入 → `0.9` ✓

## .res资源缓存问题（重要教训）

C3通过`c3rtl.rc`的RCDATA资源嵌入RTL库。当RTL `.lib`文件更新后：
- CMake增量构建**不会**检测到`.lib`变化需要重编译`.rc`
- 必须手动删除`.build/CMakeFiles/c3.dir/src/driver/c3rtl.rc.res`
- 然后删除`c3.exe`强制重链接

完整重建流程：
```
1. build_rtl_libs.bat          → 重建x64和x86 RTL lib
2. 删除 c3rtl.rc.res + c3.exe  → 强制RC重编译+重链接
3. cmake --build .build         → 重建C3
```

## 修改文件汇总

| 文件 | 修改 |
|------|------|
| `src/rtl/core/vb6rtl.c` | vb6_Open改binary + vb6_Print显式CRLF + Format完整重写(~600行) |
| `src/backend/cgen_expr.cpp` | String literal代码生成: ""折叠步骤 |
| `src/semantics/semantic_analyzer.cpp` | constStringValue去引号+折叠"" + inline const C转义 |
| `src/rtl/lib/*.lib` (6个) | 重建RTL库(/MT) |

## 统计

- **代码变更**: +650 / -41 行
- **回归测试**: 82/82 PASS，零回归
- **修复Bug数**: 2个字符串语义Bug + 1个Format函数完整缺失
