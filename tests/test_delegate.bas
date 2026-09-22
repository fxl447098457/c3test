Option Explicit

' ============================================================
'  test_delegate.bas - Delegate 语句 (tB 扩展) 运行门禁
'
'  覆盖口径:
'    1. Delegate Sub/Function 声明; CDecl 尾置; 委托值 = 调用桩地址, LongPtr 位兼容
'    2. op = AddressOf f 签名检查通过后的直调 / 初始化器绑定 / 重赋值
'    3. stdcall API 回调 (EnumWindows) 与 cdecl API 回调 (qsort) —— 双约定双架构
'    4. 模块级委托变量
'  负例 (签名不符报错/跨模块拒绝) 见 ai 台账记录的一次性探针.
' ============================================================

Private Declare Function EnumWindows Lib "user32" (ByVal lpEnumFunc As LongPtr, ByVal lParam As LongPtr) As Long
Private Declare Sub QSort Lib "msvcrt" Alias "qsort" (ByRef base As Any, ByVal num As LongPtr, ByVal width As LongPtr, ByVal cmp As LongPtr)

Private Delegate Function Operation(ByVal A As Long, ByVal B As Long) As Long
Private Delegate Function EnumSink(ByVal hWnd As LongPtr, ByVal lParam As LongPtr) As Long
Private Delegate Function CmpFn CDecl (ByRef a As Long, ByRef b As Long) As Long

Dim gOp As Operation

Private m_Count As Long

Private Function Addition(ByVal A As Long, ByVal B As Long) As Long
    Addition = A + B
End Function

Private Function MulFunc(ByVal A As Long, ByVal B As Long) As Long
    MulFunc = A * B
End Function

Private Function WndSeen(ByVal hWnd As LongPtr, ByVal lParam As LongPtr) As Long
    m_Count = m_Count + 1
    WndSeen = 1  ' TRUE: 继续枚举
End Function

Private Function CmpQ(ByRef a As Long, ByRef b As Long) As Long
    CmpQ = a - b
End Function

Sub Main()
    ' (1) 赋值绑定 + 直调
    Dim op As Operation
    op = AddressOf Addition
    If op(5, 6) = 11 Then Debug.Print "CALL=Y"

    ' (2) 初始化器绑定
    Dim op2 As Operation = AddressOf MulFunc
    If op2(5, 6) = 30 Then Debug.Print "INIT=Y"

    ' (3) 重赋值换目标
    op = AddressOf MulFunc
    If op(2, 3) = 6 Then Debug.Print "REASSIGN=Y"

    ' (4) LongPtr 位兼容直通
    Dim d As LongPtr
    d = op
    If d <> 0 Then Debug.Print "BITCMP=Y"

    ' (5) stdcall API 回调 (x86 下由桩吸收约定)
    Dim sink As EnumSink
    sink = AddressOf WndSeen
    m_Count = 0
    Dim r As Long
    r = EnumWindows(sink, 0)
    If m_Count > 0 Then Debug.Print "APICB=Y"

    ' (6) cdecl (CDecl) API 回调: qsort
    Dim arr(0 To 2) As Long
    arr(0) = 3
    arr(1) = 1
    arr(2) = 2
    Dim cf As CmpFn
    cf = AddressOf CmpQ
    QSort arr(0), 3, 4, cf
    If arr(0) = 1 And arr(1) = 2 And arr(2) = 3 Then Debug.Print "QSORT=Y"

    ' (7) 模块级委托变量
    gOp = AddressOf Addition
    If gOp(4, 5) = 9 Then Debug.Print "MODVAR=Y"

    Debug.Print "DELEGATE-DONE"
End Sub
