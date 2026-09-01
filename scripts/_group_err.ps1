# Group C2440 errors by file and message pattern (ASCII only)
param([string]$Log = "C:\Users\vi\Desktop\c3.vb6.pro\vbman\dist\c3-error.log")
$lines = Get-Content $Log
$byFile = @{}
$byMsg = @{}
foreach ($l in $lines) {
    if ($l -match 'error C2440') {
        # file:line extraction: "path\file.c(123): error C2440: msg"
        if ($l -match '([A-Za-z0-9_]+\.c)\((\d+)\): error C2440') {
            $f = $Matches[1]
            $ln = [int]$Matches[2]
            if (-not $byFile.ContainsKey($f)) { $byFile[$f] = 0 }
            $byFile[$f]++
        }
        # message key: normalize numbers/variables -> first 60 chars after "C2440:"
        $idx = $l.IndexOf('error C2440')
        if ($idx -ge 0) {
            $msg = $l.Substring($idx + 11).Trim()
            if ($msg.Length -gt 70) { $msg = $msg.Substring(0, 70) }
            if (-not $byMsg.ContainsKey($msg)) { $byMsg[$msg] = 0 }
            $byMsg[$msg]++
        }
    }
}
Write-Output "=== TOTAL C2440: $($lines | Where-Object { $_ -match 'error C2440' } | Measure-Object).Count ==="
Write-Output ""
Write-Output "=== BY FILE ==="
$byFile.GetEnumerator() | Sort-Object Value -Descending | ForEach-Object {
    Write-Output ("{0,5}  {1}" -f $_.Value, $_.Key)
}
Write-Output ""
Write-Output "=== BY MESSAGE (top 40) ==="
$byMsg.GetEnumerator() | Sort-Object Value -Descending | Select-Object -First 40 | ForEach-Object {
    Write-Output ("{0,5}  {1}" -f $_.Value, $_.Key)
}
