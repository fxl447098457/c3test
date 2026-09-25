Interface IBad(Of T)
    Sub S()
End Interface

Interface IOk2
    Function F(Of T)(x As T) As Long
End Interface

Sub Main()
    Debug.Print "done"
End Sub
