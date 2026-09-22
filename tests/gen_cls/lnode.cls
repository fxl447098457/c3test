VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
END
Class LNode(Of T)
Attribute VB_Name = "LNode"
Option Explicit

' 自引用泛型类: mNext As LNode (裸类名 TypeRef 特化后指向同一特化)
Private mVal As T
Private mNext As LNode

Public Sub SetVal(ByVal v As T)
    mVal = v
End Sub

Public Function GetVal() As T
    GetVal = mVal
End Function

Public Sub Link(n As LNode)
    Set mNext = n
End Sub

Public Function NextVal() As T
    NextVal = mNext.GetVal()
End Function
