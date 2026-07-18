# BugFix - COM DLL DISPID不一致修复 (Calc.Add "Could not find member")

**日期**: 2026-07-18
**里程碑**: M29
**回归**: 82/82 PASS
**提交**: (待补)

## 问题背景

ActiveX DLL工程编译注册后，通过PowerShell COM调用任何方法均失败：

```powershell
$c = New-Object -ComObject TestAXDLL.Calc
$c.Add(1, 2)
# Error: Could not find member. (HRESULT 0x80131512 / DISP_E_MEMBERNOTFOUND)
```

`CreateObject`/`New-Object -ComObject`成功（CLSID链路正确），但`Add`/`Multiply`/`SetValue`/`GetValue`全部报"找不到成员"。所有4个类（StringLib/ExtLib/Calc/MathLib）所有方法均失败。

## 根因（三层）

### 第一层：TypeLib与dll_entry.c DISPID分配方式不一致

TypeLib builder（`driver.cpp:1243`）使用**全局连续DISPID**——`dispidCounter`定义在`for (auto* symTab : allSymTabs)`循环外，4个coclass共享一个1..N计数器：

```
StringLib: Prefix=1, Concat=2, Length=3, Upper=4, Lower=5
Calc:      SetValue=6, Add=7, Multiply=8, GetValue=9
MathLib:   SetPi=10, GetPi=11, CircleArea=12, Factorial=13, IsEven=14
ExtLib:    NotBool=15, AndBool=16, ..., Count=19, DoubleIt=20, TripleIt=21
```

而dll_entry.c生成器（`cgen_util.cpp:316-317`）使用**按类重置DISPID**——`nextDispid=1`定义在`for (auto& cc : coClasses)`循环内：

```
StringLib: Prefix=1, Concat=2, ..., Lower=6        (与TypeLib完全一致, 巧合)
Calc:      SetValue=1, Add=2, Multiply=3, GetValue=4  (TypeLib是6-9, 错位!)
ExtLib:    NotBool=1, ..., TripleIt=8                (TypeLib是15-21, 错位!)
MathLib:   SetPi=1, ..., IsEven=5                    (TypeLib是10-14, 错位!)
```

COM调用流程：`GetIDsOfNames("Add")`从TypeLib返回7，`Invoke(7)`在dll_entry.c的方法表（只有1-4）中找不到7 → `DISP_E_MEMBERNOTFOUND`。

第一层cgen_util.cpp的注释`// Per-class DISPID assignment (matches TypeLib builder sequential)`说明开发者以为按类重置就匹配，实际不匹配。

### 第二层：跨符号表Symbol*不共享

修复第一层时新增了`Symbol::comDispid`字段，driver TypeLib阶段写入`mr.sym->comDispid`，cgen读取`methodSym->comDispid`。但实测`methodSym->comDispid`全部为0——cgen和driver操作的不是同一个`Symbol*`。

driver.cpp:1380已有注释说明这一现象：

> TypeLib builder writes to per-module Symbol objects, but cgen uses lastAnalyzer's merged table

cgen使用`lastAnalyzer->symbolTable()`（合并表）查询方法，而driver TypeLib阶段在`for (auto* symTab : allSymTabs)`中按"先到先处理"原则处理类——可能某个类的处理发生在**另一张表**上，写入的是那张表的Symbol*，而cgen从lastAnalyzer合并表查到的是另一份Symbol*副本。

### 第三层：Property Let/Set跨表查找缺失

修复第二层后， ExtLib的`Count Let`仍然DISPID=1（fallback）。驱动调试显示driver只为`Count Get`(kind=4)写comDispid=19，没为`Count Let`(kind=5)写。

原因：driver.cpp:1265的`propLetSym = symTab->lookupModuleByKind(memberName, PropertyLet)`仅在当前`symTab`中查找，未做跨表fallback。当ExtLib类在lastAnalyzer合并表中被处理时，PropertyLet变体可能只存在于ExtLib原始分析器的表中，合并表查不到→返回null→refs列表缺少Property Let→comDispid从未被写入。

cgen_util.cpp:106-110的fallback逻辑早已实现Property Let/Set跨表查找，driver侧却缺失对称处理。

## 修复

### 修复1: cgen读comDispid（`src/backend/cgen_util.cpp`）

dll_entry.c方法表生成器改为优先读取`methodSym->comDispid`：

```cpp
int dispid = methodSym->comDispid;
if (dispid == 0) {
    // 安全fallback: 理论上不应发生 (driver TypeLib阶段已经回写comDispid) 仅防意外崩溃
    dispid = nextDispid++;
    dispIdMap[lowerBareName] = dispid;
} else if (dispIdMap.find(lowerBareName) == dispIdMap.end()) {
    dispIdMap[lowerBareName] = dispid;
}
```

### 修复2: 新增Symbol::comDispid字段（`src/semantics/symbol_table.hpp`）

```cpp
int32_t comDispid = 0;  // M29: 方法的TypeLib DISPID (由driver在TypeLib阶段回写,
                        // 供cgen生成dll_entry.c方法表用, 确保两边dispid一致; 0=未分配)
```

### 修复3: driver TypeLib阶段回写comDispid（`src/driver/driver.cpp`）

```cpp
// M29: 回写dispid到method symbol, 供cgen生成dll_entry.c方法表使用,
// 确保TypeLib与dll_entry.c dispid一致
mr.sym->comDispid = mi.dispid;
```

### 修复4: driver跨符号表Symbol*同步（`src/driver/driver.cpp`）

在TypeLib生成后、`generateDllEntry()`调用前，将comDispid从"driver写入的Symbol*"传播到"所有符号表中的同名Symbol*"（参照已有comDefaultIfaceIid同步模式）：

```cpp
// M29: Sync comDispid across all symbol tables
for (auto& [mKey, mClsSym] : lastAnalyzer->symbolTable().moduleScope()->symbols()) {
    if (!mClsSym || mClsSym->kind != SymbolKind::Class || mClsSym->isInterface) continue;
    for (auto& memberName : mClsSym->memberNames) {
        for (auto mKind : {Sub, Function, PropertyGet, PropertyLet, PropertySet}) {
            // 找到任意一个已设comDispid的Symbol作为"事实源"
            int knownDispid = 0;
            for (auto* st : allSymTabs) {
                Symbol* s = st->lookupModuleByKind(memberName, mKind);
                if (s && s->comDispid != 0) { knownDispid = s->comDispid; break; }
            }
            if (knownDispid == 0) continue;
            // 传播到所有符号表中的同名Symbol*(仅填充0值)
            for (auto* st : allSymTabs) {
                Symbol* s = st->lookupModuleByKind(memberName, mKind);
                if (s && s->comDispid == 0) s->comDispid = knownDispid;
            }
        }
    }
}
```

### 修复5: driver PhaseLib阶段跨表fallback查找Property变体（`src/driver/driver.cpp`）

driver的Property Let/Set查找补齐与cgen对称的fallback：

```cpp
auto* propLetSym = symTab->lookupModuleByKind(memberName, SymbolKind::PropertyLet);
if (!propLetSym)
    for (auto* st_ : allSymTabs) {
        propLetSym = st_->lookupModuleByKind(memberName, SymbolKind::PropertyLet);
        if (propLetSym) break;
    }
// (Sub/Function/PropertyGet/PropertySet同样补齐)
```

## 调试技巧

通过`fprintf(stderr, ...)`分别在driver写入点和cgen读取点打印Symbol地址与comDispid值，对比地址确认是否为同一Symbol*：

```
[M29-D2] cls=ExtLib member=Count kind=4 sym=0x22BB1DF8AF0 dispid=19   ← driver写的
[M29-CG] cls=ExtLib m=Count kind=4 sym=0x241934359F0 cd=19             ← cgen读的 (不同地址!)
```

地址不同直接证明cgen读取的是不同Symbol*副本，需要跨表同步。

`--keep-for-debug`参数保留C3C临时目录，可直接查看`dll_entry.c`中间文件的方法表内容。

## 验证

### DISPID一致性验证

修复后dll_entry.c方法表与TypeLib完全一致：

```c
// StringLib (TypeLib 1-5)
{ L"Prefix", 1, 2, ..._get_invoke },   // 共享dispid=1
{ L"Prefix", 1, 4, ..._let_invoke },   // Get/Let共享 ✓
{ L"Concat", 2, 1, ... },
{ L"Length", 3, 1, ... },
{ L"Upper", 4, 1, ... },
{ L"Lower", 5, 1, ... },

// ExtLib (TypeLib 15-21)
{ L"NotBool", 15, 1, ... },
...
{ L"Count", 19, 2, ..._get_invoke },   // Get
{ L"Count", 19, 4, ..._let_invoke },   // Let 共享dispid=19 ✓
{ L"DoubleIt", 20, 1, ... },
{ L"TripleIt", 21, 1, ... },

// Calc (TypeLib 6-9)
{ L"SetValue", 6, 1, ... },
{ L"Add", 7, 1, ... },
{ L"Multiply", 8, 1, ... },
{ L"GetValue", 9, 1, ... },

// MathLib (TypeLib 10-14)
{ L"SetPi", 10, 1, ... },
...
```

### PowerShell COM调用验证 (9/9 PASS)

```
PASS Calc.Add(1,2) = 3                  ← 原bug
PASS Calc.Multiply(3,4) = 12
PASS Calc.SetValue(100)/GetValue() = 100
PASS StringLib.Upper('hello') = HELLO
PASS StringLib.Concat('foo','bar') = foobar
PASS MathLib.Factorial(5) = 120
PASS ExtLib.Count Let/Get = 42           ← Property Let/Get共享dispid验证
PASS ExtLib.DoubleIt(21) = 42
PASS ExtLib.NotBool(True) = False
```

### 回归测试 82/82 PASS 零回归

## 修改文件汇总

| 文件 | 修改 |
|------|------|
| `src/semantics/symbol_table.hpp` | 新增`int32_t comDispid = 0`字段 |
| `src/driver/driver.cpp` | TypeLib阶段回写`mr.sym->comDispid` + 5种kind跨表fallback查找 + comDispid跨表同步块 |
| `src/backend/cgen_util.cpp` | dll_entry.c方法表优先读`methodSym->comDispid` (fallback保留) + 更新注释 |

## 统计

- **代码变更**: +约75 / -3 行（核心修复）
- **回归测试**: 82/82 PASS，零回归
- **修复Bug数**: 1个COM DLL调用完全失效的根因Bug
- **修复层数**: 3层（DISPID分配方式 + 跨表Symbol*不共享 + Property Let/Set跨表查找缺失）

## 教训

1. **"看似匹配"的注释最危险**：cgen_util.cpp原注释声称"matches TypeLib builder sequential"，实际两者根本不匹配。任何"应该一致"的假设都需实测验证
2. **跨符号表Symbol*不共享是隐藏陷阱**：driver和cgen查询不同的SymbolTable实例，获得不同Symbol*副本，简单赋值不互通。已有comDefaultIfaceIid同步块是同类问题的先例
3. **对称性原则**：cgen有fallback跨表查找Property Let/Set，driver侧也必须有对称实现，否则driver写入"残缺"的数据，后续跨表同步也无法补救
4. **调试用fprintf临时代码**：对于"哪个指针指向谁"的指针身份判断，fprintf打印地址是最直接的方式，比推理更可靠
