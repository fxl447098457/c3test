# ============================================================
#  build_vbman.ps1 - 用 C3 编译 vbman 真实工程 (tests\vbman)
#  目标: src\VBMAN.vbp (Type=OleDll, 129 模块, 与 vbman 自带
#        build-vbman.bat 用 MS VB6 编译的是同一工程)
#
#  用法:
#    .\build_vbman.ps1                  # 完整编译 (x86, 默认)
#    .\build_vbman.ps1 -SyntaxOnly      # 仅前端语法/语义检查 (快)
#    .\build_vbman.ps1 -Arch x64        # 换架构
#    .\build_vbman.ps1 -LogName xxx.log # 自定义日志名 (存 docs\vbman\)
#
#  输出:
#    - 编译日志: docs\vbman\<LogName>
#    - cl 错误明细: output\c3-error.log (仅当进入 cl 阶段后失败)
#    - 编译后自动汇总 error / warning 统计
# ============================================================

param(
    [ValidateSet("x86", "x64")]
    [string]$Arch = "x86",
    [string]$LogName = "build_vbman_last.log",
    [switch]$SyntaxOnly
)

$ErrorActionPreference = "Continue"

# === 路径 ===
$Root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$C3 = Join-Path $Root ".build\C3.exe"
$Vbp = "tests\vbman\src\VBMAN.vbp"
$LogPath = Join-Path $PSScriptRoot $LogName
$ErrLog = Join-Path $Root "output\c3-error.log"

if (-not (Test-Path $C3)) {
    Write-Host "[ERROR] 未找到 $C3, 请先构建 C3 (scripts\build)" -ForegroundColor Red
    exit 1
}

# 切到项目根, 使 C3 的相对路径与 bat 方式一致
Set-Location $Root

# 清除上一轮的 cl 错误日志, 保证统计只反映本次编译
Remove-Item $ErrLog -ErrorAction SilentlyContinue

# === vcvarsall 探测: C3_VCVARSALL 环境变量优先, 其次 vswhere 自动定位 ===
# 约定: 脚本不硬编码机器路径, 机器差异一律通过环境变量/传参输入 (保持可递交与脱敏)
$VcVars = $env:C3_VCVARSALL
if (-not $VcVars) {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2>$null
        if ($vsPath) {
            $cand = Join-Path $vsPath "VC\Auxiliary\Build\vcvarsall.bat"
            if (Test-Path $cand) { $VcVars = $cand }
        }
    }
}
if (-not $VcVars) {
    Write-Host "[ERROR] 未找到 vcvarsall.bat, 请设置 C3_VCVARSALL 环境变量指向完整路径 (详见 scripts\README.md)" -ForegroundColor Red
    exit 1
}

# === 加载 MSVC 环境 (通过临时 bat 导出, 同 tests\run_tests.ps1) ===
$tempBat = "$env:TEMP\vcvars_env.bat"
cmd /c "call `"$VcVars`" x64 >nul 2>&1 && set" | Out-File $tempBat -Encoding ASCII
Get-Content $tempBat | ForEach-Object {
    if ($_ -match '^([^=]+)=(.*)$') {
        [Environment]::SetEnvironmentVariable($matches[1], $matches[2], "Process")
    }
}
Remove-Item $tempBat -ErrorAction SilentlyContinue

$OutDir = Join-Path $Root "output"
if (-not (Test-Path $OutDir)) { New-Item -ItemType Directory -Path $OutDir | Out-Null }

# === 编译 (经 cmd 执行, 重定向保持 C3 原生 GBK 字节流, 便于 grep 统计) ===
$redirect = "> `"$LogPath`" 2>&1"
if ($SyntaxOnly) {
    $c3Args = "`"$Vbp`" --syntax-only"
} else {
    $c3Args = "`"$Vbp`" --dll --arch $Arch --output-dir `"$OutDir`""
}

$mode = if ($SyntaxOnly) { "syntax-only" } else { "full build (Type=OleDll, arch=$Arch)" }
Write-Host "[INFO] project: $Vbp ($mode)"
Write-Host "[INFO] log: $LogPath"

cmd /c "`"$C3`" $c3Args $redirect"
$rc = $LASTEXITCODE

# === 日志脱敏: 相对化项目内绝对路径并隐藏用户目录 ===
# 本目录文档会递交公开仓库, 日志中不得残留本机绝对路径 (路径全部运行时推导)
$enc = [Text.Encoding]::GetEncoding(936)
$logText = [IO.File]::ReadAllText($LogPath, $enc)
$logText = $logText.Replace("$Root\", "").Replace("$Root/", "")
$homeDir = $env:USERPROFILE
$logText = $logText.Replace("$homeDir\AppData\Local\Temp", "<TEMP>")
$logText = $logText.Replace("$homeDir\", "<HOME>\")
[IO.File]::WriteAllText($LogPath, $logText, $enc)

# === 结果汇总 ===
$warnCount = (Select-String -Path $LogPath -Pattern "warning VB" -AllMatches | Measure-Object).Count
$errCount = 0
if ((-not $SyntaxOnly) -and ($rc -ne 0) -and (Test-Path $ErrLog)) {
    $errCount = (Select-String -Path $ErrLog -Pattern "error C\d+" -AllMatches | Measure-Object).Count
}

Write-Host ""
Write-Host "========================================"
if ($rc -eq 0) {
    if ($SyntaxOnly) {
        Write-Host "  FRONTEND OK (语法/语义检查通过)" -ForegroundColor Green
    } else {
        Write-Host "  BUILD OK" -ForegroundColor Green
    }
} else {
    if ($SyntaxOnly) {
        Write-Host "  FRONTEND FAILED (C3 exit $rc)" -ForegroundColor Red
    } else {
        Write-Host "  BUILD FAILED (C3 exit $rc)" -ForegroundColor Red
    }
}
Write-Host "  C3 warnings: $warnCount"
if ($errCount -gt 0) {
    Write-Host "  cl errors  : $errCount  (明细: output\c3-error.log)" -ForegroundColor Red
}
Write-Host "========================================"

exit $rc
