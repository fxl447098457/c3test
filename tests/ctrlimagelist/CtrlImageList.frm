VERSION 5.00
Begin VB.Form CtrlImageList
   Caption         =   "CtrlImageList"
   ClientHeight    =   3000
   ClientLeft      =   120
   ClientTop       =   465
   ClientWidth     =   6000
   LinkTopic       =   "Form3"
   ScaleHeight     =   3000
   ScaleWidth      =   6000
   StartUpPosition =   3  '窗口缺省
   Begin MSComctlLib.ImageList ImageList1
      _ExtentX        =   449
      _ExtentY        =   449
      _Version        =   49152
      ImageWidth      =   16
      ImageHeight     =   16
      ListImages
         .ListImage(1, "dt1", "CtrlImageList.frx":00000000)
         .ListImage(2, "dt2", "CtrlImageList.frx":00000344)
      End
   End
End
Attribute VB_Name = "CtrlImageList"
Option Explicit

' P20-39: ImageList 控件复刻 (comctl32 ImageList_*, 不加载 mscomctl.ocx)。
' 三路图片来源都覆盖:
'   ① 设计期: .frx 里的裸 DIB, 由 cgen 在窗体创建时烤成字节数组喂进来 (IL1~IL3)
'   ② 运行期: LoadPicture() 的返回值 (VB6 的 StdPicture = 活着的 IPicture)
'   ③ Remove / Clear 对 ① ② 混插的集合同样要正确搬动 key 表
'
' 已知偏差: VB6 里 ImageList.ImageWidth/ImageHeight 在**运行期是只读**的
' (非空时赋值报 383), 这里选择静默接受 (IL12)。等错误 raising 机制到位再收紧。

Private Sub Form_Load()
    Dim n As Long

    ' --- ① 设计期图片 (.frx 裸 DIB) ---
    n = ImageList1.ListImages.Count
    Debug.Print "IL1-DTCOUNT=" & n                              ' 2
    Debug.Print "IL2-DTKEY1=" & ImageList1.ListImages(1).Key    ' dt1
    Debug.Print "IL3-DTKEY2=" & ImageList1.ListImages("dt2").Key ' dt2

    ' --- ② 运行期 LoadPicture ---
    n = ImageList1.ListImages.Add(, "rt", LoadPicture(App.Path & "\il_red.bmp"))
    Debug.Print "IL4-ADDRT=" & n                                 ' 3
    Debug.Print "IL5-COUNT=" & ImageList1.ListImages.Count       ' 3

    ' --- ③ Remove 按 Key (混插集合, 验证 key 表跟着搬动) ---
    ImageList1.ListImages.Remove "dt1"
    Debug.Print "IL6-AFTERRM=" & ImageList1.ListImages.Count     ' 2
    Debug.Print "IL7-KEY1=" & ImageList1.ListImages(1).Key       ' dt2
    ' --- Remove 按下标 (1 基) ---
    ImageList1.ListImages.Remove 1
    Debug.Print "IL8-AFTERRM2=" & ImageList1.ListImages.Count    ' 1
    Debug.Print "IL9-KEY1=" & ImageList1.ListImages(1).Key       ' rt

    ' --- Add 带显式 index (插到最前) ---
    n = ImageList1.ListImages.Add(1, "first", LoadPicture(App.Path & "\il_green.bmp"))
    Debug.Print "IL10-COUNT=" & ImageList1.ListImages.Count      ' 2
    Debug.Print "IL11-KEY1=" & ImageList1.ListImages(1).Key      ' first

    ' --- 按 Key 取项 (Item 实参是字符串不是下标) ---
    Debug.Print "IL12-BYKEY=" & ImageList1.ListImages("first").Key   ' first
    Debug.Print "IL13-BYKEYIDX=" & ImageList1.ListImages("first").Index ' 1

    ' --- ImageWidth / ImageHeight ---
    Debug.Print "IL14-W=" & ImageList1.ImageWidth                ' 16
    ImageList1.ImageWidth = 32
    Debug.Print "IL15-SETW=" & ImageList1.ImageWidth             ' 32 (见顶部已知偏差)
    Debug.Print "IL16-H=" & ImageList1.ImageHeight               ' 16

    ' --- Clear ---
    ImageList1.ListImages.Clear
    Debug.Print "IL17-AFTERCLR=" & ImageList1.ListImages.Count   ' 0
    Debug.Print "CTRLIMAGELIST-DONE"
    Unload Me
End Sub
