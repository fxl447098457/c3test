Attribute VB_Name = "AsmAttrNeg"
' ai/vb-asm-extension-spec 负例: 角括号属性与 Clobber 的形状错误
'   <Naked> 修饰非过程声明  → 2012
'   Clobber 参数不是字符串  → 2014
Option Explicit

<Naked>
Public Const K As Long = 1

Public Function BadClobber(ByVal v As Long) As Long
    Asm Clobber(123)
        mov eax, [v]
        mov [Function], eax
    End Asm
End Function

Sub Main()
    Debug.Print "ATTR-NEG:" & BadClobber(1)
End Sub
