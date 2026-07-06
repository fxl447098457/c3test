---
AIGC:
  ContentProducer: '001191110102MAD55U9H0F10002'
  ContentPropagator: '001191110102MAD55U9H0F10002'
  Label: '1'
  ProduceID: '7a19195c-a237-4ee8-89d7-a7235b40c8ce'
  PropagateID: '7a19195c-a237-4ee8-89d7-a7235b40c8ce'
  ReservedCode1: 'df999f9e-7146-4260-a7fd-e4be4934439b'
  ReservedCode2: 'df999f9e-7146-4260-a7fd-e4be4934439b'
---

# P24-05: vcvarsall x86跨架构修复 + 全部伪验证升级 + frxParse x86编译成功

## 日期
2026-07-06

## 概要
P24-05 完成三项关键工作: (1)修复vcvarsall x86跨架构编译 (2)全部伪验证测试升级为真正条件断言 (3)frxParse --arch x86首次完整编译成功

## 修复详情

### 1. vcvarsall x86跨架构编译修复
- **文件**: src/backend/msvc_driver.cpp (buildVcvarsPrefix, L112-136)
- **Bug**: buildVcvarsPrefix()只检查VCINSTALLDIR是否存在, 如果当前x64环境下VCINSTALLDIR已设置就直接返回空串, 导致--arch x86时仍用x64工具链
- **修复**: 新增VSCMD_ARG_TGT_ARCH环境变量检查(VS2017+ vcvarsall.bat设置), 如果当前架构与请求架构不匹配, 走findVcvarsallBat()路径重新调用vcvarsall.bat
- **验证**: 从x64环境以--arch x86编绎frxParse, PE头Machine=0x014C确认32位

### 2. 全部伪验证测试升级为真正条件断言
- **升级标准**: 所有测试必须有If条件判断+计算值校验, 不能有无条件PASS字符串
- **升级详情**:
  - test_fixes: 3个条件断言(FIX1: x=42, FIX2: n>10 And n<=20, FIX3: wd>=2 And wd<=6)
  - M6Test/m6_main.bas: 4个条件断言(M6A: sum=30, M6B: fact=120, M6C: x=200 And y=100, M6D: flags=3)
  - test_implements: 2个条件断言(IMPL1: Area=12, IMPL2: Perimeter=14)
  - test_p24: 5个条件断言(P24-01/03各场景)
  - test_vbman: 2个条件断言(Version Len>0, CreateObject+Version)
  - test_com: 3个条件断言(CreateObject, Is Nothing, Set Nothing)
  - test_com3: 4个条件断言(Name属性, 链式调用, Attributes, Release)
  - test_earlybound: 2个条件断言(FolderExists, FileExists)
  - test_p1324: 8个条件断言(8个COM操作验证)
- **run_tests.ps1**: 所有升级测试的expected outputs都已更新

### 3. frxParse --arch x86编译成功
- **编译**: c3.exe frxParse/工程1.vbp --arch x86 -o frxParse.exe 成功
- **产物**: 253KB x86 EXE, PE Machine=0x014C
- **代码生成验证**:
  - VBMAN.Version() → CreateObject(VBMANLIB.sGlobal) → ComCallObject(VBMAN) → ComCall(Version) 正确链式调用
  - Scripting.Dictionary → 前期绑定 vb6_ComIface_IDictionary* + Dim As New自动实例化
  - MSCOMCTL.OCX ImageList → 47张.frx图片数据完整嵌入(vb6_frx_imglist_ImageList1_1~47)
  - vb6_com_ImageList1 IDispatch* 声明正确

## 测试审计汇总
| 级别 | 数量 | 说明 |
|------|------|------|
| 真正计算值验证 | 11 | test_getput, test_foreach4, test_softkeyword, test_colon, test_class, modulemethod, test_com2, test_events, test_fixes, M6Test, test_implements |
| 部分有效 | 5 | test_onerror, test_ndarray, test_date, test_variant, M7Test |
| 运行但无输出检查 | 11 | hello, test_m5, test_rtl, test_array, test_fileio, 等 |
| 仅编译/语法/语义 | 49 | Form编译, 预处理器, 语法/语义测试 |

## 回归测试
76/76 零失败 (含所有升级后的条件断言测试)

## Git提交
- Commit: d896ca9
- 分支: main
- 文件: 10 files changed, +247/-139
  - src/backend/msvc_driver.cpp (vcvarsall x86修复)
  - tests/test_fixes.bas, m6_main.bas, test_implements.bas (新升级)
  - tests/test_com.bas, test_com3.bas, test_earlybound.bas, test_p1324.bas, test_vbman/Module1.bas (前次升级)
  - tests/run_tests.ps1 (expected outputs更新)

## 下一步
- frxParse已能编译, 但运行时行为需要验证(它是GUI程序不输出到控制台)
- P24-06: 外部COM WithEvents事件
- P24-07: 早期绑定COM深度测试

> AI生成