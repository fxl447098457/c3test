Attribute VB_Name = "AsmClsNeg"
' ai/vb-asm-extension-spec 负例: 类方法里的 Asm 块 → 3037 (spec §10 边界)
Option Explicit

Sub Main()
    Dim c As CAsmBad
    Set c = New CAsmBad
    Debug.Print "ASM-CLS:" & c.F(1)
End Sub
