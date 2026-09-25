Attribute VB_Name = "LateMain"
Option Explicit

' ai/022 B17: 外部晚绑定客户 —— 与 tests\test_p613_typelib.bas 同一族
' (编译 DLL -> 注册 -> **另一个进程** CreateObject 后按名调用), 差别只在于这次点的对象
' 来自一个新式接口宿主的 DLL。刻意不写 Reference=: CreateObject 走真实注册表链路,
' 成员调用走 IDispatch 晚绑定 —— 这条路本批之前从没有真客户走过。
' 契约成员 (Ping/Got) 是 Private, 类的默认面上点不到; 那条读数由探针在 IDispatch 原语上
' 钉住 (NAMES=Ping hr=0x80020006), 这里不重复。
Sub Main()
    ' 激活失败（比如 DLL 没注册）时不许弹模态框：本工程是 GUI 子系统 exe，RTL 未设错误处理时
    ' 会走 MessageBox 那条路，套件里就是"跑超时被杀"—— 那是最难查的一种红。改成让失败落到
    ' "FAIL null" 这一类读数上，用例按缺 needle 判红。
    On Error Resume Next
    Dim o As Object
    Set o = CreateObject("CoDll.CImpl")
    If o Is Nothing Then
        Debug.Print "EXT1:FAIL null"
    Else
        Dim v As Long
        v = o.Twice(21)
        If v = 42 Then
            Debug.Print "EXT1:OK"
        Else
            Debug.Print "EXT1:FAIL v=" & v
        End If
    End If

    ' 组行的 ProgID (CoClass 块的 [ComCreatable(True)]) 也要认得, 且指向同一个 CLSID
    Dim g As Object
    Set g = CreateObject("CoDll.PG")
    If g Is Nothing Then
        Debug.Print "EXT2:FAIL null"
    Else
        Dim w As Long
        w = g.Twice(4)
        If w = 8 Then
            Debug.Print "EXT2:OK"
        Else
            Debug.Print "EXT2:FAIL v=" & w
        End If
    End If

    Debug.Print "EXT-DONE"
End Sub
