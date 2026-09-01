$log = 'C:\Users\vi\Desktop\c3.vb6.pro\vbman\dist\c3-error.log'
$lines = Get-Content $log
$c2440 = $lines | Where-Object { $_ -match 'error C2440' }
$c2440 | Select-Object -First 20 | ForEach-Object { $_ }
