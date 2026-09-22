VERSION 5.00
Begin VB.Form Form1
   Caption         =   "ExtShow caller"
   ClientHeight    =   1500
   ClientLeft      =   60
   ClientTop       =   345
   ClientWidth     =   3200
   LinkTopic       =   "Form1"
   ScaleHeight     =   1500
   ScaleWidth      =   3200
End
Attribute VB_Name = "Form1"
Attribute VB_GlobalNameSpace = False
Attribute VB_Creatable = False
Attribute VB_PredeclaredId = True
Attribute VB_Exposed = False
' Regression target (vbman C2198, 2026-09-20): cross-module form default-instance
' Show WITHOUT args. The generated vb6_form_show_<Form> takes (hMDIClient, modal)
' since Fix 146, so the caller must pass the default modal=0.
' The actual call lives in Module1.ShowForm2 (.bas caller, same shape as vbman
' cLogs.bas calling FLogs.Show). Compile failure (C2198) is the failure signal.
Private Sub Form_Load()
    ShowForm2
End Sub
