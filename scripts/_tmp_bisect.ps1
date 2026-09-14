# _tmp_bisect.ps1 - vbman 编译回归 bisect (FIX_bisect.vbp)
#   -N <n>     取 VBMAN.vbp 中前 n 个 Class/Module 条目
#   -Forms     同时纳入 Form= 条目 (VBMAN.vbp 的 Form 行无分号, 默认正则要求分号故天然排除;
#              含窗体后 38 个条目 = 以 FLayer.frm 结尾, 即历史「窗体 release 崩溃」最小批次)
param([int]$N = 125, [switch]$Forms)
$root = Split-Path -Parent $PSScriptRoot
$src  = Join-Path $root 'archive\vbman\src'
$fix  = Join-Path $src '_fix\bisect'
$vbpSrc = Join-Path $src 'VBMAN.vbp'
$text = [System.IO.File]::ReadAllText($vbpSrc)
$lines = $text -split "`r?`n"
$refLines = @($lines | Where-Object { $_ -like 'Reference=*' })
$modLines = @()
foreach ($ln in $lines) {
    if ($Forms) {
        if ($ln -match '^(Class|Module|Form)=(.+)$') { $modLines += $ln }
    } else {
        if ($ln -match '^(Class|Module)=([^;]+);(.+)$') { $modLines += $ln }
    }
}
if ($N -gt $modLines.Count) { $N = $modLines.Count }
$tailStart = -1
for ($i = 0; $i -lt $lines.Count; $i++) {
    if ($lines[$i] -like 'ResFile32=*') { $tailStart = $i; break }
}
$tailLines = @($lines[$tailStart..($lines.Count - 1)])
$outLines = [System.Collections.Generic.List[string]]::new()
$outLines.Add('Type=OleDll')
foreach ($r in $refLines) { $outLines.Add($r) }
for ($i = 0; $i -lt $N; $i++) { $outLines.Add($modLines[$i]) }
foreach ($t in $tailLines) { $outLines.Add($t) }
New-Item -ItemType Directory -Force -Path $fix | Out-Null
$sfx = if ($Forms) { 'frm' } else { '' }
$vbpOut = Join-Path $src ("FIX_bisect{0}.vbp" -f $sfx)
[System.IO.File]::WriteAllText($vbpOut, ($outLines -join "`r`n") + "`r`n")
Write-Host "[OK] bisect vbp: $vbpOut ($N modules, forms=$($Forms.IsPresent))"
Write-Host ("[BISECT] 末条 = " + $modLines[$N-1])
$out = Join-Path $fix ("out{0}" -f $sfx)
$c3 = Join-Path $root '.build\C3.exe'
$log = Join-Path $fix ("run{0}.log" -f $sfx)
$run = Join-Path $root ("scripts\_fix_run{0}.cmd" -f $sfx)
$vc = '"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat"'
$q = { param($s) '"' + $s + '"' }
$cmdFile = "@echo off`r`ncall $vc x64 >nul 2>&1`r`n" +
    "$(& $q $c3) $(& $q $vbpOut) --dll --arch x64 --output-dir $(& $q $out) --no-warn 3001,3003 > $(& $q $log) 2>&1`r`n" +
    "exit /b %ERRORLEVEL%"
[System.IO.File]::WriteAllText($run, $cmdFile)
$p = Start-Process -FilePath $run -Wait -PassThru -WindowStyle Hidden
$hex = ('0x{0:X8}' -f $p.ExitCode)
Write-Host "[BISECT] C3 exit=$($p.ExitCode) ($hex)"
$errLog = Join-Path $out 'c3-error.log'
if (Test-Path $errLog) {
    $enc = [System.Text.Encoding]::GetEncoding(936)
    $el = [System.IO.File]::ReadAllLines($errLog, $enc)
    $errs = @($el | Where-Object { $_ -match '\.c\(\d+\): error C\d+' })
    Write-Host ("[BISECT] errors=" + $errs.Count)
    $errs | Group-Object { ($_ -match 'error (C\d{4})') | Out-Null; $Matches[1] } | Sort-Object Count -Descending | ForEach-Object { Write-Host ("{0} : {1}" -f $_.Name, $_.Count) }
} else {
    Write-Host "[BISECT] no c3-error.log"
}
