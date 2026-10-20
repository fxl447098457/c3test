// owned_temp_keeps.hpp — 账 #454 §B164 形① 那张 KEEPS 表的**发码侧可问形态**（单一权威）
//
// 这一张表回答一个问题：**一条语句里现造的带权临时量，传进去之后谁还留着它**。
// 三条轴，各答各的：
//   keeps_pointer  指针/槽位/引用有没有**带出调用**（0 = 没带出 / 1 = 带出了 / U = 证不出）
//                 ⇒ **回收只看这一轴**（放掉它之前必须问这一句）
//   persists_data  这条形参的**数据**有没有落到比调用活得久的地方（注册表写 / 文件写 / 剪贴板）
//                 ⇒ 这一轴**不参与回收**（数据持久化 ≠ 指针留存）
//   gate           closed 的行**不进提名面**（注册表那一族：运行期破坏未归因 ⇒ 停机）
// 另有一张**借用轴**（返回侧）：这一枚出口**交出去的指针是借来的**，不归调用方所有 ⇒ 调用方不许放它。
//
// **为什么要两份形态**（一张表在 scripts/ 里、一份在 src/common/ 里）：
//   * `scripts/owned_temp_keeps_164.txt` = **推导出来**的整张表（出口面出自 RTL 的 24 个权威头，
//     由 `scripts/check_owned_temp_keeps.ps1` 现算）⇒ 它是**证据**。
//   * 本头 = **给发码侧问的那一份**（只有例外行：KEEPS=1 / U / closed / 借用族）⇒ 它是**出口**。
//   两者由那道哨兵**双向**对账（少一行/多一行/分类不同都红）⇒ 漂不了。
//   这与仓里既有的做法同形：`kBorrowedBstrExports167` 是手写的**出口**，
//   `check_bstr_borrow_slot.ps1` 的 Q6 从 RTL **推导**后双向对账。
//
// **本题的由来**：撤回那一版（9546dbfe / 6bf5989 / be5d6abf）**只问了 mint 那一侧**
// （「这一格是不是现造的带权临时量」），**没问 borrow 那一侧**（「这一格是借来的吗」），
// 也**从没问消费者那一侧**（「这个消费者会不会把指针留下来」）⇒ 它把
// `vb6_BSTR_Concat` 换成 `vb6_BSTR_ConcatFree`，放掉了**借来的** `vb6_ErrDescription()`
// （RTL 全局 vb6_err 的指针）⇒ 全局悬空 ⇒ 读数变堆垃圾（`test_bstr_borrow_slot`）、
// `test_host_decimal`(x86) 直接 0xC0000374。归因见台账 §B227。
//
// 规矩：**消费者那一侧答不出（U）⇒ 不许提名**；**借用族 ⇒ 一律不许放**；
// **closed ⇒ 一律不许提名**（未归因 ⇒ 停机）。
#pragma once
#include <cstddef>
#include <cstring>

namespace vb6c3 {

// 一行 = 一枚例外（出口名 + 形参序号 + 分类 + 逃逸通道）
struct OwnedTempException {
    const char* exitName;   // 出口名（vb6_*）
    int         slot;       // 形参序号（0 起）
    char        keeps;      // '1' = 指针带出调用 / 'U' = 证不出
    const char* escape;     // 逃逸通道：store / ret / write / handoff-*（为什么是这一枚）
};

// 例外行：**只有**这些不许提名（其余的出口默认 keeps_pointer=0 ⇒ 提名面内）。
// 26 枚 KEEPS=1 + 1 枚 U（vb6_DecSetTag 第 0 格）—— 与台账 §B227 的表逐行对齐。
inline const OwnedTempException* ownedTempExceptions() {
    static const OwnedTempException kRows[] = {
        { "vb6_ArraySetVariant",  2, '1', "write" },
        { "vb6_BSTR_AssignMove",  1, '1', "write" },
        { "vb6_CDec",             0, '1', "ret" },
        { "vb6_DecimalAsVariant", 0, '1', "write" },
        { "vb6_DecimalToBSTR",    0, '1', "store" },
        { "vb6_DecimalToR8",      0, '1', "store" },
        { "vb6_DecSetTag",        0, 'U', "handoff-unknown:vb6_DEC_VT" },
        { "vb6_ErrRaise",         1, '1', "store" },
        { "vb6_ErrRaise",         2, '1', "store" },
        { "vb6_ho_setVariantBstr",1, '1', "store" },
        { "vb6_IIfVariant",       1, '1', "ret+write" },
        { "vb6_IIfVariant",       2, '1', "ret+write" },
        { "vb6_LoadResData",      0, '1', "write" },
        { "vb6_LoadResData",      1, '1', "write" },
        { "vb6_LoadResPicture",   0, '1', "write" },
        { "vb6_LoadResPicture",   1, '1', "write" },
        { "vb6_LoadResString",    0, '1', "write" },
        { "vb6_RaiseError",       1, '1', "store" },
        { "vb6_SendKeys",         0, '1', "write" },
        { "vb6_VariantArrayGetVal",0, '1', "write" },
        { "vb6_VariantArraySet",  2, '1', "write" },
        { "vb6_VariantAssign",    1, '1', "write" },
        { "vb6_VariantIdentity",  0, '1', "ret" },
        { "vb6_VariantString",    0, '1', "store" },
        { "vb6_VariantToBool",    0, '1', "store" },
        { "vb6_VariantToLong",    0, '1', "store" },
        { "vb6_VariantToLongPtr", 0, '1', "write" },
    };
    return kRows;
}

// closed 的那一族（注册表）：运行期破坏未归因 ⇒ 不进提名面。
// 名单与 scripts/owned_temp_keeps_164.txt 的 gate=closed 那一节逐行对齐（哨兵双向对账）。
inline const char* const* ownedTempClosedExits() {
    static const char* const kClosed[] = {
        "vb6_SaveSetting", "vb6_GetSetting", "vb6_DeleteSetting", "vb6_GetAllSettings",
    };
    return kClosed;
}

// 借用族（返回侧）：这一枚出口**交出去的指针是借来的** ⇒ 调用方不许放它。
// 与编译器侧 kBorrowedBstrExports167 同名单（哨兵双向对账；那份名单本身由
// check_bstr_borrow_slot.ps1 的 Q6 从 RTL 双向推导钉住）。
inline const char* const* ownedTempBorrowedExits() {
    static const char* const kBorrowed[] = {
        "vb6_ErrDescription", "vb6_ErrSource", "vb6_ho_variantToBstr",
    };
    return kBorrowed;
}

// 消费者那一侧的答案：这一枚出口的**这一格**会不会把指针带出调用？
//   '0' = 不会（可提名）  '1' = 会（禁止提名）  'U' = 证不出（禁止提名）
inline char ownedTempKeepsPointerKind(const char* exitName, int slot) {
    if (!exitName) return 'U';
    for (const OwnedTempException* r = ownedTempExceptions(); r->exitName; ++r) {
        if (r->slot == slot && std::strcmp(r->exitName, exitName) == 0) return r->keeps;
    }
    return '0';
}

// 这一族进不进提名面？closed ⇒ 不进（未归因 ⇒ 停机）。
inline bool ownedTempConsumerClosed(const char* exitName) {
    if (!exitName) return true;
    for (const char* const* p = ownedTempClosedExits(); *p; ++p) {
        if (std::strcmp(*p, exitName) == 0) return true;
    }
    return false;
}

// 这一枚出口交出去的是**借来的**指针吗？借来的 ⇒ 调用方不许放它。
inline bool ownedTempExitIsBorrowed(const char* exitName) {
    if (!exitName) return true;
    for (const char* const* p = ownedTempBorrowedExits(); *p; ++p) {
        if (std::strcmp(*p, exitName) == 0) return true;
    }
    return false;
}

}  // namespace vb6c3
