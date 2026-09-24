Attribute VB_Name = "AsmNeg"
' ai/vb-asm-extension-spec 负例: Asm 块与 VB 语句混排 (x64 v1 不支持) → 3037
Option Explicit

Public Function Mixed(ByVal a As Long) As Long
    Asm
        mov eax, [a]
        add eax, 1
        mov [Function], eax
    End Asm
    Mixed = 1
End Function

Sub Main()
    Debug.Print "MIXED:" & Mixed(1)
End Sub
