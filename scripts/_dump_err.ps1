param([string]$LogPath = 'C:\Users\vi\Desktop\c3.vb6.pro\vbman\dist\c3-error.log', [string]$Filter = '')
$lines = Select-String -Path $LogPath -Pattern 'error C2440'
foreach ($l in $lines) {
    $m = [regex]::Match($l.Line, '([A-Za-z0-9_\\/]+\.c)\((\d+)\):\s*error C2440: (.*)')
    if ($m.Success) {
        $f = [System.IO.Path]::GetFileName($m.Groups[1].Value)
        $msg = $m.Groups[3].Value
        if ($Filter -ne '' -and $msg -notmatch $Filter -and $f -notmatch $Filter) { continue }
        Write-Output ($f + "|" + $m.Groups[2].Value + "|" + $msg)
    }
}
