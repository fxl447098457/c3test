// vb6com.c - VB6 COM互操作运行时实现 (P6)
// 使用Windows原生COM API, 独立于vb6rtl.h避免VARIANT冲突

#include "vb6com.h"
#include "vb6com_internal.h"
#include <stdio.h>

#include <stdlib.h>
#include <string.h>


// P24-08: COM错误传播 — 将HRESULT/EXCEPINFO转换为VB6运行时错误
// VB6行为: COM方法失败时自动触发Err.Raise, 可被On Error捕获
extern void vb6_RaiseError(int32_t errNum, void* description);
extern int32_t vb6_err_resume_next;

// COM错误→VB6错误转换辅助函数
// hr: Invoke返回的HRESULT
// excep: EXCEPINFO结构 (可能包含scode/bstrDescription)
// context: 调用上下文 (用于默认错误描述, 如L"ComCall" / L"ComSetProp")
// VB6标准错误号→描述映射
// 数据源: Microsoft官方Trappable Errors列表 + MSVBVM60.DLL STRINGTABLE交叉验证
// DLL STRINGTABLE ID 规则:
//   - 经典运行时错误(seq 0~39): ID = 10000 + 序列号 (序列号≠错误号, 需交叉匹配)
//   - 窗体/控件/扩展错误: ID部分直接对应错误号, 部分需对照Microsoft文档
// 翻译策略: 中文描述为主, 英文原文保留在注释中供参考
// 注: 735/744/746 和 485/486/487 都是合法的VB6错误号, 不可互相替代
// 非 static: vb6com_collection.c 的内建 Collection 也要用它填 EXCEPINFO.bstrDescription
const wchar_t* vb6_StdErrorDesc(int32_t errNum) {
    switch (errNum) {
    // ---- VB6 经典运行时错误 (ErrNum 3~94) ----
    // 来源: Microsoft官方Trappable Errors + DLL STRINGTABLE ID 10000~10039
    case   3: return L"没有GoSub时的Return";              // Return without GoSub
    case   5: return L"无效的过程调用或参数";               // Invalid procedure call or argument
    case   6: return L"溢出";                              // Overflow
    case   7: return L"内存不足";                           // Out of memory
    case   9: return L"下标越界";                           // Subscript out of range
    case  10: return L"此数组被固定或暂时锁定";              // This array is fixed or temporarily locked
    case  11: return L"除数为零";                           // Division by zero
    case  13: return L"类型不匹配";                          // Type mismatch
    case  14: return L"字符串空间不足";                       // Out of string space
    case  16: return L"表达式太复杂";                        // Expression too complex
    case  17: return L"不能执行请求的操作";                   // Can't perform requested operation
    case  18: return L"发生用户中断";                         // User interrupt occurred
    case  20: return L"没有错误时Resume";                     // Resume without error
    case  28: return L"栈空间不足";                           // Out of stack space
    case  35: return L"Sub、Function或Property未定义";        // Sub, Function, or Property not defined
    case  47: return L"DLL应用程序客户端过多";                 // Too many DLL application clients
    case  48: return L"加载DLL时出错";                        // Error in loading DLL
    case  49: return L"DLL调用约定错误";                      // Bad DLL calling convention
    case  51: return L"内部错误";                             // Internal error
    case  52: return L"文件名或文件号错误";                     // Bad file name or number
    case  53: return L"找不到文件";                            // File not found
    case  54: return L"文件模式错误";                          // Bad file mode
    case  55: return L"文件已打开";                            // File already open
    case  57: return L"设备I/O错误";                          // Device I/O error
    case  58: return L"文件已存在";                            // File already exists
    case  59: return L"记录长度错误";                          // Bad record length
    case  61: return L"磁盘已满";                             // Disk full
    case  62: return L"输入超出文件尾";                        // Input past end of file
    case  63: return L"记录号错误";                            // Bad record number
    case  67: return L"文件过多";                             // Too many files
    case  68: return L"设备不可用";                            // Device unavailable
    case  70: return L"权限被拒绝";                            // Permission denied
    case  71: return L"磁盘未准备好";                          // Disk not ready
    case  74: return L"不能用不同的驱动器重命名";                // Can't rename with different drive
    case  75: return L"路径/文件访问错误";                      // Path/File access error
    case  76: return L"找不到路径";                            // Path not found
    case  91: return L"对象变量或With块变量未设置";              // Object variable or With block variable not set
    case  92: return L"For循环未初始化";                       // For loop not initialized
    case  93: return L"无效的模式字符串";                       // Invalid pattern string
    case  94: return L"Null的使用无效";                        // Invalid use of Null

    // ---- VB6 扩展运行时错误 (ErrNum 96~98) ----
    case  96: return L"无法接收对象事件,已达最大接收数";         // Unable to sink events of object because it's already firing events to the maximum number of event receivers
    case  97: return L"不能在不是定义类实例的对象上调用Friend过程"; // Can't call Friend procedure on an object that is not an instance of the defining class
    case  98: return L"属性或方法调用不能包含对私有对象的引用";     // A property or method call cannot include a reference to a private object, either as an argument or as a return value

    // ---- 系统DLL/文件格式错误 (ErrNum 298~328) ----
    // 来源: Microsoft官方Trappable Errors + DLL STRINGTABLE交叉验证
    case 298: return L"不能加载系统DLL";                      // System DLL could not be loaded
    case 320: return L"不能在指定文件名中使用字符设备名";         // Can't use character device names in specified file names
    case 321: return L"无效的文件格式";                         // Invalid file format
    case 322: return L"无法创建必要的临时文件";                  // Can't create necessary temporary file
    case 325: return L"资源文件中格式无效";                      // Invalid format in resource file
    case 327: return L"找不到命名的数据值";                      // Data value named not found
    case 328: return L"非法参数;不能写入数组";                   // Illegal parameter; can't write arrays

    // ---- 注册表/组件错误 (ErrNum 335~338) ----
    case 335: return L"不能访问系统注册表";                      // Could not access system registry
    case 336: return L"ActiveX部件未正确注册";                  // Component not correctly registered
    case 337: return L"找不到ActiveX部件";                      // Component not found
    case 338: return L"ActiveX部件不能正确运行";                // Component did not run correctly

    // ---- 控件数组错误 (ErrNum 339~345) ----
    // DLL STRINGTABLE ID 1339~1345, VB6错误号 = ID - 1000
    case 339: return L"组件或其依赖项未正确注册:文件缺失或无效";   // Component '|' or one of its dependencies not correctly registered: a file is missing or invalid
    case 340: return L"控件数组元素不存在";                      // Control array element '|' doesn't exist
    case 341: return L"控件数组索引无效";                         // Invalid control array index
    case 342: return L"没有足够空间分配控件数组";                  // Not enough room to allocate control array
    case 343: return L"对象不是数组";                             // Object not an array
    case 344: return L"必须指定对象数组的索引";                    // Must specify index for object array
    case 345: return L"已达上限:无法为此窗体创建更多控件";          // Reached limit: cannot create any more controls for this form

    // ---- 窗体/对象加载错误 (ErrNum 360~374) ----
    case 360: return L"对象已加载";                              // Object already loaded
    case 361: return L"不能加载或卸载此对象";                     // Can't load or unload this object
    case 362: return L"不能卸载设计时创建的控件";                  // Can't unload controls created at design time
    case 363: return L"找不到指定的ActiveX控件";                  // Control specified not found
    case 364: return L"对象已卸载";                               // Object was unloaded
    case 365: return L"无法在此上下文中卸载";                      // Unable to unload within this context
    case 366: return L"没有可用的MDI窗体来加载";                   // No MDI form available to load
    case 367: return L"不能加载或注册ActiveX控件";                 // Can't load (or register) ActiveX control: '|'
    case 368: return L"指定文件已过期;此程序需要较新版本";          // The specified file is out of date. This program requires a later version
    case 369: return L"在ActiveX DLL中操作无效";                  // Operation not valid in an ActiveX DLL
    case 371: return L"指定的对象不能用作Show的所有者窗体";         // The specified object can't be used as an owner form for Show
    case 372: return L"从文件加载控件失败;版本可能已过期";          // Failed to load control '|' from |2. Your version may be outdated
    case 373: return L"编译环境与运行时组件之间的交互不支持";       // Interaction between compiled and design environment components is not supported
    case 374: return L"激活控件失败;控件可能不兼容";               // Failed to activate control '|'. This control may be incompatible

    // ---- 属性错误 (ErrNum 378~399) ----
    case 378: return L"加载或保存时不能设置此属性";                // '|' cannot be set while loading or saving
    case 379: return L"不能在属性页上放置默认或取消按钮";           // You can't put a Default or Cancel button on a Property Page
    case 380: return L"属性值无效";                               // Invalid property value
    case 381: return L"属性数组索引无效";                          // Invalid property array index
    case 382: return L"运行时不能设置此属性";                      // Property Set can't be executed at run time
    case 383: return L"只读属性不能设置";                          // Property Set can't be used with a read-only property
    case 384: return L"窗体最小化或最大化时不能移动或调整大小";      // A form can't be moved or sized while minimized or maximized
    case 385: return L"需要属性数组索引";                          // Need property array index
    case 387: return L"不允许设置此属性";                          // Property Set not permitted
    case 388: return L"不能从父菜单设置Visible属性";               // Can't set Visible property from a parent menu
    case 393: return L"运行时不能获取此属性";                      // Property Get can't be executed at run time
    case 394: return L"只写属性不能读取";                          // Property Get can't be executed on write-only property
    case 395: return L"不能将分隔条用作此控件的菜单名";             // Cannot use separator bar as menu name for this control
    case 396: return L"在属性页中不能设置此属性";                   // Property cannot be set within a page
    case 397: return L"合并时不能加载、卸载或设置顶层菜单的Visible属性"; // Can't load, unload, or set Visible property for top level menus while they are merged
    case 398: return L"客户端站点不可用";                           // Client Site not available

    // ---- 模态窗体错误 (ErrNum 400~406) ----
    case 400: return L"窗体已显示;不能以模态方式显示";              // Form already displayed; can't show modally
    case 401: return L"显示模态窗体时不能显示非模态窗体";           // Can't show non-modal form when modal form is displayed
    case 402: return L"必须先关闭或隐藏最上层模态窗体";             // Must close or hide topmost modal form first
    case 403: return L"MDI窗体不能以模态方式显示";                  // MDI forms cannot be shown modally
    case 404: return L"MDI子窗体不能以模态方式显示";                // MDI child forms cannot be shown modally
    case 405: return L"无法在此上下文中显示模态窗体";               // Unable to show modal form within this context
    case 406: return L"不能从ActiveX DLL中显示非模态窗体";          // Non-modal forms cannot be displayed from an ActiveX DLL

    // ---- 对象权限错误 (ErrNum 419~425) ----
    case 419: return L"拒绝使用对象的权限";                        // Permission to use object denied
    case 422: return L"找不到属性";                                // Property not found
    case 423: return L"找不到属性或方法";                          // Property or method not found
    case 424: return L"需要对象";                                  // Object required
    case 425: return L"无效的对象使用";                            // Invalid object use

    // ---- COM/ActiveX错误 (ErrNum 429~463) ----
    case 429: return L"ActiveX部件不能创建对象或返回引用";          // Component can't create object or return reference to this object
    case 430: return L"类不支持自动化";                             // Class doesn't support Automation
    case 432: return L"自动化操作期间找不到文件名或类名";            // File name or class name not found during Automation operation
    case 438: return L"对象不支持此属性或方法";                      // Object doesn't support this property or method
    case 440: return L"自动化错误";                                 // Automation error
    case 442: return L"远程处理的类型库或对象库的连接已丢失";         // Connection to type library or object library for remote process has been lost
    case 443: return L"自动化对象没有默认值";                        // Automation object doesn't have a default value
    case 445: return L"对象不支持此操作";                            // Object doesn't support this action
    case 446: return L"对象不支持命名参数";                          // Object doesn't support named arguments
    case 447: return L"对象不支持当前区域设置";                      // Object doesn't support current locale setting
    case 448: return L"找不到命名参数";                              // Named argument not found
    case 449: return L"参数不是可选的或属性赋值无效";                 // Argument not optional or invalid property assignment
    case 450: return L"参数个数错误或属性赋值无效";                   // Wrong number of arguments or invalid property assignment
    case 451: return L"对象不是集合";                                // Object not a collection
    case 452: return L"无效的序号";                                  // Invalid ordinal
    case 453: return L"找不到指定的DLL函数";                         // Specified DLL function not found
    case 454: return L"找不到代码资源";                              // Code resource not found
    case 455: return L"代码资源锁定错误";                            // Code resource lock error
    case 457: return L"此键已经与该集合的一个元素关联";               // This key is already associated with an element of this collection
    case 458: return L"变量使用了Visual Basic不支持的类型";          // Variable uses a type not supported in Visual Basic
    case 459: return L"此组件不支持事件集";                          // This component doesn't support the set of events
    case 460: return L"剪贴板格式无效";                              // Invalid Clipboard format
    case 461: return L"找不到方法或数据成员";                         // Method or data member not found
    case 462: return L"远程服务器机器不存在或不可用";                 // The remote server machine does not exist or is unavailable
    case 463: return L"类未在本地机器上注册";                         // Class not registered on local machine

    // ---- 图片/打印机错误 (ErrNum 480~487) ----
    case 480: return L"不能创建AutoRedraw图像";                      // Can't create AutoRedraw image
    case 481: return L"图片无效";                                    // Invalid picture
    case 482: return L"打印机错误";                                  // Printer error
    case 483: return L"打印机驱动程序不支持指定属性";                 // Printer driver does not support specified property
    case 484: return L"从系统获取打印机信息时出错";                   // Problem getting printer information from the system
    case 485: return L"无效的图片类型";                               // Invalid picture type
    case 486: return L"不能用这种类型的打印机打印窗体图像";            // Can't print form image to this type of printer
    case 487: return L"不能打印最小化的窗体图像";                     // Can't print minimized form image

    // ---- 剪贴板错误 (ErrNum 520~521) ----
    case 520: return L"不能清空剪贴板";                               // Can't empty Clipboard
    case 521: return L"不能打开剪贴板";                               // Can't open Clipboard

    // ---- 其他运行时错误 (ErrNum 735~746) ----
    // 注: 735/744/746 与 485/486/487 是不同的VB6错误号, 两者都合法
    case 735: return L"不能将文件保存到TEMP目录";                      // Can't save file to TEMP directory
    case 744: return L"找不到搜索文本";                               // Search text not found
    case 746: return L"替换内容太长";                                 // Replacements too long

    // ---- OLE/嵌入对象错误 (ErrNum 31001~31037) ----
    // 来源: Microsoft官方Trappable Errors列表 (1-1000范围外)
    case 31001: return L"内存不足";                                   // Out of memory
    case 31004: return L"无对象";                                     // No object
    case 31018: return L"未设置类";                                   // Class is not set
    case 31027: return L"不能激活对象";                               // Unable to activate object
    case 31032: return L"不能创建嵌入对象";                            // Unable to create embedded object
    case 31036: return L"保存到文件时出错";                            // Error saving to file
    case 31037: return L"从文件加载时出错";                            // Error loading from file

    default: return NULL;
    }
}


// COM错误→VB6错误转换辅助函数
// hr: Invoke返回的HRESULT
// excep: EXCEPINFO结构 (可能包含scode/bstrDescription)
// context: 调用上下文 (用于默认错误描述, 如L"ComCall" / L"ComSetProp")
// Fix 177: HRESULT → VB6 错误号。
// "低16位即VB6错误号" 只对 0x8004xxxx (标准 COM) 与 0x800Axxxx (VB6 运行时) 成立,
// 对 DISP_E_* 系列 (0x8002xxxx) 不成立 —— 其低16位落在 DISPID 子码空间: 例如
// DISP_E_MEMBERNOTFOUND (0x80020003) 的低16位 = 3, 会被当成 VB6 错误 3
// "没有GoSub时的Return", 与真实原因 (对象不支持该属性/方法) 毫无关系。
// 实测: VBMAN 对 Scripting.Dictionary 调自有扩展 API 时即报出这个假象,
// 把排查方向引到 GoSub/Return 上。此处显式映射 DISP_E_* 到 VB6 官方错误号
// (描述表 vb6_StdErrorDesc 中 438/449/450/451 均已有条目)。
static int32_t vb6_HresultToErrNum(long hr) {
    switch ((unsigned long)hr) {
    case 0x80020003UL: return 438;  // DISP_E_MEMBERNOTFOUND → 对象不支持此属性或方法
    case 0x80020006UL: return 438;  // DISP_E_UNKNOWNNAME    → 同上
    case 0x80020005UL: return 13;   // DISP_E_TYPEMISMATCH   → 类型不匹配
    case 0x8002000EUL: return 450;  // DISP_E_BADPARAMCOUNT  → 参数个数错误
    case 0x8002000FUL: return 449;  // DISP_E_PARAMNOTOPTIONAL → 参数不是可选的
    case 0x8002000BUL: return 451;  // DISP_E_BADINDEX       → 对象不是集合
    default: {
        int32_t errNum = (int32_t)((unsigned long)hr & 0xFFFF);  // VB6错误号 = HRESULT低16位
        if (errNum == 0) errNum = (int32_t)hr;  // 非标准HRESULT直接用整个值
        return errNum;
    }
    }
}

void vb6_ComCheckError(HRESULT hr, EXCEPINFO* excep, const wchar_t* context) {
    int32_t errNum = vb6_HresultToErrNum((long)hr);

    // 优先使用EXCEPINFO中的scode (OLE自动化错误码; DISP_E_EXCEPTION 时携带真实错误号)
    if (excep && excep->scode) {
        errNum = vb6_HresultToErrNum((long)excep->scode);
    }

    // 构造错误描述BSTR — 四级fallback:
    //   1. EXCEPINFO.bstrDescription (COM对象直接提供)
    //   2. IErrorInfo::GetDescription (COM错误接口)
    //   3. VB6标准错误号中文查表 (scode有效时优先)
    //   4. FormatMessageW(FROM_SYSTEM) (系统HRESULT描述)
    //   5. 默认: "COM error in <context>: 0xXXXXXXXX"
    BSTR desc = NULL;
    if (excep && excep->bstrDescription) {
        // 1. EXCEPINFO有描述: 直接使用
        desc = excep->bstrDescription;
        excep->bstrDescription = NULL;  // 转移所有权
    } else {
        // 2. 尝试IErrorInfo
        IErrorInfo* perror = NULL;
        if (SUCCEEDED(GetErrorInfo(0, &perror)) && perror) {
            BSTR eiDesc = NULL;
            perror->lpVtbl->GetDescription(perror, &eiDesc);
            DWORD helpCtx = 0;
            perror->lpVtbl->GetHelpContext(perror, &helpCtx);
            if (helpCtx) errNum = (int32_t)(helpCtx & 0xFFFF);
            perror->lpVtbl->Release(perror);
            if (eiDesc) desc = eiDesc;
        }
        if (!desc) {
            // 3. VB6标准错误号中文查表 (scode提取的VB6错误号优先)
            const wchar_t* stdDesc = vb6_StdErrorDesc(errNum);
            if (stdDesc) desc = SysAllocString(stdDesc);
        }
        if (!desc) {
            // 4. FormatMessageW(FROM_SYSTEM) — 仅当VB6表查不到时使用
            //    注意: DISP_E_EXCEPTION(0x80020009)会返回"发生意外"等无用通用描述
            //    所以当scode有效(VB6错误号)时不应走到这里
            wchar_t sysBuf[512] = {0};
            DWORD fmLen = FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                NULL, (DWORD)hr, 0, sysBuf, 512, NULL);
            if (fmLen > 0) {
                while (fmLen > 0 && (sysBuf[fmLen-1] == L'\r' || sysBuf[fmLen-1] == L'\n')) sysBuf[--fmLen] = L'\0';
                if (fmLen > 0) desc = SysAllocString(sysBuf);
            }
        }
        if (!desc) {
            // 5. 默认描述
            wchar_t buf[256];
            swprintf(buf, 256, L"COM error in %ls: 0x%08lX", context ? context : L"?", (unsigned long)hr);
            desc = SysAllocString(buf);
        }
    }

    // 清理EXCEPINFO中的其他BSTR (bstrDescription已转移)
    if (excep) {
        if (excep->bstrSource) { SysFreeString(excep->bstrSource); excep->bstrSource = NULL; }
        if (excep->bstrHelpFile) { SysFreeString(excep->bstrHelpFile); excep->bstrHelpFile = NULL; }
    }

    vb6_RaiseError(errNum, desc);
}
// C-style vtable helpers for IUnknown (avoid C++ IUnknown method call issues)
// ============================================================
HRESULT vb6_UnknownQI(void* obj, REFIID riid, void** ppv) {
    IUnknown* pUnk = (IUnknown*)obj;
    return pUnk->lpVtbl->QueryInterface(pUnk, riid, ppv);
}
ULONG vb6_UnknownRelease(void* obj) {
    IUnknown* pUnk = (IUnknown*)obj;
    return pUnk->lpVtbl->Release(pUnk);
}

// ============================================================
// COM初始化/退出
// ============================================================

void vb6_ComInit(void) {
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
}

void vb6_ComExit(void) {
    CoUninitialize();
}

// ============================================================
// CreateObject / GetObject
// ============================================================

void* vb6_CreateObject(const wchar_t* progId) {
    if (!progId) return NULL;

    CLSID clsid;
    HRESULT hr = CLSIDFromProgID(progId, &clsid);
    if (FAILED(hr)) {
        // VB6 Error 429: ActiveX component can't create object
        wchar_t buf429[512];
        swprintf(buf429, 512, L"ActiveX component can't create object (ProgID: %ls, HRESULT: 0x%08lX)", progId, (unsigned long)hr);
        BSTR desc = SysAllocString(buf429);
        vb6_RaiseError(429, desc);
        return NULL;
    }

    IDispatch* pDisp = NULL;
    hr = CoCreateInstance(&clsid, NULL, CLSCTX_LOCAL_SERVER | CLSCTX_INPROC_SERVER,
                          &IID_IDispatch, (void**)&pDisp);
    if (FAILED(hr)) {
        BSTR desc429 = NULL;
        IErrorInfo* perror429 = NULL;
        if (SUCCEEDED(GetErrorInfo(0, &perror429)) && perror429) {
            perror429->lpVtbl->GetDescription(perror429, &desc429);
            perror429->lpVtbl->Release(perror429);
        }
        if (!desc429) {
            wchar_t buf429b[512];
            swprintf(buf429b, 512, L"ActiveX component can't create object (ProgID: %ls, HRESULT: 0x%08lX)", progId, (unsigned long)hr);
            desc429 = SysAllocString(buf429b);
        }
        vb6_RaiseError(429, desc429);
        return NULL;
    }

    return (void*)pDisp;
}

void* vb6_GetObject(const wchar_t* pathName, const wchar_t* progId) {
    if (!progId) return NULL;

    CLSID clsid;
    HRESULT hr = CLSIDFromProgID(progId, &clsid);
    if (FAILED(hr)) {
        fwprintf(stderr, L"vb6_GetObject: CLSIDFromProgID(\"%ls\") failed: 0x%08lX\n",
                 progId, (unsigned long)hr);
        return NULL;
    }

    IUnknown* pUnk = NULL;

    if (pathName && wcslen(pathName) > 0) {
        // 从文件加载: GetObject(pathName, progId)
        IPersistFile* pFile = NULL;
        hr = CoCreateInstance(&clsid, NULL, CLSCTX_LOCAL_SERVER | CLSCTX_INPROC_SERVER,
                              &IID_IPersistFile, (void**)&pFile);
        if (SUCCEEDED(hr) && pFile) {
            hr = pFile->lpVtbl->Load(pFile, pathName, STGM_READ);
            if (SUCCEEDED(hr)) {
                hr = pFile->lpVtbl->QueryInterface(pFile, &IID_IDispatch, (void**)&pUnk);
            }
            pFile->lpVtbl->Release(pFile);
        }
        // 回退: 尝试GetActiveObject
        if (!pUnk) {
            hr = GetActiveObject(&clsid, NULL, &pUnk);
        }
    } else {
        // 获取运行中的对象: GetObject(, progId)
        hr = GetActiveObject(&clsid, NULL, &pUnk);
    }

    if (FAILED(hr) || !pUnk) {
        fwprintf(stderr, L"vb6_GetObject: failed for \"%ls\": 0x%08lX\n",
                 progId, (unsigned long)hr);
        return NULL;
    }

    IDispatch* pDisp = NULL;
    hr = pUnk->lpVtbl->QueryInterface(pUnk, &IID_IDispatch, (void**)&pDisp);
    pUnk->lpVtbl->Release(pUnk);

    if (FAILED(hr)) {
        fwprintf(stderr, L"vb6_GetObject: QueryInterface IDispatch failed: 0x%08lX\n",
                 (unsigned long)hr);
        return NULL;
    }

    return (void*)pDisp;
}

// ============================================================
// IsNothing / ReleaseObject
// ============================================================

int32_t vb6_IsNothing(void* obj) {
    return (obj == NULL) ? -1 : 0;  // VB6 True=-1, False=0
}

void vb6_ReleaseObject(void** objPtr) {
    if (!objPtr || !*objPtr) return;

    IUnknown* pUnk = (IUnknown*)(*objPtr);
    pUnk->lpVtbl->Release(pUnk);
    *objPtr = NULL;
}
