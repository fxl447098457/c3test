Attribute VB_Name = "LocalArrayCarrier"
Option Explicit

' Acct #305 (甲) knife 156: a LOCAL array's carrier (vb6_SafeArray1D / ND) used to be minted on
' every call and reclaimed by nobody - scope exit only handed the class elements back (knife 152).
' Measured on the pre-fix compiler: 316 B/call (x86) and 334 B/call (x64) for a 64-element Long
' array, fixed or dynamic. This fixture asks the INCREMENT per call over N iterations and reports a
' band flag, never an absolute number (page commits are quantised; knife 56 set that precedent).
'
'   fix / dyn / nd  = the three shapes the exit now reclaims. Band = 4 B/call: the pre-fix reading
'                    is ~80x over it, so the flag has real red power, while a few pages of noise
'                    cannot make it flap.
'   alias / bound / give / take = the four shapes the escape rule refuses to free, because
'                    the carrier left the procedure and a second owner still reads it
'                    (into a module Variant / only borrowed through UBound / handed to the CALLER
'                    through VB6's zero-arg whole-array form `w()` / the LOCAL slot receiving the
'                    MODULE array's carrier through `l = mg`, which emits a bare pointer copy).
'                    These must KEEP leaking - that flag is what goes red if the escape rule is
'                    ever loosened. Measured: with free-forced-on for every shape, all three
'                    read 0 (red) while the element values STILL printed 123456 / 246810, because
'                    a freed-but-not-yet-reused block keeps its bytes in a Release build. So the
'                    read below is a same-direction witness, not the guard - do not trust it alone.
'                    take (B225) mints nothing, so its leak flag reads 0 in both builds; the guard
'                    for that shape is the Absent emit needle (no Destroy1D(l) with the exit
'                    comment), which the pre-fix build did emit.
'   Rule-mutation controls (this machine, x64): free-always=false -> fix/dyn/nd all N; true ->
'   alias/bound/give all 0. Both ends of the band can go red. Pre-fix readings of the six windows
'   are archived at .build/k1927_*.txt + k1931_destroys.txt.

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

Private Const N As Long = 20000

Private gmv As Variant
Private ggive As Variant
Private gborrow As Long
Private mg() As Long

Function PF() As Long
    Dim pmc As PMC
    pmc.cb = Len(pmc)
    If K32GetProcessMemoryInfo(-1, pmc, Len(pmc)) <> 1 Then
        PF = -1
    Else
        PF = CLng(pmc.PagefileUsage)
    End If
End Function

' "did this loop reclaim what it minted?" - 4 bytes per call, both directions of error visible.
Function Flag(ByVal bytes As Long) As String
    Dim per As Double
    If bytes < 0 Then bytes = 0
    per = CDbl(bytes) / CDbl(N)
    If per < 4# Then Flag = "Y" Else Flag = "N"
End Function

' "did this loop keep leaking?" - the mirror flag. The floor is 30 B/call because the measured
' unit cost of the three escaping shapes is not one number: alias/bound carry the array descriptor
' plus the data block (~97 and ~316 B/call), while the give form only carries the box plus its
' carrier (~93 B/call) - all three are 3x over the floor, and the reclaimed windows sit at 0.
Function LeakFlag(ByVal bytes As Long) As String
    Dim per As Double
    If bytes < 0 Then bytes = 0
    per = CDbl(bytes) / CDbl(N)
    If per > 30# Then LeakFlag = "1" Else LeakFlag = "0"
End Function

Sub UseFix()
    Dim f(0 To 63) As Long
    f(0) = 7
End Sub

Sub UseDyn()
    Dim d() As Long
    ReDim d(0 To 63)
    d(0) = 7
End Sub

Sub UseND()
    Dim m(1 To 2, 1 To 4) As Long
    m(1, 2) = 7
End Sub

' escapes: the carrier lands in a slot that outlives this procedure
Sub UseAlias()
    Dim a() As Long
    ReDim a(0 To 8)
    a(3) = 123456
    gmv = a
End Sub

' escapes (从严): only borrowed through UBound, but the whole array was used as a value
Sub UseBorrow()
    Dim u() As Long
    ReDim u(0 To 63)
    gborrow = UBound(u)
End Sub

' the VB6 zero-arg whole-array form - the carrier goes to the CALLER, not to the exit
Function GiveDyn() As Variant
    Dim w() As Long
    ReDim w(0 To 8)
    w(3) = 246810
    GiveDyn = w()
End Function

' escapes (the left-hand side!): `l = mg` emits a bare pointer copy, so after that line the LOCAL
' slot holds the MODULE array's carrier. Freeing it here would tear down the module's array -
' the worst shape this knife can reach, and it has 74 real-world occurrences (dyn<-unknown in the
' 9400-file corpus scan). The exit must keep its hands off, and mg(3) must still read 123456.
Sub TakeModuleArray()
    Dim l() As Long
    l = mg
    l(1) = 7
End Sub

Function FixBytes() As Long
    Dim p0 As Long, p1 As Long, i As Long
    p0 = PF()
    For i = 1 To N
        UseFix
    Next i
    p1 = PF()
    FixBytes = p1 - p0
End Function

Function DynBytes() As Long
    Dim p0 As Long, p1 As Long, i As Long
    p0 = PF()
    For i = 1 To N
        UseDyn
    Next i
    p1 = PF()
    DynBytes = p1 - p0
End Function

Function NdBytes() As Long
    Dim p0 As Long, p1 As Long, i As Long
    p0 = PF()
    For i = 1 To N
        UseND
    Next i
    p1 = PF()
    NdBytes = p1 - p0
End Function

Function AliasBytes() As Long
    Dim p0 As Long, p1 As Long, i As Long
    p0 = PF()
    For i = 1 To N
        UseAlias
    Next i
    p1 = PF()
    AliasBytes = p1 - p0
End Function

Function BorrowBytes() As Long
    Dim p0 As Long, p1 As Long, i As Long
    p0 = PF()
    For i = 1 To N
        UseBorrow
    Next i
    p1 = PF()
    BorrowBytes = p1 - p0
End Function

Function GiveBytes() As Long
    Dim p0 As Long, p1 As Long, i As Long
    p0 = PF()
    For i = 1 To N
        ggive = GiveDyn()
    Next i
    p1 = PF()
    GiveBytes = p1 - p0
End Function

Function TakeBytes() As Long
    Dim p0 As Long, p1 As Long, i As Long
    p0 = PF()
    For i = 1 To N
        TakeModuleArray
    Next i
    p1 = PF()
    TakeBytes = p1 - p0
End Function

Public Sub Main()
    Dim r As Long
    r = FixBytes()
    Debug.Print "B218-fix-ok="; Flag(r)
    r = DynBytes()
    Debug.Print "B219-dyn-ok="; Flag(r)
    r = NdBytes()
    Debug.Print "B220-nd-ok="; Flag(r)

    Dim mv0 As Variant
    mv0 = Empty
    gmv = mv0
    r = AliasBytes()
    Dim ra As Long
    ra = 0
    On Error Resume Next
    ra = CLng(gmv(3))
    Debug.Print "B221-alias-leak="; LeakFlag(r); " read="; ra

    r = BorrowBytes()
    Debug.Print "B222-bound-leak="; LeakFlag(r); " ubound="; gborrow

    r = GiveBytes()
    Dim rg As Long
    rg = 0
    ggive = GiveDyn()
    rg = CLng(ggive(3))
    Debug.Print "B223-give-leak="; LeakFlag(r); " read="; rg

    ReDim mg(0 To 8)
    mg(3) = 123456
    r = TakeBytes()
    Dim rm As Long
    rm = 0
    rm = CLng(mg(3))
    Debug.Print "B225-take-leak="; LeakFlag(r); " module-read="; rm
    Debug.Print "B224-DONE"
End Sub
