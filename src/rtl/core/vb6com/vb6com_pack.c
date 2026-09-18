// vb6com_pack.c - vb6com 模块拆分: VARIANT 参数打包/解包
// 由 vb6com.c 按 COM 调用层次拆分而来 (纯搬移, 零行为改动)

#include "vb6com.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vb6com_internal.h"


// ============================================================
// VARIANT 封装 / 解封 (P6.2 cgen使用)
// ============================================================

// 将BSTR封装为VARIANT (返回堆分配的VARIANT*, 需vb6_ComVarClear释放)
void* vb6_ComPackBSTR(const wchar_t* bstr) {
    VARIANT* pv = (VARIANT*)calloc(1, sizeof(VARIANT));
    VariantInit(pv);
    if (bstr) {
        pv->vt = VT_BSTR;
        pv->bstrVal = SysAllocString(bstr);
    } else {
        pv->vt = VT_BSTR;
        pv->bstrVal = NULL;
    }
    return (void*)pv;
}

// 将int32_t封装为VARIANT
void* vb6_ComPackInt(int32_t val) {
    VARIANT* pv = (VARIANT*)calloc(1, sizeof(VARIANT));
    VariantInit(pv);
    pv->vt = VT_I4;
    pv->lVal = val;
    return (void*)pv;
}

// 将VB6 Boolean (int32_t: -1=True, 0=False) 封装为VARIANT VT_BOOL
// VBScript/COM 期望 VT_BOOL 而非 VT_I4
void* vb6_ComPackBool(int32_t val) {
    VARIANT* pv = (VARIANT*)calloc(1, sizeof(VARIANT));
    VariantInit(pv);
    pv->vt = VT_BOOL;
    pv->boolVal = val ? VARIANT_TRUE : VARIANT_FALSE;
    return (void*)pv;
}

// 将double封装为VARIANT
void* vb6_ComPackDouble(double val) {
    VARIANT* pv = (VARIANT*)calloc(1, sizeof(VARIANT));
    VariantInit(pv);
    pv->vt = VT_R8;
    pv->dblVal = val;
    return (void*)pv;
}

// 将void*(IDispatch*)封装为VARIANT (用于对象参数)
void* vb6_ComPackObject(void* obj) {
    VARIANT* pv = (VARIANT*)calloc(1, sizeof(VARIANT));
    VariantInit(pv);
    pv->vt = VT_DISPATCH;
    pv->pdispVal = (IDispatch*)obj;
    if (obj) ((IDispatch*)obj)->lpVtbl->AddRef((IDispatch*)obj);  /* AddRef: VariantClear will Release */
    return (void*)pv;
}

// Fix 104: 将"省略的实参"封装为 VARIANT (VT_ERROR + DISP_E_PARAMNOTFOUND).
// VB6 对 IDispatch 调用省略实参时 (obj.Method a, , c) 传的正是该值, 接收方据此
// 把"未提供"与"显式传 0/空串"区分开. 直接丢弃省略实参会让后续实参前移 (参数错位).
void* vb6_ComPackMissing(void) {
    VARIANT* pv = (VARIANT*)calloc(1, sizeof(VARIANT));
    if (!pv) return NULL;
    VariantInit(pv);
    pv->vt = VT_ERROR;
    pv->scode = DISP_E_PARAMNOTFOUND;   /* 0x80020004 */
    return (void*)pv;
}

// 从VARIANT*解封BSTR (返回BSTR, 调用方需vb6_BSTR_Free释放)
wchar_t* vb6_ComUnpackBSTR(void* variant) {
    if (!variant) return NULL;
    VARIANT* pv = (VARIANT*)variant;
    if (pv->vt == VT_BSTR) {
        if (pv->bstrVal) {
            // 复制BSTR内容, 调用方用vb6_BSTR_Free释放
            return SysAllocString(pv->bstrVal);
        } else {
            // VT_BSTR + bstrVal=NULL = 空BSTR, 返回NULL (与VB6 vb6_BSTR_Empty()一致)
            return NULL;
        }
    }
    // 尝试变体转换
    if (pv->vt != VT_EMPTY && pv->vt != VT_NULL) {
        VARIANT vBstr;
        VariantInit(&vBstr);
        if (SUCCEEDED(VariantChangeType(&vBstr, pv, 0, VT_BSTR))) {
            BSTR tmp = SysAllocString(vBstr.bstrVal);
            VariantClear(&vBstr);
            return tmp;
        }
    }
    return NULL;
}

// 从VARIANT*解封int32_t
int32_t vb6_ComUnpackInt(void* variant) {
    if (!variant) return 0;
    VARIANT* pv = (VARIANT*)variant;
    if (pv->vt == VT_I4) return pv->lVal;
    if (pv->vt == VT_I2) return (int32_t)pv->iVal;
    if (pv->vt == VT_BOOL) return (pv->boolVal == VARIANT_TRUE) ? -1 : 0;
    if (pv->vt == VT_UI1) return (int32_t)pv->bVal;
    // 尝试变体转换
    if (pv->vt != VT_EMPTY && pv->vt != VT_NULL) {
        VARIANT vInt;
        VariantInit(&vInt);
        if (SUCCEEDED(VariantChangeType(&vInt, pv, 0, VT_I4))) {
            return vInt.lVal;
        }
    }
    return 0;
}

// 从VARIANT*解封double
double vb6_ComUnpackDouble(void* variant) {
    if (!variant) return 0.0;
    VARIANT* pv = (VARIANT*)variant;
    if (pv->vt == VT_R8) return pv->dblVal;
    if (pv->vt == VT_R4) return (double)pv->fltVal;
    if (pv->vt == VT_I4) return (double)pv->lVal;
    // 尝试变体转换
    if (pv->vt != VT_EMPTY && pv->vt != VT_NULL) {
        VARIANT vDbl;
        VariantInit(&vDbl);
        if (SUCCEEDED(VariantChangeType(&vDbl, pv, 0, VT_R8))) {
            return vDbl.dblVal;
        }
    }
    return 0.0;
}

// 从VARIANT*解封对象(void*/IDispatch*)
void* vb6_ComUnpackObject(void* variant) {
    if (!variant) return NULL;
    VARIANT* pv = (VARIANT*)variant;
    if (pv->vt == VT_DISPATCH) return (void*)pv->pdispVal;
    if (pv->vt == VT_UNKNOWN) return (void*)pv->punkVal;
    return NULL;
}

// 释放ComCall/ComGetProp返回的VARIANT* (值类型安全: 释清BSTR/数字等)
void vb6_ComVarClear(void* variant) {
    if (!variant) return;
    VARIANT* pv = (VARIANT*)variant;
    VariantClear(pv);
    free(pv);
}

// 仅释放VARIANT结构体, 不清除内容 (对象所有权转移: pdispVal已被取走)
void vb6_ComVarFree(void* variant) {
    if (!variant) return;
    free(variant);  // 不调用VariantClear, 对象引用已转移给调用方
}
