# -*- coding: utf-8 -*-
"""重新生成 FrxData.frx (Fix 195 回归夹具的资源文件)。

VB6 的行式设计器格式存不下多行文本, 于是把这类属性值甩进同名 .frx, .frm 里只留
`属性 = "FrxData.frx":偏移`。实测到的三种 blob 布局:

  字符串 (Text/Caption):  [1B 长度][ANSI/GBK 文本]
                          (兼容形式: [dword 长度][...]; readText 两条都认)
  List:                   [2B count][2B 前导值][每项: 2B 长度 + ANSI/GBK 文本]
  ItemData:               与 List **同构**, 每项是十进制文本 —— 注意不是"2B 整数数组",
                          旧 readIntList 就是按后者读的, 解出恒定的结构字节假值。

用法: python make_frx.py           (在 tests/frxdata 目录下执行, 覆盖 FrxData.frx)
"""

import struct

GBK = lambda s: s.encode("gbk")


def str_blob(text: str) -> bytes:
    """字符串属性: [1B 长度][GBK 文本]"""
    raw = GBK(text)
    assert len(raw) < 256
    return bytes([len(raw)]) + raw


def list_blob(items):
    """列表属性 (List / ItemData): [2B count][2B count][每项: 2B 长度 + 文本]"""
    out = struct.pack("<HH", len(items), len(items))
    for it in items:
        raw = GBK(it)
        out += struct.pack("<H", len(raw)) + raw
    return out


def main():
    itemdata = list_blob(["5", "300", "-7"])
    lst = list_blob(["1234", "4567", "你好"])
    text = str_blob("alpha\r\nbeta")

    off_itemdata = 0
    off_list = off_itemdata + len(itemdata)
    off_text = off_list + len(lst)

    with open("FrxData.frx", "wb") as f:
        f.write(itemdata)
        f.write(lst)
        f.write(text)

    print("FrxData.frx: ItemData@0x%04X List@0x%04X Text@0x%04X (共 %d 字节)"
          % (off_itemdata, off_list, off_text, off_text + len(text)))
    print("把 .frm 里的引用改成:  ItemData = \"FrxData.frx\":%04X" % off_itemdata)
    print("                      List     = \"FrxData.frx\":%04X" % off_list)
    print("                      Text     = \"FrxData.frx\":%04X" % off_text)


if __name__ == "__main__":
    main()
