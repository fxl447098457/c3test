VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
END
Attribute VB_Name = "InhDerived"
Option Explicit

Inherits InhMid

Public Function Name2() As String
    Name2 = "derived"
End Function

' bare (unqualified) call of an inherited member: merged member face must resolve inside the class
Public Function BumpTwice() As Long
    Me.Bump
    Me.Bump
    BumpTwice = Me.Hits()
End Function
