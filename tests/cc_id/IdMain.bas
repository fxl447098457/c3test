Attribute VB_Name = "IdMain"
Option Explicit

' ai/022 B11/C02: the identity resolver has no runtime surface yet (C05 is what turns
' ProgID/New into code), so this project only has to reach stage 2.7. The assertions live
' in run_tests.ps1 and match the "C3: CoClass ..." line the resolver prints per block.

Sub Main()
    Debug.Print "cc_id"
End Sub
