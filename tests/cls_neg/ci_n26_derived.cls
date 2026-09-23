VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
END
Attribute VB_Name = "Vt26Derived"
Option Explicit

Inherits Vt26Base

Public Overrides Function Speak() As String
    Speak = "derived"
End Function
