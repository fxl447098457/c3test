VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
END
Attribute VB_Name = "CWriter"
Option Explicit

Implements IWriter

Private m_total As Long
Private m_last As String

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
