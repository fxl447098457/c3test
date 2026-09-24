Attribute VB_Name = "N24ViaInBas"
Option Explicit

Interface IN24
    Sub Ping()
End Interface

Implements IN24 Via m_h

Private m_h As Long
