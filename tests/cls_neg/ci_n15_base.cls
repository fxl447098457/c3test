VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
END
Attribute VB_Name = "Ov15Base"
Option Explicit

Public Overridable Function Pick() As String
    Pick = "base"
End Function

Public Function Talk() As String
    Talk = Me.Pick()
End Function
