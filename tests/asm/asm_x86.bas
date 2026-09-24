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

' ---- 项4: 浮点参数/返回 (x86 下 double 在栈上, 返回经 ST(0)) ----
Public Function DblAdd(ByVal a As Double, ByVal b As Double) As Double
    Asm
        fld qword ptr [a]
        fadd qword ptr [b]
        fstp [Function]         ' 返回双精度 (ST0 → 返回变量)
    End Asm
End Function

' ---- 项5: 5 个参数 (x86 全部走栈, 按名解析天然正确) ----
Public Function Sum5(ByVal a As Long, ByVal b As Long, ByVal c As Long,
                     ByVal d As Long, ByVal e As Long) As Long
    Asm
        mov eax, [a]
        add eax, [b]
        add eax, [c]
        add eax, [d]
        add eax, [e]
        mov [Function], eax
    End Asm
End Function

' ---- 项6: int64 返回 (edx:eax 对) ----
' x86 下 LongLong 按值返回走 EDX:EAX; 返回变量是 64 位 C 变量, 低 32 位在 +0, 高 32 位在 +4,
' 故用 [Function] / [Function+4] 这对偏移写法 (asmRewriteLines 的偏移规则负责整体展开)。
' 注意 x86 内联汇编中写 64 位变量的低 32 位必须显式 `dword ptr` (否则 C2443 操作数大小冲突)。
'
' 这里演示「两个 Long 相加, 结果以 64 位返回且**不丢溢出**」—— 正确做法是先把 int32 操作数
' 各自符号扩展到 64 位 (cdq 只扩 eax), 再作 64 位加法, 否则 eax 上先溢出回绕就晚了:
'   反面写法 `mov eax,[a]; add eax,[b]; cdq` 在 a=b=2e9 时 eax 先变成 -294967296,
'   再 cdq 得到 edx=-1, 结果 -294967296 而非 4000000000。
Public Function BigAdd(ByVal a As Long, ByVal b As Long) As LongLong
    Asm
        mov eax, [a]
        cdq                             ' 扩 a 到 edx:eax
        mov ecx, eax
        mov ebx, edx                    ' ebx:ecx = (int64)a
        mov eax, [b]
        cdq                             ' 扩 b 到 edx:eax
        add ecx, eax                    ' 低 32 位相加
        adc ebx, edx                    ' 高 32 位带进位相加
        mov dword ptr [Function], ecx
        mov dword ptr [Function+4], ebx
    End Asm
End Function

' 演示 EDX:EAX 对的直接搬运: 64 位值拆成 (lo, hi) 两半传入, 组装后返回
Public Function Make64(ByVal lo As Long, ByVal hi As Long) As LongLong
    Asm
        mov eax, [lo]
        mov edx, [hi]                   ' edx:eax = hi:lo
        mov dword ptr [Function], eax
        mov dword ptr [Function+4], edx
    End Asm
End Function

Sub Main()
    Debug.Print "X86-ADD:" & AddFive(37)          ' 42
    Debug.Print "X86-KEEP-EBX:" & KeepEbx(30)      ' 37
    Debug.Print "X86-CLOBBER:" & ClobberDriven(1)  ' 12
    Debug.Print "X86-NAKED:" & NakedFive()         ' 5
    Debug.Print "X86-ONELINE:" & Identity(1234)    ' 1234
    Debug.Print "X86-DBL:" & DblAdd(1.5, 2.25)     ' 3.75
    Debug.Print "X86-SUM5:" & Sum5(1, 2, 3, 4, 5)  ' 15
    Debug.Print "X86-BIG:" & BigAdd(2000000000, 2000000000)  ' 4000000000
    Debug.Print "X86-MAKE64:" & Make64(1, 1)                 ' 4294967297
    Debug.Print "X86-DONE"
End Sub
