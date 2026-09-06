# C3Fix - cZipArchive 簇清零 (090o-p: For拆分标签后缀/Erase Variant字段/Date参数提取/As Any UDT实参临时变量)

**日期**: 2026-09-06
**里程碑**: 090o-p
**回归**: 全量 vbman 155 → 142（C2440 85→79）；cZipArchive 簇 6 → 0（error C = 0）
**提交**: aacaebb（代码）、2702ab1（HANDOFF + vbman/dist/c3-error.log）

## 问题背景

`cZipArchive` VFS 模块簇（cZipArchive + cMemoryStream + cBufferStream + cDummyStream 四模块裁剪工程，
`FIX_cZipArchive.vbp`）残留 6 个 MSVC 编译错误，全部清零：

```
cZipArchive.c(683) C2094: goto vb6_label_QH_d2 标签未定义（事件回调内 GoTo 出口标签）
(2439)  C2440: vb6_VariantArrayGet(...) VARIANT → double（pvToFileTime ByVal Date 形参）
(2658)  C2440 x2: Erase uFile.BufferArray 裸 SafeArrayDestroy1D(VARIANT)
(2672)  C2440 + C2198: (void*)(intptr_t)(vb6_type_FILETIME 值) 强转 struct
```

## 根因与修复（各一条）

### 090o — For 方向拆分副本的 GoTo 标签后缀过度附加（C2094）

`For i = lo To hi` 生成方向拆分的两份副本时，body 内标签会定义两次（`vb6_label_X` 与 `vb6_label_X_d2`）。
原 Fix 086 在 `labelCopyIdx_ > 0` 时一律给 GoTo 加 `_dN` 后缀——但若目标是**函数级出口标签**（QH/EH，
GoTo 从循环跳出到过程尾），标签只定义一份，加后缀指向不存在的 `vb6_label_QH_d2` → C2094。

修复：新增 `forSplitLabelStack_` 栈 + `collectForBodyLabels` 递归收集被拆分 For body 内定义的标签集；
GoTo 后缀仅在「labelCopyIdx_ > 0 且目标标签在栈中某 For body 内」时附加。

### 090p — Erase 对 UDT 的 As Variant 数组字段未走 Variant 感知（C2440 x2）

`Erase uFile.BufferArray`（BufferArray 字段 As Variant，uFile 是 ByRef UDT 参数）生成
`vb6_SafeArrayDestroy1D((*uFile).BufferArray); (*uFile).BufferArray = NULL` —— 把 VARIANT 当 SafeArray*。

修复：把 090m 的「UDT 字段 As Variant」判定提炼为成员函数 `isVariantArrayTarget(name)`
（ReDim 084a/090m 逻辑重构复用），Erase 命中时改 `vb6_VariantClear(&name);`（释放数组并置 VT_EMPTY）。

### 090r — 参数打包 Variant→double 提取漏 Vb6Type::Date（C2440）

实参是 Variant（数组元素 `vb6_VariantArrayGet`）、形参 ByVal Date 时，提取分支只有
Double/Single/Currency（无 Date）→ VARIANT 裸传 double 形参。同行 pvFromInt64（Currency）有
`vb6_VariantToDouble` 形成差异对照。修复：分支补 `|| paramBase == Vb6Type::Date`（OLE date = double）。

### 090q — Declare As Any 参数实参是「返回 UDT 的函数调用」（C2440 + C2198）

`SetFileTime hFile, 0, 0, pvToFileTime(...)`（FILETIME）：`(void*)(intptr_t)(struct 值)` → C2440；
尝试复合字面量 `(T){fnRetUdt()}` 也被 MSVC C 拒绝（C2440 "初始化…无法从 T 转换"——实测确认
MSVC 对 struct 函数返回初始化器只支持 `{e1, e2, ...}` 字段列表，不支持单值拷贝）。

修复（三处协同）：
1. `cgen_base.cpp generate()` 入口预扫模块 declarations，把返回 UDT 的函数/方法注册进
   `funcUdtRetCType_`（小写名 → C 返回类型，如 `pvtofiletime` → `vb6_type_FILETIME`）。
   **必须预扫**：类方法无 VB 顺序约束，调用点（pvVfsSetEof 函数体）可能先于 pvToFileTime 声明输出。
2. `CodeEmitter` 新增 pending 行机制（`addPending` + `flushPending`）：表达式拼接期无法在行中插语句，
   声明行挂 pending，`emitLine`/`emitBlank` 时先落地再输出引用行 → 「声明先于引用」恒成立。
3. As Any 打包命中 UDT 返回调用时：`argVal = "vb6_type_X _vb6_anytmpN = <call>;"`（addPending）
   + `argVal = "(void*)&_vb6_anytmpN"`。

## 验证

- 裁剪簇 `FIX_cZipArchive.vbp`：errors 5 → 0（C3 exit 1 仅链接缺 vb6rtl di stub 支撑，fix_tool 口径 `error C` = 0）
- 全量 VBMAN 回归：155 → 142（C2440 85→79），`cZipArchive error C: 0`

## 备注（遗留，非本簇范围）

- 裁剪簇链接有 23 个 LNK2019（vb6rtl di stub 未覆盖的 Declare 转发 + With COM 对象 `.Replace`
  被误生成为 `vb6_cZipArchive_Replace` 类方法调用）——未入 fix_tool 判定口径，待独立专项。
