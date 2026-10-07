' Ledger 248 = handing a floating value to an integer target rounds now, and it rounds
' through ONE exit that CLng/CInt already used. Before, codegen fed the double straight
' into vb6_ChkLong(int64_t), so C truncated at the call boundary: `l = 6.73` answered 6
' and `l = 7 / 2` answered 3, while the *same* expression written `l = CLng(7 / 2)`
' answered 4 (vb6_CLng calls round()). Two answers for one decision = the family this
' repo keeps finding (#234 acquire-DC, #235 pen colour, #239 pen position, #247 geometry
' in the host model). Corpus exposure measured over the 90 emit captures: 36 sites
' (21 of them in Charts 2020's main project) store a division/float result into a narrow
' integer, so this was silently off by one in real layout math.
'
' F2L01..F2L07   values VB6 also rounds this way (no exact .5 tie involved)
' F2L08          the loop form of the same claim, deliberately built so truncation and
'                rounding CANNOT agree (x.75 values) -- the first version of this judge
'                used k/(2k+1), which is always below .5, and won on the broken compiler
' F2L09/F2L10    the invariant this cut creates: implicit narrowing == explicit
'                CLng/CInt, including the exact .5 ties where this repo deliberately
'                keeps half-away-from-zero (rev37 note in vb6rtl_conv.c; VB6 uses
'                banker's rounding there -- that difference is a separate account, which
'                is why ties are pinned as "same answer", not as "VB6's answer")
' F2L11          the overflow net still fires (this cut must not widen or lose it)
' F2L12/F2L13    guards: integer-only math and a Double target must not move
Option Explicit

Private Function TF(ByVal b As Boolean) As String
    If b Then TF = "True" Else TF = "False"
End Function

Private Sub Chk(ByVal tag As String, ByVal got As Long, ByVal want As Long)
    Debug.Print "F2L-" & tag & "=" & TF(got = want) & " raw=" & CStr(got) & "," & CStr(want)
End Sub

Public Sub Main()
    Dim d As Double
    Dim s As Single
    Dim l As Long
    Dim i As Integer
    Dim same As Boolean
    Dim k As Long

    d = 6.73
    l = d
    Chk "01-dbl-round-up", l, 7

    l = -6.73
    Chk "02-dbl-round-neg", l, -7

    l = 7 / 2
    Chk "03-div-half-up", l, 4

    l = 5 / 3
    Chk "04-div-third", l, 2

    s = 2.4
    l = s
    Chk "05-single-down", l, 2

    s = -2.6
    l = s
    Chk "06-single-neg-up", l, -3

    i = 6.73
    Chk "07-integer-target", CLng(i), 7

    same = True
    For k = 1 To 8
        l = (4 * k + 3) / 4          ' 1.75, 2.75, ... 8.75: rounding 2..9, truncation 1..8
        If l <> k + 1 Then same = False
    Next k
    Chk "08-rounds-not-truncates", CLng(same), -1

    same = True
    For k = 1 To 8
        l = (2 * k + 1) / 2          ' exact .5 ties: 1.5, 2.5, ... 8.5
        If l <> CLng((2 * k + 1) / 2) Then same = False
    Next k
    Chk "09-parity-clng-ties", CLng(same), -1

    same = True
    For k = 1 To 8
        i = (4 * k + 3) / 4
        If CLng(i) <> CInt((4 * k + 3) / 4) Then same = False
    Next k
    Chk "10-parity-cint", CLng(same), -1

    On Error Resume Next
    d = 1.0E+12
    l = d / 1.0
    Dim e6 As Long
    e6 = Err.Number
    Err.Clear
    On Error GoTo 0
    Chk "11-overflow-still-6", e6, 6

    l = 7 \ 2
    Chk "12-intdiv-untouched", l, 3

    d = 7 / 2
    Chk "13-double-untouched", CLng(d * 10), 35

    Debug.Print "F2L-DONE"
End Sub
