VERSION 5.00
Begin VB.Form SbHmForm 
   Caption         =   "SbHmForm"
   ClientHeight    =   2400
   ClientLeft      =   120
   ClientTop       =   465
   ClientWidth     =   7000
   LinkTopic       = "Form1"
   ScaleHeight     =   2400
   ScaleWidth      =   7000
   StartUpPosition =   3  'Windows Default
   Begin MSComctlLib.StatusBar StatusBar1 
      Align           =   2  'Align Bottom
      Height          =   315
      Left            =   0
      TabIndex        =   0
      Top             =   2085
      Width           =   7000
      _ExtentX        =   12348
      _ExtentY        =   556
      Style           =   0
      BeginProperty Panels {8E3867A5-8586-11D1-B16A-00C0F0283628} 
         NumPanels       =   3
         BeginProperty Panel1 {8E3867AB-8586-11D1-B16A-00C0F0283628} 
            AutoSize        =   0
            Object.Width           =   1500
            MinWidth        =   26
            Text            =   "A"
            Key             =   "fx1"
         EndProperty
         BeginProperty Panel2 {8E3867AB-8586-11D1-B16A-00C0F0283628} 
            AutoSize        =   0
            Object.Width           =   2646
            MinWidth        =   26
            Text            =   "BB"
            Key             =   "fx2"
         EndProperty
         BeginProperty Panel3 {8E3867AB-8586-11D1-B16A-00C0F0283628} 
            AutoSize        =   1
            MinWidth        =   26
            Text            =   "SP"
            Key             =   "sp"
         EndProperty
      EndProperty
   End
End
Attribute VB_Name = "SbHmForm"
Option Explicit

' zhang #206 (B41) measure surface. Written in the shape the **real VB6 designer** saves:
' `BeginProperty Panels {GUID}` + `BeginProperty PanelN {GUID}` + the `Object.Width` line.
' Measured on 5623 third-party .frm files: those panel numbers tile the bar's `_ExtentX`
' (himetric, 0.01 mm), NOT its `Width` (twips) - and the property itself is twips in code.
' So the key name carries a prefix our ingest never looked for, and the value carries a
' unit our store never had. Before this knife the whole design value was dropped:
' fixed panels fell back to the text measure (HM01 False, RAW shows a single-digit width).
'
' What is pinned here:
'   HM01 the two fixed panels came out as himetric widths, not text widths   (the red face)
'   HM02 both went through ONE conversion (same factor, DPI-independent ratio)
'   HM03 the spring panel still absorbs the remainder (guard, True on both sides)
'   HM04 the panels themselves arrived at all                  (existence guard)
'   HM05-RAW only: Panels(i).Width is still the REQUESTED value -> B41's other face,
'        deliberately not changed by this knife (see the ledger).
Private Declare Function SbGetParts Lib "user32" Alias "SendMessageW" (ByVal hWin As LongPtr, _
    ByVal msg As Long, ByVal n As Long, ByRef parts As Any) As Long
Private Declare Function GetClientRect Lib "user32" (ByVal hWin As LongPtr, ByRef rc As RECT) As Long
Private Type RECT
    Left As Long
    Top As Long
    Right As Long
    Bottom As Long
End Type

Function TF(b As Boolean) As String
    If b Then TF = "True" Else TF = "False"
End Function

Private Sub Form_Load()
    Dim pt(0 To 3) As Long
    Dim r As RECT
    Dim w1 As Long, w2 As Long, e3 As Long, cw As Long
    Dim ok1 As Boolean, ok2 As Boolean, ok3 As Boolean, ok4 As Boolean

    SbGetParts StatusBar1.hWnd, 1030, 3, pt(0)
    w1 = pt(0)                       ' panel1 right edge == its width (it starts at 0)
    w2 = pt(1) - pt(0)               ' panel2 laid-out width
    e3 = pt(2)                       ' last edge == whole client width
    GetClientRect StatusBar1.hWnd, r
    cw = r.Right - r.Left

    Debug.Print "HM00-RAW w1=" & CStr(w1) & " w2=" & CStr(w2) & " e3=" & CStr(e3) & " cw=" & CStr(cw)

    ' HM01: 1500 hm is 57 px and 2646 hm is 100 px at 96 DPI; the texts "A"/"BB" measure
    ' far below these floors. So a pass here means the design width actually reached the RTL.
    ok1 = (w1 >= 40) And (w2 >= 80) And (w2 > w1)
    Debug.Print "HM01-HM-DESIGN-WIDTH=" & TF(ok1)

    ' HM02: one authority, so the two numbers keep the 2646:1500 ratio (integers aside).
    ok2 = Abs(w2 * 1500 - w1 * 2646) <= 3 * 2646
    Debug.Print "HM02-SINGLE-CONVERSION=" & TF(ok2)

    ' HM03/HM04: guards - they are True before this knife too, they only catch a bad fix.
    ok3 = (e3 = cw) And (cw > 0)
    Debug.Print "HM03-SPRING-TILES=" & TF(ok3)
    ok4 = (StatusBar1.Panels.Count = 3) And (StatusBar1.Panels(1).Key = "fx1") _
          And (StatusBar1.Panels(2).Text = "BB")
    Debug.Print "HM04-PANELS-ARRIVED=" & TF(ok4)

    ' RAW only: the requested-value caliber (B41's remaining face). Panel1 was given 1500 hm.
    Debug.Print "HM05-RAW cs=" & CStr(StatusBar1.Panels(1).Width) _
                & " bare=" & StatusBar1.Panels(2).Width

    Debug.Print "HM-DONE"
    Unload Me
End Sub
