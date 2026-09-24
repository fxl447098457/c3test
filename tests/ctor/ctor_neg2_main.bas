Attribute VB_Name = "ctor_neg2_main"
' ai/084c 负例2: CtorPlain 的 Class_Initialize 无参, New 带实参 → 3029
Option Explicit

Sub Main()
    Dim b As CtorPlain
    Set b = New CtorPlain(99)
    Debug.Print b.GetCount()
End Sub
