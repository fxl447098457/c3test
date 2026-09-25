VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
END
Attribute VB_Name = "N21ViaNoField"
Option Explicit

' ai/022 B10 negative: the Via target must name a module-level field of this class.
' m_missing is never declared here (only m_count is) -> VB3029.

Interface IN21
    Sub Ping()
End Interface

Implements IN21 Via m_missing

Private m_count As Long

Private Sub Ping()
End Sub
