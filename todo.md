# todo

## 搞个关键字列表，能修改的，自定义中英文 原生命令名随便改

## 群友质询 Declare 64位加宽延伸的待办（2026-09-03）
1. VB6 算术/赋值溢出检查（Error 6）未实现：Long/Integer 运算当前为 32 位静默回绕，VB6 运行时对超范围结果抛错误 6，需评估 RTL 溢出检查或编译开关（ai/009 中列为 P3 可延后，考虑提前）
2. Declare 返回值/句柄赋给 Long 变量会截断：评估加编译警告提示改用 LongPtr 接收句柄/指针，并补充老代码迁移指引
3. Declare ByVal Long 加宽 intptr_t 边界测试：覆盖纯数值参数、负数符号扩展、ByRef Long 不受影响、LongPtr 显式声明等用例，固化"仅加宽不改变语义"的行为

## 群友质询延展：问题与建议（2026-09-03 二轮）
1. [问题] Declare As Long 返回值在 x64 的零扩展符号破坏：ABI 写 EAX 清空 RAX 高位，DLL 返回的 32 位负值（如 -1 错误码）被 intptr_t 全宽读成 0xFFFFFFFF（4294967295）。赋给 Long 变量靠 C 隐式截断还原无恙，但直接比较/位运算/作中间值（如 If Foo() = -1、Foo() And mask）时语义错误
   [建议] Declare 调用消费点按 VB6 语义补 (int32_t) 截断（比较/整值运算路径特判）；真正需要 64 位句柄时由源码显式声明 As LongPtr 获取，保持"As Long=32位有符号"忠实语义，不做返回值静默加宽
2. [问题] UDT 成员不随架构加宽导致 Declare 结构体参数错位：Fix 081e 只作用于直接写 As Long 的参数/返回值，UDT 内部 Long 恒为 4 字节；含句柄/指针成员的结构体（OPENFILENAME.hwndOwner 等）x64 下应为 8 字节，老源码 As Long 生成 4 字节 → 传给 API 该成员之后全部错位；且朴素 struct 无 pack，含 Double/Currency 成员的 UDT 在 x64 默认对齐下尺寸与 VB6 布局可能不同
   [建议] 无法自动判断成员语义，需迁移指引+警告：提示将句柄类成员显式改 As LongPtr（VBA7 官方 API 声明同款做法）；对常见 Win32 API 结构体提供修正声明清单；文档说明 UDT 对齐边界
3. [建议] 补 #If Win64 回归测试固化行为（特性已支持，Fix 081h：win64 = --arch 参数，常量查找小写不敏感，支持 #If/#ElseIf/#Else/#End If/#Const）：覆盖 --arch x64/x86 两套目标分支选择、#If Win64 Then LongPtr #Else Long 的 VBA7 双平台迁移代码、-D 覆盖内置常量

## 群友质询延展：问题与建议（2026-09-03 三轮）
1. [问题] 缺少编译器身份标识常量：内置条件编译常量仅 win16/win32/win64/vba6/vba7/mac/win 七个平台与语言版本属性，无 C3 专属标识；且 vba6 与 vba7 同时为 True（C3 双兼容定位），用户无法用 #If VBA7 Then 区分"当前是 C3 还是原生 VB6/VBA7"环境
   [建议] preprocessor.cpp 内置注册 c3=True（小写键，与其他内置一致）；命名用 C3（条件编译符号表与运行时变量名空间独立，无冲突），保留 -d:C3=False 可覆盖以便测试原生分支；文档给出四分支标准写法：#If C3 / #ElseIf VBA7 And Win64 / #ElseIf VBA7 / #Else，配套回归测试

## 群友质询：VB6 原生 GUI 兼容到了什么程度（2026-09-15 结论存档）

0. [基线口径] 只谈 VB6 **内置**（工具箱 21 类 + Form/MDIForm + Menu），不含第三方 OCX。当前实测：
   - 控件：14 类全链路可用（Form/MDIForm、PictureBox、Label、TextBox、Frame、CommandButton、CheckBox、OptionButton、ComboBox、ListBox、HScrollBar、VScrollBar、Timer、Image、Menu）→ 21 类里约 67%
   - 属性：50+ 个双向读写（Left/Top/Width/Height/hWnd、Font 六件套、ForeColor/BackColor、Alignment、TabIndex/TabStop/CausesValidation、ToolTipText、Tag、MousePointer/MouseIcon、BorderStyle、Visible/Enabled + 各控件专有）
   - 事件：约 25 个（Click/DblClick/Change/Scroll/KeyDown/Press/Up/MouseDown/Up/Move/Enter/Leave/GotFocus/LostFocus/Validate(Cancel) + 窗体 Load/Unload(Cancel)/QueryUnload(Cancel)/Activate/Deactivate/Resize/鼠标键 + Timer + Menu.Click）
   - **方法：近乎空白**（仅 AddItem/RemoveItem/Clear + Timer 相关）→ 这是最短的一块板
1. [结论] 属性层最厚、事件层够日常、**方法层和绘图层是主要欠账**。常规 CRUD/工具型界面（文本框+按钮+列表+下拉+复选/单选+Frame+图片+滚动条+菜单+定时器+MDI）能编译并原生跑起来；依赖运行时操纵控件（Move/SetFocus/ZOrder/Refresh）、自绘画图（PSet/Line/Circle/Cls/PaintPicture）、拖放、多层容器嵌套的老程序跑不通，需改源码
2. [发现-关键] Shape/Line/DriveListBox/DirListBox/FileListBox 五类并非"没写"，而是**半接线**：RTL 已实现（vb6forms.c 里 Shape/Line 自绘 WndProc、文件系统控件的 Drive/Path/Pattern/FileName/Refresh 全有），属性读写表已登记、.frm 初值赋值也已生成，但 frm_parser.cpp 的 controlTypeToWin32Class 对这五类返回 nullptr → cgen_form.cpp 的创建循环不发 CreateWindow → vb6_hwnd_ 恒为 NULL → 编译零错但运行时看不见。**性价比最高，差一步**
3. [发现-次要] Toolbar/StatusBar/CommonDialog/ImageList 走 ActiveX CoCreateInstance 生成 IDispatch*，但无属性/方法映射，且这些 OCX 是 32 位 → x64 进程根本 CoCreate 失败
4. [发现-其他] 容器只遍历 Frame 单层子控件（PictureBox 当容器、多层嵌套不支持）；缇↔像素硬编码 1 比 15（96 DPI），无 per-monitor DPI；缺 Form_Click、Paint、DragDrop/DragOver 事件
5. [结论-对外口径] GUI 是**Win32 原生重实现而非复刻 VB6 运行时**，"形似"可达成、"神似"（像素级渲染、字体度量、VB6 怪癖行为）需逐项对齐；这也是 VBMAN 运行期对齐的主战场

## 下一步主方向：VB6 内置控件可用度 67% → 100%（2026-09-15 定）

范围定义："100%" = VB6 工具箱 21 类控件 + Form/MDIForm + Menu，达成 **窗口能创建 + 属性可读写 + 事件能分发 + 常用方法可调用** 四项；Data/OLE 因 x64 无 DAO/MDAC 支撑列为豁免项（但要求给出明确诊断而非静默通过）

1. [P0-第一步] 打通 5 个半残控件的最后一公里（做完约 67% → 90%）
   - frm_parser.cpp:controlTypeToWin32Class 补 Shape→"VB6_SHAPE"、Line→"VB6_LINE"、DriveListBox→"COMBOBOX"、DirListBox/FileListBox→"LISTBOX"（对应 VB6 原生即这几个窗口类）
   - 类名需与 vb6forms.c 的 vb6_RegisterShapeLineClasses 注册名严格一致（L"VB6_SHAPE"/L"VB6_LINE"），注意该函数只在 hasShape/hasLine 时生成调用，Drive/Dir/File 无需注册
   - 确认创建后 Size 时机：Shape/Line 的属性初值已在 cgen_form.cpp 1652-1676 生成，但要在 CreateWindow 之后再落有一次 WM_PAINT 触发属性生效
   - 补缺失属性到 cgen_util.cpp 属性表：DriveListBox.Drive、DirListBox.Path/ListIndex、FileListBox.Path/Pattern/FileName/ListIndex（RTL 侧已就绪：vb6_DriveListBoxDrive/SetDrive、vb6_*ListBoxPath/SetPath/SetPattern/FileName）
2. [P1] 方法层补齐（最大短板，老 VB6 代码最常用）
   - RTL 新增 vb6_SetFocus / vb6_MoveControl(Left,Top,Width,Height) / vb6_SetZOrder(pos) / vb6_RefreshControl / vb6_DragControl
   - cgen 侧补控件方法调用表（当前只有 AddItem/RemoveItem/Clear 三条路径），方法名大小写不敏感
   - SetFocus 优先用 SetFocus(hwnd) + SetForegroundWindow，注意 vb6_hwnd_ 为 NULL 时静默返回（Timer 等无窗口控件）
3. [P1] 绘图语句与画布属性（PictureBox 画图类程序的死穴）
   - **语法形态警示（群友指正）**：PSet / Line / Circle / Print 在 VB6 里是**关键字语句**，调用写法与函数完全不同，不能走普通方法调用路径：
     - `PSet [Step] (x, y), [color]` —— 坐标必须是括号对
     - `Line [Step] (x1,y1) [Step] -(x2,y2), [color], [B][F]` —— 独有的"坐标对连接"语法 + B/F 矩形/填充标志
     - `Circle [Step] (x, y), radius, [color], [start], [end], [aspect]`
     - `Print expr[; expr][, expr][;]` —— `;` 连续输出 / `,` 制表位分区 / 结尾分号不换行
     - 可带对象前缀：`Picture1.Line ...`、`Printer.Print ...`；缺省对象 = 当前窗体
   - **Line 双重身份**：既是指令关键字也是控件类型名 → 必须上下文判定（语句起始 + 后随 `(` 坐标对或 `Step` 判为语句；否则按变量/控件/类型名走），不能简单注册为保留字
   - 实现路径分工：parser 新增 GraphicsStmt / PrintStmt 语句产生式（词法层不动，避免 Line 误杀）→ 语义解析目标对象（显式前缀 / 隐式 Me / Printer）→ **cgen 最终仍翻译为调用同名 RTL 函数**：vb6_PSet(obj,x,y,color) / vb6_Line(obj,x1,y1,x2,y2,color,bf) / vb6_Circle(obj,x,y,r,color,start,end,aspect) / vb6_Print(obj,...)。即"前端负责把关键字语法翻译成函数调用，后端只有函数没有语句"
   - Cls / PaintPicture / TextHeight / TextWidth 是普通方法调用形式，走第 2 条的方法调用表
   - 配套属性 DrawWidth / DrawStyle / DrawMode / FillColor / FillStyle / ScaleLeft / ScaleTop / ScaleMode，以及 CurrentX / CurrentY（Print 连续定位依赖）
   - 落笔载体直接用已实现的 vb6_SetAutoRedraw 内存 DC/位图（vb6forms.c 3017-3051），AutoRedraw=False 时走 GetDC 即时绘制 + 下一条线圈到窗体临时 DC
   - 注意坐标：VB6 图形方法单位=ScaleMode、`Step` 为相对当前 CurrentX/CurrentY 的增量，需统一 twip/px 换算入口，避免与现有 1 比 15 硬编码打架
   - 回归测试需覆盖：语句/函数两种写法互不干扰（如自定义了名为 Line 的控件仍可用）、Step 相对坐标、B/F 标志、Print 的 `;`/`,` 分隔与结尾分号
4. [P2] Data(DAO) 与 OLE 容器：明确边界
   - 结论倾向不实作 x64 DAO（MDAC 无 x64）；改为编译期/运行期明确诊断：遇到 VB.Data 或 VB.OLE 给出"不支持"的编译错误或运行时友好提示，杜绝"静默编过去但不显示"
   - 文档标注为已定义豁免项，并给出迁移建议（改 ADODB.Recordset 走 COM 路径）
5. [P2] 容器嵌套与运行时层级
   - 当前只递归 Frame 一层（cgen_form.cpp 1783 起），改造为任意深度递归 + 记录 Parent
   - 支持 PictureBox 作为容器（VB6 里它是容器控件）
   - 缇偏移需按父容器累加，避免子控件坐标错乱
6. [P3] 事件补齐与一致性：Form_Click、Paint、DragDrop/DragOver、OLEDrag* ；MouseHover 在 TrackMouseEvent 路径上的行为统一
7. [P3] DPI：1 缇 = 1/15 像素硬编码改 per-monitor DPI-aware（至少 GetDpiForWindow 换算），避免高 DPI 屏上整体偏移
8. [验证] 每类控件配一个最小 .frm 用例，要求"编译通过 + 真跑起来 + 属性/方法/事件触发"三档验证
   - 现状痛点：GUI 只有编译和冒烟，tests 下 16 个 .frm 不做交互/渲染校验
   - 目标：补一套可自动化的 GUI 回归（SendMessage 驱动控件 + 断言属性值/事件回调计数），至少覆盖 21 类控件的创建与核心属性
9. [对外] 完成后同步更新 README 的"未支持 Shape/Line/Drive-Dir-FileListBox/Data/OLE"条目（该描述已过时：其中 5 类是半接线而非未实现）
