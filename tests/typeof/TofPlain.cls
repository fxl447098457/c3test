VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
END
Attribute VB_Name = "TofPlain"
Option Explicit

' 无继承、无虚槽的普通类 —— 走"同名判定"那条路 (不在继承链里)
Public Function Name2() As String
    Name2 = "plain"
End Function
