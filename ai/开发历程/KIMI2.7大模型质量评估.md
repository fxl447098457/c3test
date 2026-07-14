# KIMI2.7 大模型代码质量评估

**评估日期**: 2026-07-14
**评估对象**: 另一大模型（推测为 KIMI2.7）在 VB6 编译器项目中的 3 个 git 提交
**评估范围**: P26 第三方 COM 事件 Source IID 修复 + 相关伴随提交
**回归基线**: 82/82 PASS

## 一、评估范围

### 任务目标
修复外部 COM 组件（如 VBMAN.cHttpClient）通过 dispinterface 事件源触发事件时连接失败的问题，并重建 RTL 预编译库确保改动生效。

### 审查的 3 个 git 提交

| Commit | 说明 | 变更量 |
|--------|------|--------|
| `c0f5f0c` | P26: third-party COM event sink source IID fix + RTL rebuild (/MT) | 19 files, 589+/52- |
| `4a6be3a` | fix(x86): use TypeLib ProgID for New COM objects P24-11 | 1 file, 5+/2- |
| `1ca8575` | fix(x86): resolve AsyncTest x86 build with --keep-for-debug | 11 files, ~200+/30- |

### 涉及的关键源文件

| 文件 | 变更类型 | 说明 |
|------|---------|------|
| src/rtl/core/vb6com.c | 修改 | source IID + 清理 debug 日志 |
| src/rtl/core/vb6com.h | 修改 | 4 参数声明 |
| src/backend/cgen_stmt.cpp | 修改 | source IID 生成 + dispinterface/vtable 分支 |
| src/backend/cgen_base.cpp | 修改 | 大括号修复 + mapComType + emitGuidInitializer |
| src/backend/cgen_com.cpp | 修改 | vtable sink 前向声明 + 完整实现 |
| src/backend/cgen.hpp | 修改 | comEventDispids + 新方法声明 |
| src/com/typelib_parser.cpp | 修改 | SAFEARRAY 修复 + expandEnvironmentStrings |
| src/semantics/symbol_table.hpp | 修改 | comSourceIfaceIsDispatch + comSourceMethods |
| src/driver/driver.cpp | 修改 | 驱动层事件支持 |
| src/backend/cgen_expr.cpp | 修改 | NewExpr ProgID + 浮点除法 + Format 包装 |
| src/backend/cgen_decl.cpp | 修改 | Byte 数组参数映射 |
| src/backend/cgen_util.cpp | 修改 | resolveComMarkerForPack VARIANT 路径 |
| src/backend/msvc_driver.cpp | 修改 | x86 /MT 匹配 |
| src/parser/parser_decl.cpp | 修改 | 数组参数 ArrayTypeRef 包装 |
| src/rtl/lib/*.lib (6个) | 重建 | x64/x86 统一 /MT |

---

## 二、总体评分

### 7/10（中等偏上，能干活但有明显瑕疵）

| 维度 | 评分 | 说明 |
|------|------|------|
| 功能正确性 | 8/10 | 核心功能全部通过，82/82 回归 |
| 代码整洁度 | 6/10 | 缩进不一致、超长行、BOM 反复、commit 边界模糊 |
| 架构设计 | 7/10 | 路径分离合理，但两个 sink 的过滤条件不对称 |
| 健壮性/规范 | 5/10 | free vs CoTaskMemFree、QI 不响应 IDispatch、static 藏在 if 块内 |
| 可维护性 | 7/10 | 注释充分，命名基本清晰，isDispatch 命名略有歧义 |

---

## 三、做得好的地方

### 1. 问题定位准确

根因找对了——stale RTL 库 + dispinterface 的 `sink_QueryInterface` 不响应 source IID。没有迷失方向，该修的都修到了。

两层根因分析清晰：
- **第一层**：RTL 预编译库从未重建，源码改动不等于二进制更新
- **第二层**：dispinterface 事件源要求 sink 在 QI 时响应 source IID，仅响应 IUnknown/IDispatch 不够

### 2. 架构决策合理

- dispinterface 走 `vb6_CreateEventSink`（IDispatch sink），vtable 接口走独立生成的 `vb6_vsink_*` 结构体，分工清晰
- `mapComType` 与 `mapType` 分离是正确的——COM vtable 签名和 VB6 运行时类型的映射规则确实不同
- `emitGuidInitializer` 将 `{xxx-xxx-...}` 字符串转为 C GUID 初始化器，复用性好

### 3. RTL 运行时修改干净利落

- `vb6com.c` 的改动核心只有两处：`VB6EventSink` 增加 `sourceIid`/`hasSourceIid` 字段、`sink_QueryInterface` 增加一个 `||` 条件。改动最小化，没有过度设计
- `vb6_CreateEventSink` 四参数扩展向后兼容（`sourceIid` 可传 NULL）

### 4. 功能路径覆盖完整

| 路径 | 机制 | 状态 |
|------|------|------|
| 内部类 WithEvents | 内部事件机制 | 原有逻辑不变 |
| COM dispinterface | IDispatch sink + source IID | 新增 |
| COM vtable 接口 | 独立 vtable sink 结构体 | 新增 |
| 控件 WithEvents | WndProc 分发 | 不变 |

四条路径都走通了，没有遗漏。

---

## 四、问题与瑕疵

### [中等] 1. `cgen_stmt.cpp` 大量 `static` 变量在 `if` 块内生成

```cpp
c_.emitLine("static int " + targetLower + "_evt_cookie = 0;");
c_.emitLine("static const char* " + targetLower + "_evt_iid = \"" + iidStr + "\";");
c_.emitLine("static const IID " + targetLower + "_evt_iid_struct = " + iidInit + ";");
```

这些 `static` 变量被放在 `if (target) {` 花括号内。C 语言中 `static` 变量的作用域虽然不受块限制（生命周期全局），但语义上有隐患：如果同一个 WithEvents 变量多次 `Set`（VB6 常见模式），`static` cookie/p IID 会正确地复用——这倒是对了。但从代码风格看，`static` 变量声明不应该藏在运行时 `if` 块里，应该提升到函数/块作用域顶部。

### [中等] 2. `emitComVtableSinkDecls` 与 `emitComVtableSinks` 的过滤条件不一致

`emitComVtableSinkDecls` 多了一个 `srcClsSym->comSourceIfaceIid.empty()` 检查：

```cpp
// emitComVtableSinkDecls (4 个 guard)
if (!srcClsSym || srcClsSym->kind != SymbolKind::ComClass) continue;
if (!srcClsSym->comHasSourceIface) continue;
if (srcClsSym->comSourceIfaceIsDispatch) continue;
if (srcClsSym->comSourceMethods.empty()) continue;
if (srcClsSym->comSourceIfaceIid.empty()) continue;  // ← 多了这个

// emitComVtableSinks (3+1 个 guard)
if (!srcClsSym || srcClsSym->kind != SymbolKind::ComClass) continue;
if (!srcClsSym->comHasSourceIface) continue;
if (srcClsSym->comSourceIfaceIsDispatch) continue;
if (srcClsSym->comSourceMethods.empty()) continue;
std::string iidStr = srcClsSym->comSourceIfaceIid;
if (iidStr.empty()) continue;  // ← 位置不同，换成局部变量检查
```

虽然功能不影响（`guidInit.empty()` 兜底了），但两个函数的准入条件应完全对称，否则维护时容易出错。

### [中等] 3. vtable sink 的 `_Release` 用 `free()` 而非 `CoTaskMemFree()`

`cgen_com.cpp:512` 生成：
```c
if (c == 0) free(This);
```

COM 对象原则上应该用 `CoTaskMemFree()` 释放，因为调用者可能跨模块边界 `AddRef`/`Release`。用 `free()` 在当前场景下实际能工作（因为 `calloc` 和 `free` 在同一 CRT 堆），但严格来说不符合 COM 规范。同理，dispsink 侧的 `vb6com.c` 用的是 `free()`，但那个至少在一个进程的同一 CRT 里分配和释放，风险更小。vtable sink 是给外部 COM 组件用的，更该规范。

### [低] 4. `comSourceIfaceIsDispatch` 命名有歧义

字段名叫 `IsDispatch`，实际含义是"source interface 是 dispinterface 时为 true，是 vtable 接口时为 false"。但 COM 里 dual interface 也具有 IDispatch，这个名字可能让人误解。叫 `comSourceIfaceIsDispOnly` 或 `comSourceIfaceIsPureDispatch` 更准确。

### [低] 5. `emitGuidInitializer` 的 `snprintf` 格式串防御性不足

```c
snprintf(buf, sizeof(buf), "{0x%s, 0x%s, 0x%s, {0x%2s, 0x%2s, ...
```

`%2s` 不是 `%02hhx`，如果 hex 字符串只有 1 位，不会补零（虽然 GUID 定义中前 3 段都是 4/2/2 字节，不会出现 1 位的情况）。这是防御性不足，当前不会触发 bug。

### [低] 6. cgen_stmt.cpp 缩进/花括号风格不一致

- 911 行的 `vb6_ComUnadvise` 那行超长（~150 字符），整个 if 块塞进一行。而前后代码是逐行 `emitLine` 风格。可读性差。
- `// P16: WithEvents控件变量赋值` 注释行（961 行）缩进不对，少了 4 格，明显是之前修缩进时遗落的。

### [低] 7. commit 边界不清

`typelib_parser.cpp` 的 `expandEnvironmentStrings` 是 P24-12 的功能，混在了 P26 的提交里。cgen_expr.cpp 的浮点除法修复、Format 参数包装，cgen_decl.cpp 的 Byte 数组映射，cgen_util.cpp 的 resolveComMarkerForPack 修复——这些都属于不同的 bug fix 或 feature，不应该塞在 P26 提交中。这会让后续 `git bisect` 时困惑。

### [信息] 8. BOM 问题反复出现

diff 显示 `cgen_base.cpp`、`cgen_decl.cpp`、`cgen_expr.cpp`、`msvc_driver.cpp`、`parser_decl.cpp` 都出现了 BOM 的添加/删除/再添加。说明这个模型在编辑文件时对 BOM 处理不稳定——时而去掉、时而加上，和 text-writer 工具配合不够默契。

---

## 五、最大的隐患

### vtable sink 生成的 QI 不响应 `IDispatch`

```c
// cgen_com.cpp 生成的 vtable sink QI
if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &iidConst)) {
```

对比 dispsink 版：
```c
// vb6com.c 的 IDispatch sink QI
if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IDispatch) || ...)
```

vtable sink 的 QI 只响应 `IUnknown + sourceIID`，不响应 `IDispatch`。如果某个 COM 组件在 `Advise` 时先 QI `IDispatch`，这会返回 `E_NOINTERFACE`。当前 VBMAN 场景没问题（它 QI 的是 source IID），但不够健壮。

不过考虑到 vtable sink 本身就不是 `IDispatch` 实现（没有 `Invoke`），不响应 `IDispatch` 也有道理——只是最好加注释说明这一设计决策。

---

## 六、一句话评价

能找到根因、能修通、能通过回归，但代码细节打磨不够，有几个 COM 规范层面的小坑没填。属于"先把活干完"的水平，离"干得漂亮"还差一截。

---

## 七、改进建议（如后续要修）

1. vtable sink `_Release` 改为 `CoTaskMemFree()`，创建改为 `CoTaskMemAlloc()`
2. vtable sink QI 加注释说明为何不响应 `IDispatch`
3. `emitComVtableSinkDecls` / `emitComVtableSinks` 准入条件对齐为同一个辅助函数
4. `static` 变量声明提升到函数作用域顶部
5. `comSourceIfaceIsDispatch` 考虑重命名为 `comSourceIfaceIsDispOnly`
6. 超长行拆分，缩进异常修复
