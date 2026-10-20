Attribute VB_Name = "B164OwnedTemp"
Option Explicit

' Acct #454 (B164 shapes 1/2/3/5): an owned value minted inside a single statement and
' handed straight into a callee was never reclaimed - it never landed on any variable, so
' it never entered the codegen's free list. Measured on BASE (this machine, x86):
'   (1) n = Len("ab" & "cd")          75.2 B/call  (the concat result + both literals)
'   (2) n = Len(gArr(1) & "!")        50.2 B/call
'   (3) n = VarType(gColl(2))         25.6 B/call  (the owned copy Collection hands back)
'   (5) n = Len(v)   (v numeric Var)  24.8 B/call  (the string the conversion minted)
' Shape (4) - the For Each loop variable - is Acct #305 knife 55 and already 0; it stays
' here as a WITNESS: this knife must not break it.
'
' Caliber: statement-scoped typed list (the M22 `ansiTempsToFree_` precedent). The list is
' TYPED - shapes 1/2/5 yield a BSTR (vb6_BSTR_Free), shape 3 yields a vb6_VARIANT slot
' (vb6_VariantClear). One free for both would either keep leaking or read a VARIANT as a
' BSTR. The temp's lifetime = one statement, which is exactly VB6's own rule, so this does
' not replay the B165 dangling-pointer turn.
'
' The judgement asks the INCREMENT PER STORE (never an absolute number) and pins both
' arches with the same needles. Both memory reads must really succeed, otherwise the delta
' would be a fake zero (self-subtraction) and the criterion worthless.

Private Type PMC
    cb As Long
    PageFaultCount As Long
    PeakWorkingSetSize As LongPtr
    WorkingSetSize As LongPtr
    QuotaPeakPagedPoolUsage As LongPtr
    QuotaPagedPoolUsage As LongPtr
    QuotaPeakNonPagedPoolUsage As LongPtr
    QuotaNonPagedPoolUsage As LongPtr
    PagefileUsage As LongPtr
    PeakPagefileUsage As LongPtr
End Type

Declare Function K32GetProcessMemoryInfo Lib "kernel32" (ByVal hProcess As LongPtr, ByRef ppmc As PMC, ByVal cb As Long) As Long

Private gColl As Collection
Private gArr() As String

Function ReadPF(ByRef pmc As PMC) As Long
    Dim hr As Long
    pmc.cb = Len(pmc)
    hr = K32GetProcessMemoryInfo(-1, pmc, Len(pmc))
    If hr <> 1 Then
        ReadPF = -1
    Else
        ReadPF = CLng(pmc.PagefileUsage)
    End If
End Function

Sub Main()
    Dim pmc As PMC
    Dim pf0 As Long, pf1 As Long
    Dim i As Long, r As Long
    Dim n As Long
    Dim v As Variant
    Dim e As Variant
    Dim s As String
    Dim s1 As String, s2 As String
    Dim d1 As Long, d2 As Long, d3 As Long, d4 As Long, d5 As Long
    Dim sOk As String, aOk As String, cOk As String, vOk As String, fOk As String

    Set gColl = New Collection
    For i = 1 To 10
        gColl.Add String$(i, 65), "k" & i
    Next i
    ReDim gArr(1 To 10)
    For i = 1 To 10
        gArr(i) = String$(i, 66)
    Next i
    v = 12345
    s = "abcdefgh"

    ' --- shape 1: the concat result and both literals must all be reclaimed ---
    pf0 = ReadPF(pmc)
    For i = 1 To 100000
        n = Len("ab" & "cd")
    Next i
    pf1 = ReadPF(pmc)
    d1 = pf1 - pf0

    ' --- shape 2: same, with an array element as the left operand (that side is BORROWED
    ' and must never be freed - the negative control for "free the operands blindly") ---
    pf0 = ReadPF(pmc)
    For i = 1 To 100000
        n = Len(gArr(1) & "!")
    Next i
    pf1 = ReadPF(pmc)
    d2 = pf1 - pf0

    ' --- shape 3: the owned copy Collection hands back, only ever read ---
    pf0 = ReadPF(pmc)
    For i = 1 To 10000
        n = VarType(gColl(2))
    Next i
    pf1 = ReadPF(pmc)
    d3 = pf1 - pf0

    ' --- shape 5: the temp string the VARIANT->string conversion mints ---
    pf0 = ReadPF(pmc)
    For i = 1 To 100000
        n = Len(v)
    Next i
    pf1 = ReadPF(pmc)
    d4 = pf1 - pf0

    ' --- shape 4 witness: knife 55 fixed this one; it must stay 0 ---
    pf0 = ReadPF(pmc)
    For r = 1 To 300
        For Each e In gColl
            If VarType(e) <> 8 Then n = n + 1
        Next e
    Next r
    pf1 = ReadPF(pmc)
    d5 = pf1 - pf0

    ' 8 bytes per iteration is the ceiling; BASE sits one order of magnitude above it.
    If d1 < 8 * 100000 Then sOk = "Y" Else sOk = "N"
    If d2 < 8 * 100000 Then aOk = "Y" Else aOk = "N"
    If d3 < 8 * 10000 Then cOk = "Y" Else cOk = "N"
    If d4 < 8 * 100000 Then vOk = "Y" Else vOk = "N"
    If d5 < 8 * (300 * 10) Then fOk = "Y" Else fOk = "N"

    Debug.Print "B164T-api=" & IIf(pf0 >= 0, 1, 0) & "/" & IIf(pf1 >= 0, 1, 0)
    Debug.Print "B164T-concat=" & sOk
    Debug.Print "B164T-arrcat=" & aOk
    Debug.Print "B164T-coll=" & cOk
    Debug.Print "B164T-conv=" & vOk
    Debug.Print "B164T-foreach=" & fOk

    ' --- witnesses: the two shapes that were ALREADY right must not move ---
    ' (a) assigning to a Variant slot deep-copies and frees (knife 55 / knife 57)
    pf0 = ReadPF(pmc)
    For i = 1 To 10000
        v = gColl(2)
    Next i
    pf1 = ReadPF(pmc)
    Debug.Print "B164T-slot=" & IIf((pf1 - pf0) < 8 * 10000, "Y", "N")

    ' (b) a plain String argument mints nothing at all
    pf0 = ReadPF(pmc)
    For i = 1 To 100000
        n = Len(s)
    Next i
    pf1 = ReadPF(pmc)
    Debug.Print "B164T-plain=" & IIf((pf1 - pf0) < 8 * 100000, "Y", "N")

    ' --- value witnesses: freeing the temps must not disturb the values themselves ---
    n = Len("ab" & "cd")
    Debug.Print "B164T-len=" & n
    n = Len(gArr(3) & "!")
    Debug.Print "B164T-arrlen=" & n
    v = 12345
    n = Len(v)
    Debug.Print "B164T-vlen=" & n
    s1 = "A" & "B" & "C"
    s2 = s1 & "!" & s1
    Debug.Print "B164T-chain=" & s1 & "/" & s2
    ' a concat result stored into a variable still has to survive its statement
    s1 = "keep" & "me" & "alive"
    For i = 1 To 3
        s2 = s1 & CStr(i)
    Next i
    Debug.Print "B164T-stored=" & s1 & "/" & s2
    Debug.Print "B164T-DONE"
End Sub
