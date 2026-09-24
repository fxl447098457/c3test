Attribute VB_Name = "N39CoclassBlock"
Option Explicit

' ai/022 B11/C03b: a CoClass block name is not a base class. n39_der.cls inherits it and must
' be refused with a sentence that says so (VB3020 kept, wording split -- the old text blamed
' a name that does exist in the project).

Interface N39IShape
    Sub Move(dx As Long)
End Interface

CoClass N39Circle
    [Default] Interface N39IShape
End CoClass
