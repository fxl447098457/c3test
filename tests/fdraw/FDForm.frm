VERSION 5.00
Begin VB.Form FDForm 
   Caption         =   "FDraw"
   ClientHeight    =   3200
   ClientLeft      =   120
   ClientTop       =   465
   ClientWidth     =   6400
   ScaleHeight     =   3200
   ScaleWidth      =   6400
   StartUpPosition =   3  
   Begin VB.Timer tmrF 
      Interval        =   150
      Left            =   120
      Top             =   1560
   End
End
Attribute VB_Name = "FDForm"
Attribute VB_GlobalNameSpace = False
Attribute VB_Creatable = False
Attribute VB_PredeclaredId = True
Attribute VB_Exposed = False
Option Explicit
' 233 = Form drawing state (DrawWidth / CurrentX / CurrentY / ScaleMode) round-trips.
' Before: the write side was never registered in the cgen write table, so
'   `Me.DrawWidth = 3` emitted `vb6_Form_DrawGetWidth(hwnd) = 3;` (C2106, both
'   arches, nothing compiled at all). And the store itself carried two
'   incompatible encodings on one window-property name: the draw family wrote
'   int32+1 while Form Print wrote a float bit pattern -- each read the other as
'   garbage. Pins here are two-sided on purpose:
'   FD01/FD02/FD03  write then read back the very numbers handed in
'   FD07/FD08       a pixel witness (draw a point, ask the same DC about it)
'   FD10            the merged store: after Print advanced the pen, the drawing
'                   side reads a sane number, not a float reinterpreted as int
' Everything runs on the first Timer tick -- a window question is asked while the
' window is alive (the dcsurf / pcline rule).
Private Declare PtrSafe Function GetPixel Lib "gdi32" (ByVal hdc As LongPtr, ByVal x As Long, ByVal y As Long) As Long
Private Declare PtrSafe Function GetDC Lib "user32" (ByVal hwnd As LongPtr) As LongPtr
Private Declare PtrSafe Function ReleaseDC Lib "user32" (ByVal hwnd As LongPtr, ByVal hdc As LongPtr) As Long

Private Function TF(ByVal b As Boolean) As String
    If b Then TF = "True" Else TF = "False"
End Function

Private Sub tmrF_Timer()
    Dim d As LongPtr
    Dim r As Long
    Dim px2 As Long
    Dim px3 As Long
    Dim ok1 As Boolean, ok2 As Boolean, ok3 As Boolean
    Dim ok5 As Boolean, ok10 As Boolean
    Dim ok6 As Boolean

    tmrF.Enabled = False

    Me.DrawWidth = 3
    ok1 = (Me.DrawWidth = 3)
    Me.CurrentX = 100
    Me.CurrentY = 50
    ok2 = (Me.CurrentX = 100) And (Me.CurrentY = 50)
    Me.PSet (200, 120)
    Me.PSet (300, 130)
    ok3 = (Me.CurrentX = 300) And (Me.CurrentY = 130)

    Debug.Print "FD01-drawwidth=" & TF(ok1)
    Debug.Print "FD02-curxy=" & TF(ok2)
    Debug.Print "FD03-pset2=" & TF(ok3)
    Debug.Print "FD04-RAW xy=" & CStr(Me.CurrentX) & "," & CStr(Me.CurrentY) & " dw=" & CStr(Me.DrawWidth)
    Debug.Print "FD05-sm0=" & CStr(Me.ScaleMode)

    Me.ScaleMode = vbPixels
    Debug.Print "FD06-sm1=" & CStr(Me.ScaleMode)
    Me.PSet (40, 60), vbRed
    d = GetDC(Me.hwnd)
    Debug.Print "FD07-PIXEL=" & TF(GetPixel(d, 40, 60) = vbRed)
    px2 = GetPixel(d, 41, 61)
    ok5 = (px2 <> vbRed)
    ' 235: pen color = one store only. Me.ForeColor must reach a PSet that carries no
    ' color argument; the point painted earlier with an explicit vbRed must stay red,
    ' so this cannot be won by 'repaint the whole surface blue'.
    Me.ForeColor = vbBlue
    Me.PSet (100, 120)
    px3 = GetPixel(d, 100, 120)
    ok6 = (px3 = vbBlue) And (GetPixel(d, 40, 60) = vbRed)
    Debug.Print "FD11-forecolor=" & TF(ok6)
    Debug.Print "FD12-RAW pen=" & CStr(px3) & " blue=" & CStr(vbBlue) & " first=" & CStr(GetPixel(d, 40, 60))
    r = ReleaseDC(Me.hwnd, d)
    Debug.Print "FD08-neg=" & TF(ok5) & " raw=" & CStr(px2)

    Me.CurrentX = 0
    Me.CurrentY = 30
    Print "AB"
    Debug.Print "FD09-AFTERPRINT=" & CStr(Me.CurrentX) & "," & CStr(Me.CurrentY)
    ok10 = (Me.CurrentX = 0) And (Me.CurrentY > 30) And (Me.CurrentY < 3000)
    Debug.Print "FD10-printstore=" & TF(ok10)
    Debug.Print "FD-DONE"
    Unload Me
End Sub
