Attribute VB_Name = "ctor_neg_main"
' ai/084c 负例1: 实参个数不符 (CtorThing 需要 2 个, 实给 1 个) → 3035
Option Explicit

Sub Main()
    Dim a As CtorThing
    Set a = New CtorThing(42)
    Debug.Print a.Describe()
End Sub
