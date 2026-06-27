VERSION 1.0 CLASS
BEGIN
  MultiUse = -1
  Persistable = 0
  DataBindingBehavior = 0
  DataSourceBehavior = 0
  MTSTransactionMode = 0
END
Attribute VB_Name = "StringLib"
Option Explicit

Private m_Prefix As String

Public Property Let Prefix(ByVal Value As String)
    m_Prefix = Value
End Property

Public Property Get Prefix() As String
    Prefix = m_Prefix
End Property

Public Function Concat(ByVal A As String, ByVal B As String) As String
    Concat = m_Prefix & A & B
End Function

Public Function Length(ByVal S As String) As Long
    Length = Len(S)
End Function

Public Function Upper(ByVal S As String) As String
    Upper = UCase(S)
End Function

Public Function Lower(ByVal S As String) As String
    Lower = LCase(S)
End Function
