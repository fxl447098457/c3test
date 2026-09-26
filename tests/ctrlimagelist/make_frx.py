"""生成 tests/ctrlimagelist/CtrlImageList.frx —— ImageList 设计期图片的 .frx 数据。

.frx 里 ImageList 的图片记录布局 (FrxReader::readPicture(offset, isImageList=true) 的口径):

    [ 4B totalSize ][ 16B GUID ][ 4B magic ][ 4B imgSize ][ imgSize 字节的裸 DIB ]

注意 **imgSize 后面的字节没有文件头** —— 就是 BITMAPINFOHEADER 紧接着像素数据。
(.bmp 文件才有 14 字节的 'BM' BITMAPFILEHEADER, 那是 LoadPicture 走磁盘时的形态,
两种形态 RTL 都得吃, 别混为一谈。)

本脚本同时打印各图片的偏移, 好写进 CtrlImageList.frm 的
`Picture = "CtrlImageList.frx":<偏移>`。
"""

import os
import struct

W = H = 16

# ImageList 记录里那 16 字节 GUID: comctl32/mscomctl 用的大致就是这个值,
# 我们只读 imgSize 不做校验, 填个占位 GUID 即可。
# 注意是 IPicture 的 {50493000-CCD1-1119-9112-002606097EC0}, **恰好 16 字节**。
# 少写一个 '-' 会让 bytes.fromhex 吐出 17 字节, 整条记录后移 1 字节, imgSize 落到
# base+25 而读取器在 base+24 取 → 读到大数 → "Invalid image size in .frx",
# 且**不报任何错、静默丢掉两张设计期图**。这里必须断言长度。
GUID = bytes.fromhex("50493000ccd111199112002606097ec0")
assert len(GUID) == 16, "GUID 必须是 16 字节, 现在是 %d" % len(GUID)


def bmp24(rgb):
    """返回 24bpp 自底向上的裸 DIB (BITMAPINFOHEADER + 像素, 无文件头)。"""
    row = ((W * 3 + 3) // 4) * 4          # 每行 4 字节对齐, 24bpp 16px = 48B 本来就对齐
    pixels = bytearray()
    for _ in range(H):
        pixels += bytes(rgb) * W
    hdr = struct.pack('<IiiHHIIiiII', 40, W, H, 1, 24, 0, len(pixels), 0, 0, 0, 0)
    assert len(hdr) == 40
    return hdr + bytes(pixels)


def main():
    base = os.path.dirname(os.path.abspath(__file__))
    blobs = [
        ("dt1", bmp24((0x00, 0x00, 0xFF))),   # 蓝
        ("dt2", bmp24((0x00, 0xFF, 0x00))),   # 绿
    ]
    out = bytearray()
    offsets = []
    for _, d in blobs:
        offsets.append(len(out))
        rec = struct.pack('<II', 0x3EB5C5A9, len(d)) + d   # magic + imgSize + DIB
        assert len(rec) == 4 + 4 + len(d), "记录 = magic+imgSize+DIB"
        out += struct.pack('<I', 28 + len(d))   # totalSize
        out += GUID
        out += rec
        assert (len(out) - offsets[-1]) == 28 + len(d), "记录应是 28+len(d)"
    with open(os.path.join(base, 'CtrlImageList.frx'), 'wb') as f:
        f.write(bytes(out))

    print("frx bytes:", len(out))
    for (key, _), off in zip(blobs, offsets):
        print('  %s offset = %d  (0x%X)' % (key, off, off))
    print('  .frm 里写: Picture = "CtrlImageList.frx":%08d' % offsets[0])


if __name__ == '__main__':
    main()
