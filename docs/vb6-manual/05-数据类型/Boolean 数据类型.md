# Boolean 数据类型

# Boolean 数据类型

       

Boolean 变量存储为 16 位（2 个字节）的数值形式，但只能是 **True** 或是 **False**。**Boolean** 变量的值显示为 `True `或 `False`（在使用 **Print** 的时候），或者 `#TRUE#` 或 `#FALSE#`（在使用 **Write \#** 的时候）。使用关键字 **True** 与 **False** 可将 **Boolean** 变量赋值为这两个状态中的一个。

当转换其他的数值类型为 **Boolean** 值时，0 会转成 **False，**而其他的值则变成 **True**。当转换 **Boolean** 值为其他的数据类型时，**False** 成为 0，而 **True** 成为 -1。

## 本项目的实现口径

C 后端把 `As Boolean` 发成 C 的 `int16_t`，与 `As Integer` **同一个 C 类型**。因此「这是布尔」这件事不能靠 C 类型串去认，编译期另有一张按 VB 声明登记的类型表；判字符串与装箱都按它走，读数与 VB6 对齐：

| 写法 | 读数 |
| --- | --- |
| `CStr(b)` / `b & ""` / `Debug.Print b` | `True`、`False`（不是 `-1`、`0`） |
| `Format(b, "G")` | `True`、`False` |
| `VarType(b)` | `11`（`vb6_vtBoolean`，不是 Integer 的 `2`） |
| `TypeName(b)` | `Boolean` |
| `v = b`（`v As Variant`）、把 `b` 传给 `As Variant` 形参 | 装箱成 `VT_BOOL`，再 `CStr` 仍是 `True` |
| 传给 `As String` 形参 | `True`、`False` |

两条边界：

- 字面量 `True`/`False` 在 C 侧是裸的 `(-1)`/`(0)`（`int`），装箱时同样按推断结果走 `VT_BOOL`（实测 `VarType(True) = 11`、`TypeName(False) = "Boolean"`）。
- `Boolean` 与 `Integer` 的 C 类型相同，所以「装箱换了口径」这种事只可能发生在按 VB 类型分派的那几个点上。判据用例 `tests/test_bool_display.bas`（B1–B32）把 `Integer`/`Long`/`Byte` 的 `VarType`/`TypeName`/`CStr` 读数一并钉住，防止布尔的修正吃掉整数的既有行为。
- 尚未对齐的一处：`Print #` / `Write #` 的实参不分类型，一律先 `vb6_Str((int32_t)x)`，于是布尔写成 `-1`、浮点被截成整数（实测 `Print #1, d`（`d = 3.5`）落盘是 `3`）。这条不是布尔专属，另案处理，见 `ai/022` 待拍板项。
