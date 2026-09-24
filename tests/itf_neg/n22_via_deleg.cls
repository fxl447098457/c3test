VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
END
Attribute VB_Name = "N22ViaDeleg"
Option Explicit

' ai/022 B10 negative (pair of n22_via_base.cls): the holder field is a project class, but
' that class does not implement IN22 -> VB3030.

Interface IN22
    Sub Ping()
End Interface

Implements IN22 Via m_b

Private m_b As N22ViaBase
