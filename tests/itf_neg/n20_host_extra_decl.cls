VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
END
Attribute VB_Name = "IReader"
Option Explicit

Interface IReader
    Sub Fetch(ByVal s As String)
End Interface

Private m_spares As Long
