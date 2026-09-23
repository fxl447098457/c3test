VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
END
Attribute VB_Name = "Vt20Base"
Option Explicit

Public Overridable Function Pick() As String
    Pick = "base"
End Function

' bare (unqualified) call: this build only dispatches Me.Pick() / obj.Pick()
Public Function Talk() As String
    Talk = Pick()
End Function
