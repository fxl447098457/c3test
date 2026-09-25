VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
END
Attribute VB_Name = "Mb27Base"
Option Explicit

' ai/022 B09: MyBase needs a base class. This class has no Inherits clause, so the call
' must report VB3028 -- before this batch it compiled into the undefined symbol
' vb6_MyBase_Speak (C2065 at cl time, invisible to --syntax-only).
Public Function Speak() As String
    Speak = "base"
End Function

Public Function BadNoBase() As String
    BadNoBase = MyBase.Speak()
End Function
