VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
END
Attribute VB_Name = "ArrClsShape"
Option Explicit

' Fix 192: 类数组的元素类型。成员访问 arr(i).Method 此前发成
' `VB6_SA_AT(void*, arr, i).Method(...)` (void* 取成员) → MSVC C2224。

Private m_x As Double

Private Sub Class_Initialize()
    m_x = 1
End Sub

Public Sub Move(dx As Double)
    m_x = m_x + dx
End Sub

Public Function Side() As Double
    Side = m_x
End Function
