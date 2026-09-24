VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
END
Attribute VB_Name = "AccThing"
Option Explicit

Private m_v As Long

Private Sub Secret()
    Debug.Print "ACC-SECRET"
End Sub

Public Sub Setup(ByVal v As Long)
    m_v = v
    Me.Secret
    ' 084a: 同类模块内经**别的实例**摸 Private 成员也是合法 VB6 语义
    Dim o As AccThing
    Set o = New AccThing
    o.Secret
End Sub

Public Function Value() As Long
    Value = m_v
End Function

' Friend: 同一工程内等同 Public (跨包边界才受限, 属 084a M3)
Friend Sub Friendly()
    Debug.Print "ACC-FRIENDLY"
End Sub
