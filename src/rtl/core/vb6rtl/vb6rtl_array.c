// vb6rtl_array.c - VB6 运行时库: 数组家族：SAFEARRAY 一维与多维实现
// 2026-09-17 从 src/rtl/core/vb6rtl/vb6rtl.c 按家族拆出（纯搬移，逐行未改）:
//   原第 3183~3373 行
//   原第 3374~3617 行

#include "vb6rtl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdarg.h>
#include <wchar.h>
#include <wctype.h>
#include <time.h>
#ifdef _WIN32
#include <direct.h>
#include <io.h>
#include <oleauto.h>
#include <olectl.h>
#include <windows.h>
#endif

// P24-08: MessageBoxW (user32) + GetConsoleWindow (kernel32)
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "kernel32.lib")

// ============================================================
// SAFEARRAY - VB6 数组实现
// ============================================================

// 安全数组元素大小表
static int32_t vb6_sa_elem_size(vb6_safearray_elemtype t) {
    switch (t) {
        case vb6_sa_bool:    return (int32_t)sizeof(int16_t);
        case vb6_sa_byte:    return (int32_t)sizeof(uint8_t);
        case vb6_sa_int:     return (int32_t)sizeof(int16_t);
        case vb6_sa_long:    return (int32_t)sizeof(int32_t);
        case vb6_sa_single:  return (int32_t)sizeof(float);
        case vb6_sa_double:  return (int32_t)sizeof(double);
        case vb6_sa_bstr:    return (int32_t)sizeof(BSTR);
        case vb6_sa_variant: return (int32_t)sizeof(vb6_VARIANT);
        case vb6_sa_ptr:     return (int32_t)sizeof(void*);
        case vb6_sa_currency: return (int32_t)sizeof(int64_t);  /* P1-9修复: VB6 Currency = 8bytes */
        case vb6_sa_udt:     return 0;  /* UDT: elemSize set externally, cannot determine here */
        default:             return 4;
    }
}

vb6_SafeArray1D* vb6_SafeArrayCreate1D(vb6_safearray_elemtype elemType,
    int32_t lBound, int32_t uBound) {
    vb6_SafeArray1D* arr = (vb6_SafeArray1D*)calloc(1, sizeof(vb6_SafeArray1D));
    if (!arr) return NULL;
    arr->signature = 0x5A1D;  /* Fix 082g: 1D array magic */
    arr->elemType = elemType;
    arr->elemSize = vb6_sa_elem_size(elemType);
    arr->lBound = lBound;
    arr->uBound = uBound;
    arr->count = uBound - lBound + 1;
    arr->isDynamic = 0;
    if (arr->count > 0) {
        arr->data = calloc((size_t)arr->count, (size_t)arr->elemSize);
    }
    return arr;
}

vb6_SafeArray1D* vb6_SafeArrayReDim1D(vb6_safearray_elemtype elemType,
    int32_t lBound, int32_t uBound) {
    vb6_SafeArray1D* arr = vb6_SafeArrayCreate1D(elemType, lBound, uBound);
    if (arr) arr->isDynamic = 1;
    return arr;
}

vb6_SafeArray1D* vb6_SafeArrayReDim1D_Udt(int32_t elemSize,
    int32_t lBound, int32_t uBound) {
    vb6_SafeArray1D* arr = (vb6_SafeArray1D*)calloc(1, sizeof(vb6_SafeArray1D));
    if (!arr) return NULL;
    arr->signature = 0x5A1D;  /* Fix 082g: 1D array magic */
    arr->elemType = vb6_sa_udt;
    arr->elemSize = elemSize;
    arr->lBound = lBound;
    arr->uBound = uBound;
    arr->count = uBound - lBound + 1;
    arr->isDynamic = 1;
    if (arr->count > 0) {
        arr->data = calloc((size_t)arr->count, (size_t)arr->elemSize);
    }
    return arr;
}

vb6_SafeArray1D* vb6_SafeArrayReDimPreserve1D(vb6_SafeArray1D* arr,
    int32_t newLBound, int32_t newUBound) {
    if (!arr) return vb6_SafeArrayReDim1D(vb6_sa_empty, newLBound, newUBound);

    int32_t newCount = newUBound - newLBound + 1;
    if (newCount <= 0) {
        vb6_SafeArrayDestroy1D(arr);
        return NULL;
    }

    // 分配新数据区 (零初始化)
    void* newData = calloc((size_t)newCount, (size_t)arr->elemSize);
    if (!newData) return arr;  // 分配失败, 返回原数组

    // 复制旧数据到新区域 (取交集)
    int32_t copyStart = (arr->lBound > newLBound) ? arr->lBound : newLBound;
    int32_t copyEnd   = (arr->uBound < newUBound) ? arr->uBound : newUBound;
    if (copyStart <= copyEnd) {
        int32_t srcOff = copyStart - arr->lBound;
        int32_t dstOff = copyStart - newLBound;
        int32_t copyLen = (copyEnd - copyStart + 1) * arr->elemSize;
        memcpy((char*)newData + dstOff * arr->elemSize,
               (char*)arr->data + srcOff * arr->elemSize,
               (size_t)copyLen);
    }

    // BSTR元素: 旧区域中被丢弃的元素需要释放
    if (arr->elemType == vb6_sa_bstr) {
        for (int32_t i = arr->lBound; i <= arr->uBound; i++) {
            // 在新范围之外的BSTR需要释放
            if (i < newLBound || i > newUBound) {
                BSTR* slot = &VB6_SA_AT(BSTR, arr, i);
                if (*slot) vb6_BSTR_Free(*slot);
            }
        }
    }

    free(arr->data);
    arr->data = newData;
    arr->lBound = newLBound;
    arr->uBound = newUBound;
    arr->count = newCount;
    return arr;
}

// UDT版ReDim Preserve: 元素尺寸由调用方提供 (sizeof(UDT))。
// 关键修复: 当 arr==NULL 时, 不能回落 vb6_sa_empty(4字节), 否则 UDT 元素写入越界。
// 若 arr 已存在但 elemSize 与本次不符 (曾用错误尺寸分配), 则丢弃旧数据按正确尺寸重建。
vb6_SafeArray1D* vb6_SafeArrayReDimPreserve1D_Udt(int32_t elemSize,
    vb6_SafeArray1D* arr, int32_t newLBound, int32_t newUBound) {
    if (!arr) return vb6_SafeArrayReDim1D_Udt(elemSize, newLBound, newUBound);

    if (arr->elemSize != elemSize) {
        // 旧数组用错误元素尺寸分配 (如历史 4 字节), 数据不可保留, 直接重建
        vb6_SafeArrayDestroy1D(arr);
        return vb6_SafeArrayReDim1D_Udt(elemSize, newLBound, newUBound);
    }

    int32_t newCount = newUBound - newLBound + 1;
    if (newCount <= 0) {
        vb6_SafeArrayDestroy1D(arr);
        return NULL;
    }

    void* newData = calloc((size_t)newCount, (size_t)elemSize);
    if (!newData) return arr;  // 分配失败, 返回原数组

    int32_t copyStart = (arr->lBound > newLBound) ? arr->lBound : newLBound;
    int32_t copyEnd   = (arr->uBound < newUBound) ? arr->uBound : newUBound;
    if (copyStart <= copyEnd) {
        int32_t srcOff = copyStart - arr->lBound;
        int32_t dstOff = copyStart - newLBound;
        int32_t copyLen = (copyEnd - copyStart + 1) * elemSize;
        memcpy((char*)newData + dstOff * elemSize,
               (char*)arr->data + srcOff * elemSize,
               (size_t)copyLen);
    }

    free(arr->data);
    arr->data = newData;
    arr->elemSize = elemSize;
    arr->lBound = newLBound;
    arr->uBound = newUBound;
    arr->count = newCount;
    return arr;
}

void vb6_SafeArrayDestroy1D(vb6_SafeArray1D* arr) {
    if (!arr) return;
    // BSTR元素: 逐个释放
    if (arr->elemType == vb6_sa_bstr && arr->data) {
        for (int32_t i = 0; i < arr->count; i++) {
            BSTR* slot = (BSTR*)((char*)arr->data + i * arr->elemSize);
            if (*slot) vb6_BSTR_Free(*slot);
        }
    }
    // vb6_VARIANT元素: 逐个清理BSTR
    if (arr->elemType == vb6_sa_variant && arr->data) {
        for (int32_t i = 0; i < arr->count; i++) {
            vb6_VARIANT* slot = (vb6_VARIANT*)((char*)arr->data + i * arr->elemSize);
            if (slot->vt == vb6_vtBSTR && slot->bstrVal) {
                vb6_BSTR_Free(slot->bstrVal);
            }
        }
    }
    if (arr->data) free(arr->data);
    free(arr);
}

void* vb6_SafeArrayGetPtr(vb6_SafeArray1D* arr, int32_t index) {
    if (!arr || index < arr->lBound || index > arr->uBound) return NULL;
    return (char*)arr->data + (index - arr->lBound) * arr->elemSize;
}

void vb6_SafeArrayPutElem(vb6_SafeArray1D* arr, int32_t index, void* value) {
    if (!arr || index < arr->lBound || index > arr->uBound) return;
    void* dest = (char*)arr->data + (index - arr->lBound) * arr->elemSize;
    // BSTR: 先释放旧值, 再赋新值
    if (arr->elemType == vb6_sa_bstr) {
        BSTR* slot = (BSTR*)dest;
        if (*slot) vb6_BSTR_Free(*slot);
        BSTR newVal = *(BSTR*)value;
        *slot = newVal;
    } else {
        memcpy(dest, value, (size_t)arr->elemSize);
    }
}

int32_t vb6_UBound(vb6_SafeArray1D* safeArray, int32_t dimension) {
    if (!safeArray) return 0;
    // Fix 082g: use signature field to distinguish 1D vs ND arrays
    // SafeArray1D has signature=0x5A1D, SafeArrayND has dimCount (2-16)
    if (safeArray->signature == 0x5A1D) {
        // Confirmed 1D array
        return safeArray->uBound;
    }
    // Not a 1D array - try ND path
    if (dimension > 1) {
        return vb6_UBoundND((vb6_SafeArrayND*)safeArray, dimension);
    }
    // Fallback: check if it looks like an ND array
    int32_t possibleDimCount = *(int32_t*)safeArray;
    if (possibleDimCount >= 2 && possibleDimCount <= 16) {
        vb6_SafeArrayND* ndArr = (vb6_SafeArrayND*)safeArray;
        if (ndArr->totalElements > 0 && ndArr->data != NULL) {
            return vb6_UBoundND(ndArr, dimension);
        }
    }
    return safeArray->uBound;
}

int32_t vb6_LBound(vb6_SafeArray1D* safeArray, int32_t dimension) {
    if (!safeArray) return 0;
    // Fix 082g: use signature field to distinguish 1D vs ND arrays
    if (safeArray->signature == 0x5A1D) {
        return safeArray->lBound;
    }
    if (dimension > 1) {
        return vb6_LBoundND((vb6_SafeArrayND*)safeArray, dimension);
    }
    int32_t possibleDimCount = *(int32_t*)safeArray;
    if (possibleDimCount >= 2 && possibleDimCount <= 16) {
        vb6_SafeArrayND* ndArr = (vb6_SafeArrayND*)safeArray;
        if (ndArr->totalElements > 0 && ndArr->data != NULL) {
            return vb6_LBoundND(ndArr, dimension);
        }
    }
    return safeArray->lBound;
}

// ============================================================
// SAFEARRAY ND - VB6 多维数组实现
// ============================================================

vb6_SafeArrayND* vb6_SafeArrayCreateND(vb6_safearray_elemtype elemType,
    int32_t dimCount, vb6_SafeArrayBound bounds[]) {
    if (dimCount <= 0 || dimCount > 16) return NULL;

    vb6_SafeArrayND* arr = (vb6_SafeArrayND*)calloc(1, sizeof(vb6_SafeArrayND));
    if (!arr) return NULL;

    arr->dimCount = dimCount;
    arr->elemType = elemType;
    arr->elemSize = vb6_sa_elem_size(elemType);

    int32_t total = 1;
    for (int32_t d = 0; d < dimCount; d++) {
        arr->bounds[d] = bounds[d];
        if (bounds[d].cElements <= 0) {
            arr->totalElements = 0;
            arr->data = NULL;
            return arr;
        }
        total *= bounds[d].cElements;
    }
    arr->totalElements = total;

    if (total > 0) {
        arr->data = calloc((size_t)total, (size_t)arr->elemSize);
        if (!arr->data) {
            free(arr);
            return NULL;
        }
    }

    return arr;
}

void vb6_SafeArrayDestroyND(vb6_SafeArrayND* arr) {
    if (!arr) return;

    if (arr->elemType == vb6_sa_bstr && arr->data) {
        for (int32_t i = 0; i < arr->totalElements; i++) {
            BSTR* slot = (BSTR*)((char*)arr->data + i * arr->elemSize);
            if (*slot) vb6_BSTR_Free(*slot);
        }
    }
    if (arr->elemType == vb6_sa_variant && arr->data) {
        for (int32_t i = 0; i < arr->totalElements; i++) {
            vb6_VARIANT* slot = (vb6_VARIANT*)((char*)arr->data + i * arr->elemSize);
            if (slot->vt == vb6_vtBSTR && slot->bstrVal) {
                vb6_BSTR_Free(slot->bstrVal);
            }
        }
    }

    if (arr->data) free(arr->data);
    free(arr);
}

int32_t vb6_SafeArrayND_Offset(vb6_SafeArrayND* arr, int32_t dimCount, int32_t indices[]) {
    int32_t offset = indices[0] - arr->bounds[0].lBound;
    int32_t stride = arr->bounds[0].cElements;
    for (int32_t d = 1; d < dimCount; d++) {
        offset += (indices[d] - arr->bounds[d].lBound) * stride;
        stride *= arr->bounds[d].cElements;
    }
    return offset;
}

void* vb6_SafeArrayND_GetPtr(vb6_SafeArrayND* arr, ...) {
    if (!arr) return NULL;
    int32_t indices[16];
    va_list ap;
    va_start(ap, arr);
    for (int32_t d = 0; d < arr->dimCount; d++) {
        indices[d] = va_arg(ap, int32_t);
    }
    va_end(ap);

    int32_t offset = vb6_SafeArrayND_Offset(arr, arr->dimCount, indices);
    if (offset < 0 || offset >= arr->totalElements) return NULL;
    return (char*)arr->data + offset * arr->elemSize;
}

vb6_SafeArrayND* vb6_SafeArrayReDimND(vb6_safearray_elemtype elemType,
    int32_t dimCount, vb6_SafeArrayBound bounds[]) {
    return vb6_SafeArrayCreateND(elemType, dimCount, bounds);
}

vb6_SafeArrayND* vb6_SafeArrayReDimND_Udt(int32_t elemSize,
    int32_t dimCount, vb6_SafeArrayBound bounds[]) {
    if (dimCount <= 0 || dimCount > 16) return NULL;

    vb6_SafeArrayND* arr = (vb6_SafeArrayND*)calloc(1, sizeof(vb6_SafeArrayND));
    if (!arr) return NULL;

    arr->dimCount = dimCount;
    arr->elemType = vb6_sa_udt;
    arr->elemSize = elemSize;

    int32_t total = 1;
    for (int32_t d = 0; d < dimCount; d++) {
        arr->bounds[d] = bounds[d];
        if (bounds[d].cElements <= 0) {
            arr->totalElements = 0;
            arr->data = NULL;
            return arr;
        }
        total *= bounds[d].cElements;
    }
    arr->totalElements = total;

    if (total > 0) {
        arr->data = calloc((size_t)total, (size_t)arr->elemSize);
        if (!arr->data) {
            free(arr);
            return NULL;
        }
    }

    return arr;
}

vb6_SafeArrayND* vb6_SafeArrayReDimPreserveND(vb6_SafeArrayND* arr,
    int32_t dimCount, vb6_SafeArrayBound newBounds[]) {
    if (!arr) return vb6_SafeArrayCreateND(vb6_sa_empty, dimCount, newBounds);

    int32_t newTotal = 1;
    for (int32_t d = 0; d < dimCount; d++) {
        if (newBounds[d].cElements <= 0) {
            vb6_SafeArrayDestroyND(arr);
            return NULL;
        }
        newTotal *= newBounds[d].cElements;
    }

    void* newData = calloc((size_t)newTotal, (size_t)arr->elemSize);
    if (!newData) return arr;

    if (arr->data && arr->totalElements > 0) {
        int32_t minDims = (dimCount < arr->dimCount) ? dimCount : arr->dimCount;

        int32_t copyCounts[16];
        int32_t oldCounts[16];
        int32_t newCounts[16];
        int32_t oldStrides[16];
        int32_t newStrides[16];

        for (int32_t d = 0; d < dimCount; d++)
            newCounts[d] = newBounds[d].cElements;
        for (int32_t d = 0; d < minDims; d++)
            oldCounts[d] = arr->bounds[d].cElements;
        for (int32_t d = minDims; d < 16; d++)
            oldCounts[d] = 0;

        for (int32_t d = 0; d < dimCount; d++)
            copyCounts[d] = (oldCounts[d] < newCounts[d]) ? oldCounts[d] : newCounts[d];

        oldStrides[minDims - 1] = 1;
        for (int32_t d = minDims - 2; d >= 0; d--)
            oldStrides[d] = oldStrides[d + 1] * arr->bounds[d + 1].cElements;

        newStrides[dimCount - 1] = 1;
        for (int32_t d = dimCount - 2; d >= 0; d--)
            newStrides[d] = newStrides[d + 1] * newBounds[d + 1].cElements;

        int32_t iterMax = 1;
        for (int32_t d = 0; d < minDims; d++)
            iterMax *= copyCounts[d];

        for (int32_t linear = 0; linear < iterMax; linear++) {
            int32_t tmp = linear;
            int32_t oldOff = 0, newOff = 0;
            int32_t bounds_check = 1;
            for (int32_t d = minDims - 1; d >= 0; d--) {
                int32_t idx = tmp % copyCounts[d];
                tmp /= copyCounts[d];
                if (idx >= oldCounts[d] || idx >= newCounts[d]) {
                    bounds_check = 0;
                    break;
                }
                oldOff += idx * oldStrides[d];
                newOff += idx * newStrides[d];
            }
            if (bounds_check) {
                memcpy((char*)newData + newOff * arr->elemSize,
                       (char*)arr->data + oldOff * arr->elemSize,
                       (size_t)arr->elemSize);
            }
        }
    }

    // P1-12修复: BSTR/VARIANT - 旧区域中被丢弃的元素需要释放
    if (arr->data && (arr->elemType == vb6_sa_bstr || arr->elemType == vb6_sa_variant)) {
        for (int32_t linear = 0; linear < arr->totalElements; linear++) {
            int32_t tmp = linear;
            int32_t indices[16];
            int32_t d;
            for (d = arr->dimCount - 1; d >= 0; d--) {
                indices[d] = tmp % arr->bounds[d].cElements;
                tmp /= arr->bounds[d].cElements;
            }
            int32_t inNewBounds = 1;
            for (d = 0; d < arr->dimCount && d < dimCount; d++) {
                if (indices[d] >= newBounds[d].cElements) { inNewBounds = 0; break; }
            }
            if (arr->dimCount > dimCount) inNewBounds = 0;
            if (!inNewBounds) {
                if (arr->elemType == vb6_sa_bstr) {
                    BSTR* slot = (BSTR*)((char*)arr->data + linear * arr->elemSize);
                    if (*slot) vb6_BSTR_Free(*slot);
                } else if (arr->elemType == vb6_sa_variant) {
                    vb6_VARIANT* slot = (vb6_VARIANT*)((char*)arr->data + linear * arr->elemSize);
                    vb6_VariantClear(slot);
                }
            }
        }
    }

    if (arr->data) free(arr->data);
    arr->data = newData;
    arr->dimCount = dimCount;
    arr->totalElements = newTotal;
    for (int32_t d = 0; d < dimCount; d++)
        arr->bounds[d] = newBounds[d];
    for (int32_t d = dimCount; d < 16; d++) {
        arr->bounds[d].lBound = 0;
        arr->bounds[d].cElements = 0;
    }

    return arr;
}

// UDT版ReDim Preserve(多维): 元素尺寸由调用方提供 (sizeof(UDT))。
// NULL 初值不再回落 vb6_sa_empty(4字节), 避免 UDT 多维数组写入越界。
vb6_SafeArrayND* vb6_SafeArrayReDimPreserveND_Udt(int32_t elemSize,
    vb6_SafeArrayND* arr, int32_t dimCount, vb6_SafeArrayBound newBounds[]) {
    if (!arr) return vb6_SafeArrayReDimND_Udt(elemSize, dimCount, newBounds);

    if (arr->elemSize != elemSize) {
        vb6_SafeArrayDestroyND(arr);
        return vb6_SafeArrayReDimND_Udt(elemSize, dimCount, newBounds);
    }

    int32_t newTotal = 1;
    for (int32_t d = 0; d < dimCount; d++) {
        if (newBounds[d].cElements <= 0) {
            vb6_SafeArrayDestroyND(arr);
            return NULL;
        }
        newTotal *= newBounds[d].cElements;
    }

    void* newData = calloc((size_t)newTotal, (size_t)elemSize);
    if (!newData) return arr;

    if (arr->data && arr->totalElements > 0) {
        int32_t minDims = (dimCount < arr->dimCount) ? dimCount : arr->dimCount;

        int32_t copyCounts[16];
        int32_t oldCounts[16];
        int32_t newCounts[16];
        int32_t oldStrides[16];
        int32_t newStrides[16];

        for (int32_t d = 0; d < dimCount; d++)
            newCounts[d] = newBounds[d].cElements;
        for (int32_t d = 0; d < minDims; d++)
            oldCounts[d] = arr->bounds[d].cElements;
        for (int32_t d = minDims; d < 16; d++)
            oldCounts[d] = 0;

        for (int32_t d = 0; d < dimCount; d++)
            copyCounts[d] = (oldCounts[d] < newCounts[d]) ? oldCounts[d] : newCounts[d];

        oldStrides[minDims - 1] = 1;
        for (int32_t d = minDims - 2; d >= 0; d--)
            oldStrides[d] = oldStrides[d + 1] * arr->bounds[d + 1].cElements;

        newStrides[dimCount - 1] = 1;
        for (int32_t d = dimCount - 2; d >= 0; d--)
            newStrides[d] = newStrides[d + 1] * newBounds[d + 1].cElements;

        int32_t iterMax = 1;
        for (int32_t d = 0; d < minDims; d++)
            iterMax *= copyCounts[d];

        for (int32_t linear = 0; linear < iterMax; linear++) {
            int32_t tmp = linear;
            int32_t oldOff = 0, newOff = 0;
            int32_t bounds_check = 1;
            for (int32_t d = minDims - 1; d >= 0; d--) {
                int32_t idx = tmp % copyCounts[d];
                tmp /= copyCounts[d];
                if (idx >= oldCounts[d] || idx >= newCounts[d]) {
                    bounds_check = 0;
                    break;
                }
                oldOff += idx * oldStrides[d];
                newOff += idx * newStrides[d];
            }
            if (bounds_check) {
                memcpy((char*)newData + newOff * elemSize,
                       (char*)arr->data + oldOff * elemSize,
                       (size_t)elemSize);
            }
        }
    }

    if (arr->data) free(arr->data);
    arr->data = newData;
    arr->dimCount = dimCount;
    arr->elemSize = elemSize;
    arr->totalElements = newTotal;
    for (int32_t d = 0; d < dimCount; d++)
        arr->bounds[d] = newBounds[d];
    for (int32_t d = dimCount; d < 16; d++) {
        arr->bounds[d].lBound = 0;
        arr->bounds[d].cElements = 0;
    }

    return arr;
}

int32_t vb6_UBoundND(vb6_SafeArrayND* arr, int32_t dimension) {
    if (!arr || dimension < 1 || dimension > arr->dimCount) return 0;
    return arr->bounds[dimension - 1].lBound + arr->bounds[dimension - 1].cElements - 1;
}

int32_t vb6_LBoundND(vb6_SafeArrayND* arr, int32_t dimension) {
    if (!arr || dimension < 1 || dimension > arr->dimCount) return 0;
    return arr->bounds[dimension - 1].lBound;
}

