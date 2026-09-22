VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
END
Attribute VB_Name = "Box"
Option Explicit

Class Box(Of T)

Interface IThing
    Sub Go()
End Interface

Private m_v As T

Public Sub SetV(ByVal v As T)
    m_v = v
End Sub

End Class
