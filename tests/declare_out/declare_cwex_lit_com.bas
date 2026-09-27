' Fix 161c 夹具: Declare A 版 ByVal String 实参是**字面量**与**COM 属性表达式**。
' 场景源自真实工程 Charts 2020 / LabelPlus / PropPagLP.pag:434:
'   m_Hwnd = CreateWindowEx(ExtendedStyle, "Edit", LabelPlus1.Caption, ...)
' 第 2 个实参是字符串字面量 "Edit", 第 3 个是后期绑定 COM 属性读
' (生成 vb6_VariantToString(vb6_VariantFromComResult(vb6_ComGetProp(...))))。
'
' 症状: Fix 161b 的出参回写只按 AST 节点种类判左值, MemberAccessExpr 判 true,
' 于是对第 3 个实参登记回写 → 生成
'   vb6_BSTR_Assign(&(vb6_VariantToString(...)), ...)
' → error C2102: "&"要求左值 → 整个工程编译失败 (exit 2)。
' 修复后: 登记前再看生成串是否为纯左值形态; COM 属性/字面量一律不回写。
'
' 判据: 编译通过 (exit=0) 且建窗成功 hwnd<>0。

Option Explicit

Declare Function CreateWindowEx Lib "user32" Alias "CreateWindowExA" _
    (ByVal dwExStyle As Long, ByVal lpClassName As String, ByVal lpWindowName As String, _
     ByVal dwStyle As Long, ByVal x As Long, ByVal y As Long, ByVal nWidth As Long, _
     ByVal nHeight As Long, ByVal hWndParent As Long, ByVal hMenu As Long, _
     ByVal hInstance As Long, ByVal lpParam As Long) As Long

Declare Function DestroyWindow Lib "user32" (ByVal h As Long) As Long

Public Sub Main()
    Dim fso As Object
    Dim h As Long

    Set fso = CreateObject("Scripting.FileSystemObject")

    ' 关键: 第 3 实参**直接内联**单一 COM 字符串属性 (不经过中间变量、不做拼接)。
    ' 精确复刻真实工程的 LabelPlus1.Caption —— argVal 是
    '   vb6_VariantToString(vb6_VariantFromComResult(vb6_ComGetProp(..., L"Path")))
    ' 纯右值; 修复前 AST 判 MemberAccessExpr=true 被误登记回写 → &(右值) → C2102。
    h = CreateWindowEx(0, "STATIC", fso.GetSpecialFolder(2).Path, 0, 0, 0, 100, 50, 0, 0, 0, 0)
    Debug.Print "hwnd="; h
    If h <> 0 Then
        Debug.Print "lit-com-hwnd-ok=Y"
        DestroyWindow h
    Else
        Debug.Print "lit-com-hwnd-ok=N"
    End If

    ' 再走一遭: 纯字面量 (argVal 是 BSTR_FromStr 临时, 同样非左值)
    h = CreateWindowEx(0, "BUTTON", "hello", 0, 0, 0, 80, 30, 0, 0, 0, 0)
    If h <> 0 Then
        Debug.Print "lit-only-hwnd-ok=Y"
        DestroyWindow h
    Else
        Debug.Print "lit-only-hwnd-ok=N"
    End If

    Set fso = Nothing
End Sub
