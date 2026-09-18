VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
  Persistable = 0  'NotPersistable
  DataBindingBehavior = 0  'vbNone
  DataSourceBehavior  = 0  'vbNone
  MTSTransactionMode  = 0  'NotAnMTSObject
END
Attribute VB_Name = "ClsResizer"
Attribute VB_GlobalNameSpace = False
Attribute VB_Creatable = True
Attribute VB_PredeclaredId = False
Attribute VB_Exposed = False
Option Explicit
'By LeandroA
'Preserve the controls positions and size percents respect to parent
Private Declare Function SendMessage Lib "user32" Alias "SendMessageA" (ByVal hwnd As Long, ByVal wMsg As Long, ByVal wParam As Long, lParam As Any) As Long
Private Declare Function RedrawWindow Lib "user32.dll" (ByVal hwnd As Long, ByRef lprcUpdate As Any, ByVal hrgnUpdate As Long, ByVal fuRedraw As Long) As Long
Private Declare Function GetDesktopWindow Lib "user32.dll" () As Long
Private Declare Function UpdateWindow Lib "user32.dll" (ByVal hwnd As Long) As Long

Private Const WM_SETREDRAW As Long = &HB&
Private Const RDW_ALLCHILDREN As Long = &H80
Private Const RDW_INVALIDATE As Long = &H1

Private Type RectF
    Left As Single
    Top As Single
    Width As Single
    Height As Single
    Prop As Collection
End Type

Private Type FontsBufer
    CtrlTypeName As String
    PropFont As String
End Type

Private Type PropBufer
    CtrlTypeName As String
    PropName As String
End Type

Dim Rects() As RectF
Dim CtrlNames As Collection
Dim FBuf() As FontsBufer
Dim PBuf() As PropBufer
Dim CtrlFontCount As Integer
Dim CtrlPropCount As Integer

Public Function AddControlFont(CtrlTypeName As String, ParamArray PropFont() As Variant)
    Dim i As Long
    For i = 0 To UBound(PropFont)
        ReDim Preserve FBuf(CtrlFontCount)
        
        With FBuf(CtrlFontCount)
            .CtrlTypeName = CtrlTypeName
            .PropFont = PropFont(i)
        End With
        
        CtrlFontCount = CtrlFontCount + 1
    Next
End Function

Public Function AddControlProperty(CtrlTypeName As String, ParamArray PropName() As Variant)
    Dim i As Long
    For i = 0 To UBound(PropName)
        ReDim Preserve PBuf(CtrlPropCount)
        
        With PBuf(CtrlPropCount)
            .CtrlTypeName = CtrlTypeName
            .PropName = PropName(i)
        End With
        
        CtrlPropCount = CtrlPropCount + 1
    Next
End Function
Public Sub SaveControlsPositions(oForm As Form)
    Dim oCtrl As Object
    Dim i As Long
    Dim FW As Single
    Dim FH As Single
    Dim j As Integer
    
    Dim RatioAspect As Single
    
    FW = oForm.ScaleWidth
    FH = oForm.ScaleHeight
    
    Set CtrlNames = New Collection
    ReDim Rects(oForm.Count - 1)
    For Each oCtrl In oForm.Controls
        If oCtrl.Container.Name = oForm.Name Then
        CtrlNames.Add oCtrl
        
        With Rects(i)
            .Left = oCtrl.Left * 100 / FW
            .Top = oCtrl.Top * 100 / FH
            .Width = oCtrl.Width * 100 / FW
            .Height = oCtrl.Height * 100 / FH
            Set .Prop = New Collection
            
            RatioAspect = GetMax(oCtrl.Width, oCtrl.Height)
            
            For j = 0 To CtrlFontCount - 1
                If TypeName(oCtrl) = FBuf(j).CtrlTypeName Then
                    .Prop.Add CallByName(oCtrl, FBuf(j).PropFont, VbGet).Size * 100 / RatioAspect, FBuf(j).PropFont
                End If
            Next
            
            For j = 0 To CtrlPropCount - 1
                If TypeName(oCtrl) = PBuf(j).CtrlTypeName Then
                    .Prop.Add CallByName(oCtrl, PBuf(j).PropName, VbGet) * 100 / RatioAspect, PBuf(j).PropName
                End If
            Next
            
        End With
        i = i + 1
        End If
    Next
End Sub

Public Sub ResizeControls(oForm As Object)
    On Error Resume Next
   Dim oCtrl As Object
    Dim i As Long
    Dim FW As Single
    Dim FH As Single
    Dim j As Integer
    Dim RatioAspect  As Single

    FW = oForm.ScaleWidth
    FH = oForm.ScaleHeight
    
    Call SendMessage(oForm.hwnd, WM_SETREDRAW, 0&, 0&)
    For i = 1 To CtrlNames.Count
     
        With CtrlNames(i)
            .Left = FW * Rects(i - 1).Left / 100
            .Top = FH * Rects(i - 1).Top / 100
            .Width = FW * Rects(i - 1).Width / 100
            .Height = FH * Rects(i - 1).Height / 100
            RatioAspect = GetMax(.Width, .Height)
            For j = 0 To CtrlFontCount - 1
                If TypeName(CtrlNames(i)) = FBuf(j).CtrlTypeName Then
                    
                    CallByName(CtrlNames(i), FBuf(j).PropFont, VbGet).Size = RatioAspect * Rects(i - 1).Prop(FBuf(j).PropFont) / 100
                End If
            Next
            
            For j = 0 To CtrlPropCount - 1
                If TypeName(CtrlNames(i)) = PBuf(j).CtrlTypeName Then
      
                    CallByName CtrlNames(i), PBuf(j).PropName, VbLet, RatioAspect * Rects(i - 1).Prop(PBuf(j).PropName) / 100
                End If
            Next
        End With
    Next
    Call SendMessage(oForm.hwnd, WM_SETREDRAW, 1&, 0&)
    RedrawWindow oForm.hwnd, ByVal &H0, 0, RDW_INVALIDATE Or RDW_ALLCHILDREN

End Sub

Private Function GetMax(Value1 As Single, Value2 As Single) As Single
    If Value1 < Value2 Then
        GetMax = Value1
    Else
        GetMax = Value2
    End If
End Function

