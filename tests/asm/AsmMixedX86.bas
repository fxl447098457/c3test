Attribute VB_Name = "AsmMixedX86"
' ai/vb-asm-extension-spec 项2/项3 正例 (x86): Asm 片段与 VB 语句混排
'   x86 有 MSVC `__asm { }`, 且**按名解析** C 变量 → [acc] 直接就是 VB 局部变量的内存。
'   这是项3 在 x86 上的"免费"解: 不需要地址参数, 也不受 4 个变量的限制。
'
' 语义边界 (spec §12.5): 每个片段仍是独立单元 (寄存器/标志位不跨片段传播)。
Option Explicit

' 模块级变量 (混排片段也能引用本模块的模块级变量)
Private g_counter As Long

' --- 基本形态: 片段读局部变量 + 写回, VB 语句继续用同一变量 ---
Public Function MixedAcc(ByVal n As Long) As Long
    Dim acc As Long
    acc = n * 2                     ' acc = 74
    Asm
        mov eax, [acc]
        add eax, 1
        mov [acc], eax
    End Asm
    MixedAcc = acc + 100            ' acc = 75 → 175
End Function

' --- 两个片段各自独立 ---
Public Function TwoBlocks(ByVal a As Long) As Long
    Dim t As Long
    t = a
    Asm
        mov eax, [t]
        imul eax, eax, 3
        mov [t], eax
    End Asm
    Asm
        mov eax, [t]
        add eax, 7
        mov [t], eax
    End Asm
    TwoBlocks = t                   ' 22
End Function

' --- 引用 ByRef 参数 (x86 按名解析, [x] 即调用者的变量) ---
Public Sub Bump(ByRef x As Long, ByVal by As Long)
    Dim guard As Long
    guard = 0
    Asm
        mov eax, [x]
        add eax, [by]
        mov [x], eax
    End Asm
    guard = guard + 1
    If guard = 0 Then x = -1
End Sub

' --- [Function] 在混排片段里指向返回变量 ---
Public Function MixedRet(ByVal v As Long) As Long
    Dim base As Long
    base = v + 1
    Asm
        mov eax, [base]
        add eax, 40
        mov [Function], eax
    End Asm
    If base > 100 Then MixedRet = 0
End Function

' --- 混排 + callee-saved: 片段踩 ebx, 编译器必须自动保护 ---
Public Function MixedKeepEbx(ByVal v As Long) As Long
    Dim t As Long
    t = v
    Asm
        mov ebx, [t]
        add ebx, 5
        mov [t], ebx
    End Asm
    MixedKeepEbx = t * 10           ' 80
End Function

' --- 混排 + Clobber 声明 (同一份源码 x86/x64 写同一个名字) ---
Public Function MixedClobber(ByVal v As Long) As Long
    Dim t As Long
    t = v
    Asm Clobber("rbx", "memory")
        mov eax, [t]
        add eax, 2
        mov [t], eax
    End Asm
    MixedClobber = t                ' 11
End Function

' --- 带偏移引用: [q+4] 形态 (x86 下 64 位局部变量低/高 32 位) ---
Public Function MixedOffset(ByVal lo As Long, ByVal hi As Long) As LongLong
    Dim q As LongLong
    Asm
        mov eax, [lo]
        mov dword ptr [q], eax          ' 低半
        mov eax, [hi]
        mov dword ptr [q+4], eax        ' 高半
    End Asm
    MixedOffset = q                     ' 4294967297
End Function

' --- 混排里引用多个变量 (x86 无 4 个上限) ---
Public Function SumMixed(ByVal a As Long, ByVal b As Long, ByVal c As Long) As Long
    Dim t As Long
    t = 0
    Asm
        mov eax, [a]
        add eax, [b]
        add eax, [c]
        mov [t], eax
    End Asm
    SumMixed = t + 1000             ' 1006
End Function

' --- 引用模块级变量 ---
Public Function TouchGlobal(ByVal n As Long) As Long
    Dim t As Long
    t = n
    Asm
        mov eax, [t]
        add eax, [g_counter]
        mov [t], eax
    End Asm
    TouchGlobal = t                 ' 1005
End Function

Sub Main()
    Debug.Print "XMIX-ACC:" & MixedAcc(37)            ' 175
    Debug.Print "XMIX-TWO:" & TwoBlocks(5)            ' 22
    Dim b As Long
    b = 10
    Bump b, 5
    Debug.Print "XMIX-BUMP:" & b                      ' 15
    Debug.Print "XMIX-RET:" & MixedRet(1)             ' 42
    Debug.Print "XMIX-KEEP-EBX:" & MixedKeepEbx(3)    ' 80
    Debug.Print "XMIX-CLOBBER:" & MixedClobber(9)     ' 11
    Debug.Print "XMIX-OFFSET:" & MixedOffset(1, 1)    ' 4294967297
    Debug.Print "XMIX-SUMMIX:" & SumMixed(1, 2, 3)    ' 1006
    g_counter = 1000
    Debug.Print "XMIX-GLOBAL:" & TouchGlobal(5)    ' 1005
    Debug.Print "XMIX-DONE"
End Sub
