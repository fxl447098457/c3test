' Acct #305 knife 42 = section B154 (2026-10-12). The element type of a ReDim'd
' dynamic array is asked of the DECLARATION, not of the ReDim statement's own As
' clause. ASCII comments only (C3 reads sources as ANSI/GBK).
'
' What was wrong before this knife (readings .build/b1371_pre/r6.txt = BASE x64,
' rn6.txt / rn3.txt = NEW x64 / x86, the two arches line-for-line identical):
'   creation  vb6_SafeArrayReDim1D(vb6_sa_variant, ...)      <- As clause only
'   creation  vb6_SafeArrayReDimND(vb6_sa_variant, 2, ...)   <- As clause only
'   access    VB6_SA_AT(BSTR, s1, 1) / VB6_SA_ND_AT2(BSTR, ..., 2, 3)
'                                                          <- asks arrayElemTypes_
' One descriptor, two answers: allocation walked 24 bytes per element (x64) while
' the write walked 8, so the direct read agreed with the direct write (looked fine)
' while every consumer that trusts the descriptor itself walked the wrong stride.
' The witness that made it visible at run time is R3-box: v = a2 then v(2, 3) read
' back an EMPTY string on both arches (the box path landed in a different slot).
'
' Only UDT arrays had a declaration fallback before (arrayUdtElemTypes_, Fix 055);
' scalar / String / Object arrays did not. Fixed arrays were always right, and that
' is the answer this knife hands to the dynamic path too:
'   Dim f(5) As String   ->  vb6_SafeArrayCreate1D(vb6_sa_bstr, 0, 5)
'
' R1-label / R3-label / R5-label pin VarType + TypeName as they measure TODAY
' (8204 / "Variant"). VB6 answers 8200 / "String()" there: the box tag in
' vb6_VariantArray is a hardcoded vb6_vtArray | vb6_vtVariant and never asks the
' descriptor, and vb6_TypeName has no array branch. That is section B155 - these
' three needles are the shape witnesses, and they turn RED when B155 lands.
Option Explicit

Type TRElem
    i As Long
End Type

' Module-level so Main and GrowMp share one descriptor (R14 spans two procedures).
Private mp() As String

Public Sub Main()
    Dim s1() As String
    Dim a2() As String
    Dim b3() As Byte
    Dim l4() As Long
    Dim u5() As TRElem
    Dim w6() As Variant
    Dim x7 As Variant
    Dim s8() As String
    Dim v As Variant
    Dim p0() As String
    Dim w1() As Variant
    Dim b1() As Byte

    ' R1: 1-D String, bare ReDim. cell = direct round trip (worked before too -
    ' direct write and direct read use the same stride); label = the open B155 cell.
    ReDim s1(3)
    s1(1) = "A"
    Debug.Print "R1-cell="; s1(1)
    Debug.Print "R1-label-vt="; VarType(s1); " tn="; TypeName(s1)

    ' R2: ReDim Preserve keeps what was there and grows the bound (VB6 semantics).
    ReDim Preserve s1(6)
    Debug.Print "R2-preserve-cell="; s1(1); " ub="; UBound(s1)

    ' R3: ND String. box = the needle this knife fixes (empty before, "X" after).
    ReDim a2(1 To 3, 1 To 4)
    a2(2, 3) = "X"
    Debug.Print "R3-cell="; CStr(a2(2, 3))
    Debug.Print "R3-ub2="; UBound(a2, 2); " lb2="; LBound(a2, 2)
    v = a2
    Debug.Print "R3-box-cell="; CStr(v(2, 3))
    Debug.Print "R3-label-vt="; VarType(a2); " tn="; TypeName(a2)

    ' R4: ND Byte - the single most-used declared element type in the real VB6
    ' corpus (596 bare-ReDim lines), so the stride must be 1 byte, not 24.
    ReDim b3(1 To 2, 1 To 3)
    b3(2, 3) = 200
    Debug.Print "R4-cell="; b3(2, 3)
    v = b3
    Debug.Print "R4-box-cell="; CStr(v(2, 3))

    ' R5: ND Long - regression witness (this one already read back its value
    ' before the knife because write and read agreed with each other).
    ReDim l4(2, 2)
    l4(1, 2) = 77
    Debug.Print "R5-cell="; l4(1, 2)
    Debug.Print "R5-ub1="; UBound(l4, 1); " ub2="; UBound(l4, 2)
    Debug.Print "R5-label-vt="; VarType(l4); " tn="; TypeName(l4)

    ' R6: ND UDT - the family that already had a declaration fallback; must not move.
    ReDim u5(1, 2)
    Debug.Print "R6-ub2="; UBound(u5, 2)

    ' R7: declared As Variant array - Variant IS the declared answer, so the
    ' element stays a slot (this is the shape the old fallback got right by luck).
    ReDim w6(3)
    w6(2) = "V"
    Debug.Print "R7-cell="; CStr(w6(2)); " vt="; VarType(w6)

    ' R8: Variant slot (Dim x As Variant, then ReDim) - nothing declares an element
    ' type, so the descriptor keeps the variant slot and the box path reads it back.
    ReDim x7(1, 2)
    x7(1, 1) = "S"
    Debug.Print "R8-cell="; CStr(x7(1, 1))

    ' R9: ReDim WITH an As clause - the clause still wins (VB6 semantics, and the
    ' corpus measures declared == As on 1580 of 1588 such lines).
    ReDim s8(2) As String
    s8(1) = "Z"
    Debug.Print "R9-cell="; s8(1)

    ' ---- R10..R14: knife 43 = section B155 cell 4 --------------------------
    ' The FIRST allocation of a multi-dimensional array done as a ReDim Preserve.
    ' vb6_SafeArrayReDimPreserveND had no elemType parameter at all, so a NULL
    ' descriptor could only be created as vb6_sa_empty - which the size table answers
    ' with its default 4 bytes - while the access side walks the DECLARED stride
    ' (x64: String 8, Variant 24). The write runs off the end of a block a sixth of
    ' the size the accesses assume. This one is LOUD, not silent: the pre-knife
    ' binary dies with 0xC0000374 (heap corruption) having printed NOTHING (reading
    ' .build/b1382_preserve/rzb6.txt = 0 bytes).
    '
    ' One ND ReDim statement per array per procedure below, and that is not a style
    ' choice: a SECOND ND ReDim of the same array in the same scope is its own open
    ' cell - the generated bounds temp is named after the ARRAY (_redim_bounds_<name>),
    ' so it is declared twice inside one C function = cl C2374 (redefinition; multiple
    ' initialization).
    ' Measured on both binaries (.build/b1395_k43/{n6,b6}/c3-error.log), so it is
    ' older than both knives; R14 uses two procedures to work around it.
    ReDim Preserve p0(1 To 2, 1 To 3)
    p0(1, 1) = "one"
    p0(2, 3) = "six"
    Debug.Print "R10-c11="; CStr(p0(1, 1)); " c23="; CStr(p0(2, 3))
    Debug.Print "R10-ub2="; UBound(p0, 2); " lb2="; LBound(p0, 2)

    ' R11: Variant elements - 24 bytes walked inside a 4-byte block (x64).
    ReDim Preserve w1(1 To 2, 1 To 2)
    w1(1, 1) = "v11"
    w1(2, 2) = "v22"
    Debug.Print "R11-c11="; CStr(w1(1, 1)); " c22="; CStr(w1(2, 2))

    ' R12: Byte - the other half of the same mistake: 1 byte walked inside a 4-byte
    ' block, so it never overflowed and only the descriptor's own consumers (Destroy,
    ' the boxed element read) walked wrong.
    ReDim Preserve b1(1 To 2, 1 To 3)
    b1(1, 2) = 7
    b1(2, 3) = 200
    Debug.Print "R12-c12="; b1(1, 2); " c23="; b1(2, 3)

    ' R13: the box path over a Preserve-created descriptor - knife 42's second cell
    ' and knife 43's first one have to agree, or the value lands in a slot nobody
    ' reads back.
    v = p0
    Debug.Print "R13-box-c23="; CStr(v(2, 3))

    ' R14: Preserve must actually PRESERVE. The two statements sit in DIFFERENT
    ' procedures (see the C2374 note): Main allocates and writes mp(1,1), GrowMp grows
    ' the LAST dimension - the direction VB6 allows under Preserve - and writes
    ' mp(1,3). Reading both back here is the witness that the copy loop moved the old
    ' elements into the new block instead of dropping them.
    ReDim Preserve mp(1 To 2, 1 To 3)
    mp(1, 1) = "keep"
    GrowMp
    Debug.Print "R14-kept="; CStr(mp(1, 1)); " grown="; CStr(mp(1, 3))
    Debug.Print "R14-ub2="; UBound(mp, 2)

    Debug.Print "RE-DONE"
End Sub

' R14's second half: lives in its own scope so the ND bounds temp of this array is
' emitted once per function (see the C2374 note in Main).
Private Sub GrowMp()
    ReDim Preserve mp(1 To 2, 1 To 6)
    mp(1, 3) = "later"
End Sub
