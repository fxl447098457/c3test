param([string]$LogPath = 'C:\Users\vi\Desktop\c3.vb6.pro\vbman\dist\c3-error.log')
if (-not (Test-Path $LogPath)) { Write-Output "NO LOG: $LogPath"; exit 1 }
$lines = Select-String -Path $LogPath -Pattern 'error C2440'
Write-Output ("C2440 total: " + @($lines).Count)
$files = @{}
foreach ($l in $lines) {
    if ($l.Line -match '([A-Za-z0-9_]+\.c)\(\d+\)') {
        $f = $matches[1]
        if ($files.ContainsKey($f)) { $files[$f]++ } else { $files[$f] = 1 }
    }
}
$files.GetEnumerator() | Sort-Object Value -Descending | ForEach-Object { Write-Output ($_.Key + " : " + $_.Value) }
