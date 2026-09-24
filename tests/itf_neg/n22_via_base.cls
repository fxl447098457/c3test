VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
END
Attribute VB_Name = "N22ViaBase"
Option Explicit

' ai/022 B10 negative: this class has a matching member but never declares
' `Implements IN22`, so it has no interface slot field to delegate through -> VB3030.

Private m_last As String

Public Sub Ping()
    m_last = "ping"
End Sub
