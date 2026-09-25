VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
END
Attribute VB_Name = "P08Coclass"
Option Explicit

' ai/022 B11/C03b positive (probe .build/probe_c03b/pd): the class the block binds to does
' not declare the contract members itself -- its base class does. A coclass contract is met
' by the whole chain, which is exactly why the aggregation runs after stage 3.4 and not at
' stage 2.7 (D50-1).

Interface P08IShape
    Sub Move(dx As Long)
    Function Area() As Double
End Interface

CoClass P08Circle
    [Implementation("P08Der")]
    [Default] Interface P08IShape
End CoClass
