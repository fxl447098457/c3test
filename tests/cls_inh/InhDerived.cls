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

' --- ai/022 B09 (MyBase = the implementation the *immediate* base would run) ------
' Overrides a member whose return type is a project class: com_entry.c used to emit this
' extern before InhBase's typedef (error C2143).
Public Overrides Function Maker() As InhBase
    Set Maker = Me
End Function

Private Sub Class_Initialize()
    Me.g_init = Me.g_init & "leaf;"
End Sub

' Speak/Greet/Tag are slots overridden below or in InhMid: MyBase. must NOT go through the
' table (that is what Me.Speak()/d.Speak() are for), and Tag pins the nearest-declaration
' rule (InhMid declares Tag itself, so MyBase.Tag() is "mid", not InhBase's "base").
Public Function BaseSpeak() As String
    BaseSpeak = MyBase.Speak()
End Function

Public Function BaseGreet(ByVal who As String) As String
    BaseGreet = MyBase.Greet(who)
End Function

Public Function BaseTag() As String
    BaseTag = MyBase.Tag()
End Function

' ai/022 B09c: `Set MyBase.<属性> = obj` (target side must reach the base's prop_set_ with a
' proper receiver, and `Set MyBase.<属性> = Nothing` must not leak the bare identifier).
Public Function SetMyBasePeer() As String
    Set MyBase.Peer = Me
    Dim p As InhBase
    Set p = MyBase.Peer
    SetMyBasePeer = p.PeerIs(Me)
End Function

Public Function SetMyBaseNothing() As String
    Set MyBase.Peer = Nothing
    SetMyBaseNothing = "cleared"
End Function

' MyBase.Class_Initialize is the *immediate* base's initializer (InhMid's), not the root's.
Public Sub RerunBaseInit()
    MyBase.Class_Initialize
End Sub

' property read + write through the base face (InhBase's m_name is Private, so only the
' accessors are reachable), and a public data field (InhBase.Label).
Public Function NameViaBase(ByVal v As String) As String
    MyBase.Name = v
    NameViaBase = MyBase.Name
End Function

Public Function LabelViaBase(ByVal v As String) As String
    MyBase.Label = v
    LabelViaBase = MyBase.Label
End Function

' InhMid's own initializer, re-run on demand (it is Private, so it goes through the
' construction-chain bridge).
Public Sub RerunMidInit()
    MyBase.Class_Initialize
End Sub
