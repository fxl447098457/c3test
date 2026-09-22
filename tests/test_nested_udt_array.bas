Option Explicit

' ============================================================
'  test_nested_udt_array.bas - 嵌套 UDT 动态数组的按 (行,列) 存取
'
'  复现目标 (VBFlexGridDemo 实测现象): VBFlexGrid 的单元格存储是
'      Cells.Rows(iRow).Cols(iCol).Text   (UDT 数组里套 UDT 动态数组字段)
'  参考图对照后, demo 里 col>=2 的 `TextMatrix(i,j) = i & "." & j` 逐行正确,
'  而 col 0 (`= i`, Long) 与 col 1 (`= StartDate + (i-1)`, Date) **每一行都显示
'  同一个值** (行头全 149 = 最后一次写入; 日期列全 "A" = 第 0 行的列头)。
'  本用例把同样的写入次序搬出来, 用 Debug.Print 直接看是否塌到第 0 行。
' ============================================================

Private Type TCELL
    Text As String
    Flags As Long
End Type

Private Type TCOLS
    Cols() As TCELL
    RowInfo As Long
End Type

Private Type TROWS
    Rows() As TCOLS
End Type

Private Cells As TROWS
Private DefaultCell As TCELL

Private Sub InitCells(ByVal nRows As Long, ByVal nCols As Long)
    Dim i As Long, j As Long
    ReDim Cells.Rows(0 To nRows - 1) As TCOLS
    For i = 0 To nRows - 1
        With Cells.Rows(i)
            ReDim .Cols(0 To nCols - 1) As TCELL
            For j = 0 To nCols - 1
                LSet .Cols(j) = DefaultCell
            Next j
        End With
    Next i
End Sub

Private Function GetCell(ByVal r As Long, ByVal c As Long) As String
    GetCell = Cells.Rows(r).Cols(c).Text
End Function

Private Sub SetCell(ByVal r As Long, ByVal c As Long, ByVal v As String)
    Cells.Rows(r).Cols(c).Text = v
End Sub

Sub Main()
    Dim i As Long, j As Long
    Dim DecStr As String
    Dim StartDate As Date

    InitCells 6, 4
    DecStr = Mid$(1.1, 2, 1)
    StartDate = DateSerial(2026, 1, 1)

    ' 与 MainForm.frm:388 同次序: 先填 j<>1 为字符串, j=1 换成长度可控的字符串
    ' (原样 `SetCell i, j, StartDate + (i-1)` 会命中另一个缺陷: Sub 的 String 形参
    '  收到 Date/Long 实参时不插 CStr 转换 —— 见 Fix 175 记录, 此处先绕开)
    For i = 1 To 5
        For j = 0 To 3
            If j <> 1 Then
                SetCell i, j, i & DecStr & j
            Else
                SetCell i, j, "D" & CStr(i - 1)
            End If
        Next j
    Next i
    ' 行头列: 赋数值 (绕开 Fix 175 用 CStr)
    For i = 1 To 5
        SetCell i, 0, CStr(i)
    Next i
    ' 列头行: 赋 String
    For j = 0 To 3
        SetCell 0, j, Chr(64 + j)
    Next j

    ' 期望: 1|D0|1.2 / 2|D1|2.2 / ... (逐行不同)
    For i = 1 To 5
        Debug.Print "NA" & i & "=" & GetCell(i, 0) & ";NB" & i & "=" & GetCell(i, 1) & ";NC" & i & "=" & GetCell(i, 2)
    Next i
    Debug.Print "HDR"; GetCell(0, 0); GetCell(0, 1); GetCell(0, 2); GetCell(0, 3)
    Debug.Print "NESTED-DONE"
End Sub
