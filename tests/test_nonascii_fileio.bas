' test_nonascii_fileio.bas - Fix 197: VB6 runtime file/dir ops on NON-ASCII (Chinese) paths.
'
' This source is deliberately PURE ASCII. It verifies non-ASCII *paths* and non-ASCII
' file *contents*, so it must not itself depend on the compiler's source-encoding path
' (a different fix) - otherwise it could fail for an unrelated reason. The Chinese
' names are therefore built at runtime with ChrW.
'
' The directory name is 3 CJK chars = 9 UTF-8 bytes (ODD). The old code truncated each
' UTF-16 unit to one char, so an odd byte count was the reliably-broken case.
'
' Covers: MkDir / Open(Output,Input) / Print# / Line Input# / Write# / FileLen /
'         FileCopy / Name / Kill / ChDir / CurDir / Dir / RmDir.

Sub Main()
    Dim cn As String
    Dim d As String
    Dim fa As String
    Dim fb As String
    Dim fc As String
    Dim h As Integer
    Dim s As String
    Dim back As String
    Dim n1 As Long

    ' U+4E2D U+6587 U+8DEF = "Chinese path" (zhong wen lu); U+5F84 = "path"
    cn = ChrW(&H4E2D) & ChrW(&H6587) & ChrW(&H8DEF)
    d = App.Path & "\" & cn & ChrW(&H5F84)
    ' U+7532 U+4E59 U+4E19 = jia / yi / bing (three distinct file names)
    fa = d & "\" & ChrW(&H7532) & ".txt"
    fb = d & "\" & ChrW(&H4E59) & ".txt"
    fc = d & "\" & ChrW(&H4E19) & ".txt"

    ' --- 0) clean slate left over from a previous run (all ops here tolerate failure) ---
    Kill fa
    Kill fb
    Kill fc
    RmDir d

    ' --- 1) MkDir on a Chinese path, and Dir really sees the directory ---
    MkDir d
    Debug.Print "NA1-MKDIR=" & YN(Len(Dir(d)) > 0)

    ' --- 2) Print# a Chinese string, then FileLen > 0 ---
    s = cn & "=alpha|beta"
    h = FreeFile
    Open fa For Output As #h
    Print #h, s
    Close #h
    n1 = FileLen(fa)
    Debug.Print "NA2-PRINT-SIZE=" & YN(n1 > 0)

    ' --- 3) Line Input# round-trip through the same ACP codec.
    ' Windows text mode = ACP (like real VB6). On a DBCS ACP (936...) the CJK text
    ' must come back EXACTLY; on a non-DBCS ACP (CP1252 CI runners) real VB6 also
    ' degrades CJK to "?" - that lossy result is the correct, faithful outcome.
    h = FreeFile
    Open fa For Input As #h
    Line Input #h, back
    Close #h
    If back = s Then
        Debug.Print "NA3-ROUNDTRIP=Y"
    ElseIf back = "???" & "=alpha|beta" Then
        Debug.Print "NA3-ROUNDTRIP=Y"   ' non-DBCS ACP fallback: VB6-faithful
    Else
        Debug.Print "NA3-ROUNDTRIP=N"
    End If

    ' --- 4) Write# to a Chinese path: content must survive file I/O. Exact CJK on
    ' DBCS ACPs, the "?" degradation on non-DBCS ACPs (same as real VB6).
    h = FreeFile
    Open fc For Output As #h
    Write #h, cn
    Close #h
    h = FreeFile
    Open fc For Input As #h
    Line Input #h, back
    Close #h
    Debug.Print "NA4-WRITE=" & YN(InStr(back, cn) > 0 Or InStr(back, "???") > 0)

    ' --- 5) FileCopy between two Chinese paths ---
    FileCopy fa, fb
    Debug.Print "NA5-FILECOPY=" & YN(FileLen(fb) = n1)

    ' --- 6) Kill a Chinese path ---
    Kill fa
    Debug.Print "NA6-KILL=" & YN(Len(Dir(fa)) = 0)

    ' --- 7) Name (rename) a Chinese path ---
    Kill fc
    Name fb As fc
    Debug.Print "NA7-NAME=" & YN(FileLen(fc) = n1)

    ' --- 8) ChDir into a Chinese dir, CurDir must report it ---
    ChDir d
    Debug.Print "NA8-CHDIR=" & YN(InStr(CurDir, cn) > 0)
    ChDir App.Path

    ' --- 9) RmDir the Chinese dir ---
    Kill fc
    RmDir d
    Debug.Print "NA9-RMDIR=" & YN(Len(Dir(d)) = 0)

    Debug.Print "NONASCII-FILEIO-DONE"
End Sub

' "Y"/"N" so every marker above is a plain AAA=Y / AAA=N assertion.
Private Function YN(ByVal b As Boolean) As String
    If b Then
        YN = "Y"
    Else
        YN = "N"
    End If
End Function
