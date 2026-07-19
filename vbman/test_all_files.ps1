# ============================================================
#  test_all_files.ps1 - 逐文件语法检查，找出所有问题文件
# ============================================================

$C3 = "C:\Users\vi\Desktop\c3.vb6.pro\.build\C3.exe"
$Src = "C:\Users\vi\Desktop\c3.vb6.pro\vbman\src"
$LogFile = "C:\Users\vi\Desktop\c3.vb6.pro\vbman\c3log\batch_syntax_result.txt"

# Parse VBP to get file list
$vbpContent = Get-Content "$Src\VBMAN.vbp" -Encoding Default
$files = @()
foreach ($line in $vbpContent) {
    if ($line -match '^(Module|Class|Form)=(.+?);\s*(.+)$') {
        $type = $matches[1]
        $name = $matches[2].Trim()
        $path = $matches[3].Trim()
        $fullPath = Join-Path $Src $path
        $files += [PSCustomObject]@{
            Type = $type
            Name = $name
            Path = $fullPath
            RelPath = $path
        }
    }
}

Write-Host "Found $($files.Count) source files in VBP"
Write-Host ""

$results = @()
$passed = 0
$failed = 0
$crashed = 0

foreach ($f in $files) {
    $processInfo = New-Object System.Diagnostics.ProcessStartInfo
    $processInfo.FileName = $C3
    $processInfo.Arguments = "`"$($f.Path)`" --syntax-only"
    $processInfo.UseShellExecute = $false
    $processInfo.RedirectStandardOutput = $true
    $processInfo.RedirectStandardError = $true
    $processInfo.CreateNoWindow = $true

    $process = New-Object System.Diagnostics.Process
    $process.StartInfo = $processInfo

    $stdoutBuilder = New-Object System.Text.StringBuilder
    $stderrBuilder = New-Object System.Text.StringBuilder
    $stdOutAction = [System.EventHandler]{ param($s,$e) $stdoutBuilder.AppendLine($e.Data) | Out-Null }
    $stdErrAction = [System.EventHandler]{ param($s,$e) $stderrBuilder.AppendLine($e.Data) | Out-Null }
    $process.Add_OutputDataReceived($stdOutAction)
    $process.Add_ErrorDataReceived($stdErrAction)

    $process.Start() | Out-Null
    $process.BeginOutputReadLine()
    $process.BeginErrorReadLine()
    $process.WaitForExit(15000) | Out-Null

    if ($process.HasExited -eq $false) {
        $process.Kill()
        $status = "TIMEOUT"
        $crashed++
    } elseif ($process.ExitCode -eq 0) {
        $status = "PASS"
        $passed++
    } elseif ($process.ExitCode -eq -1073741819) {
        $status = "CRASH(0xC0000005)"
        $crashed++
    } else {
        $status = "FAIL(exit=$($process.ExitCode))"
        $failed++
    }

    $errors = $stderrBuilder.ToString().Trim()
    if (-not $errors) { $errors = $stdoutBuilder.ToString().Trim() }

    $results += [PSCustomObject]@{
        Status = $status
        Type = $f.Type
        Name = $f.Name
        File = $f.RelPath
        Errors = ($errors -split "`n" | Select-Object -First 5) -join " | "
    }

    $icon = switch -Wildcard ($status) {
        "PASS" { "[OK]" }
        "CRASH*" { "[!!!]" }
        "TIMEOUT" { "[T/O]" }
        default { "[ERR]" }
    }
    Write-Host "$icon $($f.Name) ($($f.RelPath)) -> $status"

    $process.Close()
}

Write-Host ""
Write-Host "===== Summary ====="
Write-Host "Total: $($files.Count)  Passed: $passed  Failed: $failed  Crashed: $crashed"
Write-Host ""

# Save full results
$results | Export-Csv $LogFile -NoTypeInformation -Encoding UTF8
Write-Host "Full results saved to: $LogFile"
