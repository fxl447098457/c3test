VERSION 1.0 CLASS
BEGIN
  MultiUse = -1
END
Attribute VB_Name = "CtorPlain"
Attribute VB_GlobalNameSpace = False
Attribute VB_Creatable = False
Attribute VB_PredeclaredId = False
Attribute VB_Exposed = False

Private mCount As Long

' 无参 Class_Initialize: 经典 VB6 形态, 不允许带实参创建
Public Sub Class_Initialize()
    mCount = 7
End Sub

Public Function GetCount() As Long
    GetCount = mCount
End Function
