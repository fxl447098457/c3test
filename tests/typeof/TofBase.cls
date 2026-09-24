VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
END
Attribute VB_Name = "TofBase"
Option Explicit

' Fix 193: `TypeOf lhs Is <项目类>` 的夹具。链: TofLeaf -> TofMid -> TofBase,
' 另有兄弟 TofSib (也 Inherits TofBase)。Overridable/Overrides 是为了让这条链
' 带上 __cvtbl 虚槽 —— 覆盖"有虚槽的类"与"无虚槽的类 (TofPlain)"两条路。

Private m_tag As String

Private Sub Class_Initialize()
    m_tag = "base"
End Sub

Public Overridable Function Speak() As String
    Speak = m_tag
End Function
