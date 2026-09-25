Attribute VB_Name = "DemoClientMain"
Option Explicit

' ai/022 B18 端到端示例（外部队列）: 本进程**不引用** DemoDll —— CreateObject 走真实注册表
' 链路、成员调用走 IDispatch 晚绑定。契约成员是 Private（类的默认面上点不到），
' 所以外部客户点的是那个公有成员 Twice（接口成员要走带引用的早绑定, 见 B17 的探针用例）。
Sub Main()
    ' 激活失败（比如 DLL 没注册）时不许弹模态框: 让失败落到读数上, 用例按缺 needle 判红
    On Error Resume Next
    Dim o As Object
    Set o = CreateObject("DemoDll.Shape")
    If o Is Nothing Then
        Debug.Print "DEMOEXT1:FAIL null"
    Else
        Dim v As Long
        v = o.Twice(21)
        If v = 42 Then
            Debug.Print "DEMOEXT1:OK"
        Else
            Debug.Print "DEMOEXT1:FAIL v=" & v
        End If
    End If
    Debug.Print "DEMOEXT-DONE"
End Sub
