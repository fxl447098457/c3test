VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
END
Attribute VB_Name = "Mb28Base"
Option Explicit

' ai/022 B09: the base face has no member called NoSuch, so `MyBase.NoSuch()` must report
' VB3028 instead of emitting a call to a symbol nobody defines.
Public Function Speak() As String
    Speak = "base"
End Function
