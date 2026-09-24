VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
END
Attribute VB_Name = "CAsmBad"
Attribute VB_GlobalNameSpace = False
Attribute VB_Creatable = False
Attribute VB_PredeclaredId = False
Attribute VB_Exposed = False
' ai/vb-asm-extension-spec 负例: 类方法里的 Asm 块 → 3037
'   类方法要带 me 参数、成员访问要走 me->, Asm 块暂不支持 (v2 边界, spec §10)。
Option Explicit

Public Function F(ByVal a As Long) As Long
    Asm
        mov eax, [a]
        add eax, 1
        mov [Function], eax
    End Asm
End Function
