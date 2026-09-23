VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
END
Attribute VB_Name = "InhHolder"
Option Explicit

' ai/022 B08e-2: a field declared InhBase that holds the InhDerived instance. The bare
' receiver form already dispatched (INH32); the Me-prefixed field chain used to bind
' statically to the base implementation, which is what INH33..INH35 pin down.
Private m_up As InhBase

Public Sub Hold(ByVal o As InhBase)
    Set m_up = o
End Sub

Public Function ViaBare() As String
    ViaBare = m_up.Speak()
End Function

Public Function ViaMeField() As String
    ViaMeField = Me.m_up.Speak()
End Function

Public Function ViaMeFieldArg(ByVal who As String) As String
    ViaMeFieldArg = Me.m_up.Greet(who)
End Function

