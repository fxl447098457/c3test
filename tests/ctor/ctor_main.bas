Attribute VB_Name = "ctor_main"
' ai/084c: 类构造函数 — 带参与无参
Option Explicit

Sub Main()
    Dim a As CtorThing
    Set a = New CtorThing(42, "amt")
    Debug.Print a.Describe()

    Dim b As CtorPlain
    Set b = New CtorPlain
    Debug.Print "CNT:" & b.GetCount()
End Sub
