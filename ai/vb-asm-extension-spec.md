# VB 汇编扩展 · 语法与 Lowering 定稿

> 工具链约束：**MSVC**（x86 32 位有 `__asm{}` 内联；x64 无内联汇编，只能走 MASM 独立过程）。
> 设计取向：用户侧语法学习 **FreeBASIC** 的 `Asm...End Asm`（BASIC 原生、按名引用、干净），后端换成 **MASM + intrinsics**（适配 MSVC/x64）。
> 状态：**已实现**（2026-09-24）。x86 走 `__asm{}` 内联、x64 走 MASM 独立过程两条后端均已落地；
> `Asm/End Asm` 块 + 单行 `Asm <指令>` + `<Naked>` + `Clobber(...)` + callee-saved 自动保存均已实现，
> 测试见 `tests/asm/`（x64 正例 `asm_ok`、x86 正例 `asm_x86`、负例 2012/2014/3036/3037），门禁 `asm` 分类 + CI 矩阵。

---

## 1. 设计目标与范围

- 给 VB 家族语言加汇编能力，用于「高级语言做不到的操作 / 手写热点优化」。
- **v1 范围**
  - 支持 `<Naked>` 整函数汇编 + 普通函数内的 `Asm` 块。
  - **x64 v1 限制**：`Asm` 块内引用的变量只能是**函数参数**（参数本就在 ABI 寄存器里，无歧义）；VB 局部变量若要进 x64 汇编，显式作为参数传入（v2 再放开自动 spill 局部变量）。
- **后端分层**
  - x86 (32 位 MSVC)：直接降级为 `__asm { }` 内联块。
  - x64 (MSVC)：生成独立 MASM `.asm` 过程，用 `ml64.exe` 汇编后链接。
  - 常见原子 / SIMD / 位操作**优先用编译器 intrinsic**（`_InterlockedIncrement`、`_mm_add_ps` …），汇编只留给内建覆盖不到的极端需求。

---

## 2. 语法规范（用户侧）

### 2.1 关键字与结构

```vb
' 普通函数内的汇编块（共享函数栈帧）
Public Function AddFive(ByVal num As Long) As Long
    Asm
        mov eax, [num]
        add eax, 5
        mov [Function], eax
    End Asm
End Function

' Naked 整函数汇编（不生成 prologue/epilogue，asm 即函数体，自己 ret）
' ⚠ 指针与 cmpxchg 累加器不能共用 RAX/EAX —— 见 §11，正解是指针放 R10。
<Naked>
Public Function AtomicAdd(ByRef target As Long, ByVal addend As Long) As Long
    Asm
        mov r9d, edx            ' 其余参数先搬走 (addend)
        mov r10, [target]       ' 指针只加载一次, 循环内不再写 r10
    .retry:
        mov eax, [r10]          ' 累加器: 与 r10 各司其职
        mov r8d, eax
        add r8d, r9d
        lock cmpxchg [r10], r8d
        jne .retry
        mov [Function], eax     ' 返回旧值
        ret                     ' <Naked>: 尾部不自动补 ret, 自己写
    End Asm
End Function
```

### 2.2 规则

| 项 | 规则 |
|---|---|
| 块结构 | `Asm ... End Asm`；单行可用 `Asm <指令>`（过程体只有一条指令时）。 |
| 按名引用 | `[var]` 引用变量；编译器做栈帧 / ABI 寄存器替换（见 §4、§5）。 |
| 返回值 | `[Function]` 占位 → 映射到 ABI 返回寄存器（x86=eax，x64=rax）。x86 内联块里若整块没写 `[Function]`，收尾自动把 EAX 落到返回值（单行形式靠这条）。 |
| 注释 | 用 VB 风格 `'`，**不用** `;`（避免与汇编冲突）。 |
| 大小标注 | 用 **MASM 风格 `dword ptr [n]`**，不要用 GAS 的 `dword Ptr [n]`（那是 GAS 怪癖，别泄漏给用户）。 |
| 标签 | `.name:` 局部标签，`jmp .name` 引用；发射期重写为 `<过程名>_<name>`（MASM PROC 内无 proc 局部标签，同文件多 PROC 会撞名）。 |
| 寄存器命名 | 标准 Intel：`eax/rax`、`xmm0`…、`st(0)`…。 |
| 可选 clobber | `Asm Clobber("rbx","r12","memory") ... End Asm`，声明踩了哪些寄存器 / 内存，与块内静态扫描取并集后自动生成 push/pop。 |
| 属性 | `<Naked>` 独占一行写在 `Sub`/`Function` 之前（角括号属性行；修饰非过程 → 2012）。 |

**指令宽度必须一致**：MASM（以及 MSVC 内联汇编）不容许宽度不等的 `mov` —— `mov rbx, ecx`（64←32）
是 A2022，`mov eax, rax` 同理。32 位值就写 32 位寄存器（写 `ebx` 零扩展到 RBX），或显式
`movsxd rbx, ecx`；配合 `[Function]` 时也按返回宽度取寄存器（`Long` 返回写 `eax`，指针/64 位写 `rax`）。

**这条已由编译器兜底（2026-09-24）**：codegen 期用与后端完全同一张替换表（`asm_proc.hpp` 的
`asmBuildX64Subs` / `asmBuildX86Subs`）模拟代入后，对两个**纯寄存器**操作数做宽度校验
（`asmCheckRegWidths`），不一致直接报 **VB3038**（带 VB 源码位置与改法提示），不再漏到
ml64 的 A2022 / cl 的 C2443。内存操作数（`dword ptr [x]`）与变宽指令（movzx/movsx/movsxd/lea）
不做判定，仍交给汇编器。

### 2.3 By-name 引用语义（重要）

- `[var]`（局部 / ByVal 参数）→ 变量存储位置（取/存其值）。
- `[var]`（ByRef 参数）→ **引用单元本身**（即那个指针）。要取值需再解一次引用，例如 `mov eax,[var]`（取指针）后 `mov eax,[eax]`（解引用）。这是 FreeBASIC 的「变量即其地址」语义，需如实告知用户。
- `[Function]` → ABI 返回寄存器。

---

## 3. 示例一：AddFive（ByVal 参数，最简单形态）

**VB 源**（见 §2.1）。

**x86 降级（MSVC `__asm{}`，共享栈帧，按名引用天然成立）**

```cpp
// cl.exe 直接内联编译
long __stdcall AddFive(long num) {
    long r;
    __asm {
        mov eax, num
        add eax, 5
        mov r, eax
    }
    return r;   // 等价于 [Function] -> eax
}
```

**x64 降级（生成 MASM 过程，参数按 Win64 ABI 落 RCX）**

```asm
; === Auto-generated (ml64.exe) ===
; Win64 ABI: RCX = num (ByVal, 32-bit in ecx); return in RAX
_TEXT SEGMENT
AddFive PROC
    mov  eax, ecx          ; [num]  -> ecx
    add  eax, 5
    mov  rax, eax          ; [Function] -> RAX (返回)
    ret
AddFive ENDP
_TEXT ENDS
END
```

---

## 4. Lowering：x86（MSVC `__asm{}`）—— 已实现

- 把 `Asm...End Asm` 块 1:1 包进 `__asm { }`；`[var]`、`[Function]` 由 MSVC 内联汇编器按名解析（它原生支持引用局部变量名）。
- **callee-saved 自动保存**：非 `<Naked>` 块，编译器在块入口 push、出口 pop —— 集合 = 块内文本扫描命中的 callee-saved ∪ `Clobber(...)` 声明（x86 集：`ebx/esi/edi/ebp`；`r12–r15` 是 x64 独有，x86 下忽略）。
  只保存**实际用到**的，而不是无条件保存四个（减少无谓指令，且 `ebp` 帧指针在共享栈帧下更该少动）。
- **返回值收尾**：非 `<Naked>` 且块内没写 `[Function]` 时，自动追加 `mov <返回变量>, eax` —— 值按 VB 约定留在 EAX。单行形式 `Asm mov eax, [num]` 正是靠这条。
- `<Naked>` 产出 `__declspec(naked)` 函数：只有 `__asm` 块，无 prologue/epilogue、不自动补 `ret`。
  ⚠ x86 `<Naked>` **不能按名引用参数**（没有栈帧，参数在调用者的栈上，名字无从解析）→ 报 **3036**。
- 生成的 C 里回填变量初始值用 `= 0` 而**不是** `{}` —— `/std:c11` 下空花括号初始化标量是 C23 语法，cl 报 C2143（实测踩过）。
- 仅 32 位可用；x64 目标自动走 MASM 路径（见 §5），用户侧语法不变。

---

## 5. Lowering：x64（MASM 独立过程）

x64 MSVC 无内联汇编，因此每个含 `Asm` 块的函数降级为一个独立 MASM 过程，按 Win64 ABI 接收参数、返回结果。

**绑定模型（v1）**
1. 参数按 Win64 ABI 落 `RCX, RDX, R8, R9`（整型）；返回值 `RAX`。
2. 被引用的参数：ByVal 直接映射到对应寄存器（如 `[num]`→`ecx`）；ByRef 映射到指针寄存器（如 `[target]`→`[rcx]`）。
3. 若块内实际踩到 callee-saved 寄存器（`RBX, RBP, RDI, RSI, R12–R15`），编译器**据静态分析 + clobber 声明自动生成 push/pop**（x64 无 MSVC 隐式保存，必须自己来）。
4. 栈对齐：函数入口 `RSP` 须 16 字节对齐；`<Naked>` 下用户自行平衡，`push` 次数为偶。

**示例二：AtomicAdd（ByRef + 解引用，展示完整模型）**

```asm
; === Auto-generated (ml64.exe) ===
; Win64 ABI: RCX=target(ptr), RDX=add(value); return in RAX
; (本块只用了 caller-saved 寄存器，无需 push/pop callee-saved)
_TEXT SEGMENT
AtomicAdd PROC
    sub  rsp, 32            ; shadow space(4*8)，保持 16B 对齐
    mov  rax, rcx           ; rax = target 指针      (was: mov eax,[target])
    mov  eax, [rax]         ; eax = *target
retry:
    mov  r8d, eax           ; r8d = 当前值
    add  r8d, edx           ; + add（edx = 第二参数值）
    mov  rax, rcx           ; 重载指针
    lock cmpxchg [rax], r8d ; 若 *target==eax 则写入 r8d，否则 eax=*target
    jne  retry
    mov  rax, eax           ; [Function] -> RAX（返回旧值）
    add  rsp, 32
    ret
AtomicAdd ENDP
_TEXT ENDS
END
```

> 注：上例未踩任何 callee-saved 寄存器，故无 push/pop。若用户 asm 用了 `rbx`，编译器会插入 `push rbx ... pop rbx` 并相应调整 `rsp` 偏移。

---

## 6. ABI 表（平台感知 —— 核心工作量）

| 平台 / 约定 | 参数传递 | 返回值 | 被调用者保存 (callee) | 调用者保存 (caller) | 栈对齐 |
|---|---|---|---|---|---|
| Win32 `cdecl` | 右→左压栈，caller 清栈 | eax (64位: edx:eax) | ebx, esi, edi, ebp, esp | eax, ecx, edx | 4B |
| Win32 `stdcall` | 右→左压栈，callee 清栈 | eax | 同上 | 同上 | 4B |
| **Win64** | RCX,RDX,R8,R9 + 栈(右→左) | RAX | RBX,RBP,RDI,RSI,R12–R15 | RAX,RCX,RDX,R8–R11 | **16B** |

> 换架构（如 ARM64 AAPCS）只填一张新表，前端语法与后端编码不动——三层解耦的价值。

---

## 7. 寄存器保存策略小结

- **x86 非 Naked**：编译器自动 push/pop `ebx, esi, edi, ebp`。
- **x64 MASM 过程**：编译器据块内使用 + clobber 声明，自动生成 callee-saved 的 push/pop；RSP 对齐由编译器 prologue/epilogue 维护。
- **`<Naked>`（任意平台）**：不生成任何保存/对齐代码，用户自己写 prologue/epilogue、`ret`、平衡栈。

---

## 8. 与 FreeBASIC 的对照（为何如此设计）

| FreeBASIC 做法 | 本设计 | 理由 |
|---|---|---|
| `Asm...End Asm` + 按名引用 + `[Function]` | **照抄** | 实战验证过的 BASIC 原生 UX，干净可读。 |
| 后端 = GAS（`.intel_syntax noprefix`） | **换成 MASM + intrinsics** | FB 是 GCC/LLVM 系；你们是 MSVC，且 x64 无内联汇编，GAS 路径物理不成立。 |
| 仅 x86 32 位可用 | **x86+x64** | x64 走 MASM 过程补齐。 |
| `dword Ptr [n]`（GAS 怪癖） | **`dword ptr [n]`（MASM）** | 后端是 MASM，别让用户背 GAS 的锅。 |
| 隐式只 push/pop `ebx,esi,edi` | **据 ABI 表 + clobber 自动保存** | x64 callee-saved 集不同，且 MASM 路径需自己管保存。 |
| 无 clobber 声明（乐观假设） | **保留可选 clobber** | x64 独立过程下，显式声明能让编译器自动生成保存代码，更稳。 |

---

## 9. 构建集成（build hook）

- **x86**：`cl.exe` 直接编译 `__asm{}`，无需额外步骤。
- **x64**：编译期扫描含 `Asm` 块的模块 → 生成 `.asm` → 调 `ml64.exe /c` 出 `.obj` → 链接进最终镜像。该 hook 是一次性的，不是每条指令的活。
- 推荐先跑通「VB 源 → 生成 `.asm` → `ml64` → 链接」的最小闭环，再补 x86 `__asm{}` 糖。

---

## 10. 开放问题 / 待办

1. **x86 `<Naked>` 不能按名引用参数** —— naked 无栈帧, x86 参数在调用者栈上, 名字无从解析。
   已实现为编译期诊断 **3036**（唯一来源）。x64 无此限制（参数在 ABI 寄存器里）。
2. x64 下是否放开「引用 VB 局部变量」（需自动 spill 到栈并映射）——当前限定仅参数。
   注: Asm 过程体独占整个过程, 此时并不存在 VB 局部变量; 仅在放开混排后才成为真问题。
3. 行内混排（非 Naked 的语句间 asm）是否支持——当前仅整块；混排需与寄存器分配器深度耦合，风险高，暂缓。
4. ARM64 后端（AAPCS 表 + `armasm64`/`clang` 集成）——待 x64 跑通后评估（已具备 x86/x64 两套 ABI 表）。
5. 标签 / 外部符号重定位：独立 MASM 过程由 `ml64` + 链接器处理；若未来做裸字节 Emit 逃生舱，需自行生成重定位项。
6. 浮点/向量参数与返回（x64 走 XMM0–3）：当前 Asm 过程只接受整型与指针 —— 浮点按值参数与
   `Single`/`Double` 返回均报 3037。
7. x86 `int64_t` 返回（edx:eax 双寄存器）与 x64 栈传参（>4 参）：报 3037。

---

## 11. 实测陷阱（2026-09-24 闭环验证时踩过，写给下一个写 Asm 块的人）

**cmpxchg 与 RAX/EAX 别名（双重陷阱，互斥暴露）**：x64 下 EAX 是 RAX 的低 32 位 —— 同一个物理寄存器。`lock cmpxchg [mem], r32` 的隐含累加器是 EAX，于是：

1. **指针放 RAX + 循环内重载**：`mov rax, [target]` … `mov eax,[rax]` … 循环内再次 `mov rax, [target]` 重载指针 → 写 RAX 连带摧毁 EAX（变成指针低 32 位）→ cmpxchg 拿指针低 32 位与内存比 → 永不相等 → `jne` 死循环（症状：满 CPU 空转，返回值"看起来正常"，因为 cmpxchg 失败路径会把内存值装回 EAX）。
2. **不重载**：`mov eax, [rax]` 之后 RAX 只剩加载值 → `cmpxchg [rax]` 访问"以值为地址" → AV 崩溃（症状：第一条打印之后段错误）。

两个 bug 不会同时出现：修掉一个必然暴露另一个。**正解：指针放 R10/R11（caller-saved），累加器独占 RAX/EAX**：

```asm
mov r9d, edx            ' 其余参数先搬去不冲突的寄存器
mov r10, [target]       ' 指针只加载一次, 循环内绝不再写 r10
.retry:
mov eax, [r10]          ' 累加器
mov r8d, eax
add r8d, r9d
lock cmpxchg [r10], r8d
jne .retry
mov [Function], eax     ' 返回旧值
```

正例夹具 `tests/asm/AsmTest.bas` 的 AtomicAdd 即此模板；定位过程（探针 p5 抓 ZF、p6 四变体二分）留在 `.temp/asmtest/` 可复查。

**环境**：`run_tests.ps1` 自建 MSVC 环境（vswhere/C3_VCVARSALL + 动态发现 SDK，本机 SDK 在 `D:\Windows Kits\10`）。手工复现时**不要再额外 call vcvars64** —— 反而会把 INCLUDE 搅乱（C1083 stddef.h）。
