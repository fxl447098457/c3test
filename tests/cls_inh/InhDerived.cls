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

' protected base members are visible to the derived class (ai/022 B08a)
Public Function RevealSecret(ByVal v As Long) As Long
    Me.SetSecret v
    RevealSecret = Me.Secret() + v
End Function

Public Function TagRoundTrip(ByVal v As String) As String
    Me.m_tag = v
    TagRoundTrip = Me.m_tag
End Function

' bare (unqualified) call of an inherited member: merged member face must resolve inside the class
Public Function BumpTwice() As Long
    Me.Bump
    Me.Bump
    BumpTwice = Me.Hits()
End Function

' overrides the base Overridable member: a direct call on the derived object binds here
Public Overrides Function Speak() As String
    Speak = "derived"
End Function

Public Function FrozenSeen() As String
    FrozenSeen = Me.Frozen()
End Function

' ai/022 B08d: the second slot is overridden here (multi-slot table + a real parameter)
Public Overrides Property Get Level() As Long
    Level = Me.m_lvl + 100
End Property

Public Overrides Property Get Item(ByVal v As Variant) As String
    Item = "derived" & CLng(v)
End Property

Public Overrides Function Greet(ByVal who As String) As String
    Greet = "hi " & who & " (derived)"
End Function
