VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
END
Attribute VB_Name = "IWriter"
Option Explicit

Interface IWriter
    Sub Emit(ByVal s As String)
    Function Total() As Long
    Property Get Last() As String
End Interface
