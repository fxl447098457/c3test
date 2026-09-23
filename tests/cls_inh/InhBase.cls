VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
END
Attribute VB_Name = "InhBase"
Option Explicit

Private Type TPoint
    x As Long
    y As Long
End Type

Private m_hits As Long
Protected m_secret As Long
Protected m_tag As String
Private m_pt As TPoint
Private m_name As String
Public Label As String

Public Sub Bump()
    m_hits = m_hits + 1
End Sub

Public Function Hits() As Long
    Hits = m_hits
End Function

Public Function Tag() As String
    Tag = "base"
End Function

Public Sub SetPt(ByVal v As Long)
    m_pt.x = v
    m_pt.y = v * 2
End Sub

Public Function PtSum() As Long
    PtSum = m_pt.x + m_pt.y
End Function

Public Function Cat(ByVal a As String, Optional b As String) As String
    If IsMissing(b) Then
        Cat = a & "/solo"
    Else
        Cat = a & "/" & b
    End If
End Function

Public Function Sum3(ByVal a As Long, ByVal b As Long, Optional c As Long) As Long
    If IsMissing(c) Then
        Sum3 = a + b
    Else
        Sum3 = a + b + c
    End If
End Function

Public Property Get Name() As String
    Name = m_name
End Property

Public Property Let Name(ByVal v As String)
    m_name = v
End Property

Protected Sub SetSecret(ByVal v As Long)
    m_secret = v
End Sub

Protected Function Secret() As Long
    Secret = m_secret
End Function
Public Overridable Function Speak() As String
    Speak = "base"
End Function

Public NotOverridable Function Frozen() As String
    Frozen = "frozen"
End Function
