# gen_di_stubs.ps1 - generate Win32 forwarding stubs for `Declare` symbols.
#
# Why: C3 maps every `Declare ... Lib "x"` to `extern <ret> __stdcall vb6_di_<alias>(...)`
# (Fix 076) and the RTL must provide the implementation. Hand-writing them one by one is
# what kept the link phase red, so this tool derives each stub from C3's own generated
# header prototype (which is the ABI the caller actually uses) and forwards 1:1.
#
# Usage:
#   powershell -File scripts\gen_di_stubs.ps1 [-SessionDir <dir>] [-SymFile <txt>] [-OutFile <c>]
#   -SessionDir defaults to the newest %TEMP%\C3C\<id> (where C3 puts generated .c/.h)
#   -SymFile    defaults to .temp\unresolved_syms.txt (from _tmp_unresolved.ps1)
#
# ASCII-only file (PowerShell 5.1 reads .ps1 as ANSI without BOM).
param(
    [string]$SessionDir = '',
    [string]$SymFile = '',
    [string]$OutFile = ''
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$utf8 = [System.Text.Encoding]::UTF8

if ($SessionDir -eq '') {
    $base = Join-Path $env:LOCALAPPDATA 'Temp\C3C'
    if (-not (Test-Path $base)) { $base = Join-Path $env:TEMP 'C3C' }
    $SessionDir = (Get-ChildItem $base -Directory | Sort-Object LastWriteTime -Descending |
        Select-Object -First 1).FullName
}
if ($SymFile -eq '') { $SymFile = Join-Path $root '.temp\unresolved_syms.txt' }
if ($OutFile -eq '') { $OutFile = Join-Path $root 'src\rtl\core\vb6_di_win32_stubs.c' }

Write-Output "session: $SessionDir"
Write-Output "symbols: $SymFile"

# ---------- 1. index prototypes from generated headers ----------
$protos = @{}
$headers = Get-ChildItem $SessionDir -File | Where-Object { $_.Extension -eq '.h' }
foreach ($f in $headers) {
    $ls = [System.IO.File]::ReadAllLines($f.FullName, $utf8)
    for ($i = 0; $i -lt $ls.Count; $i++) {
        $l = $ls[$i]
        if ($l -notmatch '^\s*extern\b') { continue }
        if ($l -notmatch 'vb6_di_') { continue }
        $buf = $l.Trim()
        $j = $i
        while ((($buf.ToCharArray() | Where-Object { $_ -eq '(' }).Count) -gt
               (($buf.ToCharArray() | Where-Object { $_ -eq ')' }).Count) -and $j + 1 -lt $ls.Count) {
            $j++
            $buf = $buf + ' ' + $ls[$j].Trim()
        }
        $m = [regex]::Match($buf, '^extern\s+(.+?)\s+(?:__stdcall\s+)?(vb6_di_[A-Za-z0-9_]+)\s*\(')
        if (-not $m.Success) { continue }
        $name = $m.Groups[2].Value
        if ($protos.ContainsKey($name)) { continue }
        $ret = $m.Groups[1].Value.Trim()
        $pOpen = $buf.IndexOf('(', $m.Index + $m.Length - 1)
        $pClose = $buf.LastIndexOf(')')
        if ($pOpen -lt 0 -or $pClose -le $pOpen) { continue }
        $params = $buf.Substring($pOpen + 1, $pClose - $pOpen - 1).Trim()
        if ($params -eq 'void') { $params = '' }
        $protos[$name] = @{ ret = $ret; params = $params }
    }
}
Write-Output ("prototypes found: " + $protos.Count)

# ---------- 2. read unresolved symbol list ----------
$syms = @()
foreach ($ln in [System.IO.File]::ReadAllLines($SymFile, $utf8)) {
    $t = ($ln -replace '^\s*\d+\s+', '').Trim()
    if ($t -ne '') { $syms += $t }
}

# ---------- 3. split parameter list on top-level commas ----------
function Split-Params([string]$p) {
    if ($p.Trim() -eq '') { return @() }
    $out = @()
    $depth = 0
    $cur = ''
    foreach ($ch in $p.ToCharArray()) {
        if ($ch -eq '(' -or $ch -eq '[') { $depth++ }
        if ($ch -eq ')' -or $ch -eq ']') { $depth-- }
        if ($ch -eq ',' -and $depth -eq 0) { $out += $cur; $cur = ''; continue }
        $cur += $ch
    }
    if ($cur.Trim() -ne '') { $out += $cur }
    return $out
}

# ---------- 4. emit ----------
$bodies = New-Object System.Collections.Generic.List[string]
$skipped = New-Object System.Collections.Generic.List[string]
$noProto = New-Object System.Collections.Generic.List[string]
$badUdt = New-Object System.Collections.Generic.List[string]
$seen = @{}
$hasDynamic = $false

foreach ($s in $syms) {
    if (-not $s.StartsWith('vb6_di_')) { continue }
    if ($seen.ContainsKey($s)) { continue }
    $seen[$s] = 1
    if (-not $protos.ContainsKey($s)) { $noProto.Add($s); continue }

    $api = $s.Substring(7)
    if ($api -like 'ord_*') { $skipped.Add($s + '  (ordinal import)'); continue }

    $ret = $protos[$s].ret
    $paramList = Split-Params $protos[$s].params
    $declParts = @()
    $typeParts = @()
    $nameParts = @()
    $n = 0
    $udtBad = $false
    foreach ($p in $paramList) {
        $pt = $p.Trim()
        $n++
        $mm = [regex]::Match($pt, '([A-Za-z_][A-Za-z0-9_]*)\s*$')
        if ($mm.Success -and $mm.Index -gt 0) {
            $pname = $mm.Groups[1].Value
            $ptype = $pt.Substring(0, $mm.Index).Trim()
        } else {
            $pname = 'p' + $n
            $ptype = $pt
        }
        if ($ptype -eq '') { $ptype = 'intptr_t' }
        # VB UDTs (vb6_type_xxx) are project-generated structs the RTL cannot see.
        # Every occurrence here is by-pointer, so void* is ABI-identical.
        if ($ptype -match 'vb6_type_[A-Za-z0-9_]*\s*\*') {
            $ptype = [regex]::Replace($ptype, 'vb6_type_[A-Za-z0-9_]*\s*\*', 'void*')
        } elseif ($ptype -match 'vb6_type_[A-Za-z0-9_]*') {
            $udtBad = $true
        }
        $declParts += ($ptype + ' ' + $pname)
        $typeParts += $ptype
        $nameParts += $pname
    }
    if ($udtBad) { $badUdt.Add($s + '  (by-value VB UDT parameter)'); continue }
    $decl = ($declParts -join ', ')
    $types = ($typeParts -join ', ')
    if ($types -eq '') { $types = 'void' }
    $args = ($nameParts -join ', ')

    # gdiplus: no C header in the SDK, so resolve the flat API lazily by name.
    $dyn = ($api -match '^(Gdip|Gdiplus)')
    if ($dyn) { $hasDynamic = $true }

    $bodies.Add('/* ' + ($s -replace '^vb6_di_', '') + ' */')
    if ($dyn) {
        if ($ret -eq 'void') {
            $bodies.Add('void __stdcall ' + $s + '(' + $decl + ') {')
            $bodies.Add('    void (WINAPI *fn)(' + $types + ') = (void (WINAPI *)(' + $types + '))vb6_di_gdiplus_proc("' + $api + '");')
            $bodies.Add('    if (fn != NULL) { fn(' + $args + '); }')
            $bodies.Add('}')
        } else {
            $bodies.Add($ret + ' __stdcall ' + $s + '(' + $decl + ') {')
            $bodies.Add('    ' + $ret + ' (WINAPI *fn)(' + $types + ') = (' + $ret + ' (WINAPI *)(' + $types + '))vb6_di_gdiplus_proc("' + $api + '");')
            $bodies.Add('    if (fn == NULL) { return (' + $ret + ')2; /* GpStatus InvalidParameter */ }')
            $bodies.Add('    return fn(' + $args + ');')
            $bodies.Add('}')
        }
    } elseif ($ret -eq 'void') {
        $bodies.Add('void __stdcall ' + $s + '(' + $decl + ') {')
        $bodies.Add('    ((void (WINAPI *)(' + $types + '))' + $api + ')(' + $args + ');')
        $bodies.Add('}')
    } else {
        $bodies.Add($ret + ' __stdcall ' + $s + '(' + $decl + ') {')
        $bodies.Add('    return ((' + $ret + ' (WINAPI *)(' + $types + '))' + $api + ')(' + $args + ');')
        $bodies.Add('}')
    }
    $bodies.Add('')
}

$banner = @()
$banner += '// vb6_di_win32_stubs.c - generated by scripts/gen_di_stubs.ps1'
$banner += '//'
$banner += '// Win32 forwarding stubs for VB6 `Declare ... Lib "x"` (Fix 076 scheme: C3 emits'
$banner += '// `extern <ret> __stdcall vb6_di_<alias>(...)` and the RTL implements it).'
$banner += '//'
$banner += '// Each stub reproduces C3''s own generated prototype verbatim (that is the ABI the'
$banner += '// caller uses: ByVal Long is widened to intptr_t, ByVal Single stays float, ByRef'
$banner += '// Long stays int32_t*) and forwards 1:1 to the real API through a function-pointer'
$banner += '// cast. The cast keeps the compiler from complaining about unrelated API parameter'
$banner += '// types while preserving the register/memory passing class of every argument.'
$banner += '//'
$banner += ('// generated from: ' + $SessionDir)
$banner += ('// date: ' + (Get-Date -Format 'yyyy-MM-dd HH:mm'))
$banner += '//'
$banner += '// Hand-maintained special cases stay in vb6_di_stubs.c (ordinals, msvbvm60 runtime,'
$banner += '// dynamically loaded DLLs). Re-run the generator after a build exposes new symbols.'
$banner += ''
$banner += '/* VB6 code uses the classic winsock names (gethostbyname, inet_addr, ...) */'
$banner += '#define _WINSOCK_DEPRECATED_NO_WARNINGS'
$banner += '/* winsock2.h must come before windows.h */'
$banner += '#include <winsock2.h>'
$banner += '#include <windows.h>'
$banner += '#include <stdint.h>'
$banner += '#include <shlwapi.h>'
$banner += '#include <shlobj.h>'
$banner += '#include <mmsystem.h>'
$banner += '#include <dbghelp.h>'
$banner += '#include <iphlpapi.h>'
$banner += '#include <bcrypt.h>'
$banner += '#include <ncrypt.h>'
$banner += '#include <ole2.h>'
$banner += '#include <oleauto.h>'
$banner += '#include <olectl.h>'
$banner += '/* NOTE: gdiplus.h is C++-only (class Gdiplus...). The GDI+ flat API is'
$banner += ' * resolved with GetProcAddress instead - see below. */'
$banner += ''
$banner += '/* Import libs for the DLLs below. Adding one that is not needed is harmless:'
$banner += ' * a static import lib is only pulled when a symbol from it is referenced. */'
$banner += '#pragma comment(lib, "user32.lib")'
$banner += '#pragma comment(lib, "gdi32.lib")'
$banner += '#pragma comment(lib, "kernel32.lib")'
$banner += '#pragma comment(lib, "advapi32.lib")'
$banner += '#pragma comment(lib, "ole32.lib")'
$banner += '#pragma comment(lib, "oleaut32.lib")'
$banner += '#pragma comment(lib, "shell32.lib")'
$banner += '#pragma comment(lib, "shlwapi.lib")'
$banner += '#pragma comment(lib, "winmm.lib")'
$banner += '#pragma comment(lib, "ws2_32.lib")'
$banner += '#pragma comment(lib, "iphlpapi.lib")'
$banner += '#pragma comment(lib, "dbghelp.lib")'
$banner += '#pragma comment(lib, "crypt32.lib")'
$banner += '#pragma comment(lib, "bcrypt.lib")'
$banner += '#pragma comment(lib, "ncrypt.lib")'
$banner += '#pragma comment(lib, "uuid.lib")'
$banner += ''

if ($hasDynamic) {
    $banner += '/* GDI+ flat API lives in gdiplus.dll but its header is C++-only, so the'
    $banner += ' * symbols below are resolved by name at first use. */'
    $banner += 'static void* vb6_di_gdiplus_proc(const char* name) {'
    $banner += '    static HMODULE mod = NULL;'
    $banner += '    if (mod == NULL) { mod = LoadLibraryA("gdiplus.dll"); }'
    $banner += '    return (mod != NULL) ? (void*)GetProcAddress(mod, name) : NULL;'
    $banner += '}'
    $banner += ''
}

$content = ($banner + $bodies) -join "`r`n"
[System.IO.File]::WriteAllText($OutFile, $content, (New-Object System.Text.UTF8Encoding($false)))

Write-Output ("stubs emitted: " + ($bodies | Where-Object { $_ -like '*/' }).Count)
Write-Output ("written: " + $OutFile)
Write-Output ''
Write-Output '=== skipped (need hand-written stub) ==='
$skipped | ForEach-Object { Write-Output ('  ' + $_) }
Write-Output ''
Write-Output '=== vb6_di_* without generated prototype ==='
$noProto | ForEach-Object { Write-Output ('  ' + $_) }
Write-Output ''
Write-Output '=== skipped: by-value VB UDT parameter ==='
$badUdt | ForEach-Object { Write-Output ('  ' + $_) }
