VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
END
Attribute VB_Name = "P09Impl"
Option Explicit

' ai/022 B11/C03b positive: a coclass contract met by `Implements I Via m_holder` (B10).
' P09Impl declares neither Move nor Area -- the holder object answers both slots, so the
' per-slot VB3012 is waived exactly the way the Implements checker waives it (D51-3).

Implements P09IShape Via m_h

Private m_h As P09Holder

CoClass P09Circle
    [Implementation("P09Impl")]
    [Default] Interface P09IShape
End CoClass
