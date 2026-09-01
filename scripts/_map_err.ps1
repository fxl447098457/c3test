# Map C2440 error lines to generated source, flag setter-related lines (ASCII only)
param([string]$Log = "C:\Users\vi\Desktop\c3.vb6.pro\vbman\dist\c3-error.log",
      [string]$Dir = "C:\Users\vi\AppData\Local\Temp\C3C\87055629193100")
$lines = Get-Content $Log
$count = 0
$setterCount = 0
$cache = @{}
foreach ($l in $lines) {
    if ($l -match '([A-Za-z0-9_]+\.c)\((\d+)\): error C2440') {
        $count++
        $f = $Matches[1]
        $ln = [int]$Matches[2]
        $full = Join-Path $Dir $f
        $srcLine = ""
        if (Test-Path $full) {
            if (-not $cache.ContainsKey($full)) { $cache[$full] = Get-Content $full }
            $fc = $cache[$full]
            if ($ln -le $fc.Count) { $srcLine = $fc[$ln - 1] }
        }
        $isSetter = $srcLine -match 'prop_set_|prop_let_|prop_get_|Set Property|Property Let|Property Set|evt_wrap|events->'
        if ($isSetter) {
            $setterCount++
            Write-Output ("SETTER  {0}({1}): {2}" -f $f, $ln, $srcLine.Trim())
        }
    }
}
Write-Output ""
Write-Output ("TOTAL C2440: {0}  SETTER/EVENT-RELATED: {1}" -f $count, $setterCount)
