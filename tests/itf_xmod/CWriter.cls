VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
END
Attribute VB_Name = "CWriter"
Option Explicit

Implements IWriter
Implements ILog

Private m_total As Long
Private m_last As String
Private m_logged As Long

' Cross-module contract: the interface lives in IWriter.cls (head-line host) and
' these members bind to it through member-level Implements clauses (B02b).
Public Sub Emit(ByVal s As String) Implements IWriter.Emit
    m_last = s
    m_total = m_total + 1
End Sub

Public Function Total() As Long Implements IWriter.Total
    Total = m_total
End Function

Public Property Get Last() As String Implements IWriter.Last
    Last = m_last
End Property

' B06a: a second interface on the same class -> exercises QueryInterface's sibling branch.
Public Sub Log(ByVal s As String) Implements ILog.Log
    m_logged = m_logged + 1
    m_last = s
End Sub

Public Property Get Logged() As Long Implements ILog.Logged
    Logged = m_logged
End Property

' B05: the reference count reaching zero is the only thing that gets us here,
' so every TERM line in the run log is an actual Release-to-zero.
Private Sub Class_Terminate()
    Debug.Print "TERM last=" & m_last
End Sub
