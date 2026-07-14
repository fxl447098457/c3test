# P26 - 第三方COM事件Source IID修复与RTL库重建

**日期**: 2026-07-14
**里程碑**: M27
**回归**: 82/82 PASS

## 问题背景

VBMAN项目的`cHttpClient`类（封装`WinHttp.WinHttpRequest`）在C3编译的程序中无法接收异步事件。`WithEvents`声明后调用`Advise`返回`E_NOINTERFACE`(0x80004002)，事件回调永远不触发。

同样的`WinHttp.WinHttpRequest`直接使用时事件正常触发。问题仅出现在VBMAN这一类"二次封装"的COM对象上。

## 根因分析

### 第一层：stale RTL库

`vb6_CreateEventSink`和`sink_QueryInterface`的source-IID支持代码已添加到`src/rtl/core/vb6com.c`和`vb6com.h`，但预编译的`.lib`文件从未重建。C3通过`c3rtl.rc`的RCDATA资源嵌入RTL库，源码变更不会生效，必须重建`.lib`并重新链接C3.exe。

### 第二层：source IID查询失败

VBMAN的`cHttpClient`用`Dim WithEvents Inst As WinHttp.WinHttpRequest`内部封装WinHTTP。其事件源接口`__cHttpClient`是dispinterface（IMPLTYPEFLAG_FSOURCE标记），不是普通dual接口。

当COM连接点容器调用`Advise`时，会对传入的sink执行`QueryInterface(sourceIID)`。原`VB6EventSink`的`sink_QueryInterface`只响应`IUnknown`和`IDispatch`，对source IID返回`E_NOINTERFACE`，导致`Advise`失败。

### 第三层：CRT不匹配

首次重建x64 RTL库后发现链接错误`LNK2019 __imp_*`，原因是原x64库用`/MD`(动态CRT)编译，而本环境`cl.exe`默认`/MT`(静态CRT)。解决方案：x64和x86统一使用`/MT`。

## 修复方案

### 1. sink_QueryInterface响应source IID

**文件**: `src/rtl/core/vb6com.c`, `src/rtl/core/vb6com.h`

- `VB6EventSink`结构体新增`sourceIid`(IID)和`hasSourceIid`(int)字段
- `sink_QueryInterface`在原有`IID_IUnknown`/`IID_IDispatch`响应基础上，若`hasSourceIid`则同时响应该source IID
- `vb6_CreateEventSink`签名从3参数扩展为4参数，新增`const IID* sourceIid`

### 2. 代码生成传递source IID

**文件**: `src/backend/cgen_stmt.cpp`

为外部COM事件源生成静态IID结构：
```c
static const IID _vb6_evt_iid_<ClassName>_evt_iid_struct = {0x...};
```
并在`vb6_CreateEventSink`调用时传入`&..._evt_iid_struct`。

### 3. RTL库重建

**文件**: `.temp/build_rtl_libs.bat`

- 创建自动化构建脚本，使用vswhere定位MSVC + vcvarsall初始化环境
- x64和x86均使用`/MT`静态CRT编译
- 重建后需删除`.build/`中的C3.exe和c3rtl.rc.res，再cmake rebuild

### 4. 其他修复

- `src/com/typelib_parser.cpp`: 修复`SAFEARRAY`类型在`VT_PTR -> VT_SAFEARRAY`转换中的丢失问题
- `src/backend/cgen_base.cpp`: 修复`emitComVtableSinks()`插入导致的大括号不匹配
- `src/backend/cgen_com.cpp`: vtable sink前向声明和实现
- `src/backend/cgen.hpp`: 新增`comEventDispids`支持
- `src/semantics/symbol_table.hpp`: 符号表COM事件扩展
- `src/driver/driver.cpp`: 驱动层事件支持

## 验证结果

| 测试项 | 结果 |
|--------|------|
| VBMAN cHttpClient AsyncTest2 事件触发 | PASS (OnResponseStart/OnResponseDataAvailable/OnError) |
| WinHttp.WinHttpRequest 事件回归 | PASS (OnResponseStart/OnResponseDataAvailable/OnResponseFinished) |
| 完整回归套件 | 82/82 PASS |

注：AsyncTest2返回400 Bad Request是应用层JSON格式问题，与COM事件无关。

## 经验教训

1. **修改RTL源码后必须重建.lib**：C3嵌入的是预编译库，不是源码。源码改动不等于二进制更新。
2. **CRT一致性**：同一项目的所有.lib必须使用相同的CRT链接方式（/MT或/MD），否则链接时出现LNK2019。
3. **dispinterface的事件Sink必须响应source IID**：COM连接点协议要求sink在QI时响应source接口IID，仅响应IUnknown/IDispatch不够。
4. **BOM问题**：Python编辑C源码会移除UTF-8 BOM，MSVC在某些情况下需要无BOM源文件。

## 修改文件清单

| 文件 | 变更类型 |
|------|---------|
| src/rtl/core/vb6com.c | 修改（source IID + 清理debug日志） |
| src/rtl/core/vb6com.h | 修改（4参数声明） |
| src/backend/cgen_stmt.cpp | 修改（source IID生成） |
| src/backend/cgen_base.cpp | 修改（大括号修复） |
| src/backend/cgen_com.cpp | 修改（vtable sink） |
| src/backend/cgen.hpp | 修改（comEventDispids） |
| src/com/typelib_parser.cpp | 修改（SAFEARRAY修复） |
| src/semantics/symbol_table.hpp | 修改（COM事件扩展） |
| src/driver/driver.cpp | 修改（驱动层支持） |
| src/rtl/lib/vb6rtl.lib | 重建（x64 /MT） |
| src/rtl/lib/vb6rtl_gui.lib | 重建（x64 /MT） |
| src/rtl/lib/vb6rtl_dll.lib | 重建（x64 /MT） |
| src/rtl/lib/x86/vb6rtl.lib | 重建（x86 /MT） |
| src/rtl/lib/x86/vb6rtl_gui.lib | 重建（x86 /MT） |
| src/rtl/lib/x86/vb6rtl_dll.lib | 重建（x86 /MT） |
| tests/test_com_events_winhttp/ | 新增（WinHTTP事件测试） |
