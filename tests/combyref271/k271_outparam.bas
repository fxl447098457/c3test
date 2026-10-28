Option Explicit

' Fixture for the ByRef-object out-parameter station (knife 271). VB6 writes a
' method's result back through an out argument, and the idiom is to hand over the
' enclosing Function's OWN return variable -- ComCtlsDemo Common/Common.bas:1017
' does exactly that with Font.Clone. The two procedures below are the same sentence
' written against the two slot kinds (the return slot and a plain local): both must
' reach the same packer, and the degraded by-value form must not come back.
' ASCII on purpose: sources are read as ANSI by the driver.

Public Function CloneByReturnSlot(ByVal Font As IFont) As StdFont
    If Not Font Is Nothing Then Font.Clone CloneByReturnSlot
End Function

Public Sub CloneByLocalVar(ByVal Font As IFont)
    Dim g As StdFont
    Font.Clone g
End Sub

' Knife 273 adds the runtime leg to the same station: on a font the caller really
' holds (C3 builds New StdFont out of its own host font object), Clone is a method
' with ONE [out] argument in stdole2.tlb, so the new font has to come back through
' the argument. Before this knife no-slot read True (the caller slot stayed empty);
' aliased would mean Clone returned the SAME object -- VB6 gives an independent copy,
' and the caller then writes Bold/Underline on that copy.
Public Function CloneViaLocal(ByVal Font As IFont) As StdFont
    Dim h As StdFont
    Font.Clone h
    Set CloneViaLocal = h
End Function

' Knife 276 adds the HANDLE leg to that same font object. VB6's IFont also answers hFont,
' and that answer is the only way a UC gets a real LOGFONT: Common/Common.bas:1012 does
' GetObjectAPI(Font.hFont, LenB(LF), LF) then CreateFontIndirect(LF). C3 used to answer
' hFont with nothing, so that GetObject asked handle 0, the local UDT stayed all-zero and
' the window got the GDI default font (34 such windows counted in the target project).
' The probe asks TWICE on purpose, before and after writing Bold/Underline: the answer is
' derived from those eight fields and cached, so the second ask is the one that proves the
' cache notices the write (the write side has no hook to invalidate it).
Private Type K276LF
    LFHeight As Long
    LFWidth As Long
    LFEscapement As Long
    LFOrientation As Long
    LFWeight As Long
    LFItalic As Byte
    LFUnderline As Byte
    LFStrikeOut As Byte
    LFCharSet As Byte
    LFOutPrecision As Byte
    LFClipPrecision As Byte
    LFQuality As Byte
    LFPitchAndFamily As Byte
    LFFaceName(0 To 63) As Byte
End Type

Private Declare PtrSafe Function GetObjectAPI Lib "gdi32" Alias "GetObjectW" (ByVal hObject As LongPtr, ByVal nCount As Long, ByRef lpObject As Any) As Long

Public Sub K276ReadFont(ByVal Font As IFont, ByRef outWeight As Long, ByRef outUnderline As Long, ByRef outHeight As Long, ByRef outFaceFirst As Long)
    Dim lf As K276LF
    GetObjectAPI Font.hFont, LenB(lf), lf
    outHeight = lf.LFHeight
    outWeight = lf.LFWeight
    outUnderline = lf.LFUnderline
    ' The face name arrives as UTF-16 inside a byte array, so byte 0 is the first letter.
    ' 77 = "M" (MS Sans Serif) and it is what separates a real face from an empty one:
    ' an empty lfFaceName is the shape that makes CreateFontIndirectW hand back Segoe UI.
    outFaceFirst = lf.LFFaceName(0)
End Sub

Sub Main()
    Dim f As New StdFont
    Dim g As StdFont
    Dim h As StdFont
    Dim w0 As Long
    Dim u0 As Long
    Dim z0 As Long
    Dim w1 As Long
    Dim u1 As Long
    Dim z1 As Long
    Dim f0 As Long
    Dim f1 As Long
    Set g = CloneByReturnSlot(f)
    Set h = CloneViaLocal(f)
    Debug.Print "K271:no-slot="; (g Is Nothing)
    Debug.Print "K271:local-slot="; (h Is Nothing)
    Debug.Print "K271:aliased="; (g Is f)
    Debug.Print "K271-DONE"
    K276ReadFont f, w0, u0, z0, f0
    f.Bold = True
    f.Underline = True
    K276ReadFont f, w1, u1, z1, f1
    Debug.Print "K276:normal-weight-400="; (w0 = 400)
    Debug.Print "K276:height-measured="; (z0 <> 0)
    Debug.Print "K276:face-M="; (f0 = 77)
    Debug.Print "K276:bold-weight-700="; (w1 = 700)
    Debug.Print "K276:underlined="; (u1 <> 0)
    Debug.Print "K276:face-still-M="; (f1 = 77)
    Debug.Print "K276-DONE"
End Sub
