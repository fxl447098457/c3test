# ============================================================
# ai/022 B14: 真客户端用例（run_tests.ps1 用 `. $PSScriptRoot\disp_invoke.ps1` 引入）
#
# B13a-B13e 五格把 DLL 那条对外管线验到了**字节**（表里的 GUID、类型库里的引用），
# 但从来没有一个调用者真的拿着产出的指针走过一次 `GetIDsOfNames`/`Invoke`。
# 本格补的就是这一层：`tests\tools\disp_probe.c` 是个不查注册表的进程内客户端 ——
# `LoadLibrary` + `DllGetClassObject` + `CreateInstance(IID_IDispatch)`，然后把
# IDispatch 四件套各问一遍。CI 上不需要 regsvr32，也不依赖机器上装过什么组件。
#
# 断言只断"点得通 / 点不通"这两件事实，不断 dispid 的具体数值（那是 typelib 回写的
# 分配顺序，改收集口径就会动，属 B15 的地盘）。
# ============================================================

function Get-DispProbeExe {
    $src = Join-Path $Tests "tools\disp_probe.c"
    $exe = Join-Path $OutDir "disp_probe.exe"
    if ((Test-Path $exe) -and ((Get-Item $src).LastWriteTime -le (Get-Item $exe).LastWriteTime)) { return $exe }
    $prev = (Get-Location).Path
    Set-Location $OutDir          # cl 把 .obj 落在当前目录，跟着产物走就不会脏工作树
    & cl /nologo /W3 /O1 /utf-8 $src /Fe:$exe /link ole32.lib oleaut32.lib *> $null
    Set-Location $prev
    if (-not (Test-Path $exe)) { return $null }
    return $exe
}

function Test-DispatchInvoke {
    param(
        [string]$Name,
        [string]$VbpFile,
        [string]$DllName,             # 产出 DLL 的文件名主体（= 工程输出名）
        [string]$Clsid,               # 传给探针的 CLSID（带花括号）
        [string[]]$Needles,
        [string[]]$Absent = @(),
        [string]$ExtraIid = ""        # 非空 = 再问一次这枚 IID，看服务器答的是哪份指针
    )
    $script:total++
    Write-Host -NoNewline "  [DISPATCH] $Name ... "

    $out = & $C3 $VbpFile --output-dir $OutDir 2>&1
    if ($LASTEXITCODE -ne 0) {
        $script:fail++
        Write-Host "FAIL (compile)" -ForegroundColor Red
        if ($Verbose) { Write-Host (($out | Out-String) -replace '\s+', ' ') }
        return
    }
    $dllPath = Join-Path $OutDir "$DllName.dll"
    if (-not (Test-Path $dllPath)) {
        $script:fail++
        Write-Host "FAIL (no dll)" -ForegroundColor Red
        return
    }
    $probe = Get-DispProbeExe
    if (-not $probe) {
        $script:fail++
        Write-Host "FAIL (no probe)" -ForegroundColor Red
        return
    }
    if ($ExtraIid) { $text = ((& $probe $dllPath $Clsid $ExtraIid) | Out-String) }
    else { $text = ((& $probe $dllPath $Clsid) | Out-String) }
    if ($LASTEXITCODE -ne 0) {
        $script:fail++
        Write-Host "FAIL (probe exit=$LASTEXITCODE)" -ForegroundColor Red
        Write-Host (($text | Out-String) -replace '\s+', ' ') -ForegroundColor DarkGray
        return
    }
    $flat = $text -replace '\s+', ' '

    $detail = @()
    foreach ($n in $Needles) { if (-not $flat.Contains($n)) { $detail += "missing: $n" } }
    foreach ($a in $Absent)  { if ($flat.Contains($a))     { $detail += "unexpected: $a" } }
    if ($detail.Count -eq 0) {
        $script:pass++
        Write-Host "PASS (real in-proc IDispatch client)" -ForegroundColor Green
    } else {
        $script:fail++
        Write-Host "FAIL" -ForegroundColor Red
        if ($Verbose) { foreach ($d in $detail) { Write-Host "        $d" -ForegroundColor DarkGray } }
        else { Write-Host ("        " + ($detail -join '; ')) -ForegroundColor DarkGray }
        Write-Host ("        " + $flat.Trim()) -ForegroundColor DarkGray
    }
}
