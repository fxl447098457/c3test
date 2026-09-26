VERSION 5.00
Begin VB.Form C29ImgObj
   Caption         =   "C29ImgObj"
   ClientHeight    =   2400
   ClientLeft      =   120
   ClientTop       =   465
   ClientWidth     =   4800
   LinkTopic       =   "Form1"
   ScaleHeight     =   2400
   ScaleWidth      =   4800
   StartUpPosition =   3  '窗口缺省
   Begin MSComctlLib.ImageList ImageList1
      _ExtentX        =   449
      _ExtentY        =   449
      _Version        =   49152
      ImageWidth      =   16
      ImageHeight     =   16
   End
End
Attribute VB_Name = "C29ImgObj"
Option Explicit

' ai/029 C29-3: 控件"成员对象"机制立样的判据夹具。
' 四条验收 (计划书 C29-3 判据要点):
'   ① Set img = ImageList1.ListImages.Add(, "Open", LoadPicture(...))
'   ② img.Key
'   ③ ListImages.Count
'   ④ For Each
' 附带 Item 按下标/按 Key 取、Remove/Clear 之后的 Count 与遍历顺序。
' 走 Test-Vbp (打完 Unload Me 拿 stdout 针), 不用 Test-GuiVbp 那种"起窗不崩"的弱判据。

Private Sub Form_Load()
    Dim img As ListImage
    Dim it As ListImage
    Dim e As ListImage
    Dim o As Object
    Dim acc As String

    ' ① Add 返回 ListImage **对象** (不是 int 下标)
    Set img = ImageList1.ListImages.Add(, "Open", LoadPicture(App.Path & "\il_red.bmp"))
    Debug.Print "MO1-ADD-KEY=" & img.Key
    Debug.Print "MO2-ADD-IDX=" & img.Index
    Debug.Print "MO3-COUNT=" & ImageList1.ListImages.Count

    ' 第二张改用 As Object 声明 —— 验证晚绑定那条路也吃同一个对象。
    ' 这里刻意把 LoadPicture 拆成单独一句: 内联实参形态已由 MO1 覆盖, 拆开是第二种写法。
    Dim pic2 As Object
    Set pic2 = LoadPicture(App.Path & "\il_green.bmp")
    Set o = ImageList1.ListImages.Add(, "Close", pic2)
    Debug.Print "MO4-KEY2=" & o.Key
    Debug.Print "MO5-COUNT=" & ImageList1.ListImages.Count

    ' Item 按下标取 (1 基)
    Set it = ImageList1.ListImages(1)
    Debug.Print "MO6-ITEM1-KEY=" & it.Key

    ' Item 按 Key 取
    Set it = ImageList1.ListImages("Close")
    Debug.Print "MO7-ITEMKEY-IDX=" & it.Index

    ' ④ For Each —— 顺序 = 显示序 (1 基递增)
    acc = ""
    For Each e In ImageList1.ListImages
        acc = acc & e.Key & ","
    Next
    Debug.Print "MO8-FOREACH=" & acc

    ' Remove 按 Key 之后: Count 与顺序都要对
    ImageList1.ListImages.Remove "Open"
    Debug.Print "MO9-AFTERRM=" & ImageList1.ListImages.Count
    acc = ""
    For Each e In ImageList1.ListImages
        acc = acc & e.Key & ","
    Next
    Debug.Print "MO10-FOREACH2=" & acc

    ' Clear
    ImageList1.ListImages.Clear
    Debug.Print "MO11-AFTERCLEAR=" & ImageList1.ListImages.Count

    Unload Me
End Sub
