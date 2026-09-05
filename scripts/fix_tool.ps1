# fix_tool.ps1 - 专项小工程编译工具
# 用途: 从 VBMAN.vbp 裁剪出只含指定模块的最小工程, 快速验证 C3 修复,
#       避免每次完整编译 136 模块的 vbman 工程 (减少迭代耗时).
# 用法:
#   powershell -File scripts/fix_tool.ps1 -Modules "Json\ToolsJsonVba.bas"
#   powershell -File scripts/fix_tool.ps1 -Modules "Filesystem\ZipArchive\src\cZipArchive.cls","Common\Common4DLL.bas" -Name zip
#   -Modules: 相对 vbman/src 的模块文件路径(可多个), 必须在 VBMAN.vbp 中出现
#   -Name:    工程名 (默认取第一个模块的文件名主干)
#   -Build:   是否立即编译 (默认 true)
#   -Show:    编译后打印错误汇总 (默认 true)
# 产物:
#   vbman/src/FIX_<Name>.vbp            裁剪工程 (位于 vbman/src, 相对引用全部有效)
#   vbman/src/_fix/<Name>/out/c3-error.log 本工程 MSVC 编译错误
param(
    [string[]]$Modules = @(''),
    [string]$Name = '',
    [bool]$Build = $true,
    [bool]$Show = $true
)
if (-not $Modules -or $Modules.Count -lt 1 -or -not $Modules[0]) {
    Write-Host '用法: fix_tool.ps1 -Modules <模块路径> [-Name 工程名] [-Build:$false]'
    exit 1
}

$ErrorActionPreference = 'Stop'
$root = 'C:\Users\vi\Desktop\c3.vb6.pro'
$src  = Join-Path $root 'vbman\src'
$fix  = Join-Path $src '_fix'
$vbpSrc = Join-Path $src 'VBMAN.vbp'
if (-not (Test-Path $vbpSrc)) { throw "VBMAN.vbp not found: $vbpSrc" }

# 1) 解析原 vbp
$text = [System.IO.File]::ReadAllText($vbpSrc)
$lines = $text -split "`r?`n"
$refLines = @($lines | Where-Object { $_ -like 'Reference=*' })
$modLineMap = @{}
foreach ($ln in $lines) {
    if ($ln -match '^(Class|Module|Form)=([^;]+);(.+)$') {
        $rel = $matches[3].Trim()
        $modLineMap[$rel.ToLower()] = $ln
    }
}
$tailStart = -1
for ($i = 0; $i -lt $lines.Count; $i++) {
    if ($lines[$i] -like 'ResFile32=*') { $tailStart = $i; break }
}
if ($tailStart -lt 0) { throw 'ResFile32 marker not found in VBMAN.vbp' }
$tailLines = @($lines[$tailStart..($lines.Count - 1)])

# 2) 生成模块行 (支持相对路径 或 仅文件名)
$outLines = [System.Collections.Generic.List[string]]::new()
$outLines.Add('Type=OleDll')
foreach ($r in $refLines) { $outLines.Add($r) }
foreach ($m in $Modules) {
    $rel = $m -replace '\\', '/'
    if ($rel -notlike '*/*') {
        $rel = '*\' + $rel.ToLower()
    }
    $relNorm = $rel.Replace('/', '\').ToLower()
    $hit = @($modLineMap.Keys | Where-Object { $_ -eq $relNorm }) | Select-Object -First 1
    if (-not $hit) {
        $tail = $relNorm.Substring($relNorm.LastIndexOf('\') + 1)
        $hit = @($modLineMap.Keys | Where-Object { $_ -like "*\$tail" }) | Select-Object -First 1
    }
    if (-not $hit) {
        Write-Warning "module not found in VBMAN.vbp: $m"
        continue
    }
    $outLines.Add($modLineMap[$hit])
}
foreach ($t in $tailLines) { $outLines.Add($t) }

# 3) 写出 vbp
if (-not $Name) {
    $Name = [System.IO.Path]::GetFileNameWithoutExtension($Modules[0])
}
# vbp 必须位于 vbman/src 下, 模块/References 相对路径才有效
$outDir = Join-Path $fix $Name
New-Item -ItemType Directory -Force -Path $outDir | Out-Null
$vbpOut = Join-Path $src "FIX_$Name.vbp"
[System.IO.File]::WriteAllText($vbpOut, ($outLines -join "`r`n") + "`r`n")
$modCount = $outLines.Count - $tailLines.Count - $refLines.Count - 1
Write-Host "[OK] fix vbp: $vbpOut ($modCount modules)"

# 4) 编译
if ($Build) {
    $out = Join-Path $outDir 'out'
    $c3 = Join-Path $root '.build\C3.exe'
    $log = Join-Path $outDir 'run.log'
    $run = Join-Path $root 'scripts\_fix_run.cmd'
    $q = { param($s) '"' + $s + '"' }
    $vc = '"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat"'
    $cmdFile = "@echo off`r`ncall $vc x64 >nul 2>&1`r`n" +
        "$(& $q $c3) $(& $q $vbpOut) --dll --arch x64 --output-dir $(& $q $out) --no-warn 3001,3003 > $(& $q $log) 2>&1`r`n" +
        "exit /b %ERRORLEVEL%"
    [System.IO.File]::WriteAllText($run, $cmdFile)
    $p = Start-Process -FilePath $run -Wait -PassThru -WindowStyle Hidden
    if ($Show) {
        $errLog = Join-Path $out 'c3-error.log'
        if (Test-Path $errLog) {
            # cl 输出为 ANSI/GBK 编码
            $enc = [System.Text.Encoding]::GetEncoding(936)
            $el = [System.IO.File]::ReadAllLines($errLog, $enc)
            $errs = @($el | Where-Object { $_ -match '\.c\(\d+\): error C\d+' })
            Write-Host ("[FIX] exit=$($p.ExitCode) errors=" + $errs.Count)
            $errs | ForEach-Object { $_ }
        } else {
            Write-Host "[FIX] exit=$($p.ExitCode) no c3-error.log (可能编译通过)"
            $enc = [System.Text.Encoding]::GetEncoding(936)
            $txt = [System.IO.File]::ReadAllText($log, $enc)
            Write-Host ($txt -split "`r?`n" | Select-Object -Last 6)
        }
    } else {
        Write-Host "[FIX] exit=$($p.ExitCode)"
    }
}
