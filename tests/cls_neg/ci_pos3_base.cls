VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
END
Attribute VB_Name = "VtPos3Base"
Option Explicit

' ai/022 B08e-6 site 9 (Fix 088d shape): inside `Function Maker() As VtPos3Base` the name
' `Maker` reads this function's return variable, which is a plain C local -- a pure
' receiver, so the overridable call on it dispatches instead of being killed. The runtime
' side of the same shape is pinned by INH39 in tests/cls_inh.
Private m_self As VtPos3Base
Public g_via As String

Public Overridable Function Speak() As String
    Speak = "base"
End Function

Public Function Maker() As VtPos3Base
    Set Maker = m_self
    g_via = Maker.Speak()
End Function
