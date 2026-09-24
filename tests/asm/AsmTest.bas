Attribute VB_Name = "AsmTest"
' ai/vb-asm-extension-spec 正例 (x64): Asm 块 → 生成 .asm → ml64 → 链接
' 语法学 FreeBASIC: Asm ... End Asm / [var] 按名引用 / [Function] 返回值占位
' 本工程覆盖: 基本块 / ByRef 解引用 / <Naked> / callee-saved 自动保存 / Clobber / 单行形式
Option Explicit

' 最简单形态: ByVal 参数 (Win64 ABI 首参落 RCX/ECX), 返回值经 [Function] → RAX
Public Function AddFive(ByVal num As Long) As Long
    Asm
        mov eax, [num]          ' eax = num (参数在 ecx)
        add eax, 5
        mov [Function], eax
    End Asm
End Function

' ByRef + 解引用: "变量即其地址" —— [target] 是指针本身, 取值需再解引用一次.
' 本块只用 caller-saved 寄存器 (rax/eax/r8d/r9d/edx), 无需保存 callee-saved.
'
' ⚠ cmpxchg 双陷阱 (实测踩过):
'   1) EAX 是 RAX 的低 32 位 —— 同一个物理寄存器. 指针放 RAX + 累加器放 EAX
'      二者不可兼得: 循环内 `mov rax,[target]` 重载指针会毁掉 EAX 累加器
'      (cmpxchg 拿指针低 32 位与内存比 → 永不相等 → jne 死循环);
'      不重载则 RAX 只剩加载值, `cmpxchg [rax]` 访问"值为地址" → AV.
'      正解: 指针放其它寄存器 (R10/R11 caller-saved), 累加器独占 RAX/EAX.
'   2) 其余参数先搬去不冲突的寄存器 (此处 addend: edx → r9d), 免得循环内被踩.
Public Function AtomicAdd(ByRef target As Long, ByVal addend As Long) As Long
    Asm
        mov r9d, edx            ' r9d = addend (腾出 edx)
        mov r10, [target]       ' r10 = target 指针 (caller-saved, 循环内不再动)
.retry:
        mov eax, [r10]          ' eax = *target (cmpxchg 累加器, 与 r10 各司其职)
        mov r8d, eax
        add r8d, r9d            ' r8d = 当前值 + addend
        lock cmpxchg [r10], r8d ' 相等则 [r10] = r8d, ZF=1; 不等则 eax = [r10], ZF=0
        jne .retry
        mov [Function], eax     ' 返回旧值
    End Asm
End Function

' callee-saved 自动保存: 块里直接用了 rbx 家族 —— 后端必须自动 push/pop rbx,
' 否则调用者 (生成的 C 代码) 手里的 rbx 会被这个函数毁掉。
' 注意: 用户侧**不必**自己 push/pop (spec §7) —— 这正是与裸汇编的差别。
' ⚠ MASM 不容许宽度不等的 mov: `mov rbx, ecx`(64←32) 会 A2022. 32 位值就写 ebx
'   (写 ebx 会零扩展进 rbx), 或显式 `movsxd rbx, ecx`。
Public Function KeepRbx(ByVal v As Long) As Long
    Asm
        mov ebx, [v]            ' 踩 callee-saved (rbx 的低 32 位写法, 宽度与 ecx 一致)
        add ebx, 7
        mov [Function], ebx     ' 32 位返回 → eax; 写成 ebx/ecx 一族才同宽
    End Asm
End Function

' Clobber 声明: 跑在注释里写清楚"我踩了 r12 和内存", 后端据此保存 r12 (即使块内
' 文本上没出现 r12 —— 比如 r12 只在被调用的内联片段里被写)。
Public Function ClobberR12(ByVal v As Long) As Long
    Asm Clobber("r12", "memory")
        mov eax, [v]
        add eax, 11
        mov [Function], eax
    End Asm
End Function

' <Naked> 整函数汇编: 不生成 prologue/epilogue, 尾部不自动补 ret —— 用户自己 ret。
' (x64 下参数仍在 ABI 寄存器里, 可按名引用; x86 下 naked 无栈帧, 见 spec §7。)
<Naked>
Public Function NakedFive(ByVal num As Long) As Long
    Asm
        mov eax, [num]
        add eax, 5
        mov [Function], eax
        ret
    End Asm
End Function

' 单行形式 `Asm <指令>`: 过程体只有一条指令时可用 (spec §2.2)。
' 这里 [num] → ecx, 结果值在 eax/rax, 非 Naked 时编译器补 ret。
Public Function Identity(ByVal num As Long) As Long
    Asm mov eax, [num]
End Function

Sub Main()
    Debug.Print "ASM-ADD:" & AddFive(37)

    Dim v As Long
    v = 10
    Dim old As Long
    old = AtomicAdd(v, 5)
    Debug.Print "ASM-ATOMIC-OLD:" & old
    Debug.Print "ASM-ATOMIC-NEW:" & v

    Debug.Print "ASM-KEEP-RBX:" & KeepRbx(30)      ' 37
    Debug.Print "ASM-CLOBBER:" & ClobberR12(1)     ' 12
    Debug.Print "ASM-NAKED:" & NakedFive(95)       ' 100
    Debug.Print "ASM-ONELINE:" & Identity(1234)    ' 1234
    Debug.Print "ASM-DONE"
End Sub
