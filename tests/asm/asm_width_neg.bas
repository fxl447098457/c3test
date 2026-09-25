' asm_width_neg — 宽度不一致负例: 期望 VB3038 (操作数宽度校验, 原本要到 ml64 才报 A2022)
' `mov rax, edx` 是 64←32 的宽度不等搬运 —— codegen 期就应拦截, 不该漏到汇编阶段。
Attribute VB_Name = "AsmWidthNeg"

Public Function BadWidth(ByVal v As Long) As Long
    Asm
        mov rax, edx            ' 64←32: 宽度不等 → 3038
        mov [Function], eax
    End Asm
End Function

Public Sub Main()
    Debug.Print BadWidth(1)
End Sub
