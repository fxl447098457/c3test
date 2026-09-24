VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
END
Attribute VB_Name = "IViaNamed"
Option Explicit

Interface IViaNamed Extends IViaShape
    Sub Rename(ByVal s As String)
    Function Join(ByVal a As String, ByVal b As String) As String
End Interface
