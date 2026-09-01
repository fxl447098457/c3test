$log = 'C:\Users\vi\Desktop\c3.vb6.pro\vbman\dist\c3-error.log'
$lines = Get-Content $log
Write-Host "--- C2440 in files (grouped) ---"
$lines | Where-Object { $_ -match 'error C2440' } | ForEach-Object {
    if ($_ -match '([^/\\]+\.c)\((\d+)\)') {
        $f = $Matches[1]; $ln = [int]$Matches[2]
        Write-Host ($f + ":" + $ln)
    }
} | Group-Object | Sort-Object Count -Descending | Select-Object -First 30 | ForEach-Object { Write-Host ($_.Name + "  x" + $_.Count) }
