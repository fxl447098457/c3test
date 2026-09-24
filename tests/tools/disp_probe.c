/* disp_probe.c - ai/022 B14: 进程内真客户端，驱动一个真编译出来的 ActiveX DLL 走 IDispatch 四件套。
 *
 * 为什么要有它：B13a-B13e 五格里，DLL 那条对外管线只被**字节级**验过（dll_entry.c 的表、
 * .tlb 里的 GUID），从来没有调用者真的拿着产出的指针走过一次 GetIDsOfNames/Invoke。
 * 本探针不查注册表：LoadLibrary + DllGetClassObject + CreateInstance(IID_IDispatch)，
 * 所以 CI 上不需要 regsvr32，也不依赖机器上装过什么组件。
 *
 * 用法：disp_probe <dll 路径> <clsid "{...}"> [另一个 iid "{...}"，看 QI 应答的是哪份指针]
 * 输出：一行一个 ASCII 读数（用例按子串断言）。
 */
#define COBJMACROS   /* 打开 IDispatch_Xxx / IClassFactory_Xxx 这类 C 形态的内联助手 */
#include <windows.h>
#include <ole2.h>
#include <oleauto.h>
#include <oaidl.h>
#include <stdio.h>
#include <string.h>

typedef HRESULT(STDAPICALLTYPE *PFN_GETCLASSOBJ)(REFCLSID, REFIID, LPVOID *);

static const IID IID_IClassFactory_local =
    {0x00000001, 0x0000, 0x0000, {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};

static int parse_guid(const char *text, GUID *out) {
    wchar_t wbuf[64];
    if (MultiByteToWideChar(CP_ACP, 0, text, -1, wbuf, 64) <= 0) return 0;
    return SUCCEEDED(CLSIDFromString(wbuf, out));
}

static void print_guid(const char *tag, const GUID *g) {
    printf("%s={%08lX-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}\n",
           tag, (unsigned long)g->Data1, g->Data2, g->Data3,
           g->Data4[0], g->Data4[1], g->Data4[2], g->Data4[3],
           g->Data4[4], g->Data4[5], g->Data4[6], g->Data4[7]);
}

/* 名字 -> dispid -> Invoke 一趟；两件事各自打一行，因为"点不到"分两种。 */
static void call_member(IDispatch *disp, const wchar_t *name, VARIANT *argv, int argc,
                        WORD wFlags, const char *tag) {
    DISPID id = DISPID_UNKNOWN;
    DISPPARAMS dp;
    VARIANT res;
    EXCEPINFO ei;
    UINT err = 0;
    LPOLESTR names[1];
    HRESULT hr;
    int i;

    names[0] = (LPOLESTR)name;
    VariantInit(&res);
    memset(&ei, 0, sizeof(ei));
    memset(&dp, 0, sizeof(dp));
    hr = IDispatch_GetIDsOfNames(disp, &IID_NULL, names, 1, LOCALE_USER_DEFAULT, &id);
    printf("NAMES=%s hr=0x%08lX dispid=%ld\n", tag, (unsigned long)hr, (long)id);
    if (FAILED(hr)) return;

    dp.rgvarg = argv;          /* DISPPARAMS 是逆序，调用方按逆序摆好 */
    dp.cArgs = (UINT)argc;
    hr = IDispatch_Invoke(disp, id, &IID_NULL, LOCALE_USER_DEFAULT, wFlags, &dp,
                          &res, &ei, &err);
    printf("CALL=%s hr=0x%08lX", tag, (unsigned long)hr);
    if (SUCCEEDED(hr) && res.vt == VT_I4) printf(" result=%ld", (long)V_I4(&res));
    else if (SUCCEEDED(hr) && res.vt != VT_EMPTY) printf(" resultvt=%u", (unsigned)res.vt);
    printf("\n");
    VariantClear(&res);
    for (i = 0; i < argc; i++) VariantClear(&argv[i]);
}

static VARIANT mk_i4(LONG v) {
    VARIANT x;
    VariantInit(&x);
    x.vt = VT_I4;
    x.lVal = v;
    return x;
}

int main(int argc, char **argv) {
    HMODULE mod;
    PFN_GETCLASSOBJ getClassObject;
    IClassFactory *fac = NULL;
    IDispatch *disp = NULL;
    IUnknown *unk = NULL;
    IUnknown *extra = NULL;
    CLSID clsid;
    IID extraIid;
    VARIANT args[2];
    UINT cinfo = 0;
    ITypeInfo *tinfo = NULL;
    HRESULT hr;

    if (argc < 3) {
        printf("USAGE=<dll> <clsid> [iid]\n");
        return 2;
    }
    printf("COINIT hr=0x%08lX\n", (unsigned long)CoInitializeEx(NULL, COINIT_APARTMENTTHREADED));

    mod = LoadLibraryExA(argv[1], NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!mod) {
        printf("LOAD=FAIL gle=%lu\n", (unsigned long)GetLastError());
        return 1;
    }
    printf("LOAD=OK\n");
    getClassObject = (PFN_GETCLASSOBJ)GetProcAddress(mod, "DllGetClassObject");
    if (!getClassObject) {
        printf("GETPROC=FAIL\n");
        return 1;
    }
    if (!parse_guid(argv[2], &clsid)) {
        printf("CLSID=BAD\n");
        return 1;
    }
    print_guid("CLSID", &clsid);

    hr = getClassObject(&clsid, &IID_IClassFactory_local, (void **)&fac);
    printf("GETFACTORY hr=0x%08lX ptr=%s\n", (unsigned long)hr, fac ? "OK" : "NULL");
    if (FAILED(hr) || !fac) return 1;

    hr = IClassFactory_CreateInstance(fac, NULL, &IID_IDispatch, (void **)&disp);
    printf("CREATE hr=0x%08lX ptr=%s\n", (unsigned long)hr, disp ? "OK" : "NULL");
    IClassFactory_Release(fac);
    if (FAILED(hr) || !disp) return 1;

    hr = IDispatch_QueryInterface(disp, &IID_IUnknown, (void **)&unk);
    printf("QI_IUNKNOWN hr=0x%08lX same=%s\n", (unsigned long)hr,
           (SUCCEEDED(hr) && unk == (IUnknown *)disp) ? "yes" : "no");
    if (unk) {
        IUnknown_Release(unk);
        unk = NULL;
    }

    /* 第二枚 IID 的应答面：答不答应 + 答应的是不是同一份胖指针（胖/瘦之分的实测，不断言） */
    if (argc >= 4 && parse_guid(argv[3], &extraIid)) {
        print_guid("EXTRA_IID", &extraIid);
        hr = IDispatch_QueryInterface(disp, &extraIid, (void **)&extra);
        printf("QI_EXTRA hr=0x%08lX same=%s\n", (unsigned long)hr,
               FAILED(hr) ? "no" : (extra == (IUnknown *)disp ? "yes" : "no"));
        if (extra) IUnknown_Release(extra);
    }

    hr = IDispatch_GetTypeInfoCount(disp, &cinfo);
    printf("TYPEINFOCOUNT=%u hr=0x%08lX\n", cinfo, (unsigned long)hr);
    hr = IDispatch_GetTypeInfo(disp, 0, LOCALE_USER_DEFAULT, &tinfo);
    printf("GETTYPEINFO0 hr=0x%08lX ptr=%s\n", (unsigned long)hr, tinfo ? "OK" : "NULL");
    if (tinfo) {
        TYPEATTR *ta = NULL;
        if (SUCCEEDED(ITypeInfo_GetTypeAttr(tinfo, &ta)) && ta) {
            printf("TYPEINFO0_kind=%d memcount=%u\n", (int)ta->typekind, (unsigned)ta->cFuncs);
            ITypeInfo_ReleaseTypeAttr(tinfo, ta);
        }
        ITypeInfo_Release(tinfo);
    }
    tinfo = NULL;
    hr = IDispatch_GetTypeInfo(disp, 1, LOCALE_USER_DEFAULT, &tinfo);
    printf("GETTYPEINFO1 hr=0x%08lX\n", (unsigned long)hr);
    if (tinfo) ITypeInfo_Release(tinfo);

    args[0] = mk_i4(40);
    args[1] = mk_i4(2);              /* 逆序 = Add(2, 40) */
    call_member(disp, L"Add", args, 2, DISPATCH_METHOD, "ADD");

    args[0] = mk_i4(7);
    call_member(disp, L"SetValue", args, 1, DISPATCH_METHOD, "SETVALUE");
    call_member(disp, L"GetValue", args, 0, DISPATCH_METHOD, "GETVALUE");

    {
        DISPID id = DISPID_UNKNOWN;
        LPOLESTR names[1];
        names[0] = (LPOLESTR)L"NotAMember";
        hr = IDispatch_GetIDsOfNames(disp, &IID_NULL, names, 1, LOCALE_USER_DEFAULT, &id);
        printf("NAMES=bogus hr=0x%08lX dispid=%ld\n", (unsigned long)hr, (long)id);
    }

    IDispatch_Release(disp);
    CoUninitialize();
    printf("DONE\n");
    return 0;
}
