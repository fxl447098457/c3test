Attribute VB_Name = "mdlSubclass"
Option Explicit

Private Const WM_NCDESTROY As Long = &H82

Private Declare Function SetWindowSubclass Lib "comctl32" Alias "#410" (ByVal hWnd As LongPtr, ByVal pfnSubclass As LongPtr, ByVal uIdSubclass As LongPtr, ByVal dwRefData As LongPtr) As Long
Private Declare Function GetWindowSubclass Lib "comctl32" Alias "#411" (ByVal hWnd As LongPtr, ByVal pfnSubclass As LongPtr, ByVal uIdSubclass As LongPtr, pdwRefData As LongPtr) As Long
Private Declare Function RemoveWindowSubclass Lib "comctl32" Alias "#412" (ByVal hWnd As LongPtr, ByVal pfnSubclass As LongPtr, ByVal uIdSubclass As LongPtr) As Long
Private Declare Function DefSubclassProc Lib "comctl32" Alias "#413" (ByVal hWnd As LongPtr, ByVal uMsg As Long, ByVal wParam As LongPtr, ByVal lParam As LongPtr) As LongPtr

Public Function IsWndSubclassed(hWnd As LongPtr, uIdSubclass As LongPtr, Optional dwRefData As LongPtr) As Boolean
    IsWndSubclassed = GetWindowSubclass(hWnd, AddressOf WndProc, uIdSubclass, dwRefData)
End Function

Public Function SubclassWnd(hWnd As LongPtr, vSubclass As Variant, Optional dwRefData As LongPtr, Optional bUpdateRefData As Boolean) As Boolean
Dim Subclass As ISubclass, uIdSubclass As LongPtr, lOldRefData As LongPtr
    If IsObject(vSubclass) Then Set Subclass = vSubclass: uIdSubclass = ObjPtr(Subclass) Else uIdSubclass = vSubclass
    If Not IsWndSubclassed(hWnd, uIdSubclass, lOldRefData) Then
        SubclassWnd = SetWindowSubclass(hWnd, AddressOf WndProc, uIdSubclass, dwRefData)
    Else
        If bUpdateRefData Then If lOldRefData <> dwRefData Then SubclassWnd = SetWindowSubclass(hWnd, AddressOf WndProc, uIdSubclass, dwRefData)
    End If
End Function

Public Function UnSubclassWnd(hWnd As LongPtr, Optional vSubclass As Variant, Optional uIdSubclass As LongPtr) As Boolean
Dim Subclass As ISubclass
    If Not IsMissing(vSubclass) Then If IsObject(vSubclass) Then Set Subclass = vSubclass: uIdSubclass = ObjPtr(Subclass) Else uIdSubclass = vSubclass
    If IsWndSubclassed(hWnd, uIdSubclass) Then UnSubclassWnd = RemoveWindowSubclass(hWnd, AddressOf WndProc, uIdSubclass)
End Function

Private Function WndProc(ByVal hWnd As LongPtr, ByVal uMsg As Long, ByVal wParam As LongPtr, ByVal lParam As LongPtr, ByVal Subclass As ISubclass, ByVal dwRefData As LongPtr) As LongPtr
Dim bDiscardMessage As Boolean
    Select Case uMsg
        Case WM_NCDESTROY ' Remove subclassing as the window is about to be destroyed
            UnSubclassWnd hWnd, , ObjPtr(Subclass)
        Case Else
            WndProc = Subclass.WndProc(hWnd, uMsg, wParam, lParam, dwRefData, bDiscardMessage)
    End Select
    If Not bDiscardMessage Then WndProc = DefSubclassProc(hWnd, uMsg, wParam, lParam)
End Function

