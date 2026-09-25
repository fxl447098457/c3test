// tlb_slots.cpp - ai/022 B16 读数：把 .tlb 里每个 FUNC 的 **vtable 面** 读回来
// （oVft / callconv / funckind / 返回 vt / 每个参数的 vt（含 VT_PTR 链的内层）/ 参数标志）。
// 与 tlbprobe.cpp 刻意分开：那个的打印格式是回归 needle 的来源（identity 通道），不能动；
// 这个只读"契约面"，B16 用它断言库里那一行真接口的成员数/槽偏移/调用约定与发出去的 vtable 一致。
//
// oVft 的单位口径（B16 实测 D62-3）：写库的人按**自己建的库的位数 flag** 归一化（存的是
// "第几槽"），读库的人再按**自己的指针宽**乘回来。所以：
//   x64 建 + x64 读 → oVft = (3+槽)*8；x86 建 + x86 读 → oVft = (3+槽)*4；
//   而 x86 建 + x64 读会把 12 读成 24 —— 这也是本套用例必须有一个 **x86 侧读数** 的原因。
//
// 编译：cl /nologo /EHsc /O1 /utf-8 tlb_slots.cpp /Fe:tlb_slots.exe /link ole32.lib oleaut32.lib
#include <windows.h>
#include <ole2.h>
#include <oleauto.h>
#include <oaidl.h>
#include <stdio.h>
#include <string.h>

static const char *kindName(TYPEKIND k) {
    switch (k) {
        case TKIND_ENUM: return "enum";
        case TKIND_RECORD: return "record";
        case TKIND_MODULE: return "module";
        case TKIND_INTERFACE: return "interface";
        case TKIND_DISPATCH: return "dispinterface";
        case TKIND_COCLASS: return "coclass";
        case TKIND_ALIAS: return "alias";
        case TKIND_UNION: return "union";
        default: return "?";
    }
}
static const char *fkName(FUNCKIND k) {
    switch (k) {
        case FUNC_VIRTUAL: return "virtual";
        case FUNC_PUREVIRTUAL: return "purevirtual";
        case FUNC_NONVIRTUAL: return "nonvirtual";
        case FUNC_STATIC: return "static";
        case FUNC_DISPATCH: return "dispatch";
        default: return "?";
    }
}
static const char *ccName(CALLCONV c) {
    switch (c) {
        case CC_FASTCALL: return "fastcall";
        case CC_CDECL: return "cdecl";
        case CC_PASCAL: return "pascal";
        case CC_STDCALL: return "stdcall";
        default: return "?";
    }
}

static void printTd(const char *tag, TYPEDESC *td, int depth) {
    if (!td) { printf(" %s=NULL", tag); return; }
    unsigned short vt = td->vt;
    if (vt == VT_PTR && depth < 3 && td->lptdesc) {
        printf(" %s=VT_PTR->", tag);
        printTd(tag, td->lptdesc, depth + 1);
        return;
    }
    printf(" %s=0x%04X", tag, (unsigned)vt);
}

int main(int argc, char **argv) {
    if (argc < 2) { printf("USAGE=<file.tlb>\n"); return 2; }
    wchar_t wpath[512];
    MultiByteToWideChar(CP_ACP, 0, argv[1], -1, wpath, 512);
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    ITypeLib *lib = NULL;
    HRESULT hr = LoadTypeLib(wpath, &lib);
    if (FAILED(hr)) { printf("LOAD hr=0x%08lX\n", (unsigned)hr); return 1; }
    UINT n = lib->GetTypeInfoCount();
    printf("TYPES=%u\n", n);
    for (UINT i = 0; i < n; i++) {
        ITypeInfo *ti = NULL;
        BSTR tname = NULL;
        if (FAILED(lib->GetTypeInfo(i, &ti))) continue;
        lib->GetDocumentation(i, &tname, NULL, NULL, NULL);
        TYPEATTR *ta = NULL;
        if (FAILED(ti->GetTypeAttr(&ta))) { if (tname) SysFreeString(tname); ti->Release(); continue; }
        GUID g = ta->guid;
        printf("TYPE %u kind=%s name=%S guid={%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X} cFuncs=%u cVars=%u cImplTypes=%u cbSizeInstance=%u\n",
               (unsigned)i, kindName(ta->typekind), (tname ? (const wchar_t *)tname : L"?"),
               (unsigned)g.Data1, (unsigned)g.Data2, (unsigned)g.Data3,
               (unsigned)g.Data4[0], (unsigned)g.Data4[1], (unsigned)g.Data4[2], (unsigned)g.Data4[3],
               (unsigned)g.Data4[4], (unsigned)g.Data4[5], (unsigned)g.Data4[6], (unsigned)g.Data4[7],
               (unsigned)ta->cFuncs, (unsigned)ta->cVars, (unsigned)ta->cImplTypes,
               (unsigned)ta->cbSizeInstance);
        for (WORD f = 0; f < ta->cFuncs; f++) {
            FUNCDESC *fd = NULL;
            if (FAILED(ti->GetFuncDesc(f, &fd)) || !fd) continue;
            BSTR fname = NULL;
            ti->GetDocumentation(fd->memid, &fname, NULL, NULL, NULL);
            printf("  FUNC %u memid=%d name=%S invkind=%u funckind=%s callconv=%s oVft=%d cParams=%u",
                   (unsigned)f, (int)fd->memid, (fname ? (const wchar_t *)fname : L"?"),
                   (unsigned)fd->invkind, fkName(fd->funckind), ccName(fd->callconv),
                   (int)fd->oVft, (unsigned)fd->cParams);
            printTd("ret", &fd->elemdescFunc.tdesc, 0);
            printf("\n");
            for (SHORT p = 0; p < fd->cParams; p++) {
                ELEMDESC *ed = &fd->lprgelemdescParam[p];
                printf("    PARAM %d flags=0x%04X", (int)p, (unsigned)ed->paramdesc.wParamFlags);
                printTd("vt", &ed->tdesc, 0);
                printf("\n");
            }
            if (fname) SysFreeString(fname);
            ti->ReleaseFuncDesc(fd);
        }
        ti->ReleaseTypeAttr(ta);
        if (tname) SysFreeString(tname);
        ti->Release();
    }
    lib->Release();
    CoUninitialize();
    return 0;
}
