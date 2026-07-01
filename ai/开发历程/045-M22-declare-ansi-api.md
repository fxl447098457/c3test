# M22 待定项：Declare A 版 API 字符串转换

## 问题描述
用户项目中如果使用 Declare 声明了 A 版 Win32 API（如 `Alias "SomeFuncA"`），
VB6 运行时会自动将 BSTR（UTF-16）转换为 ANSI（GBK）传给 A 版函数。
我们编译器当前直接传 BSTR 指针，没有做 ANSI 转换，会导致 A 版 API 收到乱码。

## 影响范围
- Declare W 版函数（默认）：BSTR 直传，无问题
- Declare A 版函数（Alias "xxxA"）：BSTR 直传，**会乱码**
- RTL 内部 Win32 调用：已全部使用 W 版，无问题

## 解决方案
对 Declare A 版函数的 `ByVal String` 参数，在调用点插入 BSTR→ANSI 转换层：
1. 调用前：`char* _ansi = vb6_BSTR_ToANSI(bstrArg);`
2. 传参：`SomeFuncA(_ansi, ...)`
3. 调用后：`vb6_FreeANSI(_ansi);`

判断条件：Declare 函数名或 Alias 以 "A" 结尾。
RTL 需新增 `vb6_BSTR_ToANSI` / `vb6_FreeANSI` 辅助函数。

## 优先级
中 — 当前 realVBP demo 不使用 Declare A 版 API，不影响 M22 里程碑。
