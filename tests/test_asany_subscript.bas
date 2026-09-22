Option Explicit

' ============================================================
'  test_asany_subscript.bas - Declare "As Any" ByRef 实参的左值判定
'
'  复现目标 (VBFlexGridDemo 实测现象): 鼠标一放到网格上进程就 0xC0000005 退出。
'  VBFlexGrid.ctl:28348
'      With NMTTDI
'          CopyMemory .szText(0), ByVal StrPtr(Text), LenB(Text)   ' szText() As Byte
'      End With
'  经 With 展开成 _vb6_with_776->szText[0]。代码生成把带 [] 下标的链判为"非左值",
'  于是 As Any ByRef 实参写成 (void*)(intptr_t)(szText[0]) —— 传的是首元素的**值**
'  而不是地址; NMTTDI 已清零, 该值即 0 → RtlMoveMemory 目的地址 0x0 → 写空指针。
'  正确形态是 (void*)&(szText[0])。
'
'  同时守住三条不受影响的既有口径: 标量变量、UDT 字段链、字段链的标量成员。
' ============================================================

Private Type TTIP
    szText(0 To 15) As Byte
    lpszText As Long
    hInst As Long
End Type

Private Declare Sub CopyMemory Lib "kernel32" Alias "RtlMoveMemory" (ByRef Dst As Any, ByRef Src As Any, ByVal nLen As Long)

Sub Main()
    ' 注意: VB6/C3 的 String 是 Unicode, StrPtr 指过去是 UTF-16 字节串,
    ' 所以拷进 Byte 数组后奇数字节是 0 —— 断言按实际字节写。
    Dim s As String
    s = "ABC"

    ' (1) With 展开 + UDT 内定长数组元素 —— 崩溃的那一条
    Dim t As TTIP
    With t
        CopyMemory .szText(0), ByVal StrPtr(s), LenB(s)
        .hInst = 7
    End With
    If t.szText(0) = 65 Then
        If t.szText(1) = 0 Then
            If t.szText(2) = 66 Then
                Debug.Print "WITH-SUB=Y"
            Else
                Debug.Print "WITH-SUB=N"; t.szText(0); ","; t.szText(1); ","; t.szText(2)
            End If
        End If
    End If

    ' (2) 下标是表达式 (带运算) 时同样是左值
    '     读回经 Long 中转: Byte 数组元素直接进 If 比较会走 Variant 通道,
    '     VT_UI1 与常量比较在 C3 现有实现里不可靠, 与本用例无关, 故绕开。
    Dim buf(0 To 15) As Byte
    Dim i As Long
    Dim v As Long
    i = 4
    CopyMemory buf(i - 1), ByVal StrPtr(s), 3
    v = buf(3)
    If v = 65 Then
        v = buf(4)
        If v = 0 Then
            v = buf(5)
            If v = 66 Then
                Debug.Print "EXPR-SUB=Y"
            Else
                Debug.Print "EXPR-SUB=N"; buf(5)
            End If
        End If
    End If

    ' (3) 回归护栏: 标量变量取址
    Dim n As Long
    n = 0
    CopyMemory n, t.szText(0), 1
    If n = 65 Then Debug.Print "SCALAR=Y"

    ' (4) 回归护栏: UDT 字段链取址
    Dim u As TTIP
    CopyMemory u.hInst, t.hInst, 4
    If u.hInst = 7 Then Debug.Print "CHAIN=Y"

    Debug.Print "ASANY-DONE"
End Sub
