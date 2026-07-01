# 015 - VB6编译到WASM与浏览器运行可行性分析

> 日期: 2026-07-01
> 触发: PureBasic Forums PBScript VM interpreter 帖子分析
> 参考: https://www.purebasic.fr/english/viewtopic.php?t=88466
> 前提约束: 不使用COM、不使用浏览器限制的对象；文件系统可利用浏览器新API
> 架构决策: **C3多后端共存（MSVC + GCC + LLVM），用户可选，非替代关系**

---

## 零、多后端共存架构设计

### 0.1 设计原则

C3的后端不是"切换"，而是**共存可选**。用户通过命令行 `--backend` 选择：

```
c3.exe --backend=msvc   myapp.vbp    # 当前默认，C代码→MSVC
c3.exe --backend=gcc    myapp.vbp    # C代码→GCC/MinGW
c3.exe --backend=llvm   myapp.vbp    # LLVM IR→原生优化
c3.exe --backend=wasm   myapp.vbp    # LLVM IR→WASM（浏览器目标）
c3.exe --backend=c      myapp.vbp    # 仅输出C代码（不驱动编译）
```

### 0.2 统一IR → 多后端架构

```
VB6源码
  │
  ▼
┌──────────────────────────┐
│  C3 前端（共享，不变）     │
│  词法 → 语法 → 语义 → IR  │
└──────────┬───────────────┘
           │
     ┌─────┼──────┬──────────┬────────────┐
     ▼     ▼      ▼          ▼            ▼
  ┌─────┐┌────┐┌──────┐┌────────┐┌──────────┐
  │MSVC ││GCC ││LLVM  ││WASM    ││C-Output  │
  │后端 ││后端││Native││(LLVM) ││(纯C导出) │
  └─────┘└────┘└──────┘└────────┘└──────────┘
     │     │      │        │          │
     ▼     ▼      ▼        ▼          ▼
   .exe  .exe  .exe/.dll  .wasm    .c/.h
```

### 0.3 各后端职责与差异

| 后端 | 代码生成 | 驱动/链接 | 输出 | 适用场景 |
|------|---------|----------|------|---------|
| **MSVC** | C代码 | cl.exe + link.exe | .exe/.dll | Windows原生，当前默认 |
| **GCC** | C代码 | gcc/g++ + ld | .exe/.dll | 跨平台（MinGW/Cygwin/交叉编译） |
| **LLVM Native** | LLVM IR | llc + linker | .exe/.dll | 最大优化，未来默认 |
| **LLVM WASM** | LLVM IR | llc(wasm32) | .wasm | 浏览器运行、Serverless |
| **C-Output** | C代码 | 无（仅输出） | .c/.h | 审查、移植、第三方编译 |

### 0.4 后端接口抽象

```cpp
// src/backend/ibackend.h — 统一后端接口
class IBackend {
public:
    virtual ~IBackend() = default;
    virtual std::string name() const = 0;          // "msvc", "gcc", "llvm", "wasm", "c"
    virtual bool supportsCOM() const = 0;          // COM能力声明
    virtual bool supportsGUI() const = 0;          // 窗体能力声明
    virtual bool supportsDeclareLib() const = 0;   // Declare Lib能力
    virtual bool emit(IRModule& ir, const EmitOptions& opts) = 0;
    virtual bool link(const LinkOptions& opts) = 0;
};

// 后端工厂
std::unique_ptr<IBackend> CreateBackend(const std::string& name);
```

### 0.5 各后端能力矩阵

| 能力 | MSVC | GCC | LLVM Native | LLVM WASM | C-Output |
|------|------|-----|-------------|-----------|----------|
| COM/ActiveX | YES | NO | YES(Win) | NO | NO |
| 窗体(Win32) | YES | WARN(Win) | YES(Win) | NO->DOM | NO |
| Declare Lib | YES | YES(Win) | YES(Win) | NO->WASI | NO |
| 文件I/O | YES | YES | YES | YES(WASI+OPFS) | YES(生成代码) |
| 优化级别 | /O2 | -O2 | -O3/Aggressive | -Os(体积优先) | N/A |
| 交叉编译 | NO | YES | YES | YES(天然跨平台) | YES |
| 调试信息 | PDB | DWARF | DWARF | DWARF+WASM DWARF | 源码映射 |

### 0.6 实现优先级

```
P0: MSVC后端（已完成，74/74测试零失败）
P1: C-Output后端（最简单，去掉驱动调用即可，1-2周）
P2: GCC后端（复用C代码生成，换驱动，2-4周）
P3: LLVM Native后端（新增LLVM IR生成，3-6个月）
P4: LLVM WASM后端（基于P3，增加wasm32 target，叠加2-4周）
```

---

## 一、PBScript VM Interpreter 技术要点提取

### 1.1 项目概况

PBScript 是 PureBasic 社区开发者 idle 创建的**字节码VM解释器**，当前版本 v0.43a。

核心定位：**嵌入式脚本引擎**——在宿主PureBasic应用中运行时编译/执行PureBasic脚本，类似 Lua 之于 C/C++。

### 1.2 关键架构特征

| 特征 | 说明 |
|------|------|
| 编译管线 | 源码 → 字节码 → VM执行（编译/运行两阶段分离） |
| 多后端 | x86/x64 ASM + C 后端，兼容 PB 6.03→6.40 |
| JS VM | **pbscript-vm.js**：将整个 DOM（11000+函数）映射为 PB 接口，可在浏览器运行 |
| 对象系统 | 线程安全，Maps（string/numeric键）、前缀Trie、双向链表 |
| 内存管理 | GC垃圾回收 + 沙箱模式（限制内存/周期/边界检查写操作） |
| FFI | 双向机器码FFI Thunks：脚本↔宿主互相调用 |
| 语法覆盖 | 大子集PB语法：变量/类型/结构体/过程/数组/枚举/接口/Select/指针 |

### 1.3 JS VM 的实现路径（最关键的参考）

PBScript 的 Web 运行方案：

1. **编译器生成字节码**（与原生路径共用）
2. **pbscript-vm.js** 在浏览器中解释执行字节码
3. DOM 全部 11000+ 函数映射为 PB 风格接口
4. 附带 Server（PBScript_server.pb），提供 Web 服务 + 自动补全

**关键启发**：PBScript 并未将 PureBasic 编译为 WASM，而是选择了 **JS VM 解释器** 路径。这是一个重要的架构决策参考。

### 1.4 性能数据

| 基准 | PBScript VM | PB原生 |
|------|-------------|--------|
| fib(20) | ~60ms | ~0-2ms |
| fib(25) | ~51ms | ~1ms |

VM比原生慢约30-50倍，但对于嵌入式脚本场景可接受。

---

## 二、C3 对接 LLVM 后端 → WASM 的可行性分析

### 2.1 当前 C3 架构回顾

```
VB6源码 → [词法/语法/语义] → IR → [C代码生成] → .c文件 → MSVC → .exe
```

多后端架构扩展后：

```
VB6源码 → [词法/语法/语义] → IR → 目标选择
                                    ├→ [C代码生成] → MSVC → .exe          (--backend=msvc)
                                    ├→ [C代码生成] → GCC  → .exe          (--backend=gcc)
                                    ├→ [C代码生成] → 仅输出.c             (--backend=c)
                                    ├→ [LLVM IR生成] → llc → native .exe  (--backend=llvm)
                                    └→ [LLVM IR生成] → llc(wasm32) → .wasm (--backend=wasm)
```

### 2.2 LLVM → WASM 的技术成熟度

LLVM 的 WASM 后端（`wasm32`）已经**生产级成熟**：

- **LLVM 18+**（2024年至今）：WASM 后端稳定，支持 WASI、异常处理、多返回值
- **WASM 3.0 + 组件模型**：2026年正在普及，WASI 0.3.0 预计2026年正式发布
- **Emscripten**：成熟的 C/C++ → WASM 工具链，内部使用 LLVM wasm 后端
- **Clang 直接输出**：`clang --target=wasm32-unknown-wasi` 可直接编译 C → WASM

**结论**：LLVM→WASM 技术链完全成熟，不是瓶颈。

### 2.3 VB6→WASM 的关键挑战

#### 挑战1：RTL运行时库的WASM移植

C3 当前 RTL（src/rtl/core/）包含：

| RTL模块 | WASM兼容性 | 需要的处理 |
|---------|-----------|-----------|
| BSTR/VARIANT/SAFEARRAY | 可 | 纯数据结构，直接可用 |
| 字符串函数 | 可 | 纯计算，直接可用 |
| 数学/日期/类型转换 | 可 | 纯计算，直接可用 |
| 文件I/O | 需适配 | 需要WASI或浏览器FS API适配层 |
| COM运行时 | 不兼容 | 前提已排除COM，不移植 |
| 窗体运行时（Win32） | 不兼容 | 需要DOM/Canvas替代方案 |
| 错误处理（On Error） | 需适配 | WASM异常处理已支持（Chrome 128+） |

#### 挑战2：窗体/GUI系统

VB6 的核心价值之一是窗体。在浏览器中有三种路径：

| 方案 | 说明 | 优劣 |
|------|------|------|
| A. DOM映射 | 像PBScript那样，将VB6控件映射到HTML元素 | 生态最好，但映射工作量巨大 |
| B. Canvas渲染 | WASM直接操作Canvas像素 | 性能好，但需要重写整个控件库 |
| C. WebView2嵌入 | 不适用于浏览器内 | 排除 |

**推荐方案A**：DOM映射，参照PBScript的11000+ DOM函数映射经验。

#### 挑战3：Win32 API 依赖

VB6代码中常见的 Win32 API 调用（`Declare Lib`）在 WASM 中不可用。需要：

- **沙箱化声明**：编译期识别 `Declare Lib` 调用，WASM目标下报错或提供替代
- **浏览器API桥接**：将常见 Win32 调用映射到 Web API（如 `Sleep` → `setTimeout`）
- **WASI替代**：文件/进程类 API 通过 WASI 标准接口提供

### 2.4 可行的VB6→WASM编译路径

```
路径1：C3 → LLVM IR → WASM（推荐，对应 --backend=wasm）
  VB6 → C3前端 → IR → LLVM IR生成 → wasm32 → .wasm
  优势：利用LLVM全链优化，输出质量高
  劣势：需要新增LLVM IR生成后端，工作量最大

路径2：C3 → C代码 → Emscripten → WASM
  VB6 → C3前端 → IR → C代码生成 → emcc → .wasm + .js
  优势：复用现有C后端，Emscripten处理OS抽象
  劣势：多一层转换，优化不如直接LLVM IR

路径3：C3 → 字节码 → JS VM（PBScript模式）
  VB6 → C3前端 → 字节码 → JS/WASM VM解释器
  优势：最快实现，灵活
  劣势：性能最差（30-50x），但脚本场景够用
```

---

## 三、C3 自身编译为 WASM 的可行性

### 3.1 场景：浏览器内运行 C3 编译器

将 c3.exe 编译为 WASM，在浏览器中提供"在线VB6编译"服务。

```
浏览器中:
  C3.wasm + VB6源码 → 编译 → WASM产物（或C代码）
```

### 3.2 C3 编译器本身的WASM兼容性

C3 是 C++17 编写，使用 CMake/Ninja/Clang 构建。关键依赖：

| 依赖 | WASM兼容性 | 处理方案 |
|------|-----------|---------|
| C++17 标准库 | 可 | Emscripten完整支持 |
| STL（vector/map/string等） | 可 | 直接可用 |
| 文件I/O（读.vbp/.frm等） | 需适配 | 通过WASI或File System Access API |
| MSVC驱动（调用cl.exe/link.exe） | 不兼容 | 浏览器内不可能，需改为直接LLVM IR/WASM输出 |
| 系统调用（Process/Env） | 不兼容 | 需要WASI或浏览器替代 |
| CMake（构建系统） | 不兼容 | C3.wasm本身不需要CMake，直接编译产物 |

### 3.3 核心障碍：MSVC驱动

当前 C3 的编译管线是 **C代码生成 + MSVC驱动**（006决策），即：

```
C3生成.c → 调用cl.exe编译 → 调用link.exe链接 → .exe
```

在浏览器中不可能调用 cl.exe/link.exe。**必须选择其他后端**：

- **LLVM IR后端**：C3直接生成LLVM IR，由LLVM wasm后端输出（需要LLVM.wasm）
- **直接WASM后端**：C3直接生成WASM字节码（工作量大，但最独立）
- **WASI + Emscripten**：仍生成C代码，但由Emscripten（也编译为WASM）驱动编译

注意：在多后端共存架构下，C3.wasm在浏览器中默认使用 `--backend=wasm` 或 `--backend=c`，不会尝试调用MSVC。

### 3.4 "C3编译为WASM"的三层含义

| 层次 | 说明 | 可行性 |
|------|------|--------|
| **第一层：C3.wasm + --backend=c** | C3在浏览器运行，输出C代码供下载 | 去掉MSVC驱动即可，**最容易实现** |
| **第二层：C3.wasm + --backend=wasm** | C3在浏览器运行，输出也是WASM | 需要LLVM后端，中等工作量 |
| **第三层：C3.wasm解释执行VB6** | C3在浏览器运行，在线解释VB6代码 | 类似PBScript的JS VM路径 |

**推荐从第一层起步**：C3.wasm 只做前端编译（词法/语法/语义/C代码生成），输出C代码。用户拿C代码在本地用任何编译器编译。这完全不需要LLVM后端，也无需移植MSVC。

---

## 四、浏览器文件系统API分析

### 4.1 两套API对比

| API | File System Access API | Origin Private File System (OPFS) |
|-----|----------------------|----------------------------------|
| 标准 | W3C File System Access API | W3C OPFS（File System Access子集） |
| 浏览器支持 | Chrome 86+, Edge 86+ | Chrome 86+, Edge 86+ |
| 用户授权 | **需要用户交互授权**（showDirectoryPicker） | **无需授权**，自动分配沙箱目录 |
| 可见性 | 可访问用户真实文件系统 | 仅对当前origin可见，不污染用户文件系统 |
| 性能 | 一般（走IPC） | **高性能**（同步访问句柄，接近原生） |
| 容量 | 取决于用户磁盘 | GB级 |
| 适用场景 | 编辑器类应用（VS Code Web） | 高性能存储、数据库、WASM后端存储 |

### 4.2 对VB6文件I/O的映射方案

VB6的文件操作（Open/Close/Input/Print#/Write#/Get/Put）需要映射到浏览器API：

```
VB6文件操作映射:

方案A：OPFS + 同步访问句柄（推荐）
  - 优势：性能最接近原生，无需用户授权
  - OPFS支持 createSyncAccessHandle()，可像真实文件系统一样读写
  - WASM可通过WASI的 fd_read/fd_write 直接对接OPFS
  - 限制：仅在origin沙箱内，用户看不到文件

方案B：File System Access API
  - 优势：可访问用户真实文件系统
  - 限制：每次需用户授权，操作异步，性能较差
  - 适合：IDE场景（打开/保存.vbp/.frm文件）

方案C：混合方案
  - OPFS作为"工作目录"（编译中间文件、临时文件）
  - File System Access API作为"导入/导出"（用户选择文件读写）
  - 最符合VB6开发体验
```

### 4.3 WASI + OPFS 集成

WASI（WebAssembly System Interface）定义了标准化的系统调用：

```
WASI fd_read/fd_write → OPFS SyncAccessHandle.read/write
WASI path_open → OPFS directory handle
WASI fd_close → OPFS handle close
```

Wasmer、Wasmtime 等运行时已有 OPFS 适配层。Emscripten 也支持 `--preload-file` 将虚拟文件系统嵌入 WASM。

---

## 五、综合方案推荐

### 5.1 分阶段路线

```
Phase 1（最快出成果）：C3.wasm + --backend=c
  ┌─────────────────────────────────────────┐
  │ 浏览器: C3.wasm(VB6源码) → C代码        │
  │ 用户下载C代码，本地用MSVC/GCC/Clang编译   │
  └─────────────────────────────────────────┘
  工作量：去掉MSVC驱动调用，C3核心编译逻辑编译为WASM
  预期：2-3周

Phase 2（本地LLVM后端）：C3 + --backend=llvm
  ┌─────────────────────────────────────────┐
  │ 本地: C3 → LLVM IR → llc → native .exe │
  │ 不依赖MSVC，优化能力更强                 │
  └─────────────────────────────────────────┘
  工作量：新增LLVM IR生成后端
  预期：3-6个月

Phase 3（WASM目标后端）：C3 + --backend=wasm
  ┌─────────────────────────────────────────┐
  │ 本地/浏览器: C3 → LLVM IR → wasm32 → .wasm │
  │ VB6应用直接编译为WASM                    │
  └─────────────────────────────────────────┘
  工作量：基于Phase 2，增加wasm32 target
  预期：叠加2-4周

Phase 4（VB6应用浏览器运行）：VB6→WASM+DOM运行时
  ┌─────────────────────────────────────────┐
  │ 浏览器: VB6.wasm + DOM运行时.js        │
  │ VB6窗体 → DOM元素映射                   │
  │ VB6控件 → HTML/Canvas组件               │
  │ 文件I/O → OPFS/WASI                    │
  └─────────────────────────────────────────┘
  工作量：DOM映射层 + 控件库 + OPFS适配
  预期：6-12个月
  参照：PBScript的11000+ DOM映射经验

Phase 5（在线IDE）：C3.wasm + Monaco + DOM运行时
  ┌─────────────────────────────────────────┐
  │ 浏览器内完整VB6开发环境                  │
  │ Monaco编辑器 + C3.wasm编译器 + DOM运行时 │
  │ 类似VS Code Web版                       │
  └─────────────────────────────────────────┘
  工作量：基于Phase 3+4，增加IDE集成
  预期：叠加3-6个月
```

### 5.2 技术风险矩阵

| 风险 | 概率 | 影响 | 缓解 |
|------|------|------|------|
| LLVM.wasm体积过大 | 高 | 加载慢 | 按需加载/裁剪LLVM/改用直接WASM后端 |
| DOM映射工作量巨大 | 高 | 开发周期长 | 先支持核心控件，按需扩展 |
| VB6 COM依赖无法排除 | 中 | 功能受限 | 编译期检测+警告，提供WASM兼容模式 |
| WASI版本碎片化 | 低 | 兼容性问题 | 固定WASI 0.2+，主流运行时均支持 |
| OPFS浏览器支持 | 低 | 仅Chrome/Edge | Firefox/Safari逐步跟进；可降级到IndexedDB |
| 性能不达标 | 中 | 用户体验差 | WASM本身接近原生；瓶颈在DOM交互 |

### 5.3 PBScript 经验对 C3 的启示

| PBScript经验 | C3可借鉴 |
|-------------|---------|
| JS VM解释器路径可行 | Phase 1可以先做JS VM解释器，最快出成果 |
| DOM映射11000+函数 | VB6控件→DOM映射可复用此思路，但VB6控件更复杂 |
| 字节码两阶段（编译+运行） | C3已有IR，可直接复用 |
| 沙箱模式（限制内存/周期/边界检查） | WASM天然沙箱，但VB6的On Error需要额外处理 |
| FFI双向调用 | WASM的import/export天然支持 |
| 性能30-50x差距 | 直接编译为WASM可消除此差距，这是C3的优势 |

---

## 六、最终结论

### Q1: VB6工程能否编译到WASM在网页运行？

**能，但有条件。**

- 不使用COM、不使用浏览器限制的对象 → 大幅降低难度
- 纯计算/算法类VB6代码 → 几乎无障碍，直接LLVM→WASM
- 窗体类VB6应用 → 需要DOM映射层，工作量大但可行
- 文件I/O → OPFS完全可替代，且性能接近原生

### Q2: C3能否编译为WASM，在浏览器编译VB6？

**能，且比PBScript方案更有优势。**

- C3是AOT编译器，输出的是优化过的WASM，而非字节码解释执行
- Phase 1（C3.wasm + --backend=c 输出C代码）最容易实现，2-3周
- Phase 3（C3.wasm + --backend=wasm 输出WASM）是终极方案
- 如果嫌LLVM.wasm太大，可以自写轻量WASM后端（输出wasm字节码）

### Q3: 浏览器File System Access API能否替代VB6文件操作？

**能，且OPFS是最佳选择。**

- OPFS：高性能、无需授权、GB级容量、支持同步访问句柄
- WASI标准接口可无缝对接OPFS
- 混合方案（OPFS工作目录 + File System Access API导入导出）最实用

### Q4: 多后端共存如何实现？

**统一IR + 后端工厂 + --backend参数选择。**

- MSVC/GCC/LLVM/WASM/C-Output 共存，非替代
- 前端（词法/语法/语义/IR）完全共享
- 后端通过 IBackend 接口抽象，工厂方法创建
- 能力矩阵声明各后端支持范围，编译期检查不兼容特性

### 推荐起步路径

**先做P1（C-Output后端）和P2（GCC后端）**：复用现有C代码生成，工作量最小。

**再做Phase 1（C3.wasm + --backend=c）**：将C3编译为WASM，浏览器中在线编译VB6输出C代码。验证整个技术栈的最小可行方案。

**然后推进P3（LLVM Native后端）→ P4（WASM后端）**：打通VB6→LLVM IR→WASM全链路。
