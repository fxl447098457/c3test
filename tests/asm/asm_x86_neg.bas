Attribute VB_Name = "AsmX86Neg"
' ai/vb-asm-extension-spec 负例: x86 目标 + <Naked> 里按名引用参数 → 3036
' (naked 无栈帧, x86 参数在调用者的栈上, 名字无从解析; 去掉 <Naked> 或改用寄存器)
Option Explicit

<Naked>
Public Function BadNaked(ByVal num As Long) As Long
    Asm
        mov eax, [num]
        ret
    End Asm
End Function

Sub Main()
    Debug.Print "ASM-X86-NAKED:" & BadNaked(1)
End Sub
