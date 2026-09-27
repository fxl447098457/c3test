' 对照夹具: Declare **ByRef UDT**(结构体指针) —— 这条**本来就正常**。
' 判据: hr=0 且 maj/min 为 comctl32 版本; 证明'出参读法不通'不适用于 UDT 路径。

Option Explicit

Private Type DLLVERSIONINFO
    cbSize As Long
    dwMajorVersion As Long
    dwMinorVersion As Long
    dwBuildNumber As Long
    dwPlatformID As Long
End Type

Declare Function DllGetVersion Lib "comctl32.dll" (ByRef pdvi As DLLVERSIONINFO) As Long

Public Sub Main()
    Dim vi As DLLVERSIONINFO
    Dim hr As Long
    vi.cbSize = 20
    hr = DllGetVersion(vi)
    Debug.Print "hr="; hr
    Debug.Print "maj="; vi.dwMajorVersion; " min="; vi.dwMinorVersion
End Sub
