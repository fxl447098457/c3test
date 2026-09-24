Attribute VB_Name = "AsmX86"
' ai/vb-asm-extension-spec 正例 (x86): Asm 块 → MSVC `__asm { }` 内联块
' 与 x64 版对照: 同一份语法, 后端换成内联汇编; [var] 按名解析, [Function] → 返回变量。
Option Explicit

Public Function AddFive(ByVal num As Long) As Long
    Asm
        mov eax, [num]
        add eax, 5
        mov [Function], eax
    End Asm
End Function

' 非 Naked: 块内踩了 ebx → 后端自动 push/pop ebx (共享栈帧, 必须还给调用者)
Public Function KeepEbx(ByVal v As Long) As Long
    Asm
        mov ebx, [v]
        add ebx, 7
        mov eax, ebx
        mov [Function], eax
    End Asm
End Function

' Clobber 声明: 同一份源码在 x86/x64 下写同一个名字即可 (rbx → ebx, 见 spec §7)
Public Function ClobberDriven(ByVal v As Long) As Long
    Asm Clobber("rbx", "memory")
        mov eax, [v]
        add eax, 11
        mov [Function], eax
    End Asm
End Function

' <Naked> (x86): __declspec(naked), 无 prologue/epilogue; 无参数引用 (参数在调用者栈上),
' 返回靠 eax, 用户自写 ret。
<Naked>
Public Function NakedFive() As Long
    Asm
        mov eax, 5
        ret
    End Asm
End Function

' 单行形式 (spec §2.2)
Public Function Identity(ByVal num As Long) As Long
    Asm mov eax, [num]
End Function

Sub Main()
    Debug.Print "X86-ADD:" & AddFive(37)          ' 42
    Debug.Print "X86-KEEP-EBX:" & KeepEbx(30)      ' 37
    Debug.Print "X86-CLOBBER:" & ClobberDriven(1)  ' 12
    Debug.Print "X86-NAKED:" & NakedFive()         ' 5
    Debug.Print "X86-ONELINE:" & Identity(1234)    ' 1234
    Debug.Print "X86-DONE"
End Sub
