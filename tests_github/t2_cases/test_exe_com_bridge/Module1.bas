Attribute VB_Name = "Module1"
' ExeComBridge 03 regression: an EXE project-class instance crossing a COM
' boundary must be wrapped into a real IDispatch.
' Before the fix, `New bHello` handed the raw C struct pointer to the peer's
' VT_DISPATCH slot; the peer's AddRef read the struct's first field as a vtable
' pointer -> 0xC0000005 (the VBMAN_DEMO.exe startup crash).
' After the fix, the COM-argument packing point wraps the instance via
' ComObject_FromInstance, so the peer can invoke it late-bound with CallByName.
' Peer is Scripting.Dictionary, so this case needs no VBMAN.dll.
Option Explicit

Sub Main()
    Dim d As Object
    Dim ok As Long

    Set d = CreateObject("Scripting.Dictionary")
    ok = 0

    ' KEY LINE: `New <project class>` used as a COM argument (Add's Item is Variant)
    Debug.Print "EXEB:1 before Add"
    d.Add "x", New bHello
    Debug.Print "EXEB:2 after Add"

    ' Fetch it back from the peer, then invoke late-bound (needs a real IDispatch)
    Dim o As Object
    Set o = d("x")
    Debug.Print "EXEB:3 got object"
    CallByName o, "Ping", VbMethod
    ok = ok + 1
    Debug.Print "EXEB:4 after CallByName"

    ' Function call round trip through the peer
    Dim s As String
    s = CallByName(o, "Echo", VbMethod, "abc")
    Debug.Print "EXEB:5 echo=" & s
    If s = "echo:abc" Then ok = ok + 1

    Debug.Print "EXEB:"; ok; "/2"
    If ok = 2 Then
        Debug.Print "EXE-COM-BRIDGE PASSED"
    Else
        Debug.Print "EXE-COM-BRIDGE FAILED"
    End If
End Sub
