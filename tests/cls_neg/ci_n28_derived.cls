VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
END
Attribute VB_Name = "Mb28Derived"
Option Explicit

Inherits Mb28Base

' A legal MyBase call next to the illegal one, so the case proves the diagnostic is about
' the missing member and not about MyBase being rejected wholesale.
Public Function OkBaseSpeak() As String
    OkBaseSpeak = MyBase.Speak()
End Function

Public Function BadNoSuchMember() As String
    BadNoSuchMember = MyBase.NoSuch()
End Function
