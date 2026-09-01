$log = 'C:\Users\vi\Desktop\c3.vb6.pro\vbman\dist\c3-error.log'
$lines = Get-Content $log
$all = $lines | Where-Object { $_ -match 'error ' }
$c2440 = $lines | Where-Object { $_ -match 'error C2440' }
$fatal = $lines | Where-Object { $_ -match 'fatal error' }
Write-Host ("TOTAL error lines: " + $all.Count)
Write-Host ("C2440 count: " + $c2440.Count)
Write-Host ("fatal count: " + $fatal.Count)
Write-Host "--- first 15 C2440 ---"
$c2440 | Select-Object -First 15 | ForEach-Object { Write-Host $_ }
Write-Host "--- first 10 fatal ---"
$fatal | Select-Object -First 10 | ForEach-Object { Write-Host $_ }
