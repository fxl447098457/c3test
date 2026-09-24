VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
END
Attribute VB_Name = "InhBase"
Option Explicit

Private Type TPoint
    x As Long
    y As Long
End Type

Private m_hits As Long
Protected m_secret As Long
Protected m_tag As String
Private m_pt As TPoint
Private m_name As String
Protected m_lvl As Long
Public Label As String

Public Sub Bump()
    m_hits = m_hits + 1
End Sub

Public Function Hits() As Long
    Hits = m_hits
End Function

Public Function Tag() As String
    Tag = "base"
End Function

Public Sub SetPt(ByVal v As Long)
    m_pt.x = v
    m_pt.y = v * 2
End Sub

Public Function PtSum() As Long
    PtSum = m_pt.x + m_pt.y
End Function

Public Function Cat(ByVal a As String, Optional b As String) As String
    If IsMissing(b) Then
        Cat = a & "/solo"
    Else
        Cat = a & "/" & b
    End If
End Function

Public Function Sum3(ByVal a As Long, ByVal b As Long, Optional c As Long) As Long
    If IsMissing(c) Then
        Sum3 = a + b
    Else
        Sum3 = a + b + c
    End If
End Function

Public Property Get Name() As String
    Name = m_name
End Property

Public Property Let Name(ByVal v As String)
    m_name = v
End Property

Public Overridable Property Get Level() As Long
    Level = m_lvl
End Property

Public Property Let Level(ByVal v As Long)
    m_lvl = v
End Property

Public Overridable Property Get Item(ByVal v As Variant) As String
    Item = "base" & CLng(v)
End Property

Protected Sub SetSecret(ByVal v As Long)
    m_secret = v
End Sub

Protected Function Secret() As Long
    Secret = m_secret
End Function
Public Overridable Function Speak() As String
    Speak = "base"
End Function

Public NotOverridable Function Frozen() As String
    Frozen = "frozen"
End Function

' --- ai/022 B08d: these two are virtual slots; the *Thru functions below are the
' dispatching call sites (Me.Pick() inside the base body must bind to the instance).
Public Overridable Function Pick() As String
    Pick = "base"
End Function

Public Overridable Function Greet(ByVal who As String) As String
    Greet = "hi " & who & " (base)"
End Function

' ai/022 B08e-6 site 9: `RefOf.Speak()` inside a Function whose return type is this class
' reads that function's return variable. The return variable is a plain C local, so the
' receiver is pure and the call dispatches (INH39/INH40 in InhMain.bas).
Public g_viaRet As String

Public Function RefOf() As InhBase
    Set RefOf = Me
    g_viaRet = RefOf.Speak()
End Function

Public Function PickThru() As String
    PickThru = Me.Pick()
End Function

Public Function SpeakThru() As String
    SpeakThru = Me.Speak()
End Function

Public Function GreetThru(ByVal who As String) As String
    GreetThru = Me.Greet(who)
End Function

' --- ai/022 B09 (MyBase + 构造链) -------------------------------------------------
' Maker returns a *project class*: an Overrides of it in a derived class is what used to
' break com_entry.c (extern emitted before that class's typedef -> error C2143).
Public Overridable Function Maker() As InhBase
    Set Maker = Me
End Function

Public g_init As String

' ai/022 B09c: a Property Set on the base, reached from the derived class through
' `Set MyBase.Peer = ...` (the target side used to emit the bare identifier MyBase -> C2065).
Private m_peer As InhBase

Public Property Get Peer() As InhBase
    Set Peer = m_peer
End Property

Public Property Set Peer(ByVal o As InhBase)
    Set m_peer = o
End Property

Public Function PeerIs(ByVal o As InhBase) As String
    If m_peer Is o Then
        PeerIs = "same"
    Else
        PeerIs = "diff"
    End If
End Function

Private Sub Class_Initialize()
    g_init = "base;"
End Sub
