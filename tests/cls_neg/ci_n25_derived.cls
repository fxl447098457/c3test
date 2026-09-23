VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
END
Attribute VB_Name = "Vt25Derived"
Option Explicit

Inherits Vt25Base

Public Overrides Function Speak() As String
    Speak = "derived"
End Function
