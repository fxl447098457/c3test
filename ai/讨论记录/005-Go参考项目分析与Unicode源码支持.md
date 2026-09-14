# 005. Go/VB 双线参考项目分析 + Unicode 源码支持设计

> 分析 gobasic 系列文档、aspgo 参考项目（RustASP、Hulo），提炼对 vb6c 编译器的可借鉴设计，并规划 Unicode 源码支持方案

---

## 1. 分析背景

伟哥早期探索过两条 Go 语言路线：
- **aspgo**：用 Go 实现经典 ASP 服务器，让遗留 ASP 应用在现代基础设施运行
- **vb6go**：用 Go 实现完整的 VB6 工具链（转译器 + LSP + 调试器 + GUI）

最终决策转向 **C++ + LLVM** 的 vb6c 编译器（见 001 文档），但 Go 路线的研究成果和参考项目中仍有大量值得借鉴的设计。

### 本文档覆盖

1. gobasic 系列文档的关键洞察提炼
2. RustASP 项目架构分析
3. Hulo 项目架构分析
4. 对 vb6c 的具体借鉴清单
5. Unicode 源码支持方案设计

---

## 2. gobasic 文档关键洞察

### 2.1 核心洞察：「VBScript 是 VB6 的子集」

这是 gobasic 系列最重要的一条结论。VBScript 的全部语法（变量/控制流/函数/Class/On Error）都是 VB6 语法的子集。这意味着：

- 如果完整实现了 VB6 的解析器，VBScript 自动支持
- aspgo 的 VBScript 引擎可以被 vb6c 的 VB6 解析器替代
- VBA 也是 VB6 的子集，同理

**对 vb6c 的启示**： 我们的编译器目标是 VB6 全集，但可以先用 VBScript 子集作为早期测试集——VBScript 程序如果不能正确解析/编译，就不必测试更复杂的 VB6 特性。

### 2.2 技术路线对比

gobasic 002 文档对比了三条路线：

| 路线 | 描述 | 最终选择 |
|------|------|---------|
| A: Transpiler | VB6 → Go 源码 → go build → EXE | 备选 |
| B: 字节码 VM | VB6 → 字节码 → Go VM 执行 | 备选 |
| C: 混合 | 开发时解释器 + 发布时转译 | 推荐 |

**我们的实际路线**（vb6c）：C++ 前端 + LLVM 后端，比 Go 路线有以下优势：
- LLVM 后端性能远超 Go 转译
- 不依赖 Go 运行时，产物更小
- 直接生成 x86/x64 机器码，无需中间语言
- COM 互操作更自然（Windows C++ ABI 直接对接）

但 Go 路线的 **LSP + DAP** 思路值得借鉴（见 2.4 节）。

### 2.3 Variant 类型系统设计

gobasic 002 文档给出了 Go 语言的 Variant 定义（VType + VValue），核心规则：

- Empty/Null/Nothing 三种空状态
- 数值统一为 f64（VBScript 模式）或分 Integer/Long/Single/Double/Currency（VB6 模式）
- 隐式类型强制转换：`数值 + 字符串(数字) → 隐式转换后加法`
- `Empty → 当 0 或 "" 处理`；`Null → 结果为 Null`

**对 vb6c 的启示**：我们已有的 `common/types.hpp` 中的 Vb6Type 枚举覆盖了 VB6 的完整类型系统，比 Go 的简化方案更精确。但 VBA 隐式类型强制转换规则需要在语义分析阶段完整实现，这是 P2 阶段的核心难点。

### 2.4 工具链集成设计

gobasic 003 文档设计了完整的 vb6go 工具链架构：

```
VS Code 扩展 ←→ LSP 语言服务器 ←→ 编译器前端
VS Code 扩展 ←→ DAP 调试适配器 ←→ VM/编译器
```

**对 vb6c 的启示**：
- **LSP 语言服务器**：在 P3 阶段可以考虑实现，让 vb6c 前端作为 LSP Server 提供语法检查/补全/跳转功能
- **DAP 调试器**：在 P5 阶段，如果实现了字节码 VM 或 LLVM JIT，可以接入 DAP 协议
- 这些可以在 Rust/Go 中独立实现，调用 vb6c 的 .dll 即可

### 2.5 vb6parse 翻译策略

gobasic 004 文档详述了从 Rust 的 vb6parse 翻译到 Go 的策略，其中包含大量 VB6 语法实现细节：

- **Pratt 解析器绑定优先级表**：14 级优先级（^ 最高，Imp 最低），与 VB6 规范一致
- **关键字表**：~120 个 VB6 关键字，按长度降序优先匹配
- **符号表**：~30 个符号，双字符优先（<>  <  <）
- **CST 构建器**：StartNode/FinishNode/Token 三操作，栈式构建
- **错误恢复**：同步点集合（End/Sub/Function/If/For/While/Do/Select/Dim/Newline）

**对 vb6c 的启示**： 我们的 Parser 设计（P1 阶段）应直接参考这些规则，特别是 Pratt 优先级表和错误恢复策略。

---

## 3. RustASP 架构分析

**项目路径**: `D:\code\go\aspgo.top\_ref\开源2\rustasp\`
**语言**: Rust + axum | **定位**: VBScript 解释器 + ASP 服务器 | **代码量**: ~8000-10000 行

### 3.1 整体架构

```
ASP 页面 → Segmenter(切分HTML/VBS) → Lexer → Pratt Parser → AST → Tree-walking Interpreter
                                                                     ↓
                                                              BuiltinExecutor(7分类内置函数)
                                                              BuiltinObject(COM对象模拟)
```

### 3.2 Pratt 解析器 — 教科书级实现

RustASP 的 Pratt 解析器核心仅 **45 行**，是迄今见过的最简洁实现：

```rust
pub fn parse_expr(&mut self, min_bp: u8) -> Result<Expr, ParseError> {
    let mut lhs = self.parse_prefix()?;
    loop {
        let (l_bp, r_bp) = match self.infix_binding_power() { ... };
        if l_bp < min_bp { break; }
        let op_token = self.advance().clone();
        let rhs = self.parse_expr(r_bp)?;
        lhs = Expr::Binary { left: Box::new(lhs), op, right: Box::new(rhs) };
    }
    Ok(lhs)
}
```

**优先级表（binding power 对）**：

| l_bp | r_bp | 运算符 | 结合性 |
|------|------|--------|--------|
| 1 | 2 | Or | 左 |
| 3 | 4 | And | 左 |
| 5 | 6 | =, <>, Is | 左 |
| 7 | 8 | <, <=, >, >= | 左 |
| 9 | 10 | & (拼接) | 左 |
| 11 | 12 | +, - | 左 |
| 13 | 14 | *, /, \, Mod | 左 |
| 16 | 15 | ^ | **右结合** |

右结合通过 `l_bp > r_bp` 实现（幂运算 16/15）。

**对 vb6c 的启示**： 我们的 Parser 应直接采用这种 (l_bp, r_bp) 对的 Pratt 设计，比传统的 "每级一个函数" 递归下降更简洁、更容易扩展。

### 3.3 Value 类型系统 — 精确的 VBScript 语义

```rust
pub enum Value {
    Empty,                                  // VBScript Empty（未初始化变量）
    Null,                                   // VBScript Null（数据库 NULL）
    Nothing,                                // VBScript Nothing（对象引用空值）
    Boolean(bool),
    Number(f64),                            // 统一 f64（VBScript 模式）
    String(String),
    Array(Arc<Mutex<VbsArray>>),            // 共享引用语义
    Object(ObjectRef),                      // Arc<Mutex<dyn BuiltinObject>>
}
```

**设计亮点**：
- `Arc<Mutex<>>` 精确模拟 VBScript 的数组/对象引用语义（arr2=arr1 后修改 arr2 影响 arr1）
- 数组/对象比较用 `Arc::ptr_eq`，符合 VBScript 的 Is 语义
- `Empty/Null/Nothing` 三种空状态严格区分

**对 vb6c 的启示**：
- VB6 的 Variant 类型比 VBScript 更复杂（16 种子类型），我们的 `variant.hpp` 需要完整覆盖
- 数组/对象的引用语义在代码生成阶段需要精确实现
- `Empty → 0/""`、`Null → 结果为 Null` 的隐式转换规则需要在语义分析阶段完成

### 3.4 其他值得借鉴的设计

| 设计 | RustASP 实现 | vb6c 可借鉴点 |
|------|-------------|--------------|
| Scope 链式作用域 | `Scope { vars, parent: Option<Arc<Scope>> }` + ErrorMode | 语义分析的作用域解析 |
| 变量名大小写不敏感 | 所有变量名 `to_lowercase()` 查找 | VB6 标识符不区分大小写 |
| 双遍执行 | preprocess_declarations 注册 Class/Function → 再按序执行 | 支持 VB6 的前向引用 |
| On Error 处理 | ErrorMode 枚举 + Guard 包装每个可能出错的操作 | 错误处理的代码生成策略 |
| BuiltinObject trait | GetProperty/SetProperty/CallMethod/DefaultMember/TypeName | COM 对象互操作的接口设计 |
| BuiltinExecutor | 7 分类（math/conversion/string/array/datetime/inspection/format） | 内置函数的分模块实现 |

---

## 4. Hulo 架构分析

**项目路径**: `D:\code\go\aspgo.top\_ref\开源1\hulo\`
**语言**: Go | **定位**: 多语言转译器编译器 | **代码量**: ~15000-20000 行

### 4.1 整体架构

```
Hulo 源码(.hl) → Parser → AST → Interpreter(comptime求值+AST重建)
                                    ↓
                               Optimizer → Transpiler → 目标语言AST → 代码生成
```

Hulo 可以转译到 VBScript/Bash/PowerShell/Batch 等多种目标语言。

### 4.2 VBScript 子系统

Hulo 中 VBScript 是**转译目标**（非源语言），包含：

| 组件 | 文件 | 行数 | 作用 |
|------|------|------|------|
| VBS Token | `syntax/vbs/token/token.go` | 166 | ~50 个词法单元定义 |
| VBS AST | `syntax/vbs/ast/ast.go` | 680 | 完整 Node/Stmt/Expr 体系 |
| VBS 文法 | `syntax/vbs/parser/vbsParser.g4` | 99 | ANTLR4 语法定义（概念原型，不完整） |
| VBS 转译器 | `internal/transpiler/vbs/transpiler.go` | 1441 | Hulo AST → VBS AST 递归转换 |
| VBS Token 映射 | `internal/transpiler/vbs/token.go` | — | Hulo Token → VBS Token 映射 |

### 4.3 关键技术：CallFrame 栈 + Emit/Flush 缓冲

VBS 转译器的核心难题是 VBScript 的声明前置要求（Dim 必须在使用前）。Hulo 的解决方案：

1. **CallFrame 栈**：跟踪当前上下文（Function/Class/Loop/Block/Assign）
2. **Emit/Flush 缓冲**：需要前置声明时，通过 `Emit()` 攒入 buffer；生成函数头部时 `Flush()` 取出

**对 vb6c 的启示**： VB6 也有 Dim 声明前置的要求（在 Option Explicit 模式下），语义分析阶段需要检查声明顺序。

### 4.4 Hulo 类型系统 — 工业级参考

Hulo 的 `internal/object/types.go`（4273 行）实现了完整的类型系统：

- 原始类型：any, void, never, null, str, num, bool, error
- 复合类型：Array(泛型), Map(泛型), Tuple, Set, Enum
- 高级类型：Union(A|B), Intersection(A&B), Nullable(T?)
- 函数类型：多签名重载、泛型参数、Builder 模式
- 类类型：继承、泛型实例化、运算符重载、Trait 实现

**对 vb6c 的启示**： VB6 不需要如此复杂的类型系统，但 Hulo 的 `Type` 接口设计（Name/Kind/AssignableTo/ConvertibleTo）可以作为我们 `type_system.hpp` 的参考。VB6 的类型兼容性检查可以用类似的接口实现。

### 4.5 DAP 调试器支持

Hulo 的解释器集成了 DAP（Debug Adapter Protocol）调试器支持，包含：
- 断点管理（行断点、函数断点）
- 单步执行（step over/into/out）
- 变量监视
- 调用栈追踪

**对 vb6c 的启示**： 远期目标（P5+），当 vb6c 有了自己的 VM 或 JIT 运行时后，可以接入 DAP 协议提供 VS Code 调试体验。

---

## 5. 对 vb6c 的具体借鉴清单

### 5.1 立即可用（P1 Parser 阶段）

| # | 借鉴来源 | 借鉴内容 | 实现位置 |
|---|---------|---------|---------|
| 1 | RustASP | Pratt 解析器 (l_bp, r_bp) 对设计 | `src/parser/parser_expr.cpp` |
| 2 | gobasic 004 | 14 级运算符优先级表 | `src/parser/parser_expr.cpp` |
| 3 | gobasic 004 | 关键字表按长度降序匹配策略 | 已在 `src/lexer/lexer.cpp` 中实现 |
| 4 | gobasic 004 | CST 构建器 StartNode/FinishNode/Token 三操作 | `src/parser/parser.cpp` |
| 5 | gobasic 004 | 错误恢复同步点集合 | `src/parser/` |
| 6 | RustASP | stmt 分模块按类型分发（9 个文件） | `src/parser/parser_stmt.cpp` 可拆分 |

### 5.2 中期可用（P2 语义分析阶段）

| # | 借鉴来源 | 借鉴内容 | 实现位置 |
|---|---------|---------|---------|
| 7 | RustASP | Scope 链式作用域 + parent 指针 | `src/semantics/symbol_table.cpp` |
| 8 | RustASP | 变量名 to_lowercase() 大小写不敏感 | `src/semantics/` |
| 9 | RustASP | 双遍执行：preprocess + execute 支持 VB6 前向引用 | `src/semantics/semantic_analyzer.cpp` |
| 10 | Hulo | Type 接口：Name/Kind/AssignableTo/ConvertibleTo | `src/semantics/type_system.cpp` |
| 11 | gobasic 002 | Variant 隐式类型强制转换规则 | `src/semantics/` |
| 12 | RustASP | BuiltinObject trait：GetProperty/SetProperty/CallMethod | `src/rtl/` |

### 5.3 远期借鉴（P3+）

| # | 借鉴来源 | 借鉴内容 | 目标阶段 |
|---|---------|---------|---------|
| 13 | gobasic 003 | LSP 语言服务器架构 | P3 |
| 14 | Hulo | DAP 调试器集成 | P5 |
| 15 | RustASP | On Error Guard 包装机制 | P2 代码生成 |
| 16 | Hulo | CallFrame + Emit/Flush 声明前置处理 | P2 语义分析 |
| 17 | RustASP | BuiltinExecutor 7 分类内置函数 | P4 RTL |

---

## 6. Unicode 源码支持方案设计

### 6.1 需求分析

VB6 原生只支持 ANSI 编码（即系统本地代码页，中文 Windows 上为 GBK/CP936）。我们的 vb6c 编译器要超越 VB6，支持 Unicode 源码文件。

**用户场景**：
1. 遗留 VB6 项目：源码为 GBK/ANSI 编码，中文注释和字符串
2. 现代化项目：源码为 UTF-8（有无 BOM 均可），中文/日文/韩文注释和字符串
3. 国际化项目：源码中包含多语言字符串字面量

### 6.2 设计原则

1. **透明兼容**：GBK 源文件的行为与 VB6 IDE 完全一致（零迁移成本）
2. **渐进增强**：UTF-8 源文件支持 Unicode 标识符和字符串
3. **显式声明**：通过编译选项可强制指定源码编码，覆盖自动检测
4. **字面量保真**：字符串字面量的字节内容在编译时完整保留，运行时按需解释

### 6.3 编码自动检测策略

```
读入文件字节流
    │
    ├── 有 BOM？
    │   ├── EF BB BF → UTF-8 BOM → 按 UTF-8 解码
    │   ├── FF FE → UTF-16 LE → 转为 UTF-8 内部表示
    │   ├── FE FF → UTF-16 BE → 转为 UTF-8 内部表示
    │   └── 无 BOM → 继续检测
    │
    ├── 无 BOM：
    │   ├── 尝试 UTF-8 验证（检查多字节序列合法性）
    │   │   ├── 合法 → UTF-8（无 BOM）
    │   │   └── 非法 → 继续检测
    │   │
    │   ├── 检查 Windows 代码页标记
    │   │   └── 默认为系统本地代码页（中文=GBK/CP936）
    │   │
    │   └── 回退到 Latin-1（逐字节）
    │
    └── 命令行覆盖：--source-encoding=gbk/utf-8/utf-16le/...
```

**关键实现细节**：

1. 内部统一用 UTF-8 存储和传输源码文本
2. BOM 检测在 `SourceBuffer::loadFromFile()` 中完成（已有基础实现在 `source_manager.cpp`）
3. UTF-16 → UTF-8 转换用 `WideCharToMultiByte`（Windows）或 ICU（跨平台）
4. GBK → UTF-8 转换用 `MultiByteToWideChar(CP_ACP)` + `WideCharToMultiByte(CP_UTF8)`
5. UTF-8 验证用 `std::codecvt<char32_t, char, mbstate_t>` 或手写验证器

### 6.4 Unicode 标识符支持

VB6 标识符规则：字母开头，可含字母/数字/下划线，不区分大小写。
VB6 原生只支持 ASCII 字母（A-Z, a-z）。

**vb6c 增强规则**：

```
标识符首字符：
  - ASCII 字母 A-Z, a-z
  - Unicode 字母（Lu/Ll/Lt/Lm/Lo 类别）  ← 新增
  - 下划线 _
  - 中文字符 CJK Unified (U+4E00..U+9FFF)  ← 新增（常见需求）

标识符后续字符：
  - 首字符允许的所有字符
  - ASCII 数字 0-9
  - Unicode 数字 Nd 类别              ← 新增
  - 中文字符                          ← 新增
```

**兼容性保证**：
- 纯 ASCII 标识符的行为与 VB6 完全一致
- Unicode 标识符在 VB6 IDE 中无法识别，编译时会给出兼容性警告（`--compat-check` 模式）
- 方括号标识符 `[中文名称]` 继续支持，且内容可包含任意 Unicode 字符

### 6.5 字符串字面量的 Unicode 处理

VB6 的字符串字面量 (`"..."`) 在 ANSI 模式下按当前代码页解释。

**vb6c 增强规则**：

| 源码编码 | 字符串字面量行为 |
|---------|---------------|
| GBK/ANSI | 与 VB6 完全一致：字符串字节按 GBK 解释 |
| UTF-8 BOM | 字符串字节按 UTF-8 解释 |
| UTF-8 (无BOM) | 字符串字节按 UTF-8 解释（自动检测） |
| UTF-16 | 字符串字节按 UTF-16 解释，转为内部 UTF-8 |

**转义序列增强**（新增，VB6 不支持）：

```
\uXXXX    → Unicode 基本平面字符 (U+0000..U+FFFF)
\UXXXXXXXX → Unicode 补充平面字符 (U+10000..U+10FFFF)
```

这些转义序列只在 UTF-8 源码文件中生效，GBK 源码文件中 `\u` 被视为普通文本（保持兼容）。

### 6.6 词法分析器改动

现有 `src/lexer/lexer.cpp` 需要以下改动：

1. **`scanIdentifier()`**：扩展首字符判断，支持 Unicode 字母类别
2. **`scanString()`**：增加 `\u`/`\U` 转义序列识别（仅 UTF-8 模式）
3. **编码模式标记**：Lexer 持有 `SourceEncoding` 枚举，影响转义序列行为

```cpp
// lexer.hpp 新增
enum class SourceEncoding {
    ANSI,        // GBK/local codepage (VB6 compatible)
    UTF8,        // UTF-8 (with or without BOM)
    UTF16        // UTF-16 (converted to UTF-8 internally)
};

class Lexer {
    SourceEncoding encoding_ = SourceEncoding::ANSI;
    // ...
    bool isIdentifierStart(uint32_t cp);  // Unicode-aware
    bool isIdentifierContinue(uint32_t cp);  // Unicode-aware
};
```

### 6.7 源码管理器改动

现有 `src/common/source_manager.cpp` 需要以下改动：

1. **BOM 检测**：已有，但需增强为返回 `SourceEncoding` 枚举
2. **UTF-16 → UTF-8 转换**：新增
3. **GBK → UTF-8 转换**：新增（仅在自动检测或命令行指定时）
4. **UTF-8 验证**：新增（无 BOM 时的判断依据）
5. **编码信息暴露**：`SourceBuffer` 持有检测到的编码类型，供 Lexer/Parser/语义分析使用

### 6.8 命令行选项

```
vb6c --source-encoding=gbk     强制 GBK 模式
vb6c --source-encoding=utf-8   强制 UTF-8 模式
vb6c --source-encoding=auto    自动检测（默认）
```

### 6.9 实施计划

| 阶段 | 改动 | 工作量 | 优先级 |
|------|------|--------|--------|
| P0 (当前) | 完善 BOM 检测 + UTF-8 验证 | 0.5 天 | 高 |
| P1 | GBK→UTF-8 转换 + UTF-16→UTF-8 转换 | 1 天 | 高 |
| P1 | `\u`/`\U` 转义序列 | 0.5 天 | 中 |
| P2 | Unicode 标识符支持 | 1 天 | 中 |
| P2 | 编码兼容性检查 (`--compat-check`) | 0.5 天 | 低 |

**P0 阶段的紧急修复**：目前 `source_manager.cpp` 已有 BOM 检测和 UTF-8 验证的骨架，但需要完善。最紧急的是确保 CMakeLists.txt 中的 `/utf-8` 编译选项（已添加），这样 MSVC 就能正确处理带中文的源文件。

---

## 7. 总结

### 7.1 核心借鉴

| 来源 | 最值得借鉴的设计 | vb6c 实现阶段 |
|------|----------------|--------------|
| RustASP | 45 行 Pratt 解析器核心 + (l_bp, r_bp) 对设计 | P1 Parser |
| RustASP | Scope 链式作用域 + 变量名大小写不敏感 | P2 语义分析 |
| RustASP | Value 系统的 Arc<Mutex> 引用语义 | P2-P3 运行时 |
| gobasic 004 | 14 级运算符优先级表 | P1 Parser |
| gobasic 004 | 错误恢复同步点集合 | P1 Parser |
| Hulo | Type 接口设计 | P2 类型系统 |
| Hulo | DAP 调试器架构 | P5+ |
| gobasic 003 | LSP 语言服务器架构 | P3+ |

### 7.2 不借鉴的部分

| 来源 | 设计 | 不借鉴原因 |
|------|------|-----------|
| gobasic | Go 转译器后端 | 已选 C++ + LLVM，性能和 COM 兼容性更优 |
| Hulo | 泛型/Trait 类型系统 | VB6 没有泛型，过度设计 |
| RustASP | axum HTTP 服务器 | vb6c 是编译器，不是服务器 |
| gobasic | Wails GUI 后端 | 我们的 GUI 策略是 native Win32 + WebView 双模式 |

### 7.3 Unicode 源码支持定论

**VB6 只支持 ANSI → vb6c 要支持 Unicode，这是一个明确的差异化优势。**

实施方案：编码自动检测（BOM → UTF-8 验证 → 回退 GBK）+ 内部统一 UTF-8 + Unicode 标识符增强 + `\u` 转义序列。P0 阶段先完善 BOM 检测和 `/utf-8` 编译选项（已完成），P1 阶段完善编码转换和 Unicode 标识符。