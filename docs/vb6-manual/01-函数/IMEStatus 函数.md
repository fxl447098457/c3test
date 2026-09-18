# IMEStatus 函数

返回一个 Integer，用来指定当前 Microsoft Windows 的输入法 (IME) 方式；只对东亚区版本有效。

**语法**

**IMEStatus**

**返回值**

下面是日本国别的返回值：

|                             |        |                              |
|-----------------------------|--------|------------------------------|
| **常数**                    | **值** | **描述**                     |
| vbIMEModeNoControl          | 0      | 不控制IME（缺省）            |
| vbIMEModeOn                 | 1      | 打开 IME                     |
| vbIMEModeOff                | 2      | 关闭 IME                     |
| vbIMEModeDisable            | 3      | IME 无效                     |
| vbIMEModeHiragana           | 4      | 完整宽度 Hiragana 模式       |
| vbIMEModeKatakana           | 5      | 完整宽度 Katakana 片假名模式 |
| vbIMEModeKatakanaHalf mode  | 6      | 半宽 Katakana 模式           |
| vbIMEModeAlphaFull mode     | 7      | 完整宽度 Alphanumeric 模式   |
| vbIMEModeAlpha mode         | 8      | 半宽 Alphanumeric 模式       |

  

下面是韩国地区的返回值：

|                     |        |                            |
|---------------------|--------|----------------------------|
| **常数**            | **值** | **描述**                   |
| vbIMEModeAlphaFull  | 7      | 完整宽度 Alphanumeric 模式 |
| vbIMEModeAlpha      | 8      | 半宽 Alphanumeric 模式     |
| vbIMEModeHangulFull | 9      | 完整宽度 Hangul 模式       |
| vbIMEModeHangul     | 10     | 半宽 Hangul 模式           |

  

下面是中文地区的返回值：

|                    |        |                   |
|--------------------|--------|-------------------|
| **常数**           | **值** | **描述**          |
| vbIMEModeNoControl | 0      | 不控制IME（缺省） |
| vbIMEModeOn        | 1      | 打开 IME          |
| vbIMEModeOff       | 2      | 关闭 IME          |
