' P8.1: 多维数组测试
Option Explicit

Public Sub Main()
    ' 一维数组 (保持兼容)
    Dim arr1d(1 To 5) As Long
    arr1d(1) = 10
    arr1d(3) = 30
    arr1d(5) = 50
    Debug.Print "1D:"; arr1d(1); arr1d(3); arr1d(5)
    Debug.Print "UBound1D="; UBound(arr1d)
    Debug.Print "LBound1D="; LBound(arr1d)

    ' 二维数组 (P8.1核心)
    Dim matrix(1 To 3, 1 To 4) As Long
    matrix(1, 1) = 11
    matrix(2, 3) = 23
    matrix(3, 4) = 34
    Debug.Print "2D:"; matrix(1, 1); matrix(2, 3); matrix(3, 4)
    Debug.Print "UBound2D_1="; UBound(matrix, 1)
    Debug.Print "UBound2D_2="; UBound(matrix, 2)
    Debug.Print "LBound2D_1="; LBound(matrix, 1)

    ' 二维数组遍历验证 (累加)
    Dim sum As Long
    Dim i As Long
    Dim j As Long
    For i = 1 To 3
        For j = 1 To 4
            matrix(i, j) = i * 10 + j
        Next j
    Next i
    sum = 0
    For i = 1 To 3
        For j = 1 To 4
            sum = sum + matrix(i, j)
        Next j
    Next i
    Debug.Print "2D sum="; sum

    ' 动态数组 ReDim (1D)
    Dim dyn() As Long
    ReDim dyn(1 To 3) As Long
    dyn(1) = 100
    dyn(2) = 200
    dyn(3) = 300
    ReDim Preserve dyn(1 To 5) As Long
    dyn(4) = 400
    dyn(5) = 500
    Debug.Print "1D dyn:"; dyn(1); dyn(2); dyn(3); dyn(4); dyn(5)

    Debug.Print "P8.1 ALL TESTS DONE"
End Sub
