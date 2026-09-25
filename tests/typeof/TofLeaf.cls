VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
END
Attribute VB_Name = "TofLeaf"
Option Explicit

Inherits TofMid

Public Overrides Function Speak() As String
    Speak = "leaf"
End Function
