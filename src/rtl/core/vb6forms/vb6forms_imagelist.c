// vb6forms_imagelist.c — VB6 ImageList 控件 (ai/内置控件/ImageList 控件.md)
//
// 复刻口径: 用 comctl32 的 ImageList_* API 复刻 VB6 ImageList 的等价行为,
// **不加载 mscomctl.ocx** (本机未注册, 且 32 位 inproc OCX 无法进 x64 进程)。
//
// 属性表 (照文档全量实现):
//   ListImages              图像集合 (ListImage 成员: Index / Key / Picture)
//   ImageWidth/ImageHeight  图像尺寸, 由第一张加入的图决定
// 方法:
//   ListImages.Add([index], [key], [picture])
//   ListImages.Remove(index 或 key)
//   ListImages.Clear
// 事件: 无 (纯数据容器) → 不做 WndProc 子类化。
//
// 实现要点:
//   - 存的是**真 HIMAGELIST**, 后续控件可直接喂给 TreeView_SetImageList /
//     ListView_SetImageList / SendMessage(TB_SETIMAGELIST) 等 —— 这才是"替代 OCX"的意义。
//   - comctl32 顺序语义与 VB6 一致:**第一张加入的图定尺寸**, 之后加入的被拉伸。
//   - VB6 的 Key 是字符串索引, comctl32 只认下标 → 额外维护一份平行 key 表,
//     下标与 comctl32 的图像下标严格同步 (Add 插到中间 / Remove 后同步搬动)。
//   - ImageList 无窗口, 生成的 C 里它就是个 void* 槽 (沿用 vb6_com_<Name> 那个变量名),
//     由 vb6_ImageList_Create 在**调用前** malloc, 之后各函数直接拿这个指针用,
//     **不走 SetPropW** —— 槽里放的不是一个 HWND, 拿它当 HWND 用纯属自欺。

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commctrl.h>
#endif

#include "vb6forms.h"
#include "vb6forms_internal.h"
#include "vb6rtl_bstr.h"
#include "vb6rtl_variant.h"     /* vb6_VARIANT (vb6rtl_class_com.h 也吃它, 先来一步) */
#include "vb6rtl_class_com.h"   /* vb6_LoadPictureEx / vb6_PictureHandleOf / vb6_ReleasePicture */
#include <oleauto.h>   /* BSTR /_sysfree_string 等 */
#include <olectl.h>    /* IPicture, OleLoadPicture, OLE_HANDLE */

#ifdef _WIN32

// ===================== ImageList 实例 =====================
// 除了 comctl32 的下标, 每张图还挂两样 VB6 语义的东西: Key (字符串索引) 和它背后
// **活着的 IPicture** (VB6 的 ListImage.Picture)。两者都 0 基、与 himl 下标严格平行。
// picture 这一列不是可选项: VB6 的 StdPicture 由 ListImage 持有, 提前 Release 会让
// 句柄语义变得不可预期。

typedef struct {
    wchar_t* key;   // 未命名则 NULL
    void*    pic;   // AddRef 过的 IPicture*; 与 key 同生命周期
} Vb6ImageEntry;

typedef struct {
    HIMAGELIST himl;         // comctl32 真句柄
    Vb6ImageEntry* ents;
    int       count;
    int       cap;
    int       w, h;          // ImageWidth / ImageHeight
} Vb6ImageList;

static void IlPicRelease(void* pic) {
    if (pic) vb6_ReleasePicture(pic);
}

static void IlEntClear(Vb6ImageEntry* e) {
    if (!e) return;
    free(e->key); e->key = NULL;
    IlPicRelease(e->pic); e->pic = NULL;
}

static void IlEntsFree(Vb6ImageList* il) {
    if (!il) return;
    for (int i = 0; i < il->count; i++) IlEntClear(&il->ents[i]);
    free(il->ents);
    il->ents = NULL; il->count = 0; il->cap = 0;
}

// 保证容量足够容纳 need 项; 失败返回 -1。
static int IlReserve(Vb6ImageList* il, int need) {
    if (need <= il->cap) return 0;
    int ncap = il->cap ? il->cap : 8;
    while (ncap < need) ncap *= 2;
    Vb6ImageEntry* ne = (Vb6ImageEntry*)realloc(il->ents, (size_t)ncap * sizeof(Vb6ImageEntry));
    if (!ne) return -1;
    for (int i = il->cap; i < ncap; i++) { ne[i].key = NULL; ne[i].pic = NULL; }
    il->ents = ne; il->cap = ncap;
    return 0;
}

static int IlPushTail(Vb6ImageList* il) {
    if (IlReserve(il, il->count + 1) != 0) return -1;
    il->ents[il->count].key = NULL;
    il->ents[il->count].pic = NULL;
    il->count++;
    return 0;
}

// 在 pos 处插入 (后面的整体后移), `Add index, key` 的语义。
// pic 是**已 AddRef** 的对象, 所有权直接转进来。
static int IlInsertAt(Vb6ImageList* il, int pos, const wchar_t* key, void* pic) {
    if (pos < 0 || pos > il->count) return -1;
    if (IlReserve(il, il->count + 1) != 0) return -1;
    for (int i = il->count; i > pos; i--) il->ents[i] = il->ents[i - 1];
    il->ents[pos].key = key && key[0] ? _wcsdup(key) : NULL;
    il->ents[pos].pic = pic;
    il->count++;
    return pos;
}

static void IlRemoveAt(Vb6ImageList* il, int idx) {
    if (idx < 0 || idx >= il->count) return;
    if (idx + 1 < il->count) {
        memmove(&il->ents[idx], &il->ents[idx + 1],
                (size_t)(il->count - idx - 1) * sizeof(Vb6ImageEntry));
        // 尾巴上这份已经被搬到 ents[idx] 了, 只留个空壳 —— **绝对不能再 IlEntClear 一遍**。
        // 那些 key/pic 指针的所有权已经归搬到的位置, 再 free/Release 一次就是
        // 双重释放: key 是堆字符串会毁堆, picture 是 AddRef 过的 IPicture 会被提前销毁。
        memset(&il->ents[il->count - 1], 0, sizeof(Vb6ImageEntry));
    } else {
        IlEntClear(&il->ents[idx]);   // 删的正是最后一项, 直接清空即可
    }
    il->count--;
}

static int IlFindKey(Vb6ImageList* il, const wchar_t* key) {
    if (!key || !key[0]) return -1;
    for (int i = 0; i < il->count; i++) {
        if (il->ents[i].key && _wcsicmp(il->ents[i].key, key) == 0) return i;
    }
    return -1;
}

// ===================== 图片字节 → HBITMAP =====================
// 三种输入都能吃:
//   ① 'BM' + BITMAPFILEHEADER(14B) + DIB        —— LoadPicture("x.bmp") 给的就是这种
//   ② 裸 DIB (直接 BITMAPINFOHEADER 起头)        —— **.frx 存的图片就是这个形态**
//   ③ 其它 (JPEG/GIF/PNG/ICO/EMF/WMF)            —— 只能走 OLE IPicture 兜底
// 注意不能用 LoadImageW —— 它对内存缓冲无效 (只吃文件/资源句柄)。
static DWORD IlDibBitsOffset(const BITMAPINFOHEADER* bh) {
    DWORD c = bh->biClrUsed ? bh->biClrUsed : (DWORD)(1u << bh->biBitCount);
    if (bh->biBitCount > 8) c = 0;                       // 无调色板
    if (bh->biCompression == BI_BITFIELDS) c += 3;        // 3 个 RGBTRIPLE (12B)
    return (DWORD)bh->biSize + c * 4;
}

// 粗判"这串字节像不像一个尺寸自洽的裸 DIB": 头是 40 字节版, 宽高和体积能对得上。
static int IlLooksLikeDib(const unsigned char* d, int size) {
    if (size < 40) return 0;
    const BITMAPINFOHEADER* bh = (const BITMAPINFOHEADER*)(const void*)d;
    if (bh->biSize != 40) return 0;
    LONG h = bh->biHeight < 0 ? -bh->biHeight : bh->biHeight;   // top-down 是负高度
    if (bh->biWidth <= 0 || bh->biWidth > 8192 || h <= 0 || h > 8192) return 0;
    DWORD px = (DWORD)h * (DWORD)bh->biWidth * ((DWORD)bh->biBitCount + 7) / 8;
    DWORD bmp = bh->biSizeImage ? bh->biSizeImage : px;
    LONG need = (LONG)bh->biSize + (LONG)IlDibBitsOffset(bh) - 40 + (LONG)bmp;
    return size >= need && size <= need + 4096;   // 尾部允许少量填充
}

static HBITMAP IlBitmapFromMem(const void* data, int size) {
    if (!data || size <= 0) return NULL;
    const unsigned char* d = (const unsigned char*)data;
    const BITMAPINFOHEADER* bh = NULL;
    const void* bits = NULL;

    if (size >= 54 && d[0] == 'B' && d[1] == 'M') {              // ① 带 14B 文件头
        bh = (const BITMAPINFOHEADER*)(const void*)(d + 14);
        if (bh->biSize >= 40) bits = d + 14 + bh->biSize;
    } else if (IlLooksLikeDib(d, size)) {                        // ② 裸 DIB (.frx)
        bh = (const BITMAPINFOHEADER*)(const void*)d;
        bits = d + IlDibBitsOffset(bh);
    }

    if (bh && bits) {
        HDC hdc = GetDC(NULL);
        HBITMAP hbmp = CreateDIBitmap(hdc, bh, CBM_INIT, bits, bh, DIB_RGB_COLORS);
        ReleaseDC(NULL, hdc);
        if (hbmp) return hbmp;
    }

    // ③ 兜底: OleLoadPicture 是唯一能救 JPEG/GIF/PNG/ICO/EMF/WMF 的路。
    void* pic = vb6_LoadPictureFromMemory(data, size);
    if (!pic) return NULL;
    IPicture* pPic = (IPicture*)pic;
    HANDLE h = NULL;
    if (SUCCEEDED(pPic->lpVtbl->get_Handle(pPic, &h)) && h) return (HBITMAP)h;
    pPic->lpVtbl->Release(pPic);
    return NULL;
}

// ===================== 对外 API =====================

// 创建: 先按 ImageWidth/ImageHeight 建空表, 之后由第一张图定最终尺寸 (与 VB6 一致)。
// slot 是生成代码里 `static void* vb6_com_<Name>` 的**地址**, 这里把实例写进去。
void vb6_ImageList_Create(void** slot, int32_t width, int32_t height) {
    if (!slot) return;
    Vb6ImageList* il = (Vb6ImageList*)calloc(1, sizeof(Vb6ImageList));
    if (!il) return;

    il->w = width  > 0 ? width  : 16;    // VB6 ImageList 默认 16x16
    il->h = height > 0 ? height : 16;

    il->himl = ImageList_Create(il->w, il->h, ILC_COLOR32 | ILC_MASK, 1, 8);
    if (!il->himl) { free(il); return; }

    *slot = il;
}

void vb6_ImageList_Destroy(void* slot) {
    Vb6ImageList* il = (Vb6ImageList*)slot;
    if (!il) return;
    if (il->himl) ImageList_Destroy(il->himl);
    IlEntsFree(il);
    free(il);
}

int32_t vb6_GetImageListImageWidth(void* slot) {
    Vb6ImageList* il = (Vb6ImageList*)slot;
    return il ? (int32_t)il->w : 0;
}
void vb6_SetImageListImageWidth(void* slot, int32_t val) {
    Vb6ImageList* il = (Vb6ImageList*)slot;
    if (!il || val <= 0) return;   // VB6: 已加图后改尺寸会报错, 这里静默忽略
    il->w = val;
}
int32_t vb6_GetImageListImageHeight(void* slot) {
    Vb6ImageList* il = (Vb6ImageList*)slot;
    return il ? (int32_t)il->h : 0;
}
void vb6_SetImageListImageHeight(void* slot, int32_t val) {
    Vb6ImageList* il = (Vb6ImageList*)slot;
    if (!il || val <= 0) return;
    il->h = val;
}

// 把一张已经是 HBITMAP/HICON 的图塞进去并同步 key 表, 返回 VB6 的 1 基 Index (失败 0)。
// pos<0 = 追加; pos>=0 = 插到该位置并把后面的往后推 (VB6 `Add index, key, picture`)。
// keepPic 是**已 AddRef** 的 IPicture (可为 NULL = 设计期字节烤出来的位图), 会挂在本条上。
static int32_t IlAddHandle(Vb6ImageList* il, int pos, const wchar_t* key, HANDLE hpic,
                           int isIcon, void* keepPic) {
    if (!il || !il->himl || !hpic) return 0;

    // VB6: index 越界 (大于当前数量) 退化为追加 —— 与 comctl32 的钳位行为一致。
    int have = ImageList_GetImageCount(il->himl);
    if (pos > have) pos = have;

    int idx = isIcon ? ImageList_AddIcon(il->himl, (HICON)hpic)
                     : ImageList_Add(il->himl, (HBITMAP)hpic, NULL);
    if (idx < 0) return 0;

    // key/pic 表的维护: 插到中间就插入+后移; 追加就挂尾; 落到已有下标上则原地替换。
    if (pos >= 0 && pos < have) {
        if (IlInsertAt(il, pos, key, keepPic) < 0) return 0;
    } else if (idx >= il->count) {
        if (IlPushTail(il) < 0) return 0;
        il->ents[il->count - 1].key = key && key[0] ? _wcsdup(key) : NULL;
        il->ents[il->count - 1].pic = keepPic;
    } else {
        IlEntClear(&il->ents[idx]);   // 原地替换: 旧 key/picture 一起放掉
        il->ents[idx].key = key && key[0] ? _wcsdup(key) : NULL;
        il->ents[idx].pic = keepPic;
    }
    return idx + 1;   // VB6 Index 1 基
}

// ListImages.Add([index], [key], [picture]) —— picture 就是 **LoadPicture() 的返回值**
// (VB6 的 StdPicture = 活着的 IPicture)。要注意不能直接拿 get_Handle 得到的句柄当
// "用完即弃的 HBITMAP": VB6 语义下 ListImage 持有这份 picture, 这里照做 ——
// AddRef 一份存在 ents[i].pic 里, 等 Remove/Clear/Destroy 时再 Release。
int32_t vb6_ImageList_AddPicture(void* slot, int32_t index, const wchar_t* key,
                                 void* picture) {
    Vb6ImageList* il = (Vb6ImageList*)slot;
    if (!il || !picture) return 0;

    // 类型判别走 IPicture::Type (PICTTYPE: 1=BITMAP 2=METAFILE 3=ICON),
    // 比 GetObjectType 问 GDI 可靠 —— 图标文件和图标化的位图都能分清。
    int32_t kind = 1;
    void* hpic = vb6_PictureHandleOf(picture, &kind);
    if (!hpic) return 0;

    IPicture* p = (IPicture*)picture;
    p->lpVtbl->AddRef(p);
    int pos = (index > 0) ? (int)index - 1 : -1;
    int32_t r = IlAddHandle(il, pos, key, hpic, kind == 3, p);
    if (r == 0) vb6_ReleasePicture(p);   // Add 失败, 把刚加的引用还回去, 别泄漏
    return r;
}

// 设计期: cgen 已把 .frx 里的图片烤成 hex 字节数组, 直接灌进去。
int32_t vb6_ImageList_AddDesignTimeImage(void* slot, const wchar_t* key,
                                         const void* data, int32_t size) {
    Vb6ImageList* il = (Vb6ImageList*)slot;
    if (!il || !il->himl || !data || size <= 0) return 0;
    HBITMAP hbmp = IlBitmapFromMem(data, (int)size);
    if (!hbmp) return 0;
    int32_t r = IlAddHandle(il, -1, key, hbmp, 0, NULL);
    DeleteObject(hbmp);   // 临时位图已经拷进 himl, 加完就可以销毁
    return r;
}

// ListImages.Remove 的数字形态 (VB6 也吃 `Remove 1`)
void vb6_ImageList_RemoveAtIndex(void* slot, int32_t index) {
    vb6_ImageList_RemoveImage(slot, NULL, index);
}

// ListImages.Remove(index 或 key) —— VB6 两种都接受。
void vb6_ImageList_RemoveImage(void* slot, const wchar_t* keyOrIndex, int32_t index) {
    Vb6ImageList* il = (Vb6ImageList*)slot;
    if (!il || !il->himl) return;

    int idx = -1;
    if (keyOrIndex && keyOrIndex[0]) {
        idx = IlFindKey(il, keyOrIndex);
        if (idx < 0) idx = _wtoi(keyOrIndex);   // 纯数字字符串按下标
    }
    if (idx < 0 && index > 0) idx = (int)index - 1;
    if (idx < 0) return;

    ImageList_Remove(il->himl, idx);
    IlRemoveAt(il, idx);
}

void vb6_ImageList_ClearImages(void* slot) {
    Vb6ImageList* il = (Vb6ImageList*)slot;
    if (!il || !il->himl) return;
    ImageList_RemoveAll(il->himl);
    IlEntsFree(il);
}

int32_t vb6_GetImageListCount(void* slot) {
    Vb6ImageList* il = (Vb6ImageList*)slot;
    return (il && il->himl) ? (int32_t)ImageList_GetImageCount(il->himl) : 0;
}

// ListImage.Index (1 基; 越界返回 0)
// 注意**:生成代码传进来的就是 VB6 的 1 基 Index** (`ListImages(2).Index` 发
// `vb6_ImageListIndexAt(slot, 2)`), 所以这里第一步减一, 出口原样返回 1 基。
int32_t vb6_ImageListIndexAt(void* slot, int32_t index) {
    Vb6ImageList* il = (Vb6ImageList*)slot;
    int i = (int)index - 1;
    if (!il || i < 0 || i >= il->count) return 0;
    return index;
}

// ListImage.Key (传 1 基 Index)
void* vb6_GetImageListKeyAt(void* slot, int32_t index) {
    Vb6ImageList* il = (Vb6ImageList*)slot;
    int i = (int)index - 1;
    if (!il || i < 0 || i >= il->count || !il->ents[i].key) return NULL;
    return (void*)vb6_BSTR_FromStr(il->ents[i].key);
}

// ListImages("KeyString") —— VB6 允许**按 Key 取项**, 生成的表达式里 Item 的实参是个
// 宽字符串字面量而不是下标。这里按下标查不着就退化成按 Key 查 (VB6 也是这个优先级)。
void* vb6_GetImageListKeyByKey(void* slot, const wchar_t* key) {
    Vb6ImageList* il = (Vb6ImageList*)slot;
    if (!il || !key || !key[0]) return NULL;
    int i = IlFindKey(il, key);
    if (i < 0) return NULL;
    return (void*)vb6_BSTR_FromStr(il->ents[i].key);
}

// ListImages("KeyString").Index —— 同上, 按 Key 找到后返回 1 基 Index
int32_t vb6_ImageListIndexByKey(void* slot, const wchar_t* key) {
    Vb6ImageList* il = (Vb6ImageList*)slot;
    if (!il || !key || !key[0]) return 0;
    int i = IlFindKey(il, key);
    return i < 0 ? 0 : i + 1;
}

// 给后续控件 (Toolbar/TreeView/ListView) 拿真 HIMAGELIST。
void* vb6_GetImageListHandle(void* slot) {
    Vb6ImageList* il = (Vb6ImageList*)slot;
    return il ? (void*)il->himl : NULL;
}

#endif  /* _WIN32 */
