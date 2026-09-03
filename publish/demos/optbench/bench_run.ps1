# bench_run.ps1 -Tag <name> -C3 <path> -Extra <args> -Clean <0|1>
param([string]$Tag = 'run', [string]$C3 = '', [string]$Extra = '', [int]$Clean = 0)
if ($C3 -eq '') { $C3 = 'C:\Users\vi\Desktop\c3.vb6.pro\.build\C3.exe' }
$bat = 'C:\Users\vi\Desktop\c3.vb6.pro\publish\demos\optbench\bench.bat'
$log = 'C:\Users\vi\Desktop\c3.vb6.pro\publish\demos\optbench\bench_' + $Tag + '.log'
$sw = [System.Diagnostics.Stopwatch]::StartNew()
& cmd /c "call `"$bat`" `"$C3`" `"$Extra`" `"$Clean`" > `"$log`" 2>&1"
$sw.Stop()
Write-Host ('TAG=' + $Tag + ' RC=' + $LASTEXITCODE + ' SEC=' + [math]::Round($sw.Elapsed.TotalSeconds, 1))
