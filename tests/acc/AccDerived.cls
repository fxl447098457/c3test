VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
END
Attribute VB_Name = "AccDerived"
Option Explicit

Inherits AccBase

' 家族内访问祖先 Private 字段: 语义放行 (与 B07b 合并模型一致)
Public Sub TouchInherited()
    Me.Bump
    Me.Bump
End Sub
