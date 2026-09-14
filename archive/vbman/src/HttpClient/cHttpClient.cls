VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
  Persistable = 0  'NotPersistable
  DataBindingBehavior = 0  'vbNone
  DataSourceBehavior  = 0  'vbNone
  MTSTransactionMode  = 0  'NotAnMTSObject
END
Attribute VB_Name = "cHttpClient"
Attribute VB_GlobalNameSpace = False
Attribute VB_Creatable = True
Attribute VB_PredeclaredId = False
Attribute VB_Exposed = True
Option Explicit
'作者：邓伟，QQ：215879458，可自由传播，但请勿删本签名

'引用"Microsoft WinHTTP Services, version 5.1"组件
'引用了Microsoft Scripting Runtime组件
'引用 cJson.cls 类文件

'增加了通过Content-Type自动识别 json 数据和强制返回json数据的参数

'增加了"长轮询"功能
'疑问:winhttp组件支持证书,,或许可以参考实现winsock的证书服务?
'疑问:winhttp返回值类型还有个"流",,这个是可以做直播等流媒体?---- 了解了一下,可以做文件上传和下载的,不过直播应该也可以

'2023-03-15 增加了对json的content-type 的处理 : AjaxInst.postContentType = Json
'2023-03-30 改大了同步请求的默认超时值为5分钟，后续增加超时参数给用户设置
'2025-05-25 修复：异步 OnError 事件缺失、异步状态码检查、新增 StatusCode/StatusText 属性、
'           修复超时最小值修改用户设置、清理废弃 RequestDataBody、改进 ParseSetCookie
'2026-07-12 新增 Proxy 链式函数：设置代理服务器，延迟到 Open 之后执行
'2026-07-12 新增异步事件：OnRedirect/OnResponseStart/OnResponseDataAvailable/OnTimeout
'           新增 FollowRedirects 属性、异步超时检测、下载进度回调

'todo 增加集合对象, 实现并发异步请求池子
'todo 增加集合对象, 实现串行异步请求队列

Dim WithEvents Inst As WinHttp.WinHttpRequest
Attribute Inst.VB_VarHelpID = -1
Dim Json As New cJson
Private Declare Function ShellExecute Lib "shell32.dll" Alias "ShellExecuteA" ( _
    ByVal hwnd As Long, _
    ByVal lpOperation As String, _
    ByVal lpFile As String, _
    ByVal lpParameters As String, _
    ByVal lpDirectory As String, _
    ByVal nShowCmd As Long) As Long

'缓存请求的返回内容
Public ResponseRaw As Variant
'模仿前几天写的php版ajax，做个完整处理过的 res.header+res.cookies+res.data
Public Event OnResponseFinished()
Public Event OnError(ByVal ErrorNumber As Long, ByVal ErrorDescription As String)
Public Event OnRedirect(ByVal Url As String)
Public Event OnResponseStart(ByVal Status As Long, ByVal ContentType As String)
Public Event OnResponseDataAvailable(Data() As Byte, ByVal BytesReceived As Long, ByVal TotalBytes As Long)
Public Event OnTimeout()

Public LastError As String
Dim pvRequestData() As Byte
Public RequestDataJson As New cJson
Public RequestDataForm As New Dictionary
Public RequestDataQuery As New Dictionary
Public RequestHeaders As New Dictionary
Public RequestContentType As String
Public RequestChartSet As String
Public RequestTimeOut As Long

Public ResponseHeaders As New Dictionary
Public Cookies As New Dictionary

Public DebugStart As Boolean
Public DebugInfo As New cJson

'链式调用函数参数变量区
Dim IsAsync As Boolean
'代理配置（延迟到 Fetch 中 Open 之后 Apply）
Dim m_ProxyConfigured As Boolean
Dim m_ProxyServer As String
Dim m_ProxyBypassList As String

'响应状态码（异步/同步均可用）
Private m_StatusCode As Long
Private m_StatusText As String

'异步进度追踪
Private m_BytesReceived As Long
Private m_TotalBytes As Long
Private m_FollowRedirects As Boolean

'=========================================================================
' 属性：响应状态码
'=========================================================================
Public Property Get StatusCode() As Long
    On Error Resume Next
    If m_StatusCode = 0 Then m_StatusCode = Inst.Status
    StatusCode = m_StatusCode
End Property

Public Property Get StatusText() As String
    On Error Resume Next
    If m_StatusText = "" Then m_StatusText = Inst.StatusText
    StatusText = m_StatusText
End Property
'=========================================================================
' 属性：是否自动跟随重定向
'=========================================================================
Public Property Get FollowRedirects() As Boolean
    FollowRedirects = m_FollowRedirects
End Property

Public Property Let FollowRedirects(ByVal Value As Boolean)
    m_FollowRedirects = Value
End Property

'=========================================================================
' 请求方法
'=========================================================================

Public Function MapRequestContentType(ReqType As EnumRequestContentType, Optional ContentType As String) As String
    MapRequestContentType = SetRequestContentType(ReqType, ContentType)
End Function
Public Function SetRequestContentType(ReqType As EnumRequestContentType, Optional ContentType As String) As String
    Dim Arr As Variant: Arr = Array("", _
    Consts.TOOLS_HTTP_CONTENT_TYPE_JSON, _
    Consts.TOOLS_HTTP_CONTENT_TYPE_FORM_URLENCODED, _
    Consts.TOOLS_HTTP_CONTENT_TYPE_FORM_MULTIPART, _
    Consts.TOOLS_HTTP_CONTENT_TYPE_TEXT_PLAIN, _
    Consts.TOOLS_HTTP_CONTENT_TYPE_TEXT_HTML _
    )
    RequestContentType = Arr(ReqType)
    SetRequestContentType = RequestContentType
End Function

Public Sub ShowPage(Url As String)
    ShellExecute 0, "open", Url, 0, 0, 1
End Sub

'设置 Cookie（支持 "name=value; name2=value2" 格式，自动解析到 Cookies 字典）
Public Function SetCookies(ByVal Value As String) As cHttpClient
    Set SetCookies = Me
    If Trim(Value) = "" Then Exit Function
    
    '解析 Cookie 字符串到 Cookies 字典，同时保留原始值作为请求头
    Dim Pairs() As String
    Pairs = Split(Value, ";")
    Dim i As Long
    For i = LBound(Pairs) To UBound(Pairs)
        Dim Pair As String
        Pair = Trim(Pairs(i))
        If Pair <> "" Then
            Dim EqPos As Long
            EqPos = InStr(Pair, "=")
            If EqPos > 0 Then
                Cookies(Trim(Left(Pair, EqPos - 1))) = Trim(Mid(Pair, EqPos + 1))
            End If
        End If
    Next
    
    '设置请求头
    RequestHeaders("Cookie") = Value
End Function

Public Function Async(Bool As Boolean) As cHttpClient
    Set Async = Me
    IsAsync = Bool
End Function

Public Function SendPost(ByVal Url As String, Optional Body As String) As cHttpClient
    Set SendPost = Fetch(ReqPost, Url, Body)
End Function
Public Function SendGet(ByVal Url As String, Optional Body As String) As cHttpClient
    Set SendGet = Fetch(ReqGet, Url, Body)
End Function
Public Function SendPut(ByVal Url As String, Optional Body As String) As cHttpClient
    Set SendPut = Fetch(ReqPut, Url, Body)
End Function
Public Function SendDelete(ByVal Url As String, Optional Body As String) As cHttpClient
    Set SendDelete = Fetch(ReqDelete, Url, Body)
End Function
Public Function SendOptions(ByVal Url As String, Optional Body As String) As cHttpClient
    Set SendOptions = Fetch(ReqOptions, Url, Body)
End Function

'alias
Public Function Send(Method As EnumRequestMethod, ByVal Url As String, Optional Body As String) As cHttpClient
    Set Send = Fetch(Method, Url, Body)
End Function
Public Function Fetch(Method As EnumRequestMethod, ByVal Url As String, Optional Body As String) As cHttpClient
    Set Fetch = Me
    On Error GoTo ERR:
    Dim x As Variant: LastError = "": Set DebugInfo = Nothing
    
    '清除上次响应状态
    m_StatusCode = 0
    m_StatusText = ""
    
    '检查url
    If Url = "" Then ERR.Raise 500, , "请求的URL地址为空,请检查!"
    '处理请求参数
    If RequestDataQuery.count > 0 Then Url = ToolsHttp.AddToQueryString(Url, ToolsHttp.MakeContent(RequestDataQuery))
    '是否添加URL资源版本号
    '    If addVer Then Url = ToolsHttp.AddToQueryString(Url, Rnd())
    '打开链路
    Dim MethodName As String: MethodName = ToolsHttp.MapMethodName(Method)
    Inst.Open MethodName, Url, True
    '应用代理配置（必须在 Open 之后、Send 之前）
    If m_ProxyConfigured Then Call ApplyProxy
    '异步模式：设置超时（WinHttp SetTimeouts 单位为毫秒）
    'resolveTimeout, connectTimeout, sendTimeout, receiveTimeout
    If IsAsync Then
        Dim WaitTimeout As Long
        WaitTimeout = IIf(RequestTimeOut < 30, 30, RequestTimeOut)
        Inst.SetTimeouts 10000, 10000, WaitTimeout * 1000, WaitTimeout * 1000
    End If
    '重定向控制（WinHttpRequestOption_EnableRedirects = 6）
    Inst.Option(6) = m_FollowRedirects
    '重置异步进度状态
    m_BytesReceived = 0
    m_TotalBytes = -1
    '处理Header
    With RequestHeaders
        If .Exists("Cookie") = False Then
            '如果有已保存的 Cookie，自动带上
            If Cookies.count > 0 Then
                Dim CookieStr As String
                CookieStr = BuildCookieHeader()
                If CookieStr <> "" Then
                    .Add "Cookie", CookieStr
                End If
            End If
        End If
        If .Exists("Content-Type") = False Then
            If RequestDataJson.RootIsEmpty = False Then
                RequestContentType = Consts.TOOLS_HTTP_CONTENT_TYPE_JSON
            End If
            If RequestContentType = "" Then
                RequestContentType = Consts.TOOLS_HTTP_CONTENT_TYPE_FORM_URLENCODED
            End If
            .Add Consts.TOOLS_HTTP_HEADER_FIELD_CONTENT_TYPE, RequestContentType
        End If
        If .count > 0 Then
            For Each x In .Keys()
                If .Item(x) <> "" Then Inst.SetRequestHeader x, .Item(x)
            Next
        End If
    End With
    '忽略错误SSL证书
    Inst.Option(WinHttpRequestOption_SslErrorIgnoreFlags) = &H3300
    '处理POSTBODY
    If Body = "" Then
        If RequestContentType = Consts.TOOLS_HTTP_CONTENT_TYPE_JSON Then
            Body = RequestDataJson.Encode(, , True)
        ElseIf RequestContentType = Consts.TOOLS_HTTP_CONTENT_TYPE_FORM_URLENCODED Then
            Body = ToolsHttp.MakeContent(RequestDataForm)
        End If
    End If
    If Body = "" Then
        '发送数据
        Inst.Send
    Else
        Dim BodyByte() As Byte
        If ToolsStr.HasStr("utf", RequestChartSet) > 0 Then
            BodyByte = ToolsUtf8.Encode(Body)
        Else
            BodyByte = ToolsArray.StringToByteArray(Body)
        End If
        Inst.SetRequestHeader Consts.TOOLS_HTTP_HEADER_FIELD_CONTENT_LENGHT, UBound(BodyByte) + 1
        '发送数据
        Inst.Send BodyByte
    End If
    '返回
    
    If IsAsync = False Then
        '设置等待超时, '由于使用异步请求,如果用户参数改为同步请求,此处则等待返回,无需使用do循环+doevents
        Dim SyncTimeout As Long
        SyncTimeout = IIf(RequestTimeOut < 30, 30, RequestTimeOut)
        If Inst.WaitForResponse(SyncTimeout) = False Then ERR.Raise 900, , "请求超时: " & SyncTimeout
        
        '保存状态码
        m_StatusCode = Inst.Status
        m_StatusText = Inst.StatusText
        
        '状态码判断：4xx/5xx 为明确错误，其余交给调用方处理
        If m_StatusCode >= 400 And m_StatusCode <= 599 Then ERR.Raise m_StatusCode, , m_StatusCode & "#" & m_StatusText & "#" & Left(Inst.ResponseText, 1024)
        '解析 返回头
        Call ToolsHttp.ParseKeyValue(Inst.GetAllResponseHeaders(), ResponseHeaders)
    End If
    '清理body,但保留头部和get请求参数,头部和参数基本上是不变的,需要清理可以手工执行字典清楚方法
    GoSub CLEAN
    Exit Function
ERR:
    Rem todo 这里可以优化一下，对超时错误代码进行准确描述
    If DebugStart = True Then
        With DebugInfo
            With .NewItem("Error")
                .Item("Number") = ERR.Number
                .Item("Source") = ERR.Source
                .Item("Description") = ERR.Description
            End With
        End With
    End If
    LastError = ERR.Description
    Dim ErrInfo(4) As Variant
    ErrInfo(0) = ERR.Number
    ErrInfo(1) = ERR.Source
    ErrInfo(2) = ERR.Description
    ErrInfo(3) = ERR.HelpFile
    ErrInfo(4) = ERR.HelpContext
    GoSub CLEAN
    ERR.Raise ErrInfo(0), ErrInfo(1), ErrInfo(2), ErrInfo(3), ErrInfo(4)
    Exit Function
CLEAN:
    If DebugStart = True Then
        With DebugInfo
            With .NewItem("Request")
                .Item("IsAsync") = IsAsync
                .Item("Method") = MethodName
                .Item("Url") = Url
                .Item("Body") = Body
                Set .Item("Headers") = RequestHeaders
                .Item("TimeOut") = RequestTimeOut
                .Item("ChartSet") = RequestChartSet
            End With
            If IsAsync = False Then
                With .NewItem("Response")
                    .Item("Status") = m_StatusCode
                    .Item("StatusText") = m_StatusText
                    Set .Item("Headers") = ResponseHeaders
                    'ReturnText 会干扰 err 对象
                    .Item("Content") = ReturnText(True)
                End With
            End If
        End With
    End If
    IsAsync = False
    DebugStart = False
    
    m_ProxyConfigured = False
    m_ProxyServer = ""
    m_ProxyBypassList = ""
    RequestDataJson.Clear
    RequestDataForm.RemoveAll
    RequestDataQuery.RemoveAll
    m_BytesReceived = 0
    m_TotalBytes = -1
    '    RequestHeaders.RemoveAll
    '    Cookies.RemoveAll
    Return
End Function

'=========================================================================
' 响应获取方法
'=========================================================================

Public Function ReturnStream() As Variant
    ReturnStream = Inst.ResponseStream
End Function

Public Function ReturnBody() As Byte()
    ReturnBody = Inst.ResponseBody
End Function

Public Function ReturnJson(Optional IsUtf8 As Boolean = True, Optional IsConvert As Boolean) As cJson
    Set ReturnJson = New cJson
    Dim s As String: s = ReturnText(IsUtf8, IsConvert)
    Call ReturnJson.Decode(s)
End Function

Public Function ReturnText(Optional IsUtf8 As Boolean = True, Optional IsConvert As Boolean) As String
    If IsUtf8 = True Then
        ReturnText = ToolsUtf8.Decode(Inst.ResponseBody)
        Exit Function
    End If
    If IsConvert = True Then
        '---------------------------二进制转字符串[直接返回字串出现乱码时尝试]
        ReturnText = StrConv(Inst.ResponseBody, vbUnicode)
    Else
        ReturnText = Inst.ResponseText
    End If
End Function

'设置代理服务器（延迟到 Fetch 的 Open 之后、Send 之前执行）
'ProxyServer 为空字符串时取消代理配置（直连，不使用任何代理）
'ProxyServer 格式示例:
'   "proxy.example.com:8080" - 所有协议共用同一代理
'   "http=proxy1:8080;https=proxy2:8443" - 分别为 HTTP/HTTPS 指定代理
'BypassList 可选，代理绕过地址列表，多个用分号分隔，如 "localhost;127.0.0.1;*.internal.com"
Public Function Proxy(ByVal ProxyServer As String, Optional ByVal BypassList As String) As cHttpClient
    Set Proxy = Me
    '保存代理配置，在 Fetch 中 Open 之后 Apply
    m_ProxyConfigured = True
    m_ProxyServer = ProxyServer
    m_ProxyBypassList = BypassList
End Function

'=========================================================================
' 私有方法
'=========================================================================

'应用代理配置（仅在 Fetch 中 Inst.Open 之后调用）
'注意：WinHttp SetProxy 必须在 Open 之后、Send 之前调用
'HTTPREQUEST_PROXYSETTING_DIRECT = 1（直连，不使用代理）
'HTTPREQUEST_PROXYSETTING_PROXY = 2（使用指定代理服务器）
Private Sub ApplyProxy()
    On Error Resume Next
    If m_ProxyServer = "" Then
        '取消代理配置，直连
        Inst.SetProxy 1
    Else
        '使用指定代理服务器
        Inst.SetProxy 2, m_ProxyServer, m_ProxyBypassList
    End If
    m_ProxyConfigured = False
    m_ProxyServer = ""
    m_ProxyBypassList = ""
End Sub

'构建 Cookie 请求头字符串（从 Cookies 字典）
Private Function BuildCookieHeader() As String
    If Cookies.count = 0 Then Exit Function
    Dim k As Variant
    Dim Result As String
    For Each k In Cookies.Keys()
        If Result <> "" Then Result = Result & "; "
        Result = Result & CStr(k) & "=" & CStr(Cookies(k))
    Next
    BuildCookieHeader = Result
End Function

'=========================================================================
' 初始化和清理
'=========================================================================

Private Sub Class_Initialize()
    Set Inst = New WinHttp.WinHttpRequest
    '    oHttp.SetProxy 1
    '    oHttp.SetTimeouts 2000, 2000, 6000, 20000
    'WinHttp.SetTimeouts 60000, 60000, 60000, 3000 '设置操作超时时间
    'WinHttp.SetTimeouts resolveTimeout, connectTimeout, sendTimeout, receiveTimeout
    'resolveTimeout = 10000 '解析 DNS 名字的超时时间，10000 毫秒。
    'connectTimeout = 10000 '建立 Winsock 连接的超时时间，10000 毫秒。
    'sendTimeout = 120000 '发送数据的超时时间，120000 毫秒。
    'receiveTimeout = 60000 '接收 response 的超时时间，60000 毫秒。
    
    '    reqHeaders.Add Consts.TOOLS_HTTP_HEADER_FIELD_CONTENT_TYPE, Consts.TOOLS_HTTP_CONTENT_TYPE_FORM_URLENCODED
    '    reqHeaders.Add "req-from", "vb ajax class by 215879458@qq.com"
    '    postBody.Add "ver", 1#
    RequestDataForm.CompareMode = TextCompare
    RequestDataQuery.CompareMode = TextCompare
    RequestHeaders.CompareMode = TextCompare
    Cookies.CompareMode = TextCompare
    ResponseHeaders.CompareMode = TextCompare
    m_FollowRedirects = True
    RequestTimeOut = 30
    RequestChartSet = "utf-8"
End Sub

Private Sub Class_Terminate()
    Set Inst = Nothing
End Sub

'=========================================================================
' WinHttpRequest 事件处理程序
'=========================================================================

Private Sub Inst_OnResponseFinished()
    On Error Resume Next
    
    '保存状态码（异步模式下也需要）
    m_StatusCode = Inst.Status
    m_StatusText = Inst.StatusText
    
    '3xx 重定向不算正常响应完成（FollowRedirects=False 时由 OnRedirect 处理）
    If m_StatusCode >= 300 And m_StatusCode < 400 Then Exit Sub
    
    '解析返回头
    Call ToolsHttp.ParseKeyValue(Inst.GetAllResponseHeaders(), ResponseHeaders)
    
    '自动处理 Set-Cookie 响应头
    If ResponseHeaders.Exists("Set-Cookie") Then
        Call ParseSetCookie(ResponseHeaders("Set-Cookie"))
    End If
    
    '异步模式下检查 HTTP 状态码
    If m_StatusCode >= 400 And m_StatusCode <= 599 Then
        'HTTP 错误，触发 OnError 事件
        LastError = m_StatusCode & " " & m_StatusText
        RaiseEvent OnError(m_StatusCode, m_StatusCode & " " & m_StatusText & " " & Left(Inst.ResponseText, 512))
    Else
        '正常响应，触发 OnResponseFinished 事件
        RaiseEvent OnResponseFinished
    End If
End Sub

Private Sub Inst_OnError(ByVal ErrorNumber As Long, ByVal ErrorDescription As String)
    '检测超时错误码（ERROR_WINHTTP_TIMEOUT = 12002 = &H2EE2）
    If ErrorNumber = 12002 Then
        LastError = "请求超时"
        RaiseEvent OnTimeout
        Exit Sub
    End If
    
    LastError = ErrorDescription & " (错误代码: " & ErrorNumber & ")"
    '记录错误到调试信息
    If DebugStart = True Then
        With DebugInfo.NewItem("AsyncError")
            .Item("Number") = ErrorNumber
            .Item("Description") = ErrorDescription
        End With
    End If
    '触发自定义错误事件，通知上层（如 cSSEClient）
    RaiseEvent OnError(ErrorNumber, ErrorDescription)
End Sub

Private Sub Inst_OnResponseStart(ByVal Status As Long, ByVal ContentType As String)
    On Error Resume Next
    
    '保存状态码
    m_StatusCode = Status
    
    '获取 Content-Length 用于进度追踪
    Dim AllHeaders As String
    AllHeaders = Inst.GetAllResponseHeaders()
    Dim CLPos As Long
    CLPos = InStr(1, AllHeaders, "Content-Length:", vbTextCompare)
    If CLPos > 0 Then
        Dim CLStart As Long
        CLStart = CLPos + Len("Content-Length:")
        Dim CLEnd As Long
        CLEnd = InStr(CLStart, AllHeaders, vbCr)
        If CLEnd > 0 Then
            m_TotalBytes = CLng(Trim(Mid(AllHeaders, CLStart, CLEnd - CLStart)))
        End If
    End If
    
    '重定向检测（仅在禁用自动重定向时生效）
    If Status = 301 Or Status = 302 Or Status = 307 Or Status = 308 Then
        '获取 Location 头
        Dim LocPos As Long
        LocPos = InStr(1, AllHeaders, "Location:", vbTextCompare)
        If LocPos > 0 Then
            Dim LocStart As Long
            LocStart = LocPos + Len("Location:")
            Dim LocEnd As Long
            LocEnd = InStr(LocStart, AllHeaders, vbCr)
            If LocEnd > 0 Then
                RaiseEvent OnRedirect(Trim(Mid(AllHeaders, LocStart, LocEnd - LocStart)))
            End If
        End If
        Exit Sub
    End If
    
    '触发响应开始事件
    RaiseEvent OnResponseStart(Status, ContentType)
End Sub

Private Sub Inst_OnResponseDataAvailable(Data() As Byte)
    On Error Resume Next
    
    '累加已接收字节数
    m_BytesReceived = m_BytesReceived + UBound(Data) + 1
    
    '触发数据可用事件（带进度信息）
    RaiseEvent OnResponseDataAvailable(Data, m_BytesReceived, m_TotalBytes)
End Sub



'=========================================================================
' Set-Cookie 解析
'=========================================================================

'解析 Set-Cookie 响应头
'注意：WinHttp 的 GetAllResponseHeaders 返回的是拼接后的字符串，
'多个 Set-Cookie 之间用 vbCrLf 分隔，不能用逗号分割（Expires 值含逗号）
Private Sub ParseSetCookie(ByVal SetCookieValue As String)
    If Trim(SetCookieValue) = "" Then Exit Sub
    
    Dim CookieName As String
    Dim CookieValue As String
    
    '按换行符分割多个 Set-Cookie（WinHttp 返回的多个同名头用换行分隔）
    Dim MultiCookies() As String
    MultiCookies = Split(SetCookieValue, vbCrLf)
    
    Dim i As Long
    For i = LBound(MultiCookies) To UBound(MultiCookies)
        Dim CookieItem As String
        CookieItem = Trim(MultiCookies(i))
        If CookieItem = "" Then GoTo NEXT_COOKIE
        
        '找到第一个等号分隔 name=value
        Dim EqPos As Long
        EqPos = InStr(CookieItem, "=")
        If EqPos > 0 Then
            CookieName = Trim(Left(CookieItem, EqPos - 1))
            '只取到分号或行尾（分号后是属性如 Path、Domain、Expires 等）
            Dim SemicolonPos As Long
            SemicolonPos = InStr(EqPos + 1, CookieItem, ";")
            If SemicolonPos > 0 Then
                CookieValue = Trim(Mid(CookieItem, EqPos + 1, SemicolonPos - EqPos - 1))
            Else
                CookieValue = Trim(Mid(CookieItem, EqPos + 1))
            End If
            Cookies(CookieName) = CookieValue
        End If
NEXT_COOKIE:
    Next i
End Sub

