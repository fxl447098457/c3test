Attribute VB_Name = "AsmMixed"
' ai/vb-asm-extension-spec 项2/项3: Asm 片段与 VB 语句混排 + 片段内引用 VB 局部变量
'   x64: 片段降级为独立 MASM 过程, 变量以地址传入 ([X] → [rcx])
'   x86: 片段就地发 __asm{} 内联块, [X] 按名解析 (同宿主见 AsmMixedX86.bas)
'
' 语义边界 (spec §12.5):
'   * 每个片段是独立单元 —— 寄存器/标志位状态不跨片段传播。
'   * x64 单个片段最多引用 4 个 VB 变量。
'   * 片段内可自由使用任意寄存器 (callee-saved 由编译器自动 push/pop)。
Option Explicit

' --- 基本形态: 片段读局部变量 + 写回, VB 语句继续用同一变量 ---
Public Function MixedAcc(ByVal n As Long) As Long
    Dim acc As Long
    acc = n * 2                     ' acc = 74
    Asm
        mov eax, [acc]              ' x64: [rcx] (rcx = &acc); x86: acc
        add eax, 1
        mov [acc], eax
    End Asm
    MixedAcc = acc + 100            ' VB 侧看到 acc = 75
End Function

' --- 片段之间不共享寄存器: 两个片段各自独立, 靠变量传递 ---
' ⚠ 地址参数寄存器约定 (spec §12.5): 第 k 个被引用的变量地址在 RCX/RDX/R8/R9。
'   片段可以自由使用其它寄存器, 但**不要踩掉还没用完的地址寄存器**。
Public Function TwoBlocks(ByVal a As Long) As Long
    Dim t As Long
    t = a
    Asm
        mov eax, [t]                ' [t] → [rcx]
        imul eax, eax, 3            ' 注意 eax/eax 同宽 (宽度校验 3038 只看纯寄存器对)
        mov [t], eax
    End Asm
    Asm
        mov eax, [t]                ' 第二片段: 重新建立自己的地址寄存器
        add eax, 7
        mov [t], eax
    End Asm
    TwoBlocks = t                   ' (5*3)+7 = 22
End Function

' --- 引用 ByRef 参数 (VB 变量就是调用者的那个) ---
' 与整过程形态的区别: 整过程形态里 [x] → 指针本身 (rcx), 要自己再解引用;
' 混排形态里 [x] → `[rcx]`, 已经是"那个变量", 直接读写即可 (spec §12.5)。
Public Sub Bump(ByRef x As Long, ByVal by As Long)
    Dim guard As Long
    guard = 0                       ' 让本过程成为"混排" (体里不只有 Asm 块)
    Asm
        mov eax, [x]                ' 混排: x 是 ByRef 参数, 传的是指针本身
        add eax, [by]
        mov [x], eax
    End Asm
    guard = guard + 1
    If guard = 0 Then x = -1        ' 永不触发, 只为让 guard 参与发码
End Sub

' --- [Function] 在混排片段里指向返回变量 ---
Public Function MixedRet(ByVal v As Long) As Long
    Dim base As Long
    base = v + 1
    Asm
        mov eax, [base]
        add eax, 40
        mov [Function], eax         ' → [rcx] (rcx = &vb6_ret_MixedRet)
    End Asm
    ' 片段写了返回变量; 这里再走一次 VB 分支覆盖它, 验证两种写法都生效
    If base > 100 Then MixedRet = 0
End Function

' --- 混排 + callee-saved: 片段踩 rbx, 编译器必须自动保护 ---
Public Function MixedKeepRbx(ByVal v As Long) As Long
    Dim t As Long
    t = v
    Asm
        mov ebx, [t]
        add ebx, 5
        mov [t], ebx
    End Asm
    MixedKeepRbx = t * 10
End Function

' --- 混排 + Clobber 声明 ---
Public Function MixedClobber(ByVal v As Long) As Long
    Dim t As Long
    t = v
    Asm Clobber("r12", "memory")
        mov eax, [t]
        add eax, 2
        mov [t], eax
    End Asm
    MixedClobber = t
End Function

' --- 带偏移引用: [X+4] 形态 (x64 → [rcx+4]) ---
' LongLong 局部变量恒为 int64_t, 低 32 位在 +0 / 高 32 位在 +4。
' ⚠ MASM 不容许 mov 的两侧同时是内存操作数 → 必须过一次寄存器。
Public Function MixedOffset(ByVal lo As Long, ByVal hi As Long) As LongLong
    Dim q As LongLong
    Asm
        mov eax, [lo]                   ' [lo] → [rcx]
        mov dword ptr [q], eax          ' 低半 → [rdx]
        mov eax, [hi]                   ' [hi] → [r8]
        mov dword ptr [q+4], eax        ' 高半 → [rdx+4]
    End Asm
    MixedOffset = q
End Function

Sub Main()
    Debug.Print "MIX-ACC:" & MixedAcc(37)            ' 175
    Debug.Print "MIX-TWO:" & TwoBlocks(5)            ' 22
    Dim b As Long
    b = 10
    Bump b, 5
    Debug.Print "MIX-BUMP:" & b                      ' 15
    Debug.Print "MIX-RET:" & MixedRet(1)             ' 42
    Debug.Print "MIX-KEEP-RBX:" & MixedKeepRbx(3)    ' 80
    Debug.Print "MIX-CLOBBER:" & MixedClobber(9)     ' 11
    Debug.Print "MIX-OFFSET:" & MixedOffset(1, 1)    ' 4294967297
    Debug.Print "MIX-DONE"
End Sub
