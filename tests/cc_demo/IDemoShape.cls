VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
END
Attribute VB_Name = "IDemoShape"
Option Explicit

' ai/022 B18 端到端示例: 接口契约（宿主文件 = 一文件一接口, 名字与模块同名）。
Interface IDemoShape
    Sub Move(ByVal dx As Double)
    Function Area() As Double
    Property Get Label() As String
End Interface
