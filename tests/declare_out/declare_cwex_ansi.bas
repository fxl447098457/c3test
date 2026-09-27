' Fix 161b-decl-out 夹具: Declare Alias "CreateWindowExA" (显式 A 版)。
' 判据: hwnd<>0 (建窗成功); 修复前 VB 名被 SDK 宏改写成 CreateWindowExW,
' 窄 char* 当 LPCWSTR → 类名乱码 → hwnd=0。

Option Explicit

' 用户的 Declare 形态: 显式 Alias "CreateWindowExA"
Declare Function CreateWindowEx Lib "user32" Alias "CreateWindowExA" _
    (ByVal dwExStyle As Long, ByVal lpClassName As String, ByVal lpWindowName As String, _
     ByVal dwStyle As Long, ByVal x As Long, ByVal y As Long, ByVal nWidth As Long, _
     ByVal nHeight As Long, ByVal hWndParent As Long, ByVal hMenu As Long, _
     ByVal hInstance As Long, ByVal lpParam As Long) As Long

Declare Function GetModuleHandleA Lib "kernel32" (ByVal n As String) As Long
Declare Function DestroyWindow Lib "user32" (ByVal h As Long) As Long

Public Sub Main()
    Dim h As Long
    h = CreateWindowEx(0, "STATIC", "hello", 0, 0, 0, 100, 50, 0, 0, 0, 0)
    Debug.Print "hwnd="; h
    If h <> 0 Then DestroyWindow h
End Sub
