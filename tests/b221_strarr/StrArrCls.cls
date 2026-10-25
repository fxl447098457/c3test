VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
END
Attribute VB_Name = "StrArrCls"
Option Explicit

' Ledger 305 B220 (knife 222). vb6_cls_<C>_Destroy used to ask "which element type is this field?"
' BEFORE it asked "is this field an array at all". resolveArrayElemType() strips the array-ness off
' the answer, so a String ARRAY field answered String and took the scalar arm:
' vb6_BSTR_Free(me->mK) -- i.e. SysFreeString() handed a SafeArray carrier. Measured on the target
' project (VbEclipse List/PopupMenu) and on this fixture's own minimal shape, x64 and x86 alike:
' the client process died at Release with 0xC0000374 (STATUS_HEAP_CORRUPTION), silently.
'
' The three field shapes here are the whole point:
'   mK  dynamic String array -- carrier appears on the first ReDim Preserve
'   mF  fixed-dim String array -- carrier appears at instance creation, so even create+Release dies
'   mS  scalar String        -- the arm that MUST stay (this field is the BSTR itself)
' All three are also reachable through IDispatch so the in-proc client can drive them.

Private mK() As String
Private mF(2) As String
Private mS As String

Public Function Add(ByVal k As String) As Long
    Dim n As Long
    n = Count()
    ReDim Preserve mK(n)
    mK(n) = k
    Add = n + 1
End Function

Public Function Count() As Long
    Dim n As Long
    On Error Resume Next
    n = UBound(mK) + 1
    If Err.Number <> 0 Then n = 0
    Count = n
End Function

' clamped so the probe's fixed argument values (40, 9, 7, 12) still read a real element
Public Function KeyAt(ByVal i As Long) As String
    Dim hi As Long
    hi = Count() - 1
    If hi < 0 Then
        KeyAt = "<empty>"
        Exit Function
    End If
    If i > hi Then i = hi
    If i < 0 Then i = 0
    KeyAt = mK(i)
End Function

Public Function TagAt(ByVal i As Long) As String
    mF(i Mod 3) = "tag" & (i Mod 3)
    TagAt = mF(i Mod 3)
End Function

Public Function EchoS(ByVal s As String) As String
    mS = s
    EchoS = mS
End Function
