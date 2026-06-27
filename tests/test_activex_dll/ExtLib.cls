VERSION 1.0 CLASS
BEGIN
  MultiUse = -1
END
Attribute VB_Name = "ExtLib"
Option Explicit

' P6.11: 扩展类型测试 - Boolean参数/返回值 + Property Let/Get扩展

Private m_BoolValue As Boolean
Private m_Count As Long

' --- Boolean参数和返回值 ---
Public Function NotBool(ByVal Value As Boolean) As Boolean
    NotBool = Not Value
End Function

Public Function AndBool(ByVal A As Boolean, ByVal B As Boolean) As Boolean
    AndBool = A And B
End Function

Public Sub SetBoolValue(ByVal Value As Boolean)
    m_BoolValue = Value
End Sub

Public Function GetBoolValue() As Boolean
    GetBoolValue = m_BoolValue
End Function

' --- Property Let/Get (Long) ---
Public Property Let Count(ByVal Value As Long)
    m_Count = Value
End Property

Public Property Get Count() As Long
    Count = m_Count
End Property

' --- 嵌套方法调用 ---
Public Function DoubleIt(ByVal N As Long) As Long
    DoubleIt = N * 2
End Function

Public Function TripleIt(ByVal N As Long) As Long
    TripleIt = N * 3
End Function
