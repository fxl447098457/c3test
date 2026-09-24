Attribute VB_Name = "AsmTest"
' ai/vb-asm-extension-spec: Asm 块最小闭环 (v1 x64 → 生成 .asm → ml64 → 链接)
' 语法学 FreeBASIC: Asm ... End Asm / [var] 按名引用 / [Function] 返回值占位
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

Sub Main()
    Dim r As Long
    r = AddFive(37)
    Debug.Print "ASM-ADD:" & r

    Dim v As Long
    v = 10
    Dim old As Long
    old = AtomicAdd(v, 5)
    Debug.Print "ASM-ATOMIC-OLD:" & old
    Debug.Print "ASM-ATOMIC-NEW:" & v
    Debug.Print "ASM-DONE"
End Sub
