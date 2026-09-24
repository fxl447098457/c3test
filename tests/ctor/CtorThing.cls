VERSION 1.0 CLASS
BEGIN
  MultiUse = -1
END
Attribute VB_Name = "CtorThing"
Attribute VB_GlobalNameSpace = False
Attribute VB_Creatable = False
Attribute VB_PredeclaredId = False
Attribute VB_Exposed = False

Private mAmount As Long
Private mLabel As String

' C3 扩展 (084c): 带参构造 — VB6 本体的 Class_Initialize 恒无参
Public Sub Class_Initialize(ByVal a As Long, ByVal lbl As String)
    mAmount = a
    mLabel = lbl
End Sub

Public Function Describe() As String
    Describe = mLabel & ":" & CStr(mAmount)
End Function
