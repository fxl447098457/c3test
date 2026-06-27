' P8.1: 三维数组测试
Option Explicit

Public Sub Main()
    ' 三维数组
    Dim cube(1 To 2, 1 To 3, 1 To 2) As Long
    cube(1, 1, 1) = 111
    cube(2, 3, 2) = 232
    Debug.Print "3D:"; cube(1, 1, 1); cube(2, 3, 2)
    Debug.Print "UBound3D_1="; UBound(cube, 1)
    Debug.Print "UBound3D_2="; UBound(cube, 2)
    Debug.Print "UBound3D_3="; UBound(cube, 3)

    ' 遍历3D验证
    Dim sum As Long
    Dim i As Long
    Dim j As Long
    Dim k As Long
    For i = 1 To 2
        For j = 1 To 3
            For k = 1 To 2
                cube(i, j, k) = i * 100 + j * 10 + k
            Next k
        Next j
    Next i
    sum = 0
    For i = 1 To 2
        For j = 1 To 3
            For k = 1 To 2
                sum = sum + cube(i, j, k)
            Next k
        Next j
    Next i
    Debug.Print "3D sum="; sum
    Debug.Print "3D DONE"
End Sub
