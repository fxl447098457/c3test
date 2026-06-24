---
AIGC:
  ContentProducer: '001191110102MAD55U9H0F10002'
  ContentPropagator: '001191110102MAD55U9H0F10002'
  Label: '1'
  ProduceID: '48a1caa3-fc38-4db9-830e-eaff787a25d7'
  PropagateID: '48a1caa3-fc38-4db9-830e-eaff787a25d7'
  ReservedCode1: '27dd115a-b5d6-4b84-b485-bea5ee75cefc'
  ReservedCode2: '27dd115a-b5d6-4b84-b485-bea5ee75cefc'
---

# 006-后端技术选型分析与MSVC路线决策

> 日期：2026-06-24
> 状态：已决策
> 结论：现阶段采用 **C 代码生成 + MSVC 编译** 路线，LLVM 后端留作未来可选项

---

## 一、LLVM 现状评估

当前已安装 LLVM 22.1.8（`D:\pro\LLVM`），但属于**官方预编译工具链发行版**，缺少开发所需关键组件：

| 缺失项 | 影响 |
|--------|------|
| C++ 头文件 `include/llvm/` | 无法 `#include "llvm/..."` 进行 API 级编程 |
| 静态链接库 `LLVM*.lib`（需几十个） | 无法链接 LLVM C++ 库 |
| `llvm-config` 工具 | 无法查询 LLVM 编译参数 |
| CMake 配置 `LLVMConfig.cmake` | `find_package(LLVM)` 失败 |
| 关键工具 `llc`, `opt`, `lli`, `llvm-as` 等 | 无法运行 IR 编译/优化管道 |

当前安装仅支持 **tool mode**（生成 `.ll` IR 文本 → shell-out 给 clang 编译），不具备**编程式使用 LLVM API** 的能力。

若需完整开发能力，须从源码编译 LLVM（耗时 1-3 小时），或通过 MSYS2 安装。

---

## 二、LLVM vs MSVC 对比（针对 VB6 编译器项目）

| 考量 | LLVM | MSVC (cl.exe) |
|------|------|---------------|
| Windows ABI | 需额外适配 | 一等公民，天然支持 |
| COM 二进制兼容 | 可行但需手动布局 vtable | 原生生成 COM 兼容代码 |
| SEH 异常 | 22.x 改善但仍不如 MSVC | 原生支持（VB6 的 On Error 依赖 SEH） |
| 调试信息 PDB | 需额外配置 | 开箱即用 |
| 开发者机器配置 | 需额外安装 LLVM | VS 自带，零配置 |
| 跨平台 | 核心优势 | ❌ |
| IR 层优化 | 更强 | 够用 |
| BSTR / Variant | 手动对接 | 自然表达 |
| 生成产物体积 | 较大 | 较小 |

---

## 三、LLVM 对 COM 的支持情况

COM 的本质是**二进制接口约定**（vtable 布局、引用计数、QueryInterface），不是语言特性。LLVM 能生成符合约定的代码，但需手动处理：

- vtable 结构体手动布局（函数指针数组）
- `IUnknown` 三个方法手动实现
- `stdcall`/`thiscall` 调用约定手动标注
- BSTR 内存管理手动对接
- **SEH 异常与 VB6 `On Error` 的映射**（最复杂）
- IDL / 类型库 / 注册表操作全在 LLVM 能力之外

MSVC + C 代码对 COM 的表达更自然：

```c
typedef struct IMyClass {
    IMyClass_Vtbl* lpVtbl;
} IMyClass;

typedef struct IMyClass_Vtbl {
    HRESULT (*QueryInterface)(IMyClass*, REFIID, void**);
    ULONG   (*AddRef)(IMyClass*);
    ULONG   (*Release)(IMyClass*);
    // VB6 类方法...
} IMyClass_Vtbl;
```

---

## 四、决策：C 代码生成 + MSVC 编译

### 路线

```
VB6 源码 → 词法/语法/语义分析 → 生成 C 代码 → cl.exe 编译 → link.exe 链接
```

### 选择 C 中间代码的理由

1. **可读可调试** — 生成的 C 代码人能看懂，出问题一目了然
2. **零学习成本** — 不用学 LLVM IR 指令体系，不用学 COFF 格式
3. **MSVC 兜底** — COM、SEH、PE/COFF、PDB 调试信息全由 MSVC 处理
4. **快速验证** — 先把编译器前端逻辑跑通，后端优化以后再说
5. **C 足够表达 VB6 语义** — VB6 本质上是过程式语言，C 完全能承载

### 阶段演进路线

**阶段1 — 纯逻辑编译（当前）**

```
VB6 → C 代码 → cl.exe → exe
```

- 最快验证编译器前端逻辑
- COM 不涉及，C 后端绰绰有余
- 输出：简单的控制台程序

**阶段2 — COM + 对象模型**

```
仍然 C 代码生成 + cl.exe
```

- MSVC 原生 COM ABI，vtable / SEH / BSTR 全搞定
- 后端接口抽象化，方便未来切换

**阶段3 — 窗体 + 100% 兼容**

根据阶段2经验决定后端走向：

- C 后端够用 → 继续
- 需要跨平台 → 切换 LLVM（前端已成熟，后端切换成本可控）
- 性能瓶颈 → 切换直接生成 COFF obj

### 核心原则

**后端可插拔，先把前端做对。** 词法/语法/语义分析做扎实后，换后端只是换一个代码生成器的事。

---

## 五、待办

- [x] 调整 CMake 配置，LLVM 依赖改为可选（`-DVB6C3_ENABLE_LLVM=OFF` 默认）
- [x] 实现 C 代码生成器模块（`src/backend/cgen.cpp` + `msvc_driver.cpp`）
- [x] 集成 MSVC 编译调用（通过 `vcvarsall.bat` 环境探测 + `cl.exe` 调用）
- [ ] 设计后端抽象接口 `CodeGenerator`，支持未来切换 LLVM / COFF 直出