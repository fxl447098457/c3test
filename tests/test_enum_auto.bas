' P15.5 test: Enum auto-increment
Enum TestEnum
    A = 5
    B
    C
    D = 10
    E
End Enum

Sub Main()
    Debug.Print "A="; TestEnum.A
    Debug.Print "B="; TestEnum.B
    Debug.Print "C="; TestEnum.C
    Debug.Print "D="; TestEnum.D
    Debug.Print "E="; TestEnum.E
End Sub
