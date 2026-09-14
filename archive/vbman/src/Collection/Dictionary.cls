VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
  Persistable = 0  'NotPersistable
  DataBindingBehavior = 0  'vbNone
  DataSourceBehavior  = 0  'vbNone
  MTSTransactionMode  = 0  'NotAnMTSObject
END
Attribute VB_Name = "Dictionary"
Attribute VB_GlobalNameSpace = False
Attribute VB_Creatable = True
Attribute VB_PredeclaredId = False
Attribute VB_Exposed = True
Option Explicit

' Dictionary.cls
' 完全兼容 Scripting.Dictionary 的字典类
' 内部直接委托 Scripting.Dictionary，确保行为和签名一致
'
' 标准 API（与 Scripting.Dictionary 完全一致）:
'   - Add(Key, Item)
'   - Exists(Key)
'   - Item(Key) [default member]  支持 Get/Let/Set，key 不存在时自动添加
'   - Remove(Key)
'   - RemoveAll()
'   - Keys()                      返回 Variant 数组
'   - Items()                     返回 Variant 数组
'   - Count                       属性
'   - CompareMode                 属性
'   - Key(Key) = NewKey           重命名 Key
'   - HashVal(Key)                返回哈希值
'
' 扩展辅助 API:
'   - KeyByIndex(Index)
'   - RenameKey(OldKey, NewKey)
'   - Update(Item, Key)

Public m_Dict As Scripting.Dictionary
' For Each 枚举辅助：持有与 m_Dict 同步的 Collection，用于提供 _NewEnum
Private m_EnumCol As Collection

Private Sub Class_Initialize()
    Set m_Dict = New Scripting.Dictionary
    m_Dict.CompareMode = vbBinaryCompare
    Set m_EnumCol = New Collection
End Sub

Private Sub Class_Terminate()
    Set m_EnumCol = Nothing
    Set m_Dict = Nothing
End Sub

'=========================================================================
' Scripting.Dictionary 标准 API
'=========================================================================

' 设置/获取比较模式：vbBinaryCompare=0, vbTextCompare=1
Public Property Get CompareMode() As CompareMethod
    CompareMode = m_Dict.CompareMode
End Property

Public Property Let CompareMode(ByVal Value As CompareMethod)
    m_Dict.CompareMode = Value
End Property

' 添加元素（参数顺序：Key 在前，Item 在后）
Public Sub Add(ByVal key As String, ByVal Item As Variant)
    m_Dict.Add key, Item
    SyncEnumCol
End Sub

' 检查键是否存在
Public Function Exists(ByVal key As String) As Boolean
    Exists = m_Dict.Exists(key)
End Function

' 默认成员：Item(Key)
' key 不存在时自动添加（与 Scripting.Dictionary 行为一致）
Public Property Get Item(ByVal key As Variant) As Variant
Attribute Item.VB_UserMemId = 0
    If IsObject(m_Dict.Item(key)) Then
        Set Item = m_Dict.Item(key)
    Else
        Item = m_Dict.Item(key)
    End If
End Property

Public Property Let Item(ByVal key As Variant, ByVal Value As Variant)
    Dim bDidAdd As Boolean
    bDidAdd = (Not m_Dict.Exists(key))
    m_Dict.Item(key) = Value
    '如果 Dictionary 自动添加了新 key，同步枚举集合
    If bDidAdd Then SyncEnumCol
End Property

Public Property Set Item(ByVal key As Variant, ByVal Value As Object)
    Dim bDidAdd As Boolean
    bDidAdd = (Not m_Dict.Exists(key))
    Set m_Dict.Item(key) = Value
    If bDidAdd Then SyncEnumCol
End Property

' 删除元素
Public Sub Remove(ByVal key As Variant)
    m_Dict.Remove key
    SyncEnumCol
End Sub

' 清空集合
Public Sub RemoveAll()
    m_Dict.RemoveAll
    Set m_EnumCol = New Collection
End Sub

' 返回所有 Key 的数组
Public Function Keys() As Variant
    Keys = m_Dict.Keys
End Function

' 返回所有 Item 的数组
Public Function Items() As Variant
    Items = m_Dict.Items
End Function

' 元素数量
Public Property Get count() As Long
    count = m_Dict.count
End Property

' 重命名 Key
Public Property Let key(ByVal OldKey As String, ByVal NewKey As String)
    On Error Resume Next
    m_Dict.key(OldKey) = NewKey
    SyncEnumCol
End Property

' 返回 Key 的内部哈希值
Public Function HashVal(ByVal key As Variant) As Variant
    On Error Resume Next
    HashVal = m_Dict.HashVal(key)
End Function

' For Each 支持
' 委托内部 VB Collection 的 _NewEnum（VB Collection 的 _NewEnum 不受限制）
Public Function NewEnum() As IUnknown
Attribute NewEnum.VB_UserMemId = -4
    Set NewEnum = m_EnumCol.[_NewEnum]
End Function

'=========================================================================
' 扩展辅助 API
'=========================================================================

' 通过索引获取 Key（0 基）
Public Function KeyByIndex(ByVal Index As Long) As String
    On Error Resume Next
    Dim Arr As Variant
    Arr = m_Dict.Keys()
    If IsArray(Arr) Then
        If Index >= 0 And Index <= UBound(Arr) Then
            KeyByIndex = CStr(Arr(Index))
        End If
    End If
End Function

' 修改指定 Key 的名称（函数版，返回是否成功）
Public Function RenameKey(ByVal OldKey As String, ByVal NewKey As String) As Boolean
    On Error GoTo ERR
    If m_Dict.Exists(OldKey) And Not m_Dict.Exists(NewKey) Then
        m_Dict.key(OldKey) = NewKey
        SyncEnumCol
        RenameKey = True
    End If
    Exit Function
ERR:
    RenameKey = False
End Function

' 更新元素（key 必须已存在，否则返回 False）
Public Function Update(ByVal Item As Variant, ByVal key As String) As Boolean
    If m_Dict.Exists(key) Then
        m_Dict.Item(key) = Item
        SyncEnumCol
        Update = True
    Else
        Update = False
    End If
End Function

'=========================================================================
' 私有辅助
'=========================================================================

' 重建 m_EnumCol 使其与 m_Dict 完全同步
Private Sub SyncEnumCol()
    Set m_EnumCol = New Collection
    Dim k As Variant
    For Each k In m_Dict.Keys
        If IsObject(m_Dict.Item(k)) Then
            m_EnumCol.Add m_Dict.Item(k), CStr(k)
        Else
            m_EnumCol.Add m_Dict.Item(k), CStr(k)
        End If
    Next
End Sub
