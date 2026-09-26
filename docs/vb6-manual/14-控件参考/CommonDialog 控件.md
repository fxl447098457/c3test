# CommonDialog 控件

# CommonDialog 控件

               

**CommonDialog** 控件提供一组标准的操作对话框，进行诸如打开和保存文件，设置打印选项，以及选择颜色和字体等操作。通过运行 Windows 帮助引擎控件还能显示帮助。

**语法**

**CommonDialog**

**说明**

**CommonDialog** 控件在 Visual Basic 和 Microsoft Windows 动态链接库 ommdlg.dll. 的例程之间提供了一个接口。为了用这个控件创建一个对话框，ommdlg.dll. 必须在 Microsoft Windows 的 SYSTEM 目录下。

在应用程序中要使用 **CommonDialog** 控件，可将其添加到窗体中并设置其属性。控件所显示的对话框由控件的方法确定。在运行时，当相应的方法被调用时，将显示一个对话框或是执行帮助引擎；在设计时，CommonDialog 控件是以图标的形式显示在窗体中。该图标的大小不能改变。

使用指定的方法，**CommonDialog** 控件能够显示下列对话。

|                 |                              |
|-----------------|------------------------------|
| **方法**        | **所显示的对话框**           |
| **ShowOpen**    | 显示“打开”对话框             |
| **ShowSave**    | 显示“另存为”对话框           |
| **ShowColor**   | 显示“颜色”对话框             |
| **ShowFont**    | 显示“字体”对话框             |
| **ShowPrinter** | 显示“打印”或“打印选项”对话框 |
| **ShowHelp**    | 调用 Windows 帮助引擎        |

  

在对话框接口上单击，**CommonDialog** 控件将自动提供与上下文有关的帮助：

- 单击标题栏中的“这是什么？”帮助按钮，然后单击想详细信息的项目。  
    
- 将鼠标放在想进一步详细信息的项目上，单击右键，然后在所显示的上下文菜单中选择这是什么命令。

操作系统提供在 Windows 95 帮助弹出中显示的文本。也可以通过设置 **Flags** 属性，在带有 **CommonDialog** 控件的对话框中显示一个帮助按钮，但是，必须在这个位置提供帮助主题。

**注意** 无法指定对话框显示在什么地方。

**详细信息** 要查看各对话的帮助主题，单击“请参阅”。

## 本项目的实现口径（ai/029 C29-9 / 决策 D6）

CommonDialog 走**原生 comdlg32**（`GetOpenFileNameW` / `GetSaveFileNameW` / `ChooseColorW` /
`ChooseFontW` / `PrintDlgW` / `ShellAboutW`），**不再经 MSComDlg.OCX**。原因很硬：那个 OCX 只有
32 位，在 64 位进程里 `CoCreateInstance` 直接失败，于是改之前这枚控件是**静默空转**的 —— 属性读回
全空、六个 `Show*` 一个都不出现，而程序照旧正常退出。API 只经 `LoadLibrary` + `GetProcAddress`
取得，不给工具链添新的 import lib。

语法面与本页上文一致：

| 写法 | 读数 |
| --- | --- |
| `CommonDialog1.Filter` | **原样读回竖线串**（VB6 口径）。原生那套 `描述\0模式\0…\0\0` 成对表只在
  调 `Show*` 的那一刻折叠出来，不外露 |
| `CommonDialog1.Flags` | 就是原生 `OFN_*` / `CC_*` / `CF_*` / `PD_*` 那一位（VB6 本来也是透传），
  原值读写，含 0 |
| `CommonDialog1.FileName` | 确认后是**全路径**；`FileTitle` 是同一次的裸文件名（含扩展名） |
| `CommonDialog1.CancelError` | 布尔：`True` 读回 `-1`。取消时报 **32755**（VB6 的 `cdlCancel`）；
  为 `False` 时取消静默返回，且**不改**任何已有读数 |
| `CommonDialog1.DialogTitle` / `InitDir` / `DefaultExt` / `Color` / `FontName` / `FontSize` /
  `Min` / `Max` / `Copies` | 属性袋挂在一枚自注册的**不可见**子窗口（`VB6_COMMONDIALOG`）上；
  设计期写进 `.frm` 的这几行在建窗之后原样落到袋里，两枚控件互不串 |

三条与 VB6 有差距的地方，写在这里免得按 VB6 去期待：

1. `ShowPrinter` 目前只把结果收回 `Copies`（以及 `Min`/`Max` 当页范围），**不返回 `Printer` 对象、
   也拿不到 DC** —— 拿到就 `DeleteDC` 了。打印那半属于"Printer 对象"这条独立线，未随本批做。
2. `ShowFont` 只回 `FontName` / `FontSize`；`FontBold` / `FontItalic` / `FontUnderline` /
   `FontStrikeThru` 四个**没接**（原生 `LOGFONT` 里都有，缺的是把它们接成 CommonDialog 的属性面）。
3. `Color` 对话框的**自定义色板**（`ColorDialog.CustomColors`）不落袋：原生那份 16 格数组是运行时
   私有的，本批只在进程内静态持有，不作为 VB6 属性对外。

判据在 `tests\ctrldlg\`（10 条读数：设计期四行落位、`Filter` 原样读回、另一枚不被继承、
运行期读写回路、两枚不串、六个 `Show*` 的发码形状），登记 `ctrldlg[_x86]`；发码两面都钉 ——
`dl_emitc_shape` 断原生入口在场，`dl_emitc_no_ocx` 断 `CoCreateInstance` / `vb6_com_<名>` 那一族
形状**不再在场**。"对话框真出现 + 取消报 32755" 需要一套起窗自关的探针，记在下一小批 C29-9b，
所以本批判据里的 `Show*` 用恒假守卫留在源码中（发码看得见、运行期不弹）。
