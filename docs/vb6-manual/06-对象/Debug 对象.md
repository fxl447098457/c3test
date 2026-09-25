# Debug 对象

**Debug** 对象在运行时将输出发送到 **Immediate** 窗口。

## C3 说明：控制台输出的编码

C3 编译出的控制台程序里 `Debug.Print` 是**标准输出**（VB6 里是 Immediate 窗口）。中文/日文/韩文
这类非 ASCII 文本在两个出口上的规矩不一样，这也是「同一份输出，cmd 里好好的、PowerShell 里却乱码」
的由来：

- **控制台**（cmd / Windows Terminal / ConPTY，`Debug.Print` 直接输出到窗口）：按控制台原生宽字符
  路径输出，**与当前代码页（`chcp`）无关** —— 代码页 936/65001/437 下渲染逐字相同，四种语言都对。
- **管道 / 文件**（被 PowerShell 接走、或 `prog > file`）：按**当时的控制台输出代码页**写字节
  （中文机默认 936 = GBK），这是 cmd 重定向与 PowerShell 默认解码期待的那一套；某一行的文本
  在该代码页里**装不下**时（例如 GBK 装不下韩文），该行**整行退回 UTF-8**。

**想让任意语言都在命令行里正确显示**（例如在中文系统上输出韩文）：先把控制台切成 UTF-8 再启动
shell ——

```
chcp 65001          :: cmd 里先切, 再启动 powershell/pwsh; 或 Windows Terminal 用 UTF-8 档
```

此时程序按 UTF-8 落字节、shell 也按 UTF-8 解码，中/日/韩/英四种同时成立。回归里
`cc_cn_console`（真控制台读屏幕缓冲）与 `cc_cn_redirect_gbk`（`chcp 936` + 落盘按字节断言）
各钉一条路。
