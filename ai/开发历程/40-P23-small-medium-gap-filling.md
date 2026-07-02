# P23 - 小投入速效+中等投入缝隙填平

**日期**: 2026-07-02
**提交**: (待commit)
**回归测试**: 74/74 PASS

## 完成项

### P23-01: VBP Object= 下游消费 (TypeLib 导入)
- **问题**: VBP Reference= 和 Object= 条目已被 vbp_parser 解析，但 driver 未将其送入 TypeLib 导入流水线
- **方案**: driver.cpp VBP 解析段中，遍历 project.references 和 project.objects，将 GUID/path 推入 effectiveOpts.typelibRefs
- **改动**: driver.cpp 12行新增（P23-01 代码块）
- **效果**: VBP 中引用的 COM 类型库和 ActiveX 控件现在自动触发 TypeLib 导入

### P23-04: 低频常量补全 (77 个新增)
- **问题**: 274 个已注册常量 vs ~350 个 VB6 标准常量，缺 85 个
- **方案**: 分析脚本 `.temp/analyze_missing_constants.py` 识别缺失，去重后 77 个实际新增
- **新增类别**: CallType(4), Calendar(2), QueryClose(6), Clipboard(7), DriveType(5), AppWinStyle(3), IMEStatus(8), Shape(6), BorderStyle(7), FillStyle(8), MousePointer(17), Alignment(3), ScrollBar(4), ScaleMode(11), WindowState(2)
- **改动**: semantic_analyzer.cpp 85 行 addConst 新增（8 个已存在被去重跳过）
- **效果**: 常量总数 274 → 359

### P23-05: VBP 版本信息 → VS_VERSION_INFO 资源嵌入
- **问题**: VBP 中 Title/MajorVer/MinorVer/RevisionVer/CompanyName 等版本字段已解析，但未写入生成 EXE
- **方案**: 
  1. driver.hpp 添加 9 个版本信息成员变量
  2. driver.cpp VBP 段收集版本信息
  3. runLinker 中生成 version_info.rc（标准 VS_VERSION_INFO RC 脚本）
  4. 调用 rc.exe 编译为 .res
  5. MsvcDriverOptions.versionInfoResFile 传递给 link.exe
- **改动**: driver.hpp(+10行), driver.cpp(+~100行), msvc_driver.hpp(+1行), msvc_driver.cpp(+3行)
- **效果**: VBP 工程编译产物现在包含完整的 Windows 版本信息资源

### P23-03: .res 资源文件传递链接器
- **问题**: VBP ResFile= 字段已解析但未消费
- **方案**: 
  1. driver.hpp 添加 userResFile_ 成员
  2. driver.cpp 收集 VBP ResFile 绝对路径并在 runLinker 中设置 msvcOpts.userResFile
  3. msvc_driver.hpp 添加 userResFile 字段
  4. msvc_driver.cpp 追加到 link 命令
- **改动**: 4 个文件各少量修改
- **效果**: VBP ResFile= 指定的 .res 文件自动传递给链接器

## 延期项

### P23-02: .frx 二进制资源读取
- 原因: 无测试用例，frm_parser 遇 .frx 引用跳过，实现工作量大

### P23-06: ActiveX Document DLL
- 原因: 需 IOleDocument/IOleDocumentView 等 COM 接口，工作量大且无测试用例

## 文件变更清单
| 文件 | 变更 |
|------|------|
| src/semantics/semantic_analyzer.cpp | +85 行 addConst (P23-04) |
| src/driver/driver.hpp | +10 行版本信息成员 +1 行 userResFile_ (P23-05/03) |
| src/driver/driver.cpp | +12 行 P23-01 +10 行 P23-05 收集 +~100 行 VS_VERSION_INFO RC 生成 +7 行 P23-03 |
| src/backend/msvc_driver.hpp | +2 行 versionInfoResFile/userResFile (P23-05/03) |
| src/backend/msvc_driver.cpp | +6 行 link 命令追加 (P23-05/03) |
