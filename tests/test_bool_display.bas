Option Explicit

' ============================================================
'  test_bool_display.bas - ai/022 W1: Boolean 的类型可见性 + 装箱口径
'
'  两半各有根因, 都必须真跑到:
'  (1) `As Boolean` 在 C 后端与 Integer 同为 int16_t, 注册表按 C 类型串分派,
'      inferExprType 看不见布尔 → CStr(b) / b & "" / String 形参收布尔实参
'      一律走 vb6_CStrLong → 打出 -1/0, VB6 打 True/False (口径同 Fix 175 的 Date)。
'  (2) 落进 Variant 的那一半: 交给 _Generic vb6_VariantFromValue 装箱时,
'      int16_t 选到 short: 分支 (VT_I2), 布尔字面量的裸 (-1) 选到 int: 分支
'      (VT_I4) → VarType(b)=2、TypeName(True)="Long"、Format 走数字分支。
'      修好后是 VT_BOOL(11)/"Boolean"/"True"。
'  反向护栏同样钉死: Integer/Byte/Long 的装箱读数一个都不许跟着动。
' ============================================================

Private Function TF(ByVal b As Boolean) As String
    If b Then TF = "Y" Else TF = "N"
End Function

Private Function ShowStr(ByVal s As String) As String
    ShowStr = s
End Function

Private Function ShowVar(ByVal v As Variant) As String
    ShowVar = "vt=" & VarType(v) & " tn=" & TypeName(v) & " s=" & CStr(v)
End Function

Private Function FlagFn() As Boolean
    FlagFn = True
End Function

Sub Main()
    Dim b As Boolean
    Dim f As Boolean
    Dim v As Variant
    Dim i As Integer
    Dim L As Long
    Dim by As Byte
    Dim va(1) As Variant

    b = True
    f = False
    i = 7
    L = 42
    by = 200

    ' --- (1) 转字符串的四条路: 变量 / 字面量 / 函数返回值 / 比较式 ---
    Debug.Print "B1=" & TF(CStr(b) = "True")
    Debug.Print "B2=" & TF(CStr(True) = "True")
    Debug.Print "B3=" & TF(CStr(f) = "False")
    Debug.Print "B4=" & TF(FlagFn() & "" = "True")
    Debug.Print "B5=" & TF((b = True) & "" = "True")
    Debug.Print "B6=" & TF(b & "" = "True")
    Debug.Print "B7=" & TF(ShowStr(b) = "True")
    Debug.Print "B8=" & TF(ShowStr(f) = "False")

    ' --- (2) 装箱读数: 类型标记必须是 VT_BOOL / Boolean ---
    Debug.Print "B9=" & TF(VarType(b) = 11)
    Debug.Print "B10=" & TF(TypeName(b) = "Boolean")
    Debug.Print "B11=" & TF(VarType(True) = 11)
    Debug.Print "B12=" & TF(TypeName(False) = "Boolean")
    Debug.Print "B13=" & TF(Format(b, "G") = "True")
    Debug.Print "B14=" & TF(Format(f, "G") = "False")

    ' --- (3) 落进 Variant 的那一半: 赋值 / 传参 / Variant 数组元素 ---
    v = b
    Debug.Print "B15=" & TF(VarType(v) = 11)
    Debug.Print "B16=" & TF(CStr(v) = "True")
    Debug.Print "B17=" & TF(ShowVar(b) = "vt=11 tn=Boolean s=True")
    Debug.Print "B18=" & TF(ShowVar(True) = "vt=11 tn=Boolean s=True")
    va(0) = b
    Debug.Print "B19=" & TF(VarType(va(0)) = 11)
    Debug.Print "B20=" & TF(CStr(va(0)) = "True")

    ' --- (4) 反向护栏: 非布尔的装箱读数一个都不许动 ---
    Debug.Print "B21=" & TF(VarType(i) = 2)
    Debug.Print "B22=" & TF(TypeName(i) = "Integer")
    Debug.Print "B23=" & TF(CStr(i) = "7")
    Debug.Print "B24=" & TF(VarType(L) = 3)
    Debug.Print "B25=" & TF(VarType(by) = 17)
    Debug.Print "B26=" & TF(CStr(3) = "3" And CStr(3.5) = "3.5")
    Debug.Print "B27=" & TF(ShowVar(L) = "vt=3 tn=Long s=42")

    ' --- (5) 判定语义不许被登记改动带偏 ---
    If b And Not f Then
        Debug.Print "B28=Y"
    End If
    Dim k As Long
    k = 0
    If v = True Then k = k + 1
    If b = f Then k = k + 10
    Debug.Print "B29=" & TF(k = 1)

    ' --- (6) 串内插值吃的就是同一条 CStr 路 ---
    Dim s As String
    s = `flag=${b} off=${f}`
    Debug.Print "B30=" & TF(s = "flag=True off=False")

    ' --- (7) 裸值的 Debug.Print: VB6 打 True/False, 不是 -1/0 ---
    Debug.Print "B31-raw"; b
    Debug.Print "B32-raw"; f
    Debug.Print "BOOL-DONE"
End Sub
