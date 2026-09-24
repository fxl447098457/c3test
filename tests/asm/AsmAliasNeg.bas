Attribute VB_Name = "AsmAliasNeg"
' ai/vb-asm-extension-spec §11 项1 负例 (x64): cmpxchg×RAX/EAX 别名陷阱 → VB3041
'
' 这两段是实测踩过的**静默**故障: 编译通过、运行才炸 (死循环 / 段错误 / 垃圾值)。
' 现在静态前移成编译期诊断 —— 关键判据: 指针被塞进 RAX (整宽写), 随后只写 EAX
' (短宽写, 高 32 位回不来), 而 cmpxchg 的隐含累加器正是这一族。
' 注: 这里刻意避开 3038 (宽度不等 mov) —— 本夹具只该命中 3041。
Option Explicit

' 形态①: 指针放 RAX, 累加器也是 RAX/EAX → W(整宽写 rax) 在 L(短宽写 eax) 之前
Public Function BadPointerInRax(ByRef target As Long, ByVal addend As Long) As Long
    Asm
        mov r9d, edx                ' addend 先腾走
        mov r10, [target]           ' r10 = 指针... 这里先不踩坑 (合法行)
        mov rax, r10                ' ← W: 整宽写 rax (指针进 rax, 高 32 位有意义)
        mov eax, 0                  ' ← L: 短宽写 eax → rax 高 32 位清零, 指针已毁
        mov ecx, [r10]              ' ecx = *target
        add ecx, r9d
        lock cmpxchg [rax], ecx     ' ← 3041: 累加器 eax, 但 [rax] 里的 rax 不再是地址
        mov [Function], eax
    End Asm
End Function

' 形态②: 错法同上, 但隐含累加器是 mul (单操作数形式)
Public Function BadMulClobber(ByVal v As Long) As Long
    Asm
        mov ecx, [v]                ' ecx = v (32 位装载, 避开 3038)
        mov rax, rcx                ' ← W: 整宽写 rax (rcx 因上一行已零扩展, 是个有效值)
        mov eax, 7                  ' ← L: 短宽写 → rax 高 32 位丢
        mov ecx, 3
        mul ecx                     ' ← 3041: 隐含累加器 rax
        mov [Function], eax
    End Asm
End Function
