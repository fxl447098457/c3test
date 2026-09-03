# 生成 optbench 基准项目: N 个保守语法 .bas 模块 + Main + VBP
# 用法: powershell -ExecutionPolicy Bypass -File gen_modules.ps1 -Count 40
param([int]$Count = 40)

$dir = Split-Path -Parent $MyInvocation.MyCommand.Path
$srcDir = Join-Path $dir 'src'
if (-not (Test-Path $srcDir)) { New-Item -ItemType Directory -Path $srcDir | Out-Null }

# 清理旧模块
Get-ChildItem $srcDir -Filter 'M*.bas' | Remove-Item -Force

function New-ModuleBody($i) {
    $p = $i.ToString('000')
    $seed = 1000 + $i
    $body = @()
    $body += "' M$p - benchmark module $p (generated, conservative VB6 syntax)"
    $body += 'Option Explicit'
    $body += ''
    $body += 'Public Function M' + $p + '_Sum(ByVal n As Long) As Long'
    $body += '    Dim i As Long, acc As Long'
    $body += '    acc = 0'
    $body += '    For i = 1 To n'
    $body += '        acc = acc + i'
    $body += '    Next i'
    $body += '    M' + $p + '_Sum = acc'
    $body += 'End Function'
    $body += ''
    $body += 'Public Function M' + $p + '_Fib(ByVal n As Long) As Long'
    $body += '    Dim a As Long, b As Long, t As Long, i As Long'
    $body += '    a = 0: b = 1'
    $body += '    For i = 2 To n'
    $body += '        t = a + b'
    $body += '        a = b'
    $body += '        b = t'
    $body += '    Next i'
    $body += '    If n <= 0 Then M' + $p + '_Fib = 0 Else M' + $p + '_Fib = b'
    $body += 'End Function'
    $body += ''
    $body += 'Public Function M' + $p + '_Concat(ByVal s As String, ByVal k As Long) As String'
    $body += '    Dim out As String, i As Long'
    $body += '    out = s'
    $body += '    For i = 1 To k'
    $body += '        out = out & "-" & CStr(i)'
    $body += '    Next i'
    $body += '    M' + $p + '_Concat = out'
    $body += 'End Function'
    $body += ''
    $body += 'Public Function M' + $p + '_Avg(ByVal n As Long) As Double'
    $body += '    Dim i As Long, acc As Double'
    $body += '    acc = 0'
    $body += '    For i = 1 To n'
    $body += '        acc = acc + Sqr(i)'
    $body += '    Next i'
    $body += '    If n > 0 Then M' + $p + '_Avg = acc / n Else M' + $p + '_Avg = 0'
    $body += 'End Function'
    $body += ''
    $body += 'Public Function M' + $p + '_Pick(ByVal a As Long, ByVal b As Long, ByVal c As Long) As Long'
    $body += '    Dim m As Long'
    $body += '    m = a'
    $body += '    If b > m Then m = b'
    $body += '    If c > m Then m = c'
    $body += '    M' + $p + '_Pick = m'
    $body += 'End Function'
    $body += ''
    $body += 'Public Function M' + $p + '_Run(ByVal n As Long) As Long'
    $body += '    Dim total As Long, s As String, d As Double'
    $body += '    total = M' + $p + '_Sum(n) + M' + $p + '_Fib(n) + M' + $p + '_Pick(n, n * 2, n * 3)'
    $body += '    s = M' + $p + '_Concat("m' + $p + '", 3)'
    $body += '    d = M' + $p + '_Avg(n)'
    $body += '    If Len(s) > 0 Then total = total + Len(s)'
    $body += '    If d > 0 Then total = total + CLng(d)'
    $body += '    M' + $p + '_Run = total'
    $body += 'End Function'
    $body += ''
    $body += 'Public Function M' + $p + '_Identity(ByVal x As Long) As Long'
    $body += '    M' + $p + '_Identity = x'
    $body += 'End Function'
    return $body -join "`r`n"
}

$vbp = @()
$vbp += 'Type=Exe'
$vbp += 'Form='  # no forms
for ($i = 1; $i -le $Count; $i++) {
    $p = $i.ToString('000')
    $file = "M$p.bas"
    $content = New-ModuleBody $i
    [System.IO.File]::WriteAllText((Join-Path $srcDir $file), $content, [System.Text.UTF8Encoding]::new($false))
    $vbp += "Module=M$p; src\$file"
}
$mainFile = 'Main.bas'
$main = @()
$main += "' Main.bas - calls every module to prevent dead-code elimination"
$main += 'Option Explicit'
$main += ''
$main += 'Public Sub Main()'
$main += '    Dim total As Long, i As Long'
$main += '    total = 0'
for ($i = 1; $i -le $Count; $i++) {
    $p = $i.ToString('000')
    $main += "    total = total + M${p}_Run(100) + M${p}_Identity($i)"
}
$main += '    Debug.Print "total="; total'
$main += 'End Sub'
[System.IO.File]::WriteAllText((Join-Path $srcDir $mainFile), ($main -join "`r`n"), [System.Text.UTF8Encoding]::new($false))
$vbp += "Module=Main; src\$mainFile"
$vbp += 'Startup="Sub Main"'
$vbp += 'Name="OptBench"'
$vbp += 'ExeName32="OptBench.exe"'
[System.IO.File]::WriteAllText((Join-Path $dir 'OptBench.vbp'), ($vbp -join "`r`n"), [System.Text.UTF8Encoding]::new($false))

Write-Host ("GENERATED modules=" + $Count + " -> " + $dir)
