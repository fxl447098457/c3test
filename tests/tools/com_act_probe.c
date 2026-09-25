/* com_act_probe.c - ai/022 B17: 真注册 + 外部激活双路，驱动一个真编译出来的 ActiveX DLL。
 *
 * 与 disp_probe.c 的分工：后者刻意**不查注册表**（LoadLibrary + DllGetClassObject），
 * 所以它证明的是"进程内那条管线通"；本探针走的是**系统那条路** ——
 *   DllRegisterServer 写注册表 → CoCreateInstance 按 CLSID 让 COM 自己去找 InprocServer32
 *   → 装载 DLL → DllGetClassObject → 拿 IDispatch（= CreateObject 晚绑定）或直接拿接口
 *   薄指针（= 早绑定客户按库里 oVft 直调契约槽）。
 * 收尾必须证明**可清理**：DllUnregisterServer 之后三类键都不许留。
 *
 * 用法：com_act_probe <dll 路径> <clsid "{...}"> <progid> [接口 iid "{...}"]
 * 输出：一行一个 ASCII 读数（用例按子串断言）。
 */
#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <oleauto.h>
#include <oaidl.h>
#include <stdio.h>
#include <string.h>

typedef HRESULT(STDAPICALLTYPE *PFN_SVR)(void);

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

/* 读一个 REG_SZ 值；返回 0 = 不在（这就是"键没写进去"与"写进去了"的分界）。 */
static int read_sz(HKEY root, const wchar_t *sub, const wchar_t *name, wchar_t *out, DWORD cch) {
    HKEY k;
    DWORD type = 0, cb = cch * sizeof(wchar_t);
    if (RegOpenKeyExW(root, sub, 0, KEY_READ, &k) != ERROR_SUCCESS) return 0;
    if (RegQueryValueExW(k, name, NULL, &type, (BYTE *)out, &cb) != ERROR_SUCCESS || type != REG_SZ) {
        RegCloseKey(k);
        return 0;
    }
    RegCloseKey(k);
    return 1;
}

static void report_sz(const char *tag, HKEY root, const wchar_t *sub, const wchar_t *name) {
    wchar_t buf[MAX_PATH * 2];
    if (read_sz(root, sub, name, buf, MAX_PATH * 2)) {
        printf("%s=OK %ls\n", tag, buf);
    } else {
        printf("%s=MISSING\n", tag);
    }
}

static void report_absent(const char *tag, HKEY root, const wchar_t *sub, const wchar_t *name) {
    wchar_t buf[MAX_PATH * 2];
    printf("%s=%s\n", tag, read_sz(root, sub, name, buf, MAX_PATH * 2) ? "STILL" : "gone");
}

static VARIANT mk_i4(LONG v) {
    VARIANT x;
    VariantInit(&x);
    x.vt = VT_I4;
    x.lVal = v;
    return x;
}

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

    dp.rgvarg = argv;
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

/* 薄指针的形状（与生成码 vb6_ivtbl_<I> 逐槽对齐）：先解一层取槽表再按槽调。
 * 本支路写死 cc_dll 的 IProbe: [3]=Ping(ByVal n As Long), [4]=Got() As Long。 */
typedef struct probe_ivtbl_IProbe {
    long (__stdcall *QueryInterface)(void *self, const void *riid, void **ppv);
    unsigned long (__stdcall *AddRef)(void *self);
    unsigned long (__stdcall *Release)(void *self);
    void (__stdcall *Ping)(void *self, LONG n);
    LONG (__stdcall *Got)(void *self);
} probe_ivtbl_IProbe;

static void call_vtable_slots(void *iface) {
    probe_ivtbl_IProbe *vt = *(probe_ivtbl_IProbe **)iface;
    printf("VTBL_GET_BEFORE=%ld\n", (long)vt->Got(iface));
    vt->Ping(iface, 21);
    printf("VTBL_GET_AFTER=%ld\n", (long)vt->Got(iface));
}

/* 从 DLL 的内嵌资源里取 LIBID/版本, 再看注册表里按 LIBID 找不找得到这份库 —— "外部工具
 * (客户、工具链) 认不认这一档"就看这一条。返回 1 = 找到, 路径写进 out。 */
static int typelib_lookup(const char *dllPathA, wchar_t *out, DWORD cch) {
    wchar_t dllW[MAX_PATH * 2];
    ITypeLib *tlb = NULL;
    TLIBATTR *tla = NULL;
    int found = 0;

    MultiByteToWideChar(CP_ACP, 0, dllPathA, -1, dllW, MAX_PATH * 2);
    if (FAILED(LoadTypeLibEx(dllW, REGKIND_NONE, &tlb)) || !tlb) {
        printf("TYPELIB_LOAD=MISSING\n");
        return 0;
    }
    if (SUCCEEDED(tlb->lpVtbl->GetLibAttr(tlb, &tla)) && tla) {
        GUID libid = tla->guid;
        USHORT major = (USHORT)tla->wMajorVerNum;
        USHORT minor = (USHORT)tla->wMinorVerNum;
        tlb->lpVtbl->ReleaseTLibAttr(tlb, tla);
        print_guid("LIBID", &libid);
        printf("LIBVER=%u.%u\n", (unsigned)major, (unsigned)minor);
        found = SUCCEEDED(QueryPathOfRegTypeLib(&libid, major, minor, 0x0409, out)) ? 1 : 0;
    }
    tlb->lpVtbl->Release(tlb);
    return found;
}

int main(int argc, char **argv) {
    HMODULE mod;
    PFN_SVR reg, unreg;
    CLSID clsid;
    IID ifaceIid;
    wchar_t clsidSub[128];
    wchar_t progidSub[256];
    wchar_t progidW[256];
    wchar_t libPath[MAX_PATH * 2];
    IDispatch *disp = NULL;
    void *thin = NULL;
    VARIANT args[2];
    HRESULT hr;

    if (argc < 4) {
        printf("USAGE=<dll> <clsid> <progid> [iid] [reg|unreg]\n");
        return 2;
    }
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("COINIT hr=0x%08lX\n", (unsigned long)CoInitializeEx(NULL, COINIT_APARTMENTTHREADED));

    if (!parse_guid(argv[2], &clsid)) {
        printf("CLSID=BAD\n");
        return 2;
    }
    print_guid("CLSID", &clsid);
    MultiByteToWideChar(CP_ACP, 0, argv[3], -1, progidW, 256);
    {
        wchar_t clsidW[64];
        StringFromGUID2(&clsid, clsidW, 64);
        swprintf(clsidSub, 128, L"CLSID\\%ls", clsidW);
    }
    swprintf(progidSub, 256, L"%ls", progidW);
    printf("PROGID=%ls\n", progidW);

    mod = LoadLibraryExA(argv[1], NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!mod) {
        printf("LOAD=FAIL gle=%lu\n", (unsigned long)GetLastError());
        return 1;
    }
    printf("LOAD=OK\n");
    reg = (PFN_SVR)GetProcAddress(mod, "DllRegisterServer");
    unreg = (PFN_SVR)GetProcAddress(mod, "DllUnregisterServer");
    if (!reg || !unreg) {
        printf("EXPORTS=FAIL reg=%d unreg=%d\n", reg ? 1 : 0, unreg ? 1 : 0);
        return 1;
    }

    /* 只注册 / 只反注册: 给"另一个进程才是被测客户"的用例当夹具用 (B17 里 C3 编译的客户 EXE
     * 要在 DLL 已注册的前提下跑, 而注册动作本身得由某个进程做)。反注册那条顺带把
     * "三类键都不许留"的三个读数打出来, 免得调用方还要自己查注册表。
     * 模式词从 argv[4] 起找 —— 接口 IID 可以省略 (只做注册/反注册时用不到它), 写死 argv[5]
     * 会让 `... <progid> reg` 静默退化成整趟跑 (注册完顺手反注册 ⇒ 调用方以为"已注册")。 */
    {
        const char *mode = NULL;
        int i;
        for (i = 4; i < argc; i++) {
            if (strcmp(argv[i], "reg") == 0 || strcmp(argv[i], "unreg") == 0) { mode = argv[i]; break; }
        }
        if (mode) {
            int wantReg = (strcmp(mode, "reg") == 0);
            hr = wantReg ? reg() : unreg();
            printf("%s hr=0x%08lX\n", wantReg ? "REGSVR" : "UNREGSVR", (unsigned long)hr);
            if (!wantReg) {
                wchar_t sub[256];
                report_absent("CLEAN_CLSID", HKEY_CLASSES_ROOT, clsidSub, NULL);
                swprintf(sub, 256, L"%ls\\CLSID", progidSub);
                report_absent("CLEAN_PROGID", HKEY_CLASSES_ROOT, sub, NULL);
                printf("CLEAN_TYPELIB=%s\n",
                       typelib_lookup(argv[1], libPath, MAX_PATH * 2) ? "STILL" : "gone");
            }
            FreeLibrary(mod);
            CoUninitialize();
            return SUCCEEDED(hr) ? 0 : 1;
        }
    }

    /* ---- 注册：写 HKCR（非管理员会被 Windows 重定向到 HKCU\Software\Classes） ---- */
    hr = reg();
    printf("REGSVR hr=0x%08lX\n", (unsigned long)hr);
    report_sz("REG_CLSID_DEFAULT", HKEY_CLASSES_ROOT, clsidSub, NULL);
    {
        wchar_t sub[256];
        swprintf(sub, 256, L"%ls\\InprocServer32", clsidSub);
        report_sz("REG_INPROC_PATH", HKEY_CLASSES_ROOT, sub, NULL);
        report_sz("REG_THREADING", HKEY_CLASSES_ROOT, sub, L"ThreadingModel");
    }
    {
        wchar_t sub[256];
        swprintf(sub, 256, L"%ls\\CLSID", progidSub);
        report_sz("REG_PROGID_CLSID", HKEY_CLASSES_ROOT, sub, NULL);
    }
    /* 类型库注册：从 DLL 资源里 LoadTypeLib 取 LIBID，再用 QueryPathOfRegTypeLib 反查
     * 注册表里那条路径 —— 这一步证明"外部工具按 LIBID 找得到这个库"。 */
    if (typelib_lookup(argv[1], libPath, MAX_PATH * 2)) {
        printf("REG_TYPELIB_PATH=OK %ls\n", libPath);
    } else {
        printf("REG_TYPELIB_PATH=MISSING\n");
    }

    /* ---- 外部激活 ①：晚绑定 = CreateObject 走的那条路（ProgID -> CLSID -> CoCreateInstance） ---- */
    {
        CLSID fromProg = {0};
        hr = CLSIDFromProgID(progidW, &fromProg);
        printf("PROGID_LOOKUP hr=0x%08lX same=%s\n", (unsigned long)hr,
               (SUCCEEDED(hr) && memcmp(&fromProg, &clsid, sizeof(CLSID)) == 0) ? "yes" : "no");
    }
    hr = CoCreateInstance(&clsid, NULL, CLSCTX_INPROC_SERVER, &IID_IDispatch, (void **)&disp);
    printf("COCREATE_DISP hr=0x%08lX ptr=%s\n", (unsigned long)hr, disp ? "OK" : "NULL");
    if (disp) {
        /* 晚绑定: 类的**公有成员** (接口实现是 Private, 不在这一面上 —— VB6 语义) */
        args[0] = mk_i4(21);
        call_member(disp, L"Twice", args, 1, DISPATCH_METHOD, "Twice");
        /* 契约成员按名字**点不到**: 空面是这个形状的应有读数, 不是缺陷 */
        args[0] = mk_i4(21);
        call_member(disp, L"Ping", args, 1, DISPATCH_METHOD, "Ping");
        call_member(disp, L"Got", args, 0, DISPATCH_PROPERTYGET, "Got");
        IDispatch_Release(disp);
        disp = NULL;
    }

    /* ---- 外部激活 ②：早绑定 = 客户按库里 oVft 直调契约槽 ---- */
    if (argc >= 5 && parse_guid(argv[4], &ifaceIid)) {
        print_guid("IFACE_IID", &ifaceIid);
        hr = CoCreateInstance(&clsid, NULL, CLSCTX_INPROC_SERVER, &ifaceIid, &thin);
        printf("COCREATE_IFACE hr=0x%08lX ptr=%s\n", (unsigned long)hr, thin ? "OK" : "NULL");
        if (thin) {
            call_vtable_slots(thin);
            IUnknown_Release((IUnknown *)thin);
            thin = NULL;
        }
    } else {
        printf("IFACE_IID=SKIP\n");
    }

    /* ---- 可清理：反注册之后三类键都不许留 ---- */
    hr = unreg();
    printf("UNREGSVR hr=0x%08lX\n", (unsigned long)hr);
    report_absent("CLEAN_CLSID", HKEY_CLASSES_ROOT, clsidSub, NULL);
    {
        wchar_t sub[256];
        swprintf(sub, 256, L"%ls\\CLSID", progidSub);
        report_absent("CLEAN_PROGID", HKEY_CLASSES_ROOT, sub, NULL);
    }
    if (typelib_lookup(argv[1], libPath, MAX_PATH * 2)) {
        printf("CLEAN_TYPELIB=STILL\n");
    } else {
        printf("CLEAN_TYPELIB=gone\n");
    }

    FreeLibrary(mod);
    CoUninitialize();
    printf("DONE\n");
    return 0;
}
