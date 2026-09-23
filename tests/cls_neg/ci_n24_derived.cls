VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
END
Attribute VB_Name = "Vt24Derived"
Option Explicit

Inherits Vt24Base

Public Overrides Function Speak() As String
    Speak = "derived"
End Function
