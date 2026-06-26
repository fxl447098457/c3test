# 014 - 双版本发布策略与 MinGW-w64 COM 兼容性分析

> 状态：讨论中
> 创建：2026-06-26
> 关联决策：006-后端路线（C代码生成+MSVC驱动）、008-库优先架构

---

## 1. 背景

c3.exe 当前架构为 **VB6 → C代码生成 → MSVC(cl.exe)编译链接**，用户端必须安装 Visual Studio Build Tools（1-5GB），对 VB6 老用户群体不友好。

决定提供 **两个发行版**：

| 版本 | 编译器后端 | 目标用户 |
|------|-----------|----------|
| MSVC 版 | cl.exe | 已有 VS 环境的开发者 |
| MinGW 版 | gcc (MinGW-w64) | 开箱即用，解压即用 |

---

## 2. MinGW-w64 对 COM 的兼容性分析

### 2.1 COM 客户端（完全支持）

c3 当前 P6 已实现的 COM 客户端功能，MinGW-w64 **零风险**：

| 能力 | MinGW 支持 | 说明 |
|------|-----------|------|
| `CoInitialize` / `CoCreateInstance` | ✅ | 链接 `-lole32 -loleaut32` |
| `IDispatch` 后期绑定（GetIDsOfNames / Invoke） | ✅ | 头文件和导入库完整 |
| `VARIANT` / `BSTR` 类型 | ✅ | `oleauto.h` 定义完整 |
| GUID / IID 结构体布局 | ✅ | Win64 调用约定统一，vtable 二进制兼容 |

**核心原理**：COM 是 vtable + 结构体布局的约定，Win64 下 MSVC 和 MinGW-w64 共享 Microsoft x64 calling convention，二进制层面互通。

### 2.2 COM 服务端 / ActiveX DLL（基本可行，需额外处理）

| 问题 | 严重度 | 解法 |
|------|--------|------|
| 无 MIDL 编译器，不能编译 .idl 生成代理/存根 | 低 | c3 手写 C 代码实现 vtable，不依赖 MIDL |
| TypeLib (.tlb) 创建 | 中 | 见第3节详细方案 |
| `__declspec(uuid)` 扩展 | 无 | c3 生成 C 代码手动定义 GUID 常量 |
| ATL/MFC 不存在 | 无 | c3 自己生成裸 COM 代码，不用 ATL |
| DLL 导出（DllGetClassObject 等） | 低 | MinGW 用 `__declspec(dllexport)` 或 `.def` 文件 |

### 2.3 c3 的天然优势

c3 的 COM 代码全部是 **手写 C vtable 实现**，不依赖 ATL/MFC/编译器扩展，天然与编译器解耦。这是相比其他项目最大的优势——不存在被 MinGW 卡死的场景。

---

## 3. TypeLib 嵌入方案

### 3.1 VB6 的做法（参考标准）

VB6 编译 ActiveX DLL/OCX 时，将 TypeLib 作为 **PE 资源** 嵌入 DLL 内部：

- 资源类型：`RTYPELIB`（资源 ID 1）
- `DllRegisterServer` 内部调用 `LoadTypeLib(self_path)` → Windows 自动从 PE 资源段提取
- 然后 `RegisterTypeLib` 完成注册
- **不需要单独的 `.tlb` 文件**，`regsvr32 mylib.dll` 一步搞定

```
VB6 DLL 内部结构：
├── PE Header
├── .text    (代码段)
├── .data    (数据段)
├── .rsrc    (资源段)
│   ├── TYPELIB (ID=1)  ← TypeLib 嵌在这里
│   └── VERSION_INFO
└── .reloc
```

### 3.2 c3 MinGW 版的实现路径

MinGW-w64 完全支持 PE 资源嵌入，流程如下：

```
c3 编译期                    链接期                      运行时
─────────                   ──────                     ──────
生成 .tlb 文件                windres 编译 .rc → .res     LoadTypeLib(self)
      ↓                           ↓                         ↓
写 .rc 资源脚本            ld 链接 .res 进 DLL         从 PE 资源自动提取
1 TYPELIB "mylib.tlb"           ↓                    RegisterTypeLib
                           DLL 内包含 TYPELIB 资源
```

具体代码模式：

```c
// DllRegisterServer 实现
STDAPI DllRegisterServer() {
    WCHAR szPath[MAX_PATH];
    GetModuleFileName(g_hModule, szPath, MAX_PATH);
    
    ITypeLib* pTypeLib;
    // LoadTypeLib 发现路径是 DLL，自动从 PE 资源段提取 TYPELIB
    HRESULT hr = LoadTypeLib(szPath, &pTypeLib);
    if (SUCCEEDED(hr)) {
        RegisterTypeLib(pTypeLib, szPath, NULL);
        pTypeLib->Release();
    }
    
    // 写注册表 CLSID/ProgID/InprocServer32 等其余逻辑
    // ...
    return S_OK;
}
```

资源脚本（`mylib.rc`）：
```
1 TYPELIB "mylib.tlb"
```

编译链接：
```bash
windres mylib.rc -O coff -o mylib.res
gcc -shared -o mylib.dll mylib.o mylib.res -lole32 -loleaut32
```

### 3.3 TypeLib 生成方案（编译期）

c3 编译期需要生成 `.tlb` 数据，有两条路径：

| 方案 | 原理 | 优劣 |
|------|------|------|
| **A. ICreateTypeLib2 运行时构建** | c3 启动子进程，调用 COM API 在内存中构建 TypeLib，序列化落盘 | 不依赖外部工具，但编译期就需要 COM 运行环境 |
| **B. 纯 C++ 手写 TypeLib 二进制** | 直接按 MS .tlb 格式写字节 | 零依赖，但 .tlb 格式是 MS 私有二进制，逆向工作量大 |
| **C. MSVC 版附带 tlb 生成，MinGW 版引用预生成** | MSVC 编译时用 MIDL 或 ICreateTypeLib2 生成，MinGW 版复用 | 开发效率最高，但两个版本构建耦合 |

**建议**：短期用方案 C（MSVC 版先产出 .tlb，MinGW 版打包时嵌入），长期走方案 A（c3 自带 TypeLib 生成能力，两个版本各自独立）。

---

## 4. Driver 层改造要点

MSVC 和 MinGW 两版 Driver 的差异集中在 `MSVCDriver` 类：

| 项目 | MSVC | MinGW |
|------|------|-------|
| 编译器可执行文件 | `cl.exe` | `gcc.exe` |
| 编译选项 | `/c /utf-8 /D_CRT_SECURE_NO_WARNINGS` | `-c -std=c11 -D_CRT_SECURE_NO_WARNINGS` |
| 链接选项 | `/Fe:out.exe user32.lib gdi32.lib ole32.lib oleaut32.lib` | `-o out.exe -luser32 -lgdi32 -lole32 -loleaut32` |
| GUI 子系统 | `/SUBSYSTEM:WINDOWS` | `-mwindows` |
| DLL 构建 | `/LD /Fe:mylib.dll` | `-shared -o mylib.dll` |
| 资源编译 | `rc.exe` | `windres` |
| 环境依赖 | `vcvarsall.bat` | 无（自包含） |

改造思路：
1. 抽象 `CompilerDriver` 接口，MSVC 和 MinGW 各自实现
2. CLI 新增 `--backend=msvc|mingw` 选项（默认 MinGW）
3. MinGW 版 c3 打包时在 `bin/` 下附带 MinGW-w64 x86_64 工具链

---

## 5. 发布包结构（草案）

### 5.1 MinGW 开箱即用版

```
c3-community-mingw-x64.zip  (~150MB)
├── bin/
│   ├── c3.exe
│   ├── mingw/
│   │   ├── gcc.exe
│   │   ├── ld.exe
│   │   ├── windres.exe
│   │   └── ... (MinGW-w64 工具链)
│   └── vb6rtl/
│       ├── vb6rtl.h
│       ├── vb6rtl.c
│       ├── vb6com.h
│       └── vb6com.c
├── examples/
│   ├── hello.bas
│   └── ...
└── README.txt
```

### 5.2 MSVC 版

```
c3-community-msvc-x64.zip  (~5MB)
├── bin/
│   ├── c3.exe
│   └── vb6rtl/
│       └── ... (同上)
├── examples/
└── README.txt
```

---

## 6. 待讨论事项

- [ ] MinGW-w64 具体选型：MSYS2 精简包 vs winlibs 独立包 vs 自行裁剪
- [ ] MinGW 版对 COM 服务端的标注策略（"实验性" vs 降级支持）
- [ ] c3 编译期 TypeLib 生成方案的选择时机（短期 C / 长期 A）
- [ ] 64 位 COM DLL 的注册表写入路径差异（InprocServer32 → SysWOW64 vs System32）
- [ ] MinGW 链接 MSVC 编译的 COM DLL 是否有 CRT 兼容问题
- [ ] 两个版本的自动化集成测试方案
- [ ] 代码签名与杀毒软件误报（MinGW 编译的 EXE/DLL 常被误报）

---

## 7. 结论

MinGW-w64 对 COM **客户端完全友好**，对 **COM 服务端基本可行**，核心障碍 TypeLib 可通过 PE 资源嵌入方案解决（与 VB6 原生行为一致）。c3 手写 C vtable 的架构选择使编译器解耦成为可能，双版本发布策略技术风险可控。
