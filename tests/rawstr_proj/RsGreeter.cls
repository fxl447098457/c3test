VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
END
Attribute VB_Name = "RsGreeter"
Option Explicit

' ai/028 V1: 反引号原始多行串从类模块里返回给标准模块 —— 串要跨模块边界走一趟。
Public Function Note() As String
    Note = `第一行
第二行 = "rs"`
End Function

Public Function Tick() As String
    Tick = `R`
End Function
