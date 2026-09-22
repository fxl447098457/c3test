' Module1.bas - .bas caller for the cross-module no-arg Form.Show regression test.
' Shape copied from vbman cLogs.bas: `FLogs.Show` (external form default instance,
' no args) -> generator must emit vb6_form_show_<Form>(NULL, 0) since Fix 146.
Option Explicit

Public Sub ShowForm2()
    Form2.Show
End Sub
