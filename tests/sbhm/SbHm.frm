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

' zhang 299 (B130) spelling 3: a panel kept in a MODULE-LEVEL object variable.
Private mPo As Object

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
    Dim ok5 As Boolean
    Dim ok6 As Boolean
    Dim po As Object
    Dim pv As Variant
    Dim withW As Long
    Dim feCount As Long, feIdx As Long, feSum As Long, feWant As Long, feTxt As Long

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

    ' HM06 = zhang 300 (§B131). Panels(<number>).Index used to be emitted as the *ByKey
    ' exit with an INTEGER in a wchar_t* slot -> the RTL dereferenced address 0x1 and the
    ' process died during Form_Load. Both spellings have to answer, and with the SAME
    ' number: pinning only one of them is exactly how this shipped silently (the key form
    ' was always fine, so SB14-IDXBYKEY in the neighbour fixture stayed green).
    ok5 = (StatusBar1.Panels(2).Index = 2) And (StatusBar1.Panels("fx2").Index = 2)
    Debug.Print "HM06-INDEX-BOTH-FORMS=" & TF(ok5)
    ' RAW only: an out-of-range subscript answers 0 here. VB6 raises error 9 at that point
    ' - that caliber is a separate account, so this line is deliberately not pinned to a number.
    Debug.Print "HM07-RAW idx9=" & StatusBar1.Panels(9).Index

    ' HM08 = zhang 299 (B130): `Panels` as a real collection object. Before this knife
    ' the collection was never built at all: every "use the member as an object" spelling
    ' emitted vb6_ComGetObjectProp(<HWND>, L"Panels") - a HWND used as an IDispatch, the
    ' same symptom C29-8b wrote down for TreeView Nodes - and `For Each` did not even
    ' enter its body (zero iterations, measured on the pre-knife compiler).
    ' Pinned face: the enumerator, three ways at once - iteration count, the sum of the
    ' Index each item reports, and the total text length ("A"+"BB"+"SP" = 5). Pinning
    ' only "the loop ran" would let a one-item or mis-subscripted collection through.
    ' The widths are RAW, not pinned: Panels(i).Width still answers the REQUESTED value
    ' (B41's remaining face), so a width sum here would move when that caliber lands.
    feCount = 0
    feIdx = 0
    feSum = 0
    feTxt = 0
    For Each pv In StatusBar1.Panels
        feCount = feCount + 1
        feIdx = feIdx + CLng(pv.Index)
        feSum = feSum + CLng(pv.Width)
        feTxt = feTxt + Len(pv.Text)
    Next
    ' want= is the SAME three reads taken through the early-bound route. It measures 0:
    ' inside CLng(...) the receiver is re-resolved by the generic COM path, which is
    ' exactly B130's remaining face - so it is printed, never judged.
    feWant = CLng(StatusBar1.Panels(1).Width)
    feWant = feWant + CLng(StatusBar1.Panels(2).Width)
    feWant = feWant + CLng(StatusBar1.Panels(3).Width)
    ok6 = (feCount = 3) And (feIdx = 6) And (feTxt = 5)
    Debug.Print "HM08-FOREACH-COLLECTION=" & TF(ok6)
    Debug.Print "HM08-RAW fe=" & CStr(feCount) & " idx=" & CStr(feIdx) & " txt=" & CStr(feTxt) _
                & " sum=" & CStr(feSum) & " want=" & CStr(feWant)

    ' HM09..HM11 are RAW-only on purpose: the knife landed one of the four object
    ' spellings. `Set po =`, a module-level `As Object` and `With ...` still build their
    ' head in the emitter that handles Set-RHS / With-object-ref, which never asks the
    ' collection interceptor - so they still hand back the HWND-as-IDispatch chain and
    ' read empty/zero. Recorded in the ledger as the next face; these lines carry the
    ' reading, not a judgment, so the gate cannot silently bless them.
    Set po = StatusBar1.Panels(1)
    Debug.Print "HM09-RAW objvar w=" & CStr(po.Width) & " k=" & CStr(po.Key) _
                & " i=" & CStr(po.Index)
    Set mPo = StatusBar1.Panels(2)
    Debug.Print "HM10-RAW modvar w=" & CStr(mPo.Width) & " t=" & CStr(mPo.Text)
    With StatusBar1.Panels(1)
        withW = .Width
    End With
    Set po = StatusBar1.Panels("fx2")
    Debug.Print "HM11-RAW with=" & CStr(withW) & " keyobj i=" & CStr(po.Index) _
                & " w=" & CStr(po.Width)

    Debug.Print "HM-DONE"
    Unload Me
End Sub
