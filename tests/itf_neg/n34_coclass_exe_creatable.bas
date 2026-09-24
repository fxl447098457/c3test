Attribute VB_Name = "N34CoClassExeCreatable"
Option Explicit

' ai/022 B11/C03a negative: an EXE project cannot register a COM server, so claiming
' creatability there is refused (the in-project half of CoClass stays available).

Interface IOne
    Sub A()
End Interface

CoClass CCA
    [ComCreatable(True)]
    Interface IOne
End CoClass

Sub Main()
    Debug.Print "x"
End Sub
