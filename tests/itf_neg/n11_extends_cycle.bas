Attribute VB_Name = "itf_n11"
Option Explicit

Interface IA Extends IB
    Sub X()
End Interface

Interface IB Extends IA
    Sub Y()
End Interface
