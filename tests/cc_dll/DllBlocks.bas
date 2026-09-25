Attribute VB_Name = "DllBlocks"
Option Explicit

CoClass PG
    [Implementation("CImpl")]
    ' B13b: this bit is what puts the GROUP ProgID into the product at all.
    ' Without it the block's own ProgID is not registered (D56-6 said the bit had
    ' no observable effect; that is exactly what this fixture now pins).
    [ComCreatable(True)]
    [Default] Interface IProbe
End CoClass
