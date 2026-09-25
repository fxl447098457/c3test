VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
END
Attribute VB_Name = "VtPos3Derived"
Option Explicit

Inherits VtPos3Base

Public Overrides Function Speak() As String
    Speak = "derived"
End Function
