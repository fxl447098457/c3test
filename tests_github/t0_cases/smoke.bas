' ============================================================
'  smoke.bas - C3 冒烟测试 (构建产物全链路快速验证)
'
'  用途: 每次改完编译器 / 重建 C3.exe 之后, 用最快的方式确认
'        "解析 -> 生成C -> cl/link -> 运行输出" 整条链路可用。
'
'  运行方式:
'    scripts\test.bat smoke              ' 只跑冒烟 (推荐)
'    scripts\dev.ps1 -TestCategory smoke ' Agent 会话内
'
'  编写约束 (重要, 改这个文件时请遵守):
'    1. 禁止 MsgBox / InputBox / DoEvents 等需要人工交互的语句。
'       run_tests.ps1 的 Test-Run 用 Start-Process -Wait 收集输出,
'       弹窗没人点会把整个测试进程挂死
'       (publish\demos\hello\hello.bas 里的 MsgBox 就是现成反例)。
'    2. 只通过 Debug.Print 输出, 且必须包含 SMOKE-1:OK / SMOKE-2:OK /
'       SMOKE-3:OK / SMOKE PASS, run_tests.ps1 依赖这些串做输出校验。
'    3. 保持轻量: 不写文件、不碰注册表、不依赖外部 COM, 全程毫秒级。
' ============================================================

Option Explicit

Public Sub Main()
    ' 1. 字符串 + 条件分支
    Dim s As String
    s = "C3"
    If s = "C3" And Len(s) = 2 Then
        Debug.Print "SMOKE-1:OK"
    End If

    ' 2. 循环 + 数值累加
    Dim i As Long
    Dim total As Long
    total = 0
    For i = 1 To 100
        total = total + i
    Next i
    If total = 5050 Then
        Debug.Print "SMOKE-2:OK"
    End If

    ' 3. 函数调用
    Dim r As Long
    r = Add(17, 25)
    Debug.Print "SMOKE-3:OK "; r

    Debug.Print "SMOKE PASS"
End Sub

Private Function Add(ByVal a As Long, ByVal b As Long) As Long
    Add = a + b
End Function
