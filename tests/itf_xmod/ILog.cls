VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
END
Attribute VB_Name = "ILog"
Option Explicit

Interface ILog
    Sub Log(ByVal s As String)
    Property Get Logged() As Long
End Interface
