Attribute VB_Name = "AsmMixedNeg"
' ai/vb-asm-extension-spec 混排负例:
'   3040 — 片段里 [X] 的 X 既不是寄存器, 也不是可见的 VB 变量
'   3042 — x64 单个片段引用超过 4 个 VB 变量
'
' ⚠ 本文件**故意**编译失败; 门禁只验诊断号, 不链接。
'   为了一次跑出两个号, 两个过程各触发一个 —— 编译在诊断阶段就停, 两个都进诊断流。
Option Explicit

' 3040: [nosuchvar] 拼错了
Public Function BadRef(ByVal a As Long) As Long
    Dim t As Long
    t = a
    Asm
        mov eax, [t]
        add eax, [nosuchvar]
        mov [t], eax
    End Asm
    BadRef = t
End Function

' 3042: 5 个变量引用 (x64 只有 4 个地址寄存器)
Public Function TooMany(ByVal a As Long, ByVal b As Long, ByVal c As Long,
                        ByVal d As Long, ByVal e As Long) As Long
    Dim t As Long
    t = 0
    Asm
        mov eax, [a]
        add eax, [b]
        add eax, [c]
        add eax, [d]
        add eax, [e]
        mov [t], eax
    End Asm
    TooMany = t
End Function

Sub Main()
    Debug.Print BadRef(1), TooMany(1, 2, 3, 4, 5)
End Sub
