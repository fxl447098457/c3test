VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
END
Attribute VB_Name = "Prot23Stranger"
Option Explicit

Public Function Read() As String
    Dim w As Prot23Base
    Read = w.Title
End Function
