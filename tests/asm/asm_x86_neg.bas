Attribute VB_Name = "AsmX86"
' ai/vb-asm-extension-spec 负例: Asm 块 + x86 目标 → 3030 (v1 仅 x64)
Option Explicit

Public Function AddFive(ByVal num As Long) As Long
    Asm
        mov eax, [num]
        add eax, 5
        mov [Function], eax
    End Asm
End Function

Sub Main()
    Debug.Print "ASM-X86:" & AddFive(1)
End Sub
