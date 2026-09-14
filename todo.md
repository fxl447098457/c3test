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
