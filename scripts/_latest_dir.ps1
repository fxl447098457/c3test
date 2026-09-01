$d = Get-ChildItem 'C:\Users\vi\AppData\Local\Temp\C3C' -Directory | Sort-Object LastWriteTime -Descending | Select-Object -First 1
Write-Output $d.FullName
