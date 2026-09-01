$exe = Get-Item 'C:\Users\vi\Desktop\c3.vb6.pro\.build\C3.exe'
Write-Output ('C3.exe last write: ' + $exe.LastWriteTime)
Write-Output ('Now: ' + (Get-Date))
$src = Get-Item 'C:\Users\vi\Desktop\c3.vb6.pro\src\backend\cgen_stmt.cpp'
Write-Output ('cgen_stmt.cpp last write: ' + $src.LastWriteTime)
$u = Get-Item 'C:\Users\vi\Desktop\c3.vb6.pro\src\backend\cgen_util.cpp'
Write-Output ('cgen_util.cpp last write: ' + $u.LastWriteTime)
