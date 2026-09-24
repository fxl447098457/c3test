Attribute VB_Name = "AsmAliasX86Neg"
' ai/vb-asm-extension-spec §11 项1 负例 (x86): cmpxchg×EAX 别名陷阱 → VB3042
' x86 下 32 位寄存器本来就是"累加器 + 地址"的唯一选择 (没有 r10/r11 这种富余),
' 所以这个坑在 x86 上更容易踩 —— 判据同 x64, 只是族换成 32 位拼写。
Option Explicit

Public Function BadEaxBase(ByRef target As Long, ByVal addend As Long) As Long
    Asm
        mov ebx, [target]           ' ebx = 指针 (callee-saved, 后端自动 push/pop)
        mov eax, ebx                ' ← W: 整宽写 eax (指针进 eax; x86 下 eax 即 32 位全宽)
        mov ax, 0                   ' ← L: 短宽写 ax → eax 高位丢, 指针已毁
        mov ecx, [ebx]
        add ecx, [addend]
        lock cmpxchg [eax], ecx     ' ← 3042: 累加器 eax, 但 [eax] 里的 eax 不再是地址
        mov [Function], eax
    End Asm
End Function
