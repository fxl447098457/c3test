VERSION 5.00
Begin VB.Form GCForm 
   Caption         =   "GeomCache"
   ClientHeight    =   3400
   ClientLeft      =   120
   ClientTop       =   465
   ClientWidth     =   6800
   ScaleHeight     =   3400
   ScaleMode       =   1  
   ScaleWidth      =   6800
   StartUpPosition =   3  
   Begin VB.Timer tmrGC 
      Interval        =   150
      Left            =   120
      Top             =   1560
   End
   Begin VB.ComboBox cboA 
      Height          =   247
      Left            =   4607
      TabIndex        =   4
      Top             =   2213
      Width           =   1507
   End
   Begin VB.TextBox txtA 
      Height          =   247
      Left            =   1007
      TabIndex        =   0
      Top             =   449
      Width           =   3001
   End
   Begin VB.PictureBox picP 
      Height          =   1201
      Left            =   2000
      ScaleMode       =   1  
      TabIndex        =   1
      Top             =   1003
      Width           =   2003
      Begin VB.Label lblP 
         Height          =   211
         Left            =   311
         TabIndex        =   5
         Top             =   222
         Width           =   907
      End
   End
   Begin VB.PictureBox picX 
      Height          =   801
      Left            =   4201
      ScaleMode       =   3  
      TabIndex        =   2
      Top             =   1003
      Width           =   1401
      Begin VB.Label lblX 
         Height          =   21
         Left            =   101
         TabIndex        =   6
         Top             =   44
         Width           =   203
      End
   End
End
Attribute VB_Name = "GCForm"
Attribute VB_GlobalNameSpace = False
Attribute VB_Creatable = False
Attribute VB_PredeclaredId = True
Attribute VB_Exposed = False
Option Explicit
' Ledger 230 = the four control geometry properties are VB-side VALUES, not a
' projection of the window rect. Reading GetWindowRect and converting back loses
' one step of the pixel grid: `txtA.Left = 5000` answered 4995, and the numbers
' the .frm designed are re-quantized the same way. A corpus scan over 105 design
' files measured 238 geometry values that are NOT a multiple of 15 (NewTab-test 86,
' ctrlslider 22, btnfocus 15, ... -- positions authored on a 120-DPI machine), so
' every one of those read back wrong before this fixture existed.
'   GC01/GC02   design numbers read back exactly (form route + container-child route)
'   GC03        placement witness: the window is still at the pixels the design
'               number implies, so the stored value did NOT move anything
'   GC04/GC05   runtime write and Move read back exactly (write side is the same
'               cache -- Move was the unverified half of ledger 193)
'   GC06        a pixel-mode container still reads pixels (the stored-twips cache
'               fails the unit gate there -- measured identical before/after)
'   GC07        switching the container's ScaleMode re-projects, switching back
'               returns the VB-side number again
'   GC08        self-heal: ComboBox is resized by the RTL at creation (dropdown
'               area), so its cached design height is stale and the read follows
'               the window instead -- the pixel gate is what makes that safe
'   GC09..GC12  the late-bound route (the RTL host model behind IDispatch) answers
'               the same numbers as the emitted one -- it used to carry a second
'               geometry implementation that never stored the VB-side value

Private Declare PtrSafe Function GetWindowRect Lib "user32" (ByVal hwnd As LongPtr, ByRef lpRect As RECTAPI) As Long
Private Declare PtrSafe Function GetParent Lib "user32" (ByVal hwnd As LongPtr) As LongPtr
Private Declare PtrSafe Function ScreenToClient Lib "user32" (ByVal hwnd As LongPtr, ByRef lpPoint As POINTAPI) As Long
Private Declare PtrSafe Function GetDeviceCaps Lib "gdi32" (ByVal hdc As LongPtr, ByVal nIndex As Long) As Long
Private Declare PtrSafe Function GetDC Lib "user32" (ByVal hwnd As LongPtr) As LongPtr
Private Declare PtrSafe Function ReleaseDC Lib "user32" (ByVal hwnd As LongPtr, ByVal hdc As LongPtr) As Long
Private Declare PtrSafe Function MulDiv Lib "kernel32" (ByVal nNumber As Long, ByVal nNumerator As Long, ByVal nDenominator As Long) As Long

Private Type RECTAPI
    Left As Long
    Top As Long
    Right As Long
    Bottom As Long
End Type
Private Type POINTAPI
    x As Long
    y As Long
End Type

Private Function TF(ByVal b As Boolean) As String
    If b Then TF = "True" Else TF = "False"
End Function

' The window's own position in the parent's client pixels -- asked straight from
' user32, so it cannot be the cached number answering for itself.
Private Function PxLeft(ByVal h As LongPtr) As Long
    Dim rc As RECTAPI
    Dim pt As POINTAPI
    GetWindowRect h, rc
    pt.x = rc.Left
    pt.y = rc.Top
    ScreenToClient GetParent(h), pt
    PxLeft = pt.x
End Function

Private Function PxTop(ByVal h As LongPtr) As Long
    Dim rc As RECTAPI
    Dim pt As POINTAPI
    GetWindowRect h, rc
    pt.x = rc.Left
    pt.y = rc.Top
    ScreenToClient GetParent(h), pt
    PxTop = pt.y
End Function

Private Function PxW(ByVal h As LongPtr) As Long
    Dim rc As RECTAPI
    GetWindowRect h, rc
    PxW = rc.Right - rc.Left
End Function

Private Function PxH(ByVal h As LongPtr) As Long
    Dim rc As RECTAPI
    GetWindowRect h, rc
    PxH = rc.Bottom - rc.Top
End Function

' The witnesses below are built from kernel32 MulDiv + the real device DPI, i.e.
' from the same two primitives the RTL's placement uses -- but asked from outside
' the product, so the cached number cannot be answering for itself. (Me.ScaleX was
' the first thing to try here and is NOT used: a Double handed back to a VB Long
' truncates on this path, so it reads 13 where MulDiv says 14.)
Private dpiX As Long
Private dpiY As Long

Private Sub ReadDpi()
    Dim d As LongPtr
    d = GetDC(0)
    dpiX = GetDeviceCaps(d, 88)   ' LOGPIXELSX
    dpiY = GetDeviceCaps(d, 90)   ' LOGPIXELSY
    ReleaseDC 0, d
End Sub

Private Function ToPx(ByVal twips As Long) As Long
    ToPx = MulDiv(twips, dpiX, 1440)
End Function

Private Function ToPy(ByVal twips As Long) As Long
    ToPy = MulDiv(twips, dpiY, 1440)
End Function

Private Function ToTwipX(ByVal px As Long) As Long
    ToTwipX = MulDiv(px, 1440, dpiX)
End Function

Private Function ToTwipY(ByVal px As Long) As Long
    ToTwipY = MulDiv(px, 1440, dpiY)
End Function

Private Sub tmrGC_Timer()
    tmrGC.Enabled = False
    ReadDpi

    Dim ok1 As Boolean, ok2 As Boolean, ok3 As Boolean
    Dim ok4 As Boolean, ok5 As Boolean, ok6 As Boolean
    Dim ok7 As Boolean, ok8 As Boolean
    Dim pxSwitch As Long

    ok1 = (txtA.Left = 1007) And (txtA.Top = 449) And (txtA.Width = 3001) And (txtA.Height = 247)
    Debug.Print "GC01-design=" & TF(ok1) & " raw=" & txtA.Left & "," & txtA.Top & "," & txtA.Width & "," & txtA.Height

    ok2 = (lblP.Left = 311) And (lblP.Top = 222) And (lblP.Width = 907) And (lblP.Height = 211)
    Debug.Print "GC02-child=" & TF(ok2) & " raw=" & lblP.Left & "," & lblP.Top & "," & lblP.Width & "," & lblP.Height

    ok3 = (PxLeft(txtA.hwnd) = ToPx(1007)) And (PxTop(txtA.hwnd) = ToPy(449)) _
        And (PxW(txtA.hwnd) = ToPx(3001)) And (PxH(txtA.hwnd) = ToPy(247))
    Debug.Print "GC03-place=" & TF(ok3) & " raw=" & PxLeft(txtA.hwnd) & "," & PxTop(txtA.hwnd) & "," & PxW(txtA.hwnd) & "," & PxH(txtA.hwnd) & "," & ToPx(1007)

    txtA.Left = 5000
    txtA.Top = 444
    txtA.Width = 7777
    txtA.Height = 3001
    ok4 = (txtA.Left = 5000) And (txtA.Top = 444) And (txtA.Width = 7777) And (txtA.Height = 3001) _
        And (PxLeft(txtA.hwnd) = ToPx(5000)) And (PxW(txtA.hwnd) = ToPx(7777)) _
        And (PxTop(txtA.hwnd) = ToPy(444)) And (PxH(txtA.hwnd) = ToPy(3001))
    Debug.Print "GC04-write=" & TF(ok4) & " raw=" & txtA.Left & "," & txtA.Top & "," & txtA.Width & "," & txtA.Height

    txtA.Move 1231, 451, 3007, 247
    ok5 = (txtA.Left = 1231) And (txtA.Top = 451) And (txtA.Width = 3007) And (txtA.Height = 247) _
        And (PxTop(txtA.hwnd) = ToPy(451)) And (PxH(txtA.hwnd) = ToPy(247))
    Debug.Print "GC05-move=" & TF(ok5) & " raw=" & txtA.Left & "," & txtA.Top & "," & txtA.Width & "," & txtA.Height

    ok6 = (lblX.Left = PxLeft(lblX.hwnd)) And (lblX.Width = PxW(lblX.hwnd)) _
        And (PxLeft(lblX.hwnd) = ToPx(101)) And (PxW(lblX.hwnd) = ToPx(203))
    Debug.Print "GC06-pixbox=" & TF(ok6) & " raw=" & lblX.Left & "," & PxLeft(lblX.hwnd) & "," & lblX.Width & "," & ToPx(203)

    picP.ScaleMode = 3
    pxSwitch = lblP.Left
    ok7 = (pxSwitch = PxLeft(lblP.hwnd)) And (lblP.Width = PxW(lblP.hwnd))
    picP.ScaleMode = 1
    ok7 = ok7 And (lblP.Left = 311) And (lblP.Width = 907)
    Debug.Print "GC07-modesw=" & TF(ok7) & " raw=" & pxSwitch & "," & PxLeft(lblP.hwnd) & "," & lblP.Left

    ok8 = (cboA.Height <> 247) And (cboA.Height = ToTwipY(PxH(cboA.hwnd)))
    Debug.Print "GC08-stale=" & TF(ok8) & " raw=" & cboA.Height & ",247," & PxH(cboA.hwnd) & "," & ToTwipY(PxH(cboA.hwnd))

    ' GC09..GC12 -- ledger 247. The IDispatch route (the RTL host model, which is
    ' what a late-bound `With obj : .Width = ...` or a UserControl's `Parent.Width`
    ' really calls) carried its OWN geometry code: reads did GetWindowRect + a
    ' hardcoded twips conversion, writes moved all four axes at once and stored
    ' nothing. So the same property answered two different numbers depending on
    ' which route the statement took -- measured before the fix: `f.Width = 7222`
    ' read back 7222 on the emitted route only after the window had been moved,
    ' and 7215 on the late-bound one. Both routes now ask vb6forms_ctrl.c.
    Dim ok9 As Boolean, ok10 As Boolean, ok11 As Boolean, ok12 As Boolean
    Dim g As Object
    Set g = Me
    Dim gwHost As Long, ghHost As Long, gwBack As Long

    g.Width = 7222
    gwHost = CLng(g.Width)
    ok9 = (gwHost = 7222) And (Me.Width = 7222)
    Debug.Print "GC09-late-write=" & TF(ok9) & " raw=" & gwHost & "," & Me.Width

    g.Height = 3333
    ghHost = CLng(g.Height)
    ok10 = (ghHost = 3333) And (Me.Height = 3333)
    Debug.Print "GC10-late-height=" & TF(ok10) & " raw=" & ghHost & "," & Me.Height

    ok11 = (PxW(Me.hwnd) = ToPx(7222)) And (PxH(Me.hwnd) = ToPy(3333))
    Debug.Print "GC11-late-place=" & TF(ok11) & " raw=" & PxW(Me.hwnd) & "," & ToPx(7222) & "," & PxH(Me.hwnd) & "," & ToPy(3333)

    Me.Width = 6011
    gwBack = CLng(g.Width)
    ok12 = (gwBack = 6011) And (Me.Width = 6011)
    Debug.Print "GC12-direct-write=" & TF(ok12) & " raw=" & gwBack & "," & Me.Width

    ' GC13..GC14 -- ledger 250. The Controls arm of the host model asked the emitter
    ' that deliberately answers Empty, so a late-bound caller got NULL and every
    ' downstream branch (Count / Item / For Each -- all of which were already written
    ' for collections) saw an empty set. VB6: a form's Controls enumerates its own
    ' children, so both numbers below are counts of real windows (5 here: the timer,
    ' the combo, the textbox and the two picture boxes; the labels live in the boxes).
    ' What an item still cannot answer is its VB name -- standard controls created by
    ' the emitted route carry no host-side name record (measured: firstName is empty
    ' while TypeName answers Control). That half is ledger 252, not this one.
    Dim ok13 As Boolean, ok14 As Boolean
    Dim n As Long
    n = CLng(Me.Controls.Count)
    ok13 = (n >= 4) And (n <= 8)
    Debug.Print "GC13-coll-count=" & TF(ok13) & " raw=" & n

    Dim it As Long
    Dim o As Object
    it = 0
    For Each o In Me.Controls
        it = it + 1
    Next
    ok14 = (it >= 4) And (it <= 8)
    Debug.Print "GC14-foreach-iters=" & TF(ok14) & " raw=" & it

    ' GC15 -- ledger 254. VB6's Form.Count is the SAME set the Controls collection
    ' enumerates, so the two readings have to agree. The host model used to answer a
    ' hardcoded 0 here. Charts 2020's ClsResizer sizes its cache with
    ' ReDim Rects(oForm.Count - 1) and then fills it with For Each over Controls, so a
    ' 0 left it writing Rects(0) into an empty array -> runtime error 9 at startup.
    ' Read it late-bound (Set g = Me), which is the shape the resizer uses.
    ' The range is pinned TOO, not only the agreement: before ledger 250 both numbers
    ' were 0, so "fc = n" alone would read True on a compiler where nothing works.
    Dim ok15 As Boolean
    Dim fc As Long
    Set g = Me
    fc = CLng(g.Count)
    ok15 = (fc = n) And (fc >= 4) And (fc <= 8)
    Debug.Print "GC15-form-count=" & TF(ok15) & " raw=" & fc & "/" & n

    Debug.Print "GC-DONE"
    Unload Me
End Sub
