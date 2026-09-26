#pragma once
// vb6rtl_runtime.h - 运行时初始化/退出、ParamArray 支持、Option Compare、运行期补充
// 必须排在 array / builtin 之后（依赖 SafeArray 与 DebugPrintStr 声明）
#include "vb6rtl_base.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// 运行时初始化/退出
// ============================================================

// ============================================================
// ParamArray runtime support (P14.1.5)
// ============================================================

SAFEARRAY* vb6_PA_Create(int32_t count);
void vb6_PA_Destroy(SAFEARRAY* psa);
void vb6_PA_SetVariant(SAFEARRAY* psa, int32_t index, VARIANT* pv);
void vb6_PA_SetLong(SAFEARRAY* psa, int32_t index, int32_t val);
void vb6_PA_SetLongPtr(SAFEARRAY* psa, int32_t index, intptr_t val);  /* Fix 082: x64-safe VarPtr parameter */
void vb6_PA_SetDouble(SAFEARRAY* psa, int32_t index, double val);
void vb6_PA_SetBSTR(SAFEARRAY* psa, int32_t index, BSTR val);
VARIANT vb6_PA_GetVariant(SAFEARRAY* psa, int32_t index);
int32_t vb6_PA_GetLong(SAFEARRAY* psa, int32_t index);
double vb6_PA_GetDouble(SAFEARRAY* psa, int32_t index);
BSTR vb6_PA_GetBSTR(SAFEARRAY* psa, int32_t index);
int32_t vb6_IsMissing(SAFEARRAY* psa);
int32_t vb6_PA_UBound(SAFEARRAY* psa);
int32_t vb6_PA_LBound(SAFEARRAY* psa);

void vb6_Init(void);
// comctl32 通用控件引导 (ProgressBar/StatusBar/Toolbar/ListView/TreeView 依赖)。
// 由 vb6_Init 自己调用; 这里额外声明以便其它 RTL 单元直接使用。
void vb6_ComCtl_Init(void);
void vb6_Exit(void);
void vb6_End(void);
void vb6_Beep(void);

// P18-C: Option Compare (Text/Binary)
extern int g_vb6_optionCompareText;  // 0=Binary(default), 1=Text
int vb6_StrCmp(const wchar_t* a, const wchar_t* b);  // respects Option Compare

// Fix 048: Missing runtime functions that were generating C4013 warnings

// vb6_SA_Destroy — generic SafeArray destroyer (used by COM class destructor codegen)
// Destroys 1D SafeArray; for NULL it's a no-op
static inline void vb6_SA_Destroy(void* arr) {
    if (arr) vb6_SafeArrayDestroy1D((vb6_SafeArray1D*)arr);
}

// vb6_DebugAssert — Debug.Assert (no-op in compiled mode)
static inline void vb6_DebugAssert(int32_t cond) {
    (void)cond;  /* no-op: Debug.Assert only active in IDE */
}

// vb6_DebugPrint — Debug.Print (no-op in compiled mode, use DebugPrintStr for actual output)
static inline void vb6_DebugPrint(BSTR s) {
    vb6_DebugPrintStr(s);
}

// vb6_LoadResData — LoadResData (returns empty Variant, resource loading not supported)
vb6_VARIANT vb6_LoadResData(int32_t resourceId, int32_t resourceType);

#ifdef __cplusplus
}
#endif
