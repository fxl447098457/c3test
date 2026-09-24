# ============================================================
# ai/022 B13c: 类型库探针用例（run_tests.ps1 用 `. $PSScriptRoot\tlb_identity.ps1` 引入）
#
# 证明的是本格那条判据：**一个接口在一次编译里只许一枚 GUID**，而且 COM 服务器表与 `.tlb`
# 逐值一致。三条通道各自从**产物**里读一遍，不读编译器内部状态：
#   1) `dll_entry.c` 的 `IID_vb6def_<实现类>`          —— 晚绑定/早绑定 QI 的默认接口 IID
#   2) `<实现类>.h` 的 `vb6_iv_iid_<接口>[16]`          —— 新式接口 vtable 的 QueryInterface
#   3) `<工程>.tlb` 的 `_<接口>` dispinterface 与 coclass 的 DEFAULT 引用 —— 客户端看到的
# coclass 的 CLSID 另外与表里的 `clsidStr` 对一次。
#
# 为什么非要自己写探针：D57-5 记的就是「表与 .tlb 是否同值」到今天没人证过 —— 类型库是
# `CreateTypeLib2` 写出来的二进制，`--emit-c` 不含它，仓里也没有 OleView/tlbimp 依赖。
# `tests\tools\tlbprobe.cpp` 就是那 15 行的读数工具：LoadTypeLib + GetTypeAttr。
# ============================================================

function GuidFromIidInitializer {
    param([string]$Text)
    # C 的 GUID 初始化器 {0xF5CEF988,0x3217,0x6173,{0x94,...}} -> 可读文本 {F5CEF988-...}
    if ($Text -notmatch '\{0x([0-9A-Fa-f]+),0x([0-9A-Fa-f]+),0x([0-9A-Fa-f]+),\{([^}]*)\}\}') { return "" }
    $d1 = $matches[1].ToUpper().PadLeft(8, '0')
    $d2 = $matches[2].ToUpper().PadLeft(4, '0')
    $d3 = $matches[3].ToUpper().PadLeft(4, '0')
    $bytes = @($matches[4] -split ',' | ForEach-Object { $_.Trim().Substring(2).ToUpper() })
    if ($bytes.Count -ne 8) { return "" }
    return "{" + $d1 + "-" + $d2 + "-" + $d3 + "-" + ($bytes[0] + $bytes[1]) + "-" + (($bytes[2..7]) -join "") + "}"
}

function GuidFromIvBytes {
    param([string]$Text)
    # 16 个原始字节 = COM 的落地序（Data1/2/3 小端、Data4 大端），交给 [Guid] 自己解释
    $ms = [regex]::Matches($Text, '0x([0-9A-Fa-f]{2})')
    if ($ms.Count -lt 16) { return "" }
    $arr = New-Object byte[] 16
    for ($i = 0; $i -lt 16; $i++) { $arr[$i] = [Convert]::ToByte($ms[$i].Groups[1].Value, 16) }
    return ([Guid]$arr).ToString('B').ToUpper()
}

function Get-TlbProbeExe {
    $src = Join-Path $Tests "tools\tlbprobe.cpp"
    $exe = Join-Path $OutDir "tlbprobe.exe"
    if ((Test-Path $exe) -and ((Get-Item $src).LastWriteTime -le (Get-Item $exe).LastWriteTime)) { return $exe }
    $prev = (Get-Location).Path
    Set-Location $OutDir          # cl 把 .obj 落在当前目录，跟着产物走就不会脏工作树
    & cl /nologo /EHsc /O1 /utf-8 $src /Fe:$exe /link ole32.lib oleaut32.lib *> $null
    Set-Location $prev
    if (-not (Test-Path $exe)) { return $null }
    return $exe
}

function Test-TlbIdentitySingleSource {
    param(
        [string]$Name,
        [string]$VbpFile,
        [string]$TlbName,       # .tlb 的文件名主体（= 工程输出名）
        [string]$ClassName,     # 实现类模块名
        [string]$IfaceName,     # 块里 [Default] 指的那个新式接口名
        [string]$ExpectedClsid  # 期望的 CLSID（带花括号、大写）
    )
    $script:total++
    Write-Host -NoNewline "  [TLB-ID] $Name ... "

    $out = & $C3 $VbpFile --output-dir $OutDir --keep-for-debug 2>&1
    if ($LASTEXITCODE -ne 0) {
        $script:fail++
        Write-Host "FAIL (compile)" -ForegroundColor Red
        return
    }
    $genDir = $null
    foreach ($line in $out) {
        $t = "$line"
        $at = $t.IndexOf("intermediates kept at: ")
        if ($at -ge 0) { $genDir = $t.Substring($at + 23).Trim(); break }
    }
    if (-not $genDir -or -not (Test-Path $genDir)) {
        $script:fail++
        Write-Host "FAIL (no intermediates)" -ForegroundColor Red
        return
    }
    $probe = Get-TlbProbeExe
    if (-not $probe) {
        $script:fail++
        Write-Host "FAIL (no probe)" -ForegroundColor Red
        return
    }
    $tlbPath = Join-Path $genDir "$TlbName.tlb"
    if (-not (Test-Path $tlbPath)) {
        $script:fail++
        Write-Host "FAIL (no tlb)" -ForegroundColor Red
        return
    }
    $tlbText = ((& $probe $tlbPath) | Out-String)
    $entry = ""
    $hdr = ""
    $p = Join-Path $genDir "dll_entry.c"
    if (Test-Path $p) { $entry = Get-Content $p -Raw }
    $p = Join-Path $genDir "$ClassName.h"
    if (Test-Path $p) { $hdr = Get-Content $p -Raw }

    # 通道 1：COM 服务器表
    $tableIid = ""
    if ($entry -match ("static const IID IID_vb6def_" + $ClassName + " = (\{[^;]*\})")) {
        $tableIid = GuidFromIidInitializer $matches[1]
    }
    # 通道 2：类自己的 vtable QI
    $ivIid = ""
    if ($hdr -match ("vb6_iv_iid_" + $IfaceName + "\[16\] = (\{[^}]*\})")) {
        $ivIid = GuidFromIvBytes $matches[1]
    }
    # 通道 3：类型库
    $tlbIface = ""
    if ($tlbText -match ("kind=dispinterface name=_" + $IfaceName + " guid=(\{[0-9A-F-]{36}\})")) {
        $tlbIface = $matches[1]
    }
    $coclassGuid = ""
    $defaultRef = ""
    $tlbLines = $tlbText -split "`r?`n"
    for ($i = 0; $i -lt $tlbLines.Count; $i++) {
        if ($tlbLines[$i] -notmatch ("kind=coclass name=" + $ClassName + " guid=(\{[0-9A-F-]{36}\})")) { continue }
        $coclassGuid = $matches[1]
        for ($j = $i + 1; $j -lt $tlbLines.Count; $j++) {
            if ($tlbLines[$j] -notmatch '^\s+REF') { break }
            if ($tlbLines[$j] -match ' DEFAULT' -and $tlbLines[$j] -notmatch 'SOURCE') {
                if ($tlbLines[$j] -match 'guid=(\{[0-9A-F-]{36}\})') { $defaultRef = $matches[1] }
            }
        }
        break
    }

    $detail = @()
    if (-not $tableIid)   { $detail += "dll_entry.c 里没有 IID_vb6def_$ClassName" }
    if (-not $ivIid)      { $detail += "$ClassName.h 里没有 vb6_iv_iid_$IfaceName" }
    if (-not $tlbIface)   { $detail += "类型库里读不到 _$IfaceName 的 GUID" }
    if (-not $coclassGuid){ $detail += "类型库里读不到 coclass $ClassName" }
    if (-not $defaultRef) { $detail += "coclass $ClassName 没有 DEFAULT 接口引用" }
    if ($tableIid -and $ivIid -and $tableIid -ne $ivIid) {
        $detail += "表与 vtable 不同值: $tableIid vs $ivIid"
    }
    if ($tableIid -and $tlbIface -and $tableIid -ne $tlbIface) {
        $detail += "表与类型库接口不同值: $tableIid vs $tlbIface"
    }
    if ($tableIid -and $defaultRef -and $tableIid -ne $defaultRef) {
        $detail += "表的默认接口 IID 不等于类型库的 DEFAULT 引用: $tableIid vs $defaultRef"
    }
    if ($coclassGuid -ne $ExpectedClsid) { $detail += "类型库 coclass = $coclassGuid，期望 $ExpectedClsid" }
    if ($entry -notmatch ('"' + [regex]::Escape($ExpectedClsid) + '",\s*/\*\s*clsidStr')) {
        $detail += "dll_entry.c 的 clsidStr 不是 $ExpectedClsid"
    }

    if ($detail.Count -eq 0) {
        $script:pass++
        Write-Host "PASS (one IID across table/vtable/typelib)" -ForegroundColor Green
    } else {
        $script:fail++
        Write-Host "FAIL" -ForegroundColor Red
        if ($Verbose) { foreach ($d in $detail) { Write-Host "        $d" -ForegroundColor DarkGray } }
        else { Write-Host ("        " + ($detail -join '; ')) -ForegroundColor DarkGray }
    }
}
