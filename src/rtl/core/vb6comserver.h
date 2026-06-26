#pragma once
// vb6comserver.h - VB6 COM服务端运行时 (P6.6 ActiveX DLL)
// 提供ActiveX DLL所需的基础设施:
//   - 全局引用计数 (g_vb6_cRef + g_vb6_cServerLock)
//   - IClassFactory 实现
//   - DllGetClassObject / DllCanUnloadNow 骨架
//   - 注册表辅助 (DllRegisterServer / DllUnregisterServer)
//   - 每个coclass的自注册信息表
//
// 编译器cgen为每个ActiveX DLL工程生成:
//   1. g_vb6_coclasses[] 表 (coclass描述)
//   2. DllGetClassObject() 实现
//   3. DllRegisterServer() / DllUnregisterServer() 实现
//   4. 每个Public类的IDispatch vtable + 实现

#include <windows.h>
#include <oleauto.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// 全局引用计数
// ============================================================

// 全局对象引用计数 (每个AddRef+1, Release-1)
extern LONG g_vb6_cRef;

// 服务器锁计数 (Class Factory LockServer TRUE+1, FALSE-1)
extern LONG g_vb6_cServerLock;

// 增加全局引用计数
static inline void vb6_ServerAddRef(void) {
    InterlockedIncrement(&g_vb6_cRef);
}

// 减少全局引用计数
static inline void vb6_ServerRelease(void) {
    InterlockedDecrement(&g_vb6_cRef);
}

// ============================================================
// coclass描述表 (由cgen生成)
// ============================================================

// coclass描述结构
// 每个Public类(instancing >= PublicNotCreatable)生成一条
typedef struct vb6_CoClassDesc {
    const char* progId;           // ProgID (如 "MyLib.MyClass")
    const char* clsidStr;         // CLSID字符串 (如 "{xxxxxxxx-...}")
    const char* classVariable;    // VB6类变量名 (模块名，用于查找New函数)
    // 工厂函数: 创建VB6类实例 (返回void*指针)
    void* (*factoryFunc)(void);   // vb6_cls_<Name>_New()
    // 销毁函数: 释放VB6类实例
    void (*destroyFunc)(void*);   // vb6_cls_<Name>_Destroy()
    // IDispatch vtable (由cgen生成, 可为NULL表示不支持自动化)
    const void* dispatchVtable;
    // 类方法数 (用于IDispatch::GetIDsOfNames查找)
    int methodCount;
    // 方法描述表 (name/dispid/invkind)
    const struct vb6_DispMethodDesc* methods;
} vb6_CoClassDesc;

// IDispatch方法描述
typedef struct vb6_DispMethodDesc {
    const wchar_t* name;    // 方法/属性名
    int32_t dispid;         // DISPID
    int32_t invkind;        // INVOKE_KIND (1=Method, 2=PropertyGet, 4=PropertyPut, 8=PropertyPutRef)
    // 调用函数指针 (void* this, VARIANT* args, int argc, VARIANT* result)
    // this=VB6类实例, args=参数数组, argc=参数数, result=返回值(可NULL)
    void (*invokeFunc)(void*, void**, int32_t, void*);
} vb6_DispMethodDesc;

// cgen生成的全局coclass描述表
extern const vb6_CoClassDesc g_vb6_coclasses[];
extern const int g_vb6_coclassCount;

// ============================================================
// VB6 COM对象 (IDispatch包装层)
// ============================================================

// VB6 COM对象结构
// 包装VB6类实例为COM IDispatch对象
typedef struct vb6_ComObject {
    const struct vb6_IDispatchVtable* vtable;
    LONG refCount;
    const vb6_CoClassDesc* desc;
    void* vb6Instance;  // VB6类实例 (vb6_cls_<Name>*)
} vb6_ComObject;

// IDispatch vtable (C风格)
typedef struct vb6_IDispatchVtable {
    // IUnknown
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(vb6_ComObject*, REFIID, void**);
    ULONG (STDMETHODCALLTYPE *AddRef)(vb6_ComObject*);
    ULONG (STDMETHODCALLTYPE *Release)(vb6_ComObject*);
    // IDispatch
    HRESULT (STDMETHODCALLTYPE *GetTypeInfoCount)(vb6_ComObject*, UINT*);
    HRESULT (STDMETHODCALLTYPE *GetTypeInfo)(vb6_ComObject*, UINT, LCID, ITypeInfo**);
    HRESULT (STDMETHODCALLTYPE *GetIDsOfNames)(vb6_ComObject*, REFIID, LPOLESTR*, UINT, LCID, DISPID*);
    HRESULT (STDMETHODCALLTYPE *Invoke)(vb6_ComObject*, DISPID, REFIID, LCID, WORD, DISPPARAMS*, VARIANT*, EXCEPINFO*, UINT*);
} vb6_IDispatchVtable;

// 创建VB6 COM对象 (包装VB6实例为IDispatch)
vb6_ComObject* vb6_ComObject_Create(const vb6_CoClassDesc* desc);

// ============================================================
// Class Factory (IClassFactory实现)
// ============================================================

typedef struct vb6_ClassFactory {
    const struct vb6_IClassFactoryVtable* vtable;
    LONG refCount;
    const vb6_CoClassDesc* desc;  // 关联的coclass描述
} vb6_ClassFactory;

// IClassFactory vtable (C风格)
typedef struct vb6_IClassFactoryVtable {
    // IUnknown
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(vb6_ClassFactory*, REFIID, void**);
    ULONG (STDMETHODCALLTYPE *AddRef)(vb6_ClassFactory*);
    ULONG (STDMETHODCALLTYPE *Release)(vb6_ClassFactory*);
    // IClassFactory
    HRESULT (STDMETHODCALLTYPE *CreateInstance)(vb6_ClassFactory*, IUnknown*, REFIID, void**);
    HRESULT (STDMETHODCALLTYPE *LockServer)(vb6_ClassFactory*, BOOL);
} vb6_IClassFactoryVtable;

// 获取指定CLSID的Class Factory
// 由DllGetClassObject调用
HRESULT vb6_GetClassFactory(REFCLSID rclsid, REFIID riid, void** ppv, 
                            const vb6_CoClassDesc* coclasses, int count);

// ============================================================
// DLL导出函数 (由cgen生成的包装函数调用)
// ============================================================

// DllCanUnloadNow - 判断DLL是否可卸载
HRESULT vb6_DllCanUnloadNow(void);

// 注册表辅助
// 注册一个coclass (CLSID + ProgID + InprocServer32)
HRESULT vb6_RegisterCoClass(const vb6_CoClassDesc* desc, const wchar_t* dllPath);

// 反注册一个coclass
HRESULT vb6_UnregisterCoClass(const vb6_CoClassDesc* desc);

// 获取当前DLL路径 (用于注册InprocServer32)
HRESULT vb6_GetDllPath(wchar_t* path, DWORD size);

#ifdef __cplusplus
}
#endif
