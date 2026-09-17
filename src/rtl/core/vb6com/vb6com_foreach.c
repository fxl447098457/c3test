// vb6com_foreach.c - vb6com 模块拆分: For Each COM 集合枚举 (IEnumVARIANT)
// 由 vb6com.c 按 COM 调用层次拆分而来 (纯搬移, 零行为改动)

#include "vb6com.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vb6com_internal.h"
#include "vb6forms.h"   /* Fix 112: 宿主 Controls 集合枚举 */


// ============================================================
// P22-11: For Each COM collection (IEnumVARIANT)
// ============================================================

// P25-fix4: For Each enumeration with safety limit
// Some COM collections (e.g. MSComctlLib.ImageList) have buggy IEnumVARIANT
// that never terminates (Next() always returns S_OK+fetched=1, cursor never
// advances). VB6 solves this by using Count from ICollection as the upper
// bound. We do the same: store Count and never iterate more than Count items.

typedef struct vb6_ForEachState {
    IEnumVARIANT* pEnum;
    int32_t totalCount;    // from collection.Count, -1 if unknown
    int32_t fetchedCount;  // how many items we've returned to caller (non-empty)
} vb6_ForEachState;

// For Each Init: Call _NewEnum (DISPID -4) on collection object to get IEnumVARIANT
// Also reads Count property for safety limit (works around buggy IEnumVARIANT)
// Returns vb6_ForEachState* (or NULL on failure)
void* vb6_ForEach_Init(void* disp) {
    if (!disp) return NULL;
    /* Fix 112: 宿主 Controls 集合 (vb6_ComGetObjectProp(Me,"Controls") 的合成对象)
     * 不是 IDispatch, 用原生枚举器迭代窗体子控件. */
    if (vb6_UC_ControlsIsCollection(disp)) {
        return vb6_UC_ControlsEnumInit(disp);
    }
    /* Fix 112c: RTL 内建 Collection (New Collection 的产物) */
    if (vb6_Collection_IsCollection(disp)) {
        return vb6_Collection_EnumInit(disp);
    }
    IDispatch* pDisp = (IDispatch*)disp;

    // Read Count property for safety limit
    int32_t count = -1;  // unknown
    {
        DISPID dispidCount = 0;
        OLECHAR* countName = L"Count";
        HRESULT hr2 = pDisp->lpVtbl->GetIDsOfNames(pDisp, &IID_NULL, &countName, 1,
            LOCALE_USER_DEFAULT, &dispidCount);
        if (SUCCEEDED(hr2) && dispidCount > 0) {
            DISPPARAMS dpC = { NULL, NULL, 0, 0 };
            VARIANT vCount;
            VariantInit(&vCount);
            hr2 = pDisp->lpVtbl->Invoke(pDisp, dispidCount,
                &IID_NULL, LOCALE_USER_DEFAULT,
                DISPATCH_PROPERTYGET | DISPATCH_METHOD,
                &dpC, &vCount, NULL, NULL);
            if (SUCCEEDED(hr2) && (V_VT(&vCount) == VT_I4 || V_VT(&vCount) == VT_I2)) {
                count = (V_VT(&vCount) == VT_I4) ? V_I4(&vCount) : (int32_t)V_I2(&vCount);
            }
            VariantClear(&vCount);
        }
    }

    // DISPID -4 is the standard _NewEnum dispid in VB6/Automation
    DISPPARAMS dp = { NULL, NULL, 0, 0 };
    VARIANT result;
    VariantInit(&result);

    HRESULT hr = pDisp->lpVtbl->Invoke(pDisp, (DISPID)-4,
        &IID_NULL, LOCALE_USER_DEFAULT,
        DISPATCH_METHOD | DISPATCH_PROPERTYGET,
        &dp, &result, NULL, NULL);

    if (FAILED(hr) || (V_VT(&result) != VT_UNKNOWN && V_VT(&result) != VT_DISPATCH)) {
        // Try named invocation as fallback
        OLECHAR* names[] = { L"_NewEnum" };
        DISPID dispid;
        hr = pDisp->lpVtbl->GetIDsOfNames(pDisp, &IID_NULL, names, 1,
            LOCALE_USER_DEFAULT, &dispid);
        if (SUCCEEDED(hr)) {
            hr = pDisp->lpVtbl->Invoke(pDisp, dispid,
                &IID_NULL, LOCALE_USER_DEFAULT,
                DISPATCH_METHOD | DISPATCH_PROPERTYGET,
                &dp, &result, NULL, NULL);
        }
    }

    if (FAILED(hr)) {
        VariantClear(&result);
        return NULL;
    }

    // Get IEnumVARIANT from the returned IUnknown/IDispatch
    IEnumVARIANT* pEnum = NULL;
    if (V_VT(&result) == VT_UNKNOWN && V_UNKNOWN(&result)) {
        hr = V_UNKNOWN(&result)->lpVtbl->QueryInterface(
            V_UNKNOWN(&result), &IID_IEnumVARIANT, (void**)&pEnum);
    } else if (V_VT(&result) == VT_DISPATCH && V_DISPATCH(&result)) {
        hr = V_DISPATCH(&result)->lpVtbl->QueryInterface(
            V_DISPATCH(&result), &IID_IEnumVARIANT, (void**)&pEnum);
    }

    VariantClear(&result);

    if (!pEnum) return NULL;

    // Allocate wrapper struct
    vb6_ForEachState* state = (vb6_ForEachState*)calloc(1, sizeof(vb6_ForEachState));
    if (!state) {
        pEnum->lpVtbl->Release(pEnum);
        return NULL;
    }
    state->pEnum = pEnum;
    state->totalCount = count;
    state->fetchedCount = 0;
    return (void*)state;
}

// For Each Next: Fetch one element from IEnumVARIANT
// Returns 1 if element fetched, 0 if enumeration complete
// P25: VB6 compat - skip VT_EMPTY/VT_NULL/VT_ERROR items
// P25-fix4: enforce Count limit (buggy IEnumVARIANT may never terminate)
int32_t vb6_ForEach_Next(void* enumPtr, VARIANT* outVar) {
    if (!enumPtr || !outVar) return 0;
    /* Fix 112: 宿主 Controls 集合的原生枚举器 (vb6_ForEach_Init 的宿主分支) */
    if (vb6_UC_ControlsIsCollection(*((void**)enumPtr))) {
        char hv[64];
        int32_t got = vb6_UC_ControlsEnumNext(enumPtr, hv);
        VariantInit(outVar);
        if (!got) return 0;
        vb6_Host_ToWinVariant(&hv, outVar);
        vb6_Host_ClearVariant(hv);
        return 1;
    }
    /* Fix 112c: RTL 内建 Collection 的原生枚举器 */
    if (vb6_Collection_IsCollection(*((void**)enumPtr))) {
        char hv[64];
        int32_t got = vb6_Collection_EnumNext(enumPtr, hv);
        VariantInit(outVar);
        if (!got) return 0;
        vb6_Host_ToWinVariant(&hv, outVar);
        vb6_Host_ClearVariant(hv);
        return 1;
    }
    vb6_ForEachState* state = (vb6_ForEachState*)enumPtr;
    IEnumVARIANT* pEnum = state->pEnum;

    for (;;) {
        // Safety: never iterate more than Count items (if known)
        if (state->totalCount >= 0 && state->fetchedCount >= state->totalCount) {
            VariantInit(outVar);
            return 0;
        }
        VariantInit(outVar);
        ULONG fetched = 0;
        HRESULT hr = pEnum->lpVtbl->Next(pEnum, 1, outVar, &fetched);
        if (FAILED(hr) || fetched == 0) {
            VariantClear(outVar);
            return 0;
        }
        // VB6: skip items that are Empty, Null, or Error (e.g. Nothing)
        VARTYPE vt = V_VT(outVar);
        if (vt == VT_EMPTY || vt == VT_NULL || vt == VT_ERROR) {
            VariantClear(outVar);
            continue;
        }
        state->fetchedCount++;
        return 1;
    }
}

// For Each Release: Release IEnumVARIANT and free wrapper
void vb6_ForEach_Release(void* enumPtr) {
    if (!enumPtr) return;
    vb6_ForEachState* state = (vb6_ForEachState*)enumPtr;
    state->pEnum->lpVtbl->Release(state->pEnum);
    free(state);
}
