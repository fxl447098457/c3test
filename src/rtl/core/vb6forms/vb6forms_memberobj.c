// vb6forms_memberobj.c — 控件"成员对象 / 成员集合"的原生 IDispatch 实现 (ai/029 C29-3)
//
// 背景: VB6 里 ListView1.ListItems.Add(..) / TreeView1.Nodes.Add(..) / StatusBar1.Panels(i)
// 返回的是**对象**: 可以 `Set itm = ...` 接住再 itm.Text / itm.Key 访问成员;
// `For Each n In Nodes` 还要能枚举 —— 集合必须可枚举, 元素必须是对象。
//
// 计划书 D1 的口径 = **走真 IDispatch**:
//   ① `As Object` 的晚绑定 (vb6_ComGetProp / vb6_ComSetProp / vb6_ComCallObject 那一套)
//      直接生效, cgen 侧几乎零改动;
//   ② 不用为每个成员属性在 cgen 里再长一条字符串级特例 —— 历史上 vb6_CStr 那一族
//      长出一百多个 Fix, 正是"特例互相打脸"的形状;
//   ③ 顺带把 022/D71 "工程内类交给 As Object 点不到成员" 那个缺口需要的同一套建起来。
//
// 本文件是**立样**: 只挂 ImageList 两族 (ListImages 集合 / ListImage 成员)。
// 后面 ListView / TreeView / StatusBar / Toolbar 按同一形状继续加 kind + 名字表 + 一段
// get/set 即可, 不必再动通道本身。
//
// 骨架照 vb6forms_axcontainer.c (那份也把 name→dispid 表变成了真 IDispatch)。
//
// 两条容易踩的口径:
//   - **1 基**: VB6 集合与 ListImage.Index 都是 1 基, 而 RTL 里 comctl32 那层是 0 基。
//     本文件对外一律 1 基, 内部换算集中在下标解析处。
//   - **`_NewEnum` 的 DISPID 是 -4** (OLE 约定), 不在名字表的自然序号里 ——
//     vb6_ForEach_Init 是**硬发 -4** 的, 名字表里那一格只是给 GetIDsOfNames 兜底。

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#include "vb6forms.h"
#include "vb6forms_internal.h"
#include "vb6forms_prop_ctrl.h"   /* vb6_GetImageListCount / vb6_ImageList_* 原型 */
#include "vb6rtl_bstr.h"
#include "vb6rtl_variant.h"
#include "vb6rtl_class_com.h"
#include <oleauto.h>
#include <olectl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32

// ===================== 族号 =====================
// 成员对象族 (Vb6MemObj)
#define VB6_MEMK_LISTIMAGE     1
// 成员集合族 (Vb6MemColl)
#define VB6_MEMCK_LISTIMAGES   1

// ===================== 实例 =====================
typedef struct Vb6MemObj {
    IDispatchVtbl* lpVtbl;
    LONG      ref;
    int32_t   kind;
    void*     owner;    // 宿主控件的 RTL 槽 (ImageList = vb6_com_<Name> 里那个实例指针)
    int32_t   index;    // 1 基
} Vb6MemObj;

typedef struct Vb6MemColl {
    IDispatchVtbl* lpVtbl;
    LONG      ref;
    int32_t   kind;
    void*     owner;
} Vb6MemColl;

// 集合的可枚举器 (IEnumVARIANT)。For Each 走的就是它。
typedef struct Vb6MemEnum {
    IEnumVARIANTVtbl* lpVtbl;
    LONG      ref;
    int32_t   kind;     // 集合族号 (决定产出什么成员对象)
    void*     owner;
    int32_t   total;    // 枚举开始时快照的 count
    int32_t   pos;      // 下一个要产出的 1 基下标
} Vb6MemEnum;

static const IDispatchVtbl g_memObjVtbl;
static const IDispatchVtbl g_memCollVtbl;
static const IEnumVARIANTVtbl g_memEnumVtbl;

// ===================== 名字表 (DISPID = 下标 + 1) =====================
static const wchar_t* kListImageNames[] = { L"Key", L"Index", L"Picture", NULL };
static const wchar_t* kListImagesNames[] = {
    L"Count", L"Item", L"Add", L"Remove", L"Clear", L"_NewEnum", NULL };

// DISPID 明细 (与上面两张表的顺序严格一致)
#define VB6_MEMD_KEY        1
#define VB6_MEMD_INDEX      2
#define VB6_MEMD_PICTURE    3

#define VB6_MEMCD_COUNT     1
#define VB6_MEMCD_ITEM      2
#define VB6_MEMCD_ADD       3
#define VB6_MEMCD_REMOVE    4
#define VB6_MEMCD_CLEAR     5
#define VB6_MEMCD_NEWENUM   6

#define VB6_DISPID_NEWENUM  (-4)   // OLE 约定: _NewEnum

static int memLookup(const wchar_t* const* tbl, const wchar_t* name) {
    if (!name) return 0;
    for (int i = 0; tbl[i]; i++)
        if (_wcsicmp(tbl[i], name) == 0) return i + 1;
    return 0;
}

// C29-3 临时排障探针 (C3_MEMOBJ_TRACE=1 打开)。x86 上"对象被过早释放"会立刻踩到,
// x64 却常常侥幸不崩 —— 定位 use-after-free 只有把 new/Release 打出来才看得见。
static int memTrace(void) {
    static int c = -1;
    if (c < 0) c = (GetEnvironmentVariableW(L"C3_MEMOBJ_TRACE", NULL, 0) > 0) ? 1 : 0;
    return c;
}

// ===================== VARIANT 写出助手 =====================
// ⚠ 这里必须用 **Windows 的 VARIANT / VT_\***, 不能按 vb6_VARIANT 写:
// IDispatch::Invoke 的 result 是调用方按 Windows VARIANT 分配的 (vb6_ComGetProp 就是
// `calloc(1, sizeof(VARIANT))`), 而 `sizeof(vb6_VARIANT)` 在 x86 下是 **24**、
// Windows VARIANT 只有 **16** ⇒ 按 vb6_VARIANT 去 memset 会**越界 8 字节**, 堆当场被
// 写坏。症状是"崩溃点到处漂移"(同一份用例这次崩在 MO2、下次崩在 MO7), 极难归因。
// x64 下两者恰好都是 24 ⇒ 只在 32 位暴露 —— 这类"只在 x86 炸"的根因基本都在宽度假设上。
static void memSetEmpty(VARIANT* o) { VariantInit(o); }
static void memSetI4(VARIANT* o, int32_t v) {
    VariantInit(o); o->vt = VT_I4; o->lVal = v;
}
static void memSetStr(VARIANT* o, const wchar_t* s) {
    VariantInit(o); o->vt = VT_BSTR;
    o->bstrVal = SysAllocString(s ? s : L"");
}
static void memSetObj(VARIANT* o, void* iface) {
    VariantInit(o); o->vt = VT_DISPATCH; o->pdispVal = (void*)iface;
}

// ===================== 构造 =====================
static Vb6MemObj* memObjNew(int32_t kind, void* owner, int32_t index) {
    Vb6MemObj* p = (Vb6MemObj*)calloc(1, sizeof(Vb6MemObj));
    if (!p) return NULL;
    p->lpVtbl = &g_memObjVtbl;
    p->ref = 1;
    p->kind = kind;
    p->owner = owner;
    p->index = index;
    if (memTrace()) fprintf(stderr, "[MOBJ] new obj   %p kind=%d idx=%d\n", (void*)p, (int)kind, (int)index);
    return p;
}

static Vb6MemColl* memCollNew(int32_t kind, void* owner) {
    Vb6MemColl* p = (Vb6MemColl*)calloc(1, sizeof(Vb6MemColl));
    if (!p) return NULL;
    p->lpVtbl = &g_memCollVtbl;
    p->ref = 1;
    p->kind = kind;
    p->owner = owner;
    if (memTrace()) fprintf(stderr, "[MOBJ] new coll  %p\n", (void*)p);
    return p;
}

static Vb6MemEnum* memEnumNew(int32_t kind, void* owner) {
    Vb6MemEnum* p = (Vb6MemEnum*)calloc(1, sizeof(Vb6MemEnum));
    if (!p) return NULL;
    p->lpVtbl = &g_memEnumVtbl;
    p->ref = 1;
    p->kind = kind;
    p->owner = owner;
    p->pos = 1;
    // count 在**开始枚举那一刻**快照: VB6 的 For Each 也是在进入循环时定下集合的
    // 成员 (循环体里 Remove 不会让枚举器跳过/越界)。RTL 侧 count 只增不减地读。
    p->total = (kind == VB6_MEMCK_LISTIMAGES) ? vb6_GetImageListCount(owner) : 0;
    if (memTrace()) fprintf(stderr, "[MOBJ] new enum  %p total=%d\n", (void*)p, (int)p->total);
    return p;
}

// ===================== IUnknown (三个 vtable 共用一套实现) =====================
static HRESULT STDMETHODCALLTYPE mem_QI(IUnknown* This, REFIID riid, void** ppv) {
    if (!ppv) return E_POINTER;
    *ppv = NULL;
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IDispatch)) {
        *ppv = This; This->lpVtbl->AddRef(This); return S_OK;
    }
    // 枚举器另认 IEnumVARIANT —— For Each 那条路就是这么拿到的。
    if (IsEqualIID(riid, &IID_IEnumVARIANT)) {
        // 只有枚举器实例能给出 IEnumVARIANT; 其余返回 E_NOINTERFACE。
        // 判据: 头部 vtable 指针与本文件里那个枚举 vtable 相符。
        if (This->lpVtbl == (IUnknownVtbl*)&g_memEnumVtbl) {
            *ppv = This; This->lpVtbl->AddRef(This); return S_OK;
        }
    }
    return E_NOINTERFACE;
}
static ULONG STDMETHODCALLTYPE mem_AddRef(IUnknown* This) {
    return (ULONG)InterlockedIncrement(&((Vb6MemObj*)This)->ref);
}
static ULONG STDMETHODCALLTYPE mem_Release(IUnknown* This) {
    LONG r = InterlockedDecrement(&((Vb6MemObj*)This)->ref);
    if (memTrace()) fprintf(stderr, "[MOBJ] release %p -> ref=%ld\n", (void*)This, (long)r);
    if (r <= 0) free(This);
    return (ULONG)r;
}

// ===================== GetTypeInfo 三件套 =====================
static HRESULT STDMETHODCALLTYPE mem_GetTypeInfoCount(IDispatch* This, UINT* p) {
    (void)This; if (p) *p = 0; return S_OK;
}
static HRESULT STDMETHODCALLTYPE mem_GetTypeInfo(IDispatch* This, UINT i, LCID l, ITypeInfo** p) {
    (void)This; (void)i; (void)l; if (p) *p = NULL; return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE memObj_GetIDsOfNames(IDispatch* This, REFIID riid, LPOLESTR* names,
                                                     UINT cNames, LCID lcid, DISPID* dispids) {
    (void)This; (void)riid; (void)lcid;
    if (!names || !dispids) return E_POINTER;
    for (UINT i = 0; i < cNames; i++) {
        int d = memLookup(kListImageNames, names[i]);
        if (memTrace()) fprintf(stderr, "[MOBJ] obj.gidn this=%p '%ls' -> %d\n",
                                (void*)This, names[i] ? names[i] : L"?", d);
        dispids[i] = (DISPID)(d ? d : DISPID_UNKNOWN);
    }
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE memColl_GetIDsOfNames(IDispatch* This, REFIID riid, LPOLESTR* names,
                                                      UINT cNames, LCID lcid, DISPID* dispids) {
    (void)This; (void)riid; (void)lcid;
    if (!names || !dispids) return E_POINTER;
    for (UINT i = 0; i < cNames; i++) {
        // `_NewEnum` 要走 OLE 的固定 DISPID -4 (vb6_ForEach_Init 是硬发 -4 的),
        // 不能给名字表里的序号 —— 给了它 Invoke 也认 -4, 但别的调用方 (编号路径)
        // 拿到 6 会当成第 6 个成员, 两边对不上。
        if (names[i] && _wcsicmp(names[i], L"_NewEnum") == 0) {
            dispids[i] = (DISPID)VB6_DISPID_NEWENUM;
            continue;
        }
        int d = memLookup(kListImagesNames, names[i]);
        dispids[i] = (DISPID)(d ? d : DISPID_UNKNOWN);
    }
    return S_OK;
}

// ===================== Invoke 参数/返回值工具 =====================
// vb6_ComCall 把**位置参数逆序**放: rgvarg[cArgs-1-i] 才是第 i 个实参。
static VARIANT* memArg(DISPPARAMS* dp, int pos) {
    if (!dp || !dp->rgvarg || pos < 0 || pos >= (int)dp->cArgs) return NULL;
    return &dp->rgvarg[dp->cArgs - 1 - pos];
}

static int32_t memArgIsMissing(VARIANT* v) {
    return (!v || v->vt == VT_ERROR || v->vt == VT_EMPTY);
}

// 实参当成"下标或 Key"解析。VB6 的 Item/Remove 两种都吃。
// 返回: >0 = 1 基下标; 0 = 没给/不认识; -1 = 给的是字符串 (key 见 outKey 出参)。
static int32_t memParseIndexOrKey(VARIANT* v, const wchar_t** outKey) {
    if (outKey) *outKey = NULL;
    if (!v) return 0;
    if (v->vt == VT_BSTR) {
        if (outKey) *outKey = v->bstrVal;
        return -1;
    }
    if (v->vt == VT_I4 || v->vt == VT_I2 || v->vt == VT_UI4 || v->vt == VT_INT) {
        return (int32_t)v->lVal;
    }
    if (v->vt == VT_R8 || v->vt == VT_R4) {
        return (int32_t)v->dblVal;
    }
    if (v->vt == VT_BOOL) {
        return (int32_t)v->boolVal;
    }
    return 0;
}

// 从 Item/Remove 的实参解出 1 基下标; key 形态查表换算。找不到返回 0。
static int32_t memOwnerIndexFromArg(void* owner, int32_t kind, VARIANT* v) {
    const wchar_t* key = NULL;
    int32_t r = memParseIndexOrKey(v, &key);
    if (r > 0) return r;
    if (r < 0 && key && *key) {
        if (kind == VB6_MEMCK_LISTIMAGES) {
            int32_t n = vb6_ImageListIndexByKey(owner, key);
            return (n > 0) ? n : 0;
        }
    }
    return 0;
}

// ===================== 成员对象: ListImage =====================
static HRESULT STDMETHODCALLTYPE memObj_Invoke(IDispatch* This, DISPID dispid, REFIID riid, LCID lcid,
                                               WORD flags, DISPPARAMS* dp, VARIANT* result,
                                               EXCEPINFO* ei, UINT* ae) {
    (void)riid; (void)lcid; (void)dp; (void)ei; (void)ae;
    Vb6MemObj* p = (Vb6MemObj*)This;
    if (memTrace()) fprintf(stderr, "[MOBJ] obj.invoke this=%p dispid=%d idx=%d\n",
                            (void*)This, (int)dispid, (int)p->index);
    if (!result) return S_OK;
    VARIANT* out = result;
    memSetEmpty(out);
    if (p->kind != VB6_MEMK_LISTIMAGE) return DISP_E_MEMBERNOTFOUND;

    // 属性取 (DISPATCH_PROPERTYGET) 与"不带参数的默认取"都走这里;
    // 赋值 (PROPERTYPUT) 先不做 —— ListImage 的三个成员在 VB6 里都是只读。
    switch ((int)dispid) {
    case VB6_MEMD_KEY:
        memSetStr(out, (const wchar_t*)vb6_GetImageListKeyAt(p->owner, p->index));
        return S_OK;
    case VB6_MEMD_INDEX:
        memSetI4(out, p->index);
        return S_OK;
    case VB6_MEMD_PICTURE: {
        void* pic = vb6_ImageList_PictureAt(p->owner, p->index);
        if (pic) memSetObj(out, pic);
        else memSetEmpty(out);
        return S_OK;
    }
    default:
        break;
    }
    return DISP_E_MEMBERNOTFOUND;
}

// ===================== 成员集合: ListImages =====================
static HRESULT STDMETHODCALLTYPE memColl_Invoke(IDispatch* This, DISPID dispid, REFIID riid, LCID lcid,
                                                WORD flags, DISPPARAMS* dp, VARIANT* result,
                                                EXCEPINFO* ei, UINT* ae) {
    (void)riid; (void)lcid; (void)flags; (void)ei; (void)ae;
    Vb6MemColl* p = (Vb6MemColl*)This;
    if (!result) return S_OK;
    VARIANT* out = result;
    memSetEmpty(out);
    if (p->kind != VB6_MEMCK_LISTIMAGES) return DISP_E_MEMBERNOTFOUND;

    void* owner = p->owner;

    // _NewEnum: 交给 For Each。硬发 -4。
    if (dispid == (DISPID)VB6_DISPID_NEWENUM) {
        Vb6MemEnum* e = memEnumNew(p->kind, owner);
        if (!e) return E_OUTOFMEMORY;
        memSetObj(out, e);
        return S_OK;
    }

    switch ((int)dispid) {
    case VB6_MEMCD_COUNT:
        memSetI4(out, vb6_GetImageListCount(owner));
        return S_OK;

    case VB6_MEMCD_ITEM: {
        // Item(i) / Item("key") —— 越界/找不到返回 Nothing (VB6 也是 Nothing)。
        int32_t idx = memOwnerIndexFromArg(owner, p->kind, memArg(dp, 0));
        if (idx <= 0) { memSetEmpty(out); return S_OK; }
        Vb6MemObj* o = memObjNew(VB6_MEMK_LISTIMAGE, owner, idx);
        if (!o) return E_OUTOFMEMORY;
        memSetObj(out, o);
        return S_OK;
    }

    case VB6_MEMCD_ADD: {
        // Add([index], [key], [picture]) —— VB6 的 Add **返回 ListImage 对象**,
        // 这就是 C29-3 那条 `Set img = ...Add(...)` 的落点。
        VARIANT* a0 = memArg(dp, 0);   // index
        VARIANT* a1 = memArg(dp, 1);   // key
        VARIANT* a2 = memArg(dp, 2);   // picture
        int32_t idx = 0;
        if (!memArgIsMissing(a0)) {
            int32_t t = memParseIndexOrKey(a0, NULL);
            if (t > 0) idx = t;
        }
        const wchar_t* key = NULL;
        if (!memArgIsMissing(a1) && a1->vt == VT_BSTR) key = a1->bstrVal;
        void* pic = NULL;
        if (!memArgIsMissing(a2)) {
            if (a2->vt == VT_DISPATCH || a2->vt == VT_UNKNOWN) pic = a2->pdispVal;
            else if (a2->vt == VT_BYREF) pic = a2->pdispVal;
        }
        int32_t newIdx = vb6_ImageList_AddPicture(owner, idx, key, pic);
        if (newIdx <= 0) { memSetEmpty(out); return S_OK; }
        Vb6MemObj* o = memObjNew(VB6_MEMK_LISTIMAGE, owner, newIdx);
        if (!o) return E_OUTOFMEMORY;
        memSetObj(out, o);
        return S_OK;
    }

    case VB6_MEMCD_REMOVE: {
        const wchar_t* key = NULL;
        int32_t r = memParseIndexOrKey(memArg(dp, 0), &key);
        if (r > 0) vb6_ImageList_RemoveAtIndex(owner, r);
        else if (r < 0 && key) vb6_ImageList_RemoveImage(owner, key, 0);
        return S_OK;
    }

    case VB6_MEMCD_CLEAR:
        vb6_ImageList_ClearImages(owner);
        return S_OK;

    case VB6_MEMCD_NEWENUM: {   // 有人按名字表序号发 6 (非标准路径), 一并认
        Vb6MemEnum* e = memEnumNew(p->kind, owner);
        if (!e) return E_OUTOFMEMORY;
        memSetObj(out, e);
        return S_OK;
    }

    default:
        break;
    }
    return DISP_E_MEMBERNOTFOUND;
}

// ===================== IEnumVARIANT (集合枚举器) =====================
static HRESULT STDMETHODCALLTYPE memEnum_Next(IEnumVARIANT* This, ULONG celt, VARIANT* rgVar,
                                              ULONG* pCeltFetched) {
    Vb6MemEnum* e = (Vb6MemEnum*)This;
    ULONG got = 0;
    while (got < celt && e->pos <= e->total) {
        Vb6MemObj* o = memObjNew(VB6_MEMK_LISTIMAGE, e->owner, e->pos);
        if (!o) break;
        memSetObj(&rgVar[got], o);
        e->pos++;
        got++;
    }
    if (pCeltFetched) *pCeltFetched = got;
    return (got == celt) ? S_OK : S_FALSE;
}

static HRESULT STDMETHODCALLTYPE memEnum_Skip(IEnumVARIANT* This, ULONG celt) {
    Vb6MemEnum* e = (Vb6MemEnum*)This;
    e->pos += (int32_t)celt;
    if (e->pos > e->total + 1) e->pos = e->total + 1;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE memEnum_Reset(IEnumVARIANT* This) {
    ((Vb6MemEnum*)This)->pos = 1;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE memEnum_Clone(IEnumVARIANT* This, IEnumVARIANT** ppEnum) {
    Vb6MemEnum* e = (Vb6MemEnum*)This;
    if (!ppEnum) return E_POINTER;
    Vb6MemEnum* c = memEnumNew(e->kind, e->owner);
    if (!c) return E_OUTOFMEMORY;
    c->pos = e->pos;
    c->total = e->total;
    *ppEnum = (IEnumVARIANT*)c;
    return S_OK;
}

// ===================== vtable 装配 =====================
static const IDispatchVtbl g_memObjVtbl = {
    (HRESULT (STDMETHODCALLTYPE*)(IUnknown*, REFIID, void**))mem_QI,
    (ULONG (STDMETHODCALLTYPE*)(IUnknown*))mem_AddRef,
    (ULONG (STDMETHODCALLTYPE*)(IUnknown*))mem_Release,
    mem_GetTypeInfoCount,
    mem_GetTypeInfo,
    memObj_GetIDsOfNames,
    memObj_Invoke
};

static const IDispatchVtbl g_memCollVtbl = {
    (HRESULT (STDMETHODCALLTYPE*)(IUnknown*, REFIID, void**))mem_QI,
    (ULONG (STDMETHODCALLTYPE*)(IUnknown*))mem_AddRef,
    (ULONG (STDMETHODCALLTYPE*)(IUnknown*))mem_Release,
    mem_GetTypeInfoCount,
    mem_GetTypeInfo,
    memColl_GetIDsOfNames,
    memColl_Invoke
};

static const IEnumVARIANTVtbl g_memEnumVtbl = {
    (HRESULT (STDMETHODCALLTYPE*)(IUnknown*, REFIID, void**))mem_QI,
    (ULONG (STDMETHODCALLTYPE*)(IUnknown*))mem_AddRef,
    (ULONG (STDMETHODCALLTYPE*)(IUnknown*))mem_Release,
    memEnum_Next,
    memEnum_Skip,
    memEnum_Reset,
    memEnum_Clone
};

// ===================== 对外入口 =====================
// ImageList1.ListImages —— 返回集合对象 (真 IDispatch)。宿主槽是 vb6_com_<Name> 里
// 那个 ImageList 实例指针 (见 vb6forms_imagelist.c 的注释: 无窗口控件, 槽里不是 HWND)。
void* vb6_ImageList_ListImages(void* slot) {
    if (!slot) return NULL;
    return (void*)memCollNew(VB6_MEMCK_LISTIMAGES, slot);
}

// ListImages.Add([index], [key], [picture]) —— **返回 ListImage 对象**。
// comctl32 句柄与平行 key 表那一套仍在 imagelist.c 的扁平实现里, 这里只把返回的
// 1 基下标包成成员对象: VB6 的 Add 返回的就是对象, 不是下标。
void* vb6_ImageList_ListImages_Add(void* slot, int32_t index, const wchar_t* key, void* pic) {
    if (!slot) return NULL;
    int32_t idx = vb6_ImageList_AddPicture(slot, index, key, pic);
    if (idx <= 0) return NULL;
    return (void*)memObjNew(VB6_MEMK_LISTIMAGE, slot, idx);
}

#endif // _WIN32
