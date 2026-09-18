VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
  Persistable = 0  'NotPersistable
  DataBindingBehavior = 0  'vbNone
  DataSourceBehavior  = 0  'vbNone
  MTSTransactionMode  = 0  'NotAnMTSObject
END
Attribute VB_Name = "cTT"
Attribute VB_GlobalNameSpace = False
Attribute VB_Creatable = True
Attribute VB_PredeclaredId = True
Attribute VB_Exposed = False
Option Explicit

#Const bSubclassTextBox = True ' Set to True to fix the bug with lingering tooltips for TextBox controls or False if you don't mind it

#If bSubclassTextBox Then
    Implements ISubclass
#End If

Private Type TOOLINFO
    cbSize As Long
    uFlags As Long
    hWnd As LongPtr
    uId As LongPtr
    Left As Long
    Top As Long
    Right As Long
    Bottom As Long
    hInstance As LongPtr
    lpszText As LongPtr
    lParam As LongPtr
    lpReserved As LongPtr
End Type

Private Type TrackMouseEvent
    cbSize As Long
    dwFlags As Long
    hWndTrack As LongPtr
    dwHoverTime As Long
End Type

Private Const WS_EX_TOPMOST As Long = &H8, CW_USEDEFAULT As Long = &H80000000, TME_LEAVE As Long = &H2, WM_MOUSELEAVE As Long = &H2A3
Private Const TTM_ACTIVATE As Long = &H401, TTM_ADDTOOLW As Long = &H432, TTM_GETTOOLINFOW As Long = &H435, TTM_POP As Long = &H41C, TTM_SETDELAYTIME As Long = &H403, TTM_SETMAXTIPWIDTH As Long = &H418, _
              TTM_SETTIPBKCOLOR As Long = &H413, TTM_SETTIPTEXTCOLOR As Long = &H414, TTM_SETTITLEW As Long = &H421, TTM_SETTOOLINFOW As Long = &H436
Private Const TTS_ALWAYSTIP As Long = &H1, TTS_NOPREFIX As Long = &H2, TTS_BALLOON As Long = &H40, TTF_IDISHWND As Long = &H1, TTF_CENTERTIP As Long = &H2, TTF_SUBCLASS As Long = &H10, _
              TTDT_AUTOPOP As Long = &H2, TTDT_INITIAL As Long = &H3, GWL_STYLE As Long = -16, TOOLTIPS_CLASS As String = "tooltips_class32", sEmpty As String = ""

Public Enum ttIconType
    TTI_NONE
    TTI_INFO
    TTI_WARNING
    TTI_ERROR
    TTI_INFO_LARGE
    TTI_WARNING_LARGE
    TTI_ERROR_LARGE
End Enum

Private Declare Function CreateWindowExW Lib "user32" (ByVal dwExStyle As Long, ByVal lpClassName As LongPtr, ByVal lpWindowName As LongPtr, ByVal dwStyle As Long, ByVal X As Long, ByVal Y As Long, ByVal nWidth As Long, ByVal nHeight As Long, ByVal hWndParent As LongPtr, ByVal hMenu As LongPtr, ByVal hInstance As LongPtr, lpParam As Any) As LongPtr
Private Declare Function DestroyWindow Lib "user32" (ByVal hWnd As LongPtr) As Long
Private Declare Function GetWindowLongW Lib "user32" (ByVal hWnd As LongPtr, ByVal nIndex As Long) As Long
Private Declare Function SendMessageW Lib "user32" (ByVal hWnd As LongPtr, ByVal wMsg As Long, ByVal wParam As LongPtr, ByVal lParam As LongPtr) As LongPtr
Private Declare Function SetWindowLongW Lib "user32" (ByVal hWnd As LongPtr, ByVal nIndex As Long, ByVal dwNewLong As Long) As Long
Private Declare Function TrackMouseEvent Lib "user32" (ByVal lpEventTrack As LongPtr) As Long
Private Declare Function SetWindowTheme Lib "uxtheme" (ByVal hWnd As LongPtr, ByVal pszSubAppName As LongPtr, ByVal pszSubIdList As LongPtr) As Long

Private colTooltips As New Collection, m_bEnableTooltips As Boolean

Friend Property Get EnableTooltips(Optional objControl As Control) As Boolean
Dim hWndTT As LongPtr, hWndParent As LongPtr, tToolInfo As TOOLINFO
    If objControl Is Nothing Then
        EnableTooltips = m_bEnableTooltips
    Else
        With tToolInfo
            If HasWnd(objControl) Then hWndParent = objControl.hWnd: .uId = hWndParent Else hWndParent = objControl.Container.hWnd: .uId = ObjPtr(objControl)
            .cbSize = LenB(tToolInfo): .hWnd = hWndParent
            If ToolTipExists(CStr(.uId), hWndTT) Then SendMessageW hWndTT, TTM_GETTOOLINFOW, 0, VarPtr(tToolInfo): EnableTooltips = .lParam
        End With
    End If
End Property

Friend Property Let EnableTooltips(Optional objControl As Control, bEnableTooltips As Boolean)
Dim i As Long, hWndParent As LongPtr, hWndTT As LongPtr, sTipText As String, tToolInfo As TOOLINFO
    If objControl Is Nothing Then
        m_bEnableTooltips = bEnableTooltips
        For i = 1 To colTooltips.Count: SendMessageW Val(colTooltips(i)), TTM_ACTIVATE, m_bEnableTooltips, 0: Next i
    Else
        With tToolInfo
            If HasWnd(objControl) Then hWndParent = objControl.hWnd: .uId = hWndParent Else hWndParent = objControl.Container.hWnd: .uId = ObjPtr(objControl)
            .cbSize = LenB(tToolInfo): .hWnd = hWndParent: sTipText = Space$(2048): .lpszText = StrPtr(sTipText)
            If ToolTipExists(CStr(.uId), hWndTT) Then
                SendMessageW hWndTT, TTM_GETTOOLINFOW, 0, VarPtr(tToolInfo)
                sTipText = Left$(sTipText, InStr(sTipText, vbNullChar) - 1): .lpszText = StrPtr(sTipText): .lParam = bEnableTooltips
                SendMessageW hWndTT, TTM_SETTOOLINFOW, 0, VarPtr(tToolInfo): SendMessageW hWndTT, TTM_ACTIVATE, bEnableTooltips, 0
            End If
        End With
    End If
End Property

Friend Sub CreateToolTip(ParentControl As Control, sTipText As String, Optional sTitle As String, Optional uIcon As ttIconType, Optional lForeColor As ColorConstants = -1, Optional lBackColor As ColorConstants = -1, Optional bCentered As Boolean, Optional bBalloon As Boolean = True, Optional lMaxLength As Long = 200, Optional lDelayTime As Long = 500, Optional lVisibleTime As Long = 5000)
Dim hWndParent As LongPtr, hWndTT As LongPtr, lToolTipStyle As Long, tToolInfo As TOOLINFO, tTrackMouseEvent As TrackMouseEvent, ControlScaleMode As ScaleModeConstants
    With tToolInfo
        If HasWnd(ParentControl) Then hWndParent = ParentControl.hWnd: .uId = hWndParent: .uFlags = TTF_IDISHWND Else hWndParent = ParentControl.Container.hWnd: .uId = ObjPtr(ParentControl)
        .cbSize = LenB(tToolInfo): .hWnd = hWndParent
        If Not ToolTipExists(CStr(.uId), hWndTT) Then
            lToolTipStyle = TTS_ALWAYSTIP Or TTS_NOPREFIX: If bBalloon Then lToolTipStyle = lToolTipStyle Or TTS_BALLOON
            hWndTT = CreateWindowExW(WS_EX_TOPMOST, StrPtr(TOOLTIPS_CLASS), 0, lToolTipStyle, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, hWndParent, 0, App.hInstance, 0)
            SetWindowTheme hWndTT, StrPtr(sEmpty), StrPtr(sEmpty)
            .lpszText = StrPtr(sTipText): .uFlags = .uFlags Or TTF_SUBCLASS: If bCentered Then .uFlags = .uFlags Or TTF_CENTERTIP
            If (.uFlags And TTF_IDISHWND) <> TTF_IDISHWND Then
                With ParentControl
                    ControlScaleMode = GetContainerScaleMode(.Container)
                    tToolInfo.Left = .Parent.ScaleX(.Left, ControlScaleMode, vbPixels): tToolInfo.Top = .Parent.ScaleY(.Top, ControlScaleMode, vbPixels)
                    tToolInfo.Right = tToolInfo.Left + .Parent.ScaleX(.Width, ControlScaleMode, vbPixels): tToolInfo.Bottom = tToolInfo.Top + .Parent.ScaleY(.Height, ControlScaleMode, vbPixels)
                End With
            End If
            colTooltips.Add hWndTT, CStr(.uId): .lParam = True
            SendMessageW hWndTT, TTM_ADDTOOLW, 0, VarPtr(tToolInfo)
            #If bSubclassTextBox Then
                If TypeOf ParentControl Is TextBox Then
                    With tTrackMouseEvent: .cbSize = LenB(tTrackMouseEvent): .hWndTrack = hWndParent: .dwFlags = TME_LEAVE: End With
                    TrackMouseEvent VarPtr(tTrackMouseEvent): SubclassWnd hWndParent, Me
                End If
            #End If
        Else
            lToolTipStyle = GetWindowLongW(hWndTT, GWL_STYLE)
            If bBalloon Then lToolTipStyle = lToolTipStyle Or TTS_BALLOON Else lToolTipStyle = lToolTipStyle And Not TTS_BALLOON
            SetWindowLongW hWndTT, GWL_STYLE, lToolTipStyle
            SendMessageW hWndTT, TTM_GETTOOLINFOW, 0, VarPtr(tToolInfo)
            With tToolInfo
                .lpszText = StrPtr(sTipText): If bCentered Then .uFlags = .uFlags Or TTF_CENTERTIP Else .uFlags = .uFlags And Not TTF_CENTERTIP
            End With
            SendMessageW hWndTT, TTM_SETTOOLINFOW, 0, VarPtr(tToolInfo)
        End If
    End With
    If Len(sTitle) Then SendMessageW hWndTT, TTM_SETTITLEW, uIcon, StrPtr(sTitle)
    If lMaxLength Then SendMessageW hWndTT, TTM_SETMAXTIPWIDTH, 0, lMaxLength
    If lForeColor > -1 Then SendMessageW hWndTT, TTM_SETTIPTEXTCOLOR, lForeColor, 0
    If lBackColor > -1 Then SendMessageW hWndTT, TTM_SETTIPBKCOLOR, lBackColor, 0
    If lVisibleTime Then SendMessageW hWndTT, TTM_SETDELAYTIME, TTDT_AUTOPOP, lVisibleTime
    If lDelayTime Then SendMessageW hWndTT, TTM_SETDELAYTIME, TTDT_INITIAL, lDelayTime
End Sub

Private Function GetContainerScaleMode(objContainer As Object) As ScaleModeConstants
On Error GoTo ScaleModeTwips
    GetContainerScaleMode = objContainer.ScaleMode: Exit Function
ScaleModeTwips:
    GetContainerScaleMode = vbTwips: Err.Clear
End Function

Private Function HasWnd(objControl As Control) As Boolean
On Error Resume Next
    HasWnd = objControl.hWnd: If Err Then Err.Clear
End Function

Private Function ToolTipExists(sKey As String, Optional hWndTT As LongPtr) As Boolean
On Error Resume Next
    hWndTT = Val(colTooltips(sKey)): If Err = 0 Then ToolTipExists = True Else Err.Clear
End Function

Private Sub Class_Terminate()
Dim i As Long
    For i = 1 To colTooltips.Count: DestroyWindow Val(colTooltips(i)): Next i
End Sub

#If bSubclassTextBox Then
    Private Function ISubclass_WndProc(hWnd As LongPtr, uMsg As Long, wParam As LongPtr, lParam As LongPtr, dwRefData As LongPtr, bDiscardMessage As Boolean) As LongPtr
        Select Case uMsg
            Case WM_MOUSELEAVE
                SendMessageW Val(colTooltips(CStr(hWnd))), TTM_POP, 0, 0
        End Select
    End Function
#End If
