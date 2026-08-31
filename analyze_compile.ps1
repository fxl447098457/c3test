$f = 'C:\Users\vi\Desktop\c3.vb6.pro\vbman\dist\compile_run.log'
$lines = Get-Content $f
Write-Host ("TOTAL_LINES=" + $lines.Count)

# Find the cl.exe invocation line
$clIdx = -1
for ($i = 0; $i -lt $lines.Count; $i++) {
    if ($lines[$i] -match 'C3:.*cl\.exe') { $clIdx = $i; break }
}
Write-Host ("CL_CMD_INDEX=" + $clIdx)
if ($clIdx -ge 0) {
    Write-Host "=== CL command (truncated to 400 chars) ==="
    $cmd = $lines[$clIdx]
    if ($cmd.Length -gt 400) { $cmd = $cmd.Substring(0, 400) }
    Write-Host $cmd
    Write-Host ""
    Write-Host "=== Lines after CL command ==="
    $start = $clIdx + 1
    $end = [Math]::Min($clIdx + 120, $lines.Count - 1)
    for ($j = $start; $j -le $end; $j++) {
        Write-Host $lines[$j]
    }
} else {
    Write-Host "=== Searching for errors ==="
    $lines | Select-String -Pattern 'error C[0-9]|fatal error|LNK[0-9]{4}|c1xx|warning C' | Select-Object -First 60 | ForEach-Object { $_.Line }
}
