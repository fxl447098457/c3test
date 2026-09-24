Attribute VB_Name = "AsmMixedBasic"
' ai/vb-asm-extension-spec 正例 (x64): 混排的**最小**形态
'   v1 时这里发的 3037 ("Asm 块必须独占过程体") 已随 v2 变成合法正例 —— 混排本身就是
'   项2。本工程替代原来的 asm_neg.vbp, 用来盯住"整过程独占"不再是硬性要求。
Option Explicit

Public Function Mixed(ByVal a As Long) As Long
    Asm
        mov eax, [a]                ' [a] → ecx (ByVal 参数, 混排下按地址传)
        add eax, 1
        mov [Function], eax
    End Asm
    Mixed = 1                       ' VB 语句在后 → 覆盖片段写的返回值
End Function

Sub Main()
    Debug.Print "MIXED:" & Mixed(1)
End Sub
