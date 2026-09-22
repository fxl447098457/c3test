Attribute VB_Name = "Main"
Option Explicit

Sub Main()
    ' 泛型类两个特化共存 (Long/String), 成员返回类型互不串扰
    Dim b As GBox(Of Long)
    Dim s As GBox(Of String)
    Set b = New GBox(Of Long)
    Set s = New GBox(Of String)
    b.SetVal 42
    s.SetVal "hey"
    Debug.Print "GC1="; b.GetVal()                 ' 42
    Debug.Print "GC2="; b.Describe()               ' box:42
    Debug.Print "GC3="; s.GetVal()                 ' hey (String 版返回类型不得退化成 Long)
    Debug.Print "GC4="; s.Describe()               ' box:hey

    ' 自引用泛型类: 链表两节点, Long 与 String 特化各自独立
    Dim n1 As LNode(Of Long)
    Dim n2 As LNode(Of Long)
    Set n1 = New LNode(Of Long)
    Set n2 = New LNode(Of Long)
    n1.SetVal 11
    n2.SetVal 22
    n1.Link n2
    Debug.Print "GC5="; n1.GetVal() + n1.NextVal() ' 33
    Dim t1 As LNode(Of String)
    Dim t2 As LNode(Of String)
    Set t1 = New LNode(Of String)
    Set t2 = New LNode(Of String)
    t1.SetVal "x"
    t2.SetVal "y"
    t1.Link t2
    Debug.Print "GC6="; t1.NextVal()               ' y
    Debug.Print "GCLS-DONE"
End Sub
