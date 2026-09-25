# 027-接口继承与 CoClass 实施收口（B01–B18）

> 读者：接手这套语言特性的人。**规范与思路**看 `018-接口继承与CoClass设计思路.md`（外部讨论存档，
> 一字未改）；**逐格过程与全部实测读数**看 `022-继承接口CoClass自动化进度表.md` 的设计记录 D1–D64；
> 本文只回答三件事：**交付了什么、边界在哪、怎么验**。

## 一、交付总览

| 能力 | 用户写法 | 批次（代码） |
| --- | --- | --- |
| 接口契约 | `Interface I … End Interface`（可 `Extends`），类里 `Implements I` | B01–B06b、B03 |
| 接口类型与多态 | `Dim x As I`、`Set x = obj`、`TypeOf x Is I`、接口变量互相赋值 | B06a/B06b、B13d |
| 接口生命周期 | 引用计数、`Class_Terminate`（DLL 侧）、`Nothing` 语义 | B05、B13d |
| 类继承 | `Inherits <基类>`、成员合并、前缀布局、转发桩 | B07a/B07b |
| 覆盖与虚派发 | `Overridable` / `Overrides` / `NotOverridable`、类虚表、运行期真派发 | B08b/B08d |
| 家族可见性 | `Protected`（家族内可用）、家族外访问报 `VB3023` | B08a/B08c |
| 基调用与构造链 | `MyBase.M`（去虚化直调）、基类 `Class_Initialize` 先跑 | B09/B09b/B09c |
| 委托式实现 | `Implements I Via <持有字段>`（槽由生成的适配器转发） | B10 |
| CoClass 块 | `CoClass N … [Implementation]** [ComCreatable]** [Default] Interface I` | B11/C01–C05 |
| 组内激活 | `As <组名>` / `New <组名>` / `CreateObject("<Proj>.<组名>")` 编译期绑实现类 | C05（=022 的 B12） |
| 对外 COM | CLSID/IID/ProgID 唯一出口、类工厂、注册与反注册、真接口进类型库、契约成员如实进库 | B13a–B15 |
| 成员的对外调用契约 | 薄面 `__stdcall` canonical 化、`oVft=(3+槽)*指针宽`、位数 flag 跟 `--arch` | B16 |
| 外部激活 | 真注册后 `CoCreateInstance` 早绑定（按库里 oVft 直调契约槽）与 `CreateObject` 晚绑定 | B17 |
| 端到端示例 | 同一源集合编 `tests/cc_demo` 的 EXE 与 DLL 两种形态，语言侧 12 条断言 + 外部客户 | B18 |

每一格的**实测读数与取舍**都在 022 的 D 记录里（例：D19 结构体偏移约束、D22 QI/IID、D40 x86 布局陷阱、
D61–D64 对外三条线的裁决）。

## 二、语言侧要点（写代码会撞到的）

- **`Extends` = 接口继承、`Inherits` = 类继承**，两者永不同义（018 §二十一里把 `Inherits` 用于接口，
  实现落地是 `Extends`，手册与本文都按实现口径写）。
- 接口契约按**源码签名**比对（成员名 + 参数个数 + 逐参 `ByVal/ByRef/Optional/ParamArray` + 返回类型原文），
  缺/多/不符都是**错误级**。
- 接口成员按 VB6 惯例是 `Private`：它们**不进类的默认面**，只能经接口变量或对外的"真接口"那一档调用。
- 派生类里调继承成员要写 `Me.`（裸名被显式拒绝，见 022 的 D27-13）；覆盖继承来的属性时过程体内写 `Me.m_xxx`。
- `Protected` 只在家族内可用；家族外访问报 `VB3023`（判定落在 `visit(MemberAccessExpr)`，七种接收者形状全覆盖）。
- `CoClass` 块的 `[ComCreatable(True)]` **只在 ActiveX DLL 工程**合法（EXE 里报 `VB3033`：EXE 只保留组内那一半）。

## 三、COM 对外侧要点

- **三通道同值**（B13）：块身份、接口 IID、coclass CLSID 与 ProgID 都出自 stage 2.7 的唯一出口，
  服务器表、类型库、QI、vtable 一律读它，不再各算一份。
- **广告 == 应答**（B13e）：类型库里 coclass 的 DEFAULT 引用与服务器表里的默认接口 IID 指向同一档 ——
  也就是 `IDispatch::GetIDsOfNames` 真正查的那张表。
- **接口在库里是真接口**（B15/B16）：`TKIND_INTERFACE`，成员带 `oVft=(3+槽号)*指针宽`、`callconv=stdcall`、
  原生返回 vt；库的位数 flag 跟 `--arch`（x86 客户端按 4 字节一步读到的偏移与 x64 各按自己的指针宽）。
- **薄面在 x86 下一律 `__stdcall`**；接口 IID 的 `QI` 交**薄指针**（前 3 槽即 IUnknown，客户端可当
  `IUnknown*` 收尾），包装器把底座引用交还给最后一个薄引用。
- **外部激活**（B17）：`DllRegisterServer` 写 HKCR（非管理员会被 Windows 重定向到 `HKCU\Software\Classes`），
  `CoCreateInstance` 早/晚两条路都通，`DllUnregisterServer` 之后 CLSID/ProgID/TypeLib 三类键都不留。
- 契约成员是 `Private` ⇒ 外部**晚绑定**客户点不到它们（VB6 语义）；要按名可点就得在类里写**公有成员**。

## 四、v1 边界（仍未交付，逐条写清现状与影响）

| 边界 | 现状 | 影响 |
| --- | --- | --- |
| 派生类自己 `Implements` 新式接口 | 编译期拒绝（`VB3022`，实测 B18 写示例时撞到） | 示例把继承链与接口链拆成两条；v1 的继承基类只支持"无事件、不实现新式接口、无重载"的类 |
| 经继承满足的接口契约 | 未做 | 基类实现的接口不能算派生类实现 |
| 继承来的 `Public` 字段对外 COM 暴露 | 未做 | 要暴露就在派生类重写一遍 |
| 接口值作实参 / 进 `Variant` | 未做（B06c） | 只能赋给接口变量或 `Object` |
| `.bas` 里声明的新式接口当类型用 | 调用点认不出（D43 第三条） | 接口一律放 `.cls` 宿主文件 |
| 接口槽带 `Optional` 的调用点 / 接口变量上的 `Property Let/Set` | caller 侧有洞（D43） | 避开这两种形状 |
| 祖先 `Private` UDT 进方法签名 | 未做（D40 末①） | 别把祖先私有 UDT 类型写进公开签名 |
| `Class_Terminate` 在 EXE 里无触发点 | 既有缺口（D41），与继承无关 | EXE 里不要依赖 `Class_Terminate`；DLL 侧正常 |
| `com_entry` 基类 extern 的 `void*` 返回 | 未做 | — |
| `ComObj_Invoke` 的 invkind fallback | 配不上就取"第一个同 dispid 表项"（D61-6，等拍板） | 属性 Get/Let 同名时可能打到对方 |
| canonical COM 返回形状（`HRESULT` + `[out, retval]`） | **按实测不做**（B17 测量②） | 真按库里 `oVft` 直调的客户已能用；只有"把接口当 dual 自动化接口"才需要 |
| 跨世界身份合一 | 未做 | 外部客户（`CreateObject`）拿到的是 COM 包装器指针，进程内薄指针是另一个值 |

## 五、怎么验

```powershell
# 全量门（CI）：推 github/dev 即触发 Actions（.github/workflows/ci.yml），vbp job 含全部相关用例
pwsh testsun_tests.ps1 -Category vbp      # 本机跑同一批（需要 MSVC + Windows SDK）

# 只看这套特性相关的用例（名字都能在 testsun_tests.ps1 里 grep 到）
#   itf_xmod_writer / itf_via_pair / itf_via_x86       接口契约 + Via 委托
#   cls_inh_pair / cls_inh_x86                         Inherits/Overrides/Protected/MyBase
#   cc_act_pair / cc_act_x86                           CoClass 组内激活（As/New/CreateObject 改写）
#   cc_dll_identity_single_source / cc_dll_tlb_*       身份出口 + 类型库形状与契约面
#   cc_dll_dispatch_iface_only[_x86]                   进程内 IDispatch 客户端
#   cc_dll_external_activate[_x86] / cc_dll_late_client 真注册 + 外部客户（B17）
#   cc_demo_exe[_x86] / cc_demo_dll_external[_x86]     端到端示例（B18）
```

- 外部激活那几条用例会**自己注册、自己反注册**（探针调 `DllRegisterServer`/`DllUnregisterServer`，
  并断言 `CLEAN_CLSID/CLEAN_PROGID/CLEAN_TYPELIB=gone`），所以在干净 runner 上可重复跑、不留键。
- 探针源码：`tests	ools\com_act_probe.c`（外部激活）、`disp_probe.c`（进程内 IDispatch）、
  `tlb_slots.cpp`（类型库读数）；助手：`tests\com_activate.ps1`、`disp_invoke.ps1`、`tlb_contract.ps1`。

## 六、记录索引

- **逐格过程 / 全部实测读数**：`022-继承接口CoClass自动化进度表.md` 的设计记录 D1–D64 + 批次表 + 运行日志。
- **手册（用户可见口径）**：`docs/vb6-manual/02-语句/` 下的 `Interface 语句.md`、`Implements 语句.md`、
  `Inherits 语句.md`、`CoClass 语句.md`。
- **示例工程**：`tests\cc_demo\`（EXE 形态 `DemoExe.vbp` / DLL 形态 `DemoDll.vbp` / 外部客户 `DemoClient.vbp`）。
