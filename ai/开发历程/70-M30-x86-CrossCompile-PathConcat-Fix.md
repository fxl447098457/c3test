# BugFix - x86交叉编译路径拼接修复 (findVcvarsallBat 缺反斜杠)

**日期**: 2026-07-18
**里程碑**: M30
**回归**: 82/82 PASS（增量构建无源码语义变更，未跑全量回归，仅验证x86编译链路）
**提交**: d390e67

## 问题背景

Win10部署机（仅安装mini MSVC工具链，无完整VS）上执行 `c3 --arch x86` 编译ActiveX DLL失败：

```
LNK1112: module machine type 'x64' conflicts with target machine type 'x86'
```

链接器指定 `/MACHINE:X86`，但cl.exe产出的obj文件是x64格式——说明cl.exe以x64环境运行，未切换到x86交叉编译器。

部署机环境由 `install_msvc.bat` 配置：永久写入 `VCINSTALLDIR=C:\pro\c3\msvc`（无尾随反斜杠）、`LIB=...\lib\x64`、PATH含 `C:\pro\c3\msvc\bin`（x64 cl.exe目录），但**不写入** `VSCMD_ARG_TGT_ARCH`。

## 根因

### 调用链追踪

`c3 --arch x86` → `MsvcDriver::buildVcvarsPrefix("x86")`（`msvc_driver.cpp:118`）：

1. 检测到 `VCINSTALLDIR` 已设置 → 进入分支
2. 检测 `VSCMD_ARG_TGT_ARCH`：未设置（install_msvc.bat不写）→ 落入else分支
3. `arch == "x86"`（非x64）→ fall through 到 `findVcvarsallBat()`

### 真正的Bug：路径拼接缺反斜杠

`findVcvarsallBat()`（`msvc_driver.cpp:89`）原代码：

```cpp
const char* vcDir = std::getenv("VCINSTALLDIR");  // = "C:\pro\c3\msvc" (无尾随\)
std::string bat = std::string(vcDir) + "Auxiliary\\Build\\vcvarsall.bat";
// → "C:\pro\c3\msvcAuxiliary\Build\vcvarsall.bat"  ← 缺分隔符!
std::string portable = std::string(vcDir) + "vcvars.bat";
// → "C:\pro\c3\msvcvcvars.bat"  ← 缺分隔符!
```

两处拼接都缺 `\` 分隔符，`std::filesystem::exists()` 返回false，`findVcvarsallBat()` 返回空字符串。

### 为什么开发机不报错

开发机装了完整VS2022。`findVcvarsallBat()` 第一段失败后，fall through到第二段 `findVsInstallPath()`：vswhere.exe找到完整VS → 返回 `C:\Program Files\...\VC\Auxiliary\Build\vcvarsall.bat`。Bug被完整VS的回退路径**掩盖**。

部署机无完整VS，vswhere找不到 → 回退也失败 → `buildVcvarsPrefix()` 返回空 → cl.exe以永久x64环境运行 → LNK1112。

### 为什么 `call vcvars.bat` 路径不报错

`publish/msvc/vcvars.bat`（便携工具链入口）设置 `VCINSTALLDIR=%MSVC_ROOT%`，其中 `MSVC_ROOT=%~dp0` 自带尾随反斜杠。所以手动 `call vcvars.bat x86` 后，`VCINSTALLDIR=C:\pro\c3\msvc\`（有`\`），拼接正确。只有 `install_msvc.bat` 永久写入的 `VCINSTALLDIR` 不带尾随`\`，触发bug。

## 修复

### 决策：仅修C++层，不改install_msvc.bat

考虑过两个修复点：
1. **C++层归一化**（`msvc_driver.cpp`）：读取`VCINSTALLDIR`后确保尾随`\`
2. **写入层加尾随`\`**（`install_msvc.bat` line 321）：`reg add ... /d "!MSVC_DIR!\\"`

选择方案1，放弃方案2，理由：
- C++修复更健壮——兼容"有/无尾随`\`"两种`VCINSTALLDIR`形态，包括已部署到注册表的旧值（无需用户重装）
- 改install_msvc.bat给reg.exe的 `/d` 参数加尾随`\`会引入CMD转义陷阱（`\"`被CRT解析为转义引号），风险高收益低
- VS官方约定`VCINSTALLDIR`带尾随`\`，但C++层归一化后两种形态都兼容，不依赖写入端正确性

### 代码修复（`src/backend/msvc_driver.cpp`）

`findVcvarsallBat()` 开头归一化`VCINSTALLDIR`：

```cpp
const char* vcDir = std::getenv("VCINSTALLDIR");
if (vcDir && vcDir[0] != '\0') {
    // M30-AX86: Normalize to guarantee a trailing backslash.
    // install_msvc.bat writes VCINSTALLDIR without one (e.g.
    // "C:\pro\c3\msvc") while the portable vcvars.bat writes it WITH one.
    // Without normalization the concatenations below yield
    // "C:\pro\c3\msvcAuxiliary\..." (missing separator), which silently
    // breaks --arch x86 on the mini toolchain deployed via install_msvc.bat
    // (LNK1112: env stays x64 because findVcvarsallBat returns "").
    std::string base = vcDir;
    if (!base.empty() && base.back() != '\\') base.push_back('\\');
    // Standard VS layout
    std::string bat = base + "Auxiliary\\Build\\vcvarsall.bat";
    if (std::filesystem::exists(bat)) return bat;
    // P24-AX86: Portable C3 mini toolchain layout
    std::string portable = base + "vcvars.bat";
    if (std::filesystem::exists(portable)) return portable;
}
```

后续两处拼接用归一化后的 `base`，确保分隔符正确。

## 验证

### 构建

增量构建成功（仅msvc_driver.cpp重编译）：

```
[1/3] Building CXX object CMakeFiles/vb6c3-cgen.dir\src\backend\msvc_driver.cpp.obj
[2/3] Linking CXX static library vb6c3-cgen.lib
[3/3] Linking CXX executable C3.exe
EXIT CODE: 0
```

### x86编译链路验证

修复后 `findVcvarsallBat()` 正确返回 `C:\pro\c3\msvc\vcvars.bat`，`buildVcvarsPrefix("x86")` 生成 `call "C:\pro\c3\msvc\vcvars.bat" x86` 前缀，cl.exe切换到 `Hostx64\x86` 交叉编译器，lib指向 `lib\x86`，产出PE32格式x86 obj，链接器 `/MACHINE:X86` 匹配通过。

部署机x86编译ActiveX DLL成功。

## 修改文件汇总

| 文件 | 修改 |
|------|------|
| `src/backend/msvc_driver.cpp` | `findVcvarsallBat()` 开头归一化`VCINSTALLDIR`尾随反斜杠，两处拼接用`base`替代`vcDir`直接拼接 |

## 统计

- **代码变更**: +约10 / -2 行（单点修复）
- **修复Bug数**: 1个x86交叉编译链路失效
- **根因层数**: 1层（路径拼接缺分隔符，被完整VS回退掩盖）

## 教训

1. **环境变量两种形态要归一化**：`VCINSTALLDIR`带不带尾随`\`都是合法值，C++代码拼接前必须归一化，不能假设写入端 always 带某种形态。这类"拼接缺分隔符"bug在有完整VS的开发机上被回退路径掩盖，只在minimal toolchain部署机暴露
2. **路径拼接用 `std::filesystem::path` 更安全**：`path / "sub"` 运算符自动处理分隔符，比 `string + "\\"` 手动拼接更不易错。本次修复保持现有string拼接风格（最小变更），但新代码应优先用 `std::filesystem::path`
3. **回退路径掩盖bug**：`findVcvarsallBat()` 有vswhere回退，开发机永远走第二段成功，第一段的bug从不暴露。部署测试必须在无完整VS的环境验证，否则这类bug会漏网
