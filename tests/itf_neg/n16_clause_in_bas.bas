Option Explicit

Interface IShape
    Function Area() As Double
End Interface

' A member-level Implements clause only makes sense in a class module.
Private Function Cover(ByVal p As String) As Double Implements IShape.Area
    Cover = 1#
End Function
