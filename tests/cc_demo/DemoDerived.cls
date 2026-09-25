VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
END
Attribute VB_Name = "DemoDerived"
Option Explicit

' ai/022 B18 端到端示例（继承那一支）: Inherits + Overrides + MyBase 去虚化基调用。
' 注意 v1 边界: 这一个类**不能**同时 `Implements` 一个新式接口（VB3022）——
' 所以示例把它拆成两条独立的链（继承链 = DemoBase/DemoDerived; 接口链 = IDemoShape/DemoShape）。
Inherits DemoBase

Private Sub Class_Initialize()
    Me.SetShift 2#          ' Protected 基类成员: 派生类家族内可用
End Sub

Public Overrides Function Speak() As String
    Speak = "derived:" & MyBase.Speak()   ' 虚派发: 由调用方的动态类型决定
End Function

Public Function BaseSpeak() As String
    BaseSpeak = MyBase.Speak()            ' 去虚化基调用: 直调基类实现, 不查表
End Function

Public Function ShiftSeen() As Double
    ShiftSeen = Me.Shift()                ' 继承来的 Protected 成员
End Function
