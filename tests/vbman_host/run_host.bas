Attribute VB_Name = "run_host"
' ===========================================================================
' run_host.bas - vbman runtime smoke host
'
' Used by tests\run_vbman_test.ps1 as the default host. Purpose: load the
' VBMAN.dll that C3 just built, exercise it at runtime, and report whether it
' crashes or hangs. Compile-time correctness is NOT the concern here -- that
' is regress_vbman.ps1's job.
'
' Design notes:
'   * Late binding only (CreateObject + IDispatch). No VBMANLIB type library
'     reference is declared on purpose:
'       1) the .vbp then contains no absolute path (repo is public);
'       2) the host builds even when the type library is not registered yet;
'       3) it keeps the test purely about the *runtime* COM path.
'   * Creating cVBMAN runs Class_Initialize, which instantiates several dozen
'     member objects (Json/Db/HttpClient/Csv/Ini/Logs/Cmd/...). That is the
'     heavy runtime path we actually want to smoke test.
'   * Output goes through Debug.Print, which the C3 runtime writes to stdout;
'     run_vbman_test.ps1 asserts on it via -Expect.
'   * ASCII only on purpose: .bas is read as ANSI by the VB6-family toolchain.
' ===========================================================================
Option Explicit

Sub Main()
    Dim vm As Object
    Dim v As String
    Dim ok As Long

    ok = 0
    Debug.Print "vbman-host:begin"

    Set vm = CreateObject("VBMANLIB.cVBMAN")
    If vm Is Nothing Then
        Debug.Print "vbman-host:create=FAIL"
        Debug.Print "vbman-host:ok="; ok
        Debug.Print "vbman-host:end"
        Exit Sub
    End If
    ok = ok + 1
    Debug.Print "vbman-host:create=OK"

    v = vm.Version()
    If Len(v) > 0 Then
        ok = ok + 1
        Debug.Print "vbman-host:version=OK"
    Else
        Debug.Print "vbman-host:version=FAIL"
    End If

    ' Second instance: exercises Class_Initialize / Class_Terminate twice.
    Dim vm2 As Object
    Set vm2 = CreateObject("VBMANLIB.cVBMAN")
    If Not (vm2 Is Nothing) Then
        ok = ok + 1
        Debug.Print "vbman-host:create2=OK"
    Else
        Debug.Print "vbman-host:create2=FAIL"
    End If
    Set vm2 = Nothing

    Debug.Print "vbman-host:ok="; ok
    Debug.Print "vbman-host:end"
End Sub
