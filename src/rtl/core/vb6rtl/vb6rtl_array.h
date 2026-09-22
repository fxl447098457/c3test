#pragma once
// vb6rtl_array.h - SAFEARRAY（一维 / 多维）与错误状态栈
// 由 vb6rtl.h 伞头 include；生成代码不要直接 include 本文件
#include "vb6rtl_base.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// SAFEARRAY - VB6 动态/静态数组
// ============================================================

// 元素类型标识 (用于ReDim/Erase时正确清理)
typedef enum vb6_safearray_elemtype {
    vb6_sa_empty = 0,
    vb6_sa_bool  = 1,   // int16_t
    vb6_sa_byte  = 2,   // uint8_t
    vb6_sa_int   = 3,   // int16_t
    vb6_sa_long  = 4,   // int32_t
    vb6_sa_single= 5,   // float
    vb6_sa_double= 6,   // double
    vb6_sa_bstr  = 7,   // BSTR (需要逐元素释放)
    vb6_sa_variant=8,   // vb6_VARIANT (需要逐元素清理)
    vb6_sa_ptr   = 9,   // void* (对象引用)
    vb6_sa_currency = 10, // int64_t (VB6 Currency: value*10000)
    vb6_sa_udt    = 11,   // UDT (elemSize set externally)
} vb6_safearray_elemtype;

// 一维数组描述符 (VB6绝大多数用例是一维)
typedef struct vb6_SafeArray1D {
    int32_t signature;                  // Fix 082g: 魔数标识1D数组 = 0x5A1D ('SA1D')
    vb6_safearray_elemtype elemType;  // 元素类型
    int32_t elemSize;                  // 单个元素字节数
    int32_t lBound;                    // 下界 (VB6默认0, 可指定1)
    int32_t uBound;                    // 上界
    int32_t count;                     // 元素个数 = uBound - lBound + 1
    void*   data;                      // 数据指针 (calloc分配, 零初始化)
    int32_t isDynamic;                 // 是否动态数组 (ReDim创建)
} vb6_SafeArray1D;

// P14.2.3: 字符串数组函数
vb6_SafeArray1D* vb6_Split(BSTR expr, BSTR delimiter, int32_t limit, int32_t compare);
BSTR vb6_Join(vb6_SafeArray1D* arr, BSTR delimiter);

// 创建一维静态数组
vb6_SafeArray1D* vb6_SafeArrayCreate1D(vb6_safearray_elemtype elemType,
    int32_t lBound, int32_t uBound);

// 创建一维动态数组 (ReDim)
vb6_SafeArray1D* vb6_SafeArrayReDim1D(vb6_safearray_elemtype elemType,
    int32_t lBound, int32_t uBound);

// UDT版ReDim: elemSize由调用方提供 (用于UDT动态数组)
vb6_SafeArray1D* vb6_SafeArrayReDim1D_Udt(int32_t elemSize,
    int32_t lBound, int32_t uBound);

// ReDim Preserve: 保留原有数据, 调整大小
vb6_SafeArray1D* vb6_SafeArrayReDimPreserve1D(vb6_SafeArray1D* arr,
    int32_t newLBound, int32_t newUBound);

// UDT版ReDim Preserve: elemSize由调用方提供 (用于UDT动态数组)
vb6_SafeArray1D* vb6_SafeArrayReDimPreserve1D_Udt(int32_t elemSize,
    vb6_SafeArray1D* arr, int32_t newLBound, int32_t newUBound);

// 销毁数组 (释放内存)
void vb6_SafeArrayDestroy1D(vb6_SafeArray1D* arr);

// Fix 170: 整体数组赋值 `A() = B()` —— 深拷贝 src 为新载体返回, 原 dst 销毁
vb6_SafeArray1D* vb6_ArrayAssign1D(vb6_SafeArray1D* dst, vb6_SafeArray1D* src);

// Fix 178: 元素是「含所有权成员的 UDT」(vb6_sa_udt) 时, memcpy 之后每个槽位仍指向
// src 的元素 → 各行别名到同一份存储。cb(dstElem, srcElem) 负责把 src 元素深拷贝进
// dst 元素 (dst 元素此刻是 src 的位拷贝, 其中指针归 src 所有, 不得释放)。
// cb==NULL 等价于 Fix 170 的原行为。
typedef void (*vb6_udt_elem_copy)(void* dst, const void* src);
vb6_SafeArray1D* vb6_ArrayAssign1D_Cb(vb6_SafeArray1D* dst, vb6_SafeArray1D* src,
                                      vb6_udt_elem_copy cb);

// 获取/设置元素 (void*通用版)
void* vb6_SafeArrayGetPtr(vb6_SafeArray1D* arr, int32_t index);
void  vb6_SafeArrayPutElem(vb6_SafeArray1D* arr, int32_t index, void* value);

// 便捷宏: 按类型访问元素
// Fix 106: VB6 允许用非整型表达式做数组下标 (如 Dim i As Single 后 arr(i)),
// 由 VB6 隐式转换为 Long。这里把下标差值显式转 int32_t, 否则 C 端
// float/double 下标直接报 C2108 (Charts 2020 ucChartArea: 202 处),
// 且级联出 C2198 等二次错误。Variant 下标仍由生成端 toLongIfVariant 处理。
#define VB6_SA_AT(type, arr, idx) \
    (((type*)((arr)->data))[(int32_t)((idx) - (arr)->lBound)])

// UBound/LBound (替换旧stub)
int32_t vb6_UBound(vb6_SafeArray1D* safeArray, int32_t dimension);
int32_t vb6_LBound(vb6_SafeArray1D* safeArray, int32_t dimension);

// P21-B: Array() function support
vb6_SafeArray1D* vb6_ArrayCreate(int32_t count);
void vb6_ArraySetLong(vb6_SafeArray1D* arr, int32_t index, int32_t val);
void vb6_ArraySetDouble(vb6_SafeArray1D* arr, int32_t index, double val);
void vb6_ArraySetBSTR(vb6_SafeArray1D* arr, int32_t index, BSTR val);
void vb6_ArraySetVariant(vb6_SafeArray1D* arr, int32_t index, vb6_VARIANT val);
// ============================================================
// SAFEARRAY ND - VB6 多维数组
// ============================================================

typedef struct vb6_SafeArrayBound {
    int32_t lBound;
    int32_t cElements;
} vb6_SafeArrayBound;

typedef struct vb6_SafeArrayND {
    int32_t dimCount;
    vb6_safearray_elemtype elemType;
    int32_t elemSize;
    int32_t totalElements;
    void* data;
    vb6_SafeArrayBound bounds[16];
} vb6_SafeArrayND;

vb6_SafeArrayND* vb6_SafeArrayCreateND(vb6_safearray_elemtype elemType,
    int32_t dimCount, vb6_SafeArrayBound bounds[]);
void vb6_SafeArrayDestroyND(vb6_SafeArrayND* arr);
vb6_SafeArrayND* vb6_SafeArrayReDimND(vb6_safearray_elemtype elemType,
    int32_t dimCount, vb6_SafeArrayBound bounds[]);
vb6_SafeArrayND* vb6_SafeArrayReDimND_Udt(int32_t elemSize,
    int32_t dimCount, vb6_SafeArrayBound bounds[]);
vb6_SafeArrayND* vb6_SafeArrayReDimPreserveND(vb6_SafeArrayND* arr,
    int32_t dimCount, vb6_SafeArrayBound newBounds[]);

// UDT版ReDim Preserve(多维): elemSize由调用方提供
vb6_SafeArrayND* vb6_SafeArrayReDimPreserveND_Udt(int32_t elemSize,
    vb6_SafeArrayND* arr, int32_t dimCount, vb6_SafeArrayBound newBounds[]);

int32_t vb6_SafeArrayND_Offset(vb6_SafeArrayND* arr, int32_t dimCount, int32_t indices[]);
void* vb6_SafeArrayND_GetPtr(vb6_SafeArrayND* arr, ...);

int32_t vb6_UBoundND(vb6_SafeArrayND* arr, int32_t dimension);
int32_t vb6_LBoundND(vb6_SafeArrayND* arr, int32_t dimension);

// Fix 106: 同 VB6_SA_AT, 多维下标也显式转 int32_t (VB6 隐式 CLng).
#define VB6_SA_ND_AT1(elemType, arr, i) \
    (*((elemType*)((arr)->data) + \
       ((int32_t)((i) - (arr)->bounds[0].lBound))))

#define VB6_SA_ND_AT2(elemType, arr, i, j) \
    (*((elemType*)((arr)->data) + \
       (((int32_t)((i) - (arr)->bounds[0].lBound)) + \
        ((int32_t)((j) - (arr)->bounds[1].lBound)) * (arr)->bounds[0].cElements)))

#define VB6_SA_ND_AT3(elemType, arr, i, j, k) \
    (*((elemType*)((arr)->data) + \
       (((int32_t)((i) - (arr)->bounds[0].lBound)) + \
        ((int32_t)((j) - (arr)->bounds[1].lBound)) * (arr)->bounds[0].cElements + \
        ((int32_t)((k) - (arr)->bounds[2].lBound)) * (arr)->bounds[0].cElements * (arr)->bounds[1].cElements)))

// 文件 I/O
int32_t vb6_FreeFile(void);
int32_t vb6_Open(BSTR pathname, int32_t mode, int32_t access, int32_t filenumber, int32_t reclength);
int32_t vb6_Close(int32_t filenumber);
int32_t vb6_CloseAll();  // Close all open files
int32_t vb6_EOF(int32_t filenumber);
int32_t vb6_LOF(int32_t filenumber);
int32_t vb6_Loc(int32_t filenumber);// P21-13: Seek function (return current file position)
int32_t vb6_SeekFunc(int32_t filenumber);
// P21-13: Seek statement (set file position)
void vb6_SeekStmt(int32_t filenumber, int32_t position);

// P22-08: Width# — set file output line width
void vb6_Width(int32_t filenumber, int32_t width);

void vb6_Print(int32_t filenumber, BSTR s);
void vb6_Write(int32_t filenumber, BSTR s);
BSTR vb6_LineInput(int32_t filenumber);
int32_t vb6_Input(int32_t filenumber, BSTR* outVar);
BSTR vb6_InputString(int32_t filenumber, int32_t count);  // P15.4: Input function
int32_t vb6_Kill(BSTR pathname);
int32_t vb6_MkDir(BSTR pathname);
int32_t vb6_RmDir(BSTR pathname);
int32_t vb6_ChDir(BSTR pathname);
int32_t vb6_ChDrive(BSTR drive);
int32_t vb6_Name(BSTR oldPath, BSTR newPath);
int32_t vb6_FileCopy(BSTR source, BSTR destination);

// P8.2: 随机/二进制文件访问 (Get/Put)
int32_t vb6_Get(int32_t filenumber, int32_t recnumber, void* varPtr, int32_t varSize);
int32_t vb6_Put(int32_t filenumber, int32_t recnumber, void* varPtr, int32_t varSize);

// 错误处理
// Variant比较 (VB6语义: 数值vs数值, 字符串vs字符串, 混合转Double)
int32_t vb6_VarCmpEq(vb6_VARIANT* a, vb6_VARIANT* b);
int32_t vb6_VarCmpNe(vb6_VARIANT* a, vb6_VARIANT* b);
int32_t vb6_VarCmpLt(vb6_VARIANT* a, vb6_VARIANT* b);
int32_t vb6_VarCmpGt(vb6_VARIANT* a, vb6_VARIANT* b);
int32_t vb6_VarCmpLe(vb6_VARIANT* a, vb6_VARIANT* b);
int32_t vb6_VarCmpGe(vb6_VARIANT* a, vb6_VARIANT* b);
int32_t vb6_VarCmpLongEq(vb6_VARIANT* a, int32_t b);
int32_t vb6_VarCmpLongLt(vb6_VARIANT* a, int32_t b);
int32_t vb6_VarCmpLongGt(vb6_VARIANT* a, int32_t b);
int32_t vb6_VarCmpLongLe(vb6_VARIANT* a, int32_t b);
int32_t vb6_VarCmpLongGe(vb6_VARIANT* a, int32_t b);
int32_t vb6_VarCmpLongNe(vb6_VARIANT* a, int32_t b);

int32_t vb6_ErrNumber(void);
BSTR vb6_ErrDescription(void);
void vb6_ErrClear(void);
void vb6_RaiseError(int32_t errNum, BSTR description);
BSTR vb6_ErrSource(void);
void vb6_ErrRaise(int32_t errNum, BSTR source, BSTR description);
void vb6_ErrRaiseNumber(int32_t errNum);

// 全局错误处理状态 (由cgen生成的代码直接使用)
extern int32_t vb6_err_resume_next;  // On Error Resume Next 标志
extern int32_t vb6_err_jmp_active;    // On Error GoTo label 标志
extern void* vb6_err_handler_label;   // 错误跳转标签 (MVP, 暂不用)
extern jmp_buf* vb6_error_jmp_ptr;    // 指向当前函数的局部jmp_buf (P12.3)
extern int32_t vb6_error_jmp_set;     // setjmp 是否已设置

// P14.1.2: Resume恢复点跟踪
extern int32_t vb6_err_resume_point;    // 出错语句resume点索引
extern int32_t vb6_err_resume_next_point;   // 出错下一句resume点索引
extern int32_t vb6_err_dispatch;        // dispatch switch变量
extern int32_t vb6_err_in_handler;      // 当前在On Error GoTo处理器中

// P12.3: On Error嵌套 — 保存/恢复调用者的错误处理状态
#define VB6_ERR_STACK_SIZE 8
void vb6_SaveErrState(void);      // 保存当前错误状态到栈 (函数入口调用)
void vb6_RestoreErrState(void);   // 从栈恢复错误状态 (函数出口调用)

// 字典访问
BSTR vb6_BSTR_Concat(BSTR a, BSTR b);

#ifdef __cplusplus
}
#endif
