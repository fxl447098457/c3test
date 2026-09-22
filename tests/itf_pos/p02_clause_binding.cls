VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
END
Attribute VB_Name = "Doc"
Option Explicit

Interface IStorage
    Function OpenFile(ByVal p As String) As Long
    Sub CloseFile()
    Property Get Title() As String
    Property Let Title(ByVal v As String)
End Interface

Interface IStream Extends IStorage
    Sub Grab(ByVal n As Long)
End Interface

Interface IAudit
    Sub CloseFile()
End Interface

Implements IStream
Implements IAudit

Private m_title As String

' Explicit binding: member name has nothing to do with the slot name
Private Function TakeOver(ByVal p As String) As Long Implements IStorage.OpenFile
    TakeOver = Len(p)
End Function

' One member claims the same-named slot of two interfaces (inherited slot named
' through the child interface + a slot of another implemented interface)
Private Sub Kick() Implements IStream.CloseFile, IAudit.CloseFile
End Sub

' Slot of the implemented child interface
Private Sub ReadBytes(ByVal n As Long) Implements IStream.Grab
End Sub

' Property tri-slot keys: get_/put_ comes from the member's Get/Let kind
Private Property Get Caption() As String Implements IStorage.Title
    Caption = m_title
End Property

Private Property Let Caption(ByVal v As String) Implements IStorage.Title
    m_title = v
End Property
