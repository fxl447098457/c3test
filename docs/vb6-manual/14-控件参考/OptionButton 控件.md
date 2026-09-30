# OptionButton 控件

**OptionButton** 控件显示一个可以打开或者关闭的选项。

**语法**

**OptionButton**

**说明**

在选项组中用 **OptionButton** 显示选项，用户只能选择其中的一项。在 **Frame** 控件、**PictureBox** 控件或者窗体这样的容器中绘制 **OptionButton** 控件，就可以把这些控件分组。为了在 **Frame** 或者**PictureBox** 中将 **OptionButton** 控件分组，首先绘制 **Frame** 或 **PictureBox**，然后在内部绘制 **OptionButton** 控件。同一容器中的 **OptionButton** 控件为一个组。

**OptionButton** 控件和 **CheckBox** 控件功能相似，但是二者间也存在着重要差别。在选择一个 **OptionButton** 时，同组中的其它 **OptionButton** 控件自动无效。相反，可以选择任意数量的 **CheckBox** 控件。

---

**本项目的实现现状**（实测，不是文档转述）

- 原生窗口是 `BUTTON` + `BS_AUTORADIOBUTTON`。`GotFocus` / `LostFocus` 同样要 **`BS_NOTIFY`**
  才有 `BN_SETFOCUS=6` / `BN_KILLFOCUS=7`（账 #158，2026-09-30 起两条创建路都挂）。
  夹具 `tests/btnfocus` 的 `BF-opt=1/1` 钉这一格。
- 组内方向键（`VK_UP` / `VK_DOWN`）由本项目自己派发：同一容器里所有 `BS_AUTORADIOBUTTON`
  按 **`TabIndex`** 走、到尾回绕，换选走 `BM_CLICK`（= 自动取消同组别人 + 发 `BN_CLICKED`，
  所以 `_Click` 会跟着来）。这一步之前交给你的是对话框管理器：它按 z-order 走，
  而且走到链尾会**跳出容器**（实测 `down=cmdTop1`）⇒ 那是账 #168 的由来。
- ⚠ 已知未修（账 #171）：方向键一声动目前发**两条** `_Click`（容器子类重发 + 窗体自己派发，
  与账 #161 那族同型）。夹具 `tests/tabwalk` 的 `clicks=` 只钉字段存在、不钉数值。
- 焦点进出容器里的单选钮另有一处和 VB6 不同：系统会把 `WS_TABSTOP` 自己挪到**勾选那枚**身上，
  所以「谁是站」读的是创建时存的那份 `VB6_TabStop`，不是实时样式位（账 #163/#168）。
- 同一容器里多枚 OptionButton 的互斥由原生 BUTTON 类给；`Value` 的读写与 `Caption` 那几格本批没动。
