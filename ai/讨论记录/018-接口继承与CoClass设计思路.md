# 018-接口继承与CoClass设计思路（外部讨论存档）

> 来源: chat-export-1.json（qwen3.8-max 回答），2026-09-23 存档。
> 配套: twinBASIC Inheritance / Interfaces-CoClasses 文档要点见文末。


---

## 原始问题

vb类似的编程语言增加接口和coclass，如何实现


## 设计思路全文

如果你是想在一种“类似 VB 的编程语言”里增加 **接口（Interface）** 和 **CoClass（COM Class）** 支持，可以从两个层面理解：

1. **语言层面**：增加 `Interface`、`Class`、`Implements`、`CoClass` 等语法。
2. **运行时/COM 层面**：让对象支持 COM 的 `IUnknown`、`IDispatch`、接口查询、引用计数、类工厂、注册表激活等机制。

下面我给一个比较完整的实现思路。

---

# 一、先明确概念

## 1. Interface 是什么

接口是一组方法、属性、事件的抽象定义，不包含实现。

例如：

```vb
Interface IAnimal
    Sub Speak()
    Function GetName() As String
End Interface
```

实现类必须实现接口里的所有成员：

```vb
Class Dog
    Implements IAnimal

    Public Sub Speak() Implements IAnimal.Speak
        Print "Woof"
    End Sub

    Public Function GetName() As String Implements IAnimal.GetName
        Return "Dog"
    End Function
End Class
```

---

## 2. CoClass 是什么

`CoClass` 通常指 COM 中的具体类，也就是可以通过 `CoCreateInstance` 或 `CreateObject` 创建的对象类。

在 IDL 里一般是这样：

```idl
coclass Dog
{
    [default] interface IDog;
    interface IAnimal;
}
```

意思是：

- `Dog` 是一个可创建的 COM 类；
- 它默认暴露 `IDog` 接口；
- 它也支持 `IAnimal` 接口。

在类似 VB 的语言里，可以设计成：

```vb
<ComVisible(True)>
<Guid("12345678-1234-1234-1234-123456789ABC")>
CoClass Dog
    Implements IAnimal
    Implements IDog
End CoClass
```

或者更接近 VB 风格：

```vb
Public Class Dog
    Implements IAnimal

    Private Sub Class_Initialize()
    End Sub

    Public Sub Speak() Implements IAnimal.Speak
    End Sub
End Class
```

如果目标是 COM，则这个 `Class Dog` 在编译后应该生成一个 `CoClass Dog`。

---

# 二、如果是给类似 VB 的语言增加接口和 CoClass

可以分为几个部分：

1. 语法解析
2. 类型系统
3. 编译器代码生成
4. 运行时对象模型
5. COM 支持
6. 注册与创建机制
7. 自动化/脚本兼容，比如 `IDispatch`

---

# 三、语法设计

可以设计如下语法。

## 1. 定义接口

```vb
Interface IShape
    Function Area() As Double
    Sub Draw()
End Interface
```

接口只能包含：

- 方法
- 属性
- 事件
- 索引器

不能包含：

- 字段
- 构造函数
- 具体实现
- 静态成员

接口成员默认公开。

---

## 2. 定义普通类

```vb
Class Circle
    Implements IShape

    Public Radius As Double

    Public Function Area() As Double Implements IShape.Area
        Return 3.14159 * Radius * Radius
    End Function

    Public Sub Draw() Implements IShape.Draw
        Print "Draw circle"
    End Sub
End Class
```

---

## 3. 定义 CoClass

可以单独定义 `CoClass`：

```vb
CoClass CircleClass
    Implements IShape
    DefaultInterface IShape
    Creatable True
    Guid "2E1B5C40-8A5F-4C2A-9E2D-6D6A0F2B9C11"
End CoClass
```

或者更简单地让普通类自动成为 `CoClass`：

```vb
<Guid("2E1B5C40-8A5F-4C2A-9E2D-6D6A0F2B9C11")>
Public Class Circle
    Implements IShape
End Class
```

编译器将其编译为：

```text
CoClass: Circle
CLSID:   {2E1B5C40-8A5F-4C2A-9E2D-6D6A0F2B9C11}
默认接口: IShape
```

---

# 四、接口的实现机制

接口最核心的实现方式是 **虚函数表 / vtable**。

COM 接口本质上就是一个指向函数指针表的指针。

例如：

```text
IAnimal vtable:
+------------------+
| QueryInterface   |
| AddRef           |
| Release          |
| Speak            |
| GetName          |
+------------------+
```

所有 COM 接口都必须继承 `IUnknown`：

```cpp
interface IUnknown
{
    HRESULT QueryInterface(REFIID riid, void** ppvObject);
    ULONG AddRef();
    ULONG Release();
}
```

所以你的接口在底层应该变成：

```text
IAnimal : IUnknown
{
    HRESULT Speak();
    HRESULT GetName(BSTR* pRetVal);
}
```

---

# 五、接口在编译器里的表示

你可以给每个接口生成一个接口描述结构。

例如用 C/C++ 运行时表示：

```cpp
struct IAnimalVtbl
{
    HRESULT (*QueryInterface)(void* thisPtr, GUID* iid, void** ppv);
    ULONG (*AddRef)(void* thisPtr);
    ULONG (*Release)(void* thisPtr);

    HRESULT (*Speak)(void* thisPtr);
    HRESULT (*GetName)(void* thisPtr, BSTR* result);
};

struct IAnimal
{
    IAnimalVtbl* lpVtbl;
};
```

对象实例里保存指向虚表的指针：

```cpp
struct DogObject
{
    IAnimalVtbl* animalVtbl;
    long refCount;
    // 对象内部字段
};
```

当调用接口方法时：

```cpp
dog->lpVtbl->Speak(dog);
```

---

# 六、CoClass 的实现机制

一个 `CoClass` 至少要支持：

1. 一个全局唯一类 ID：`CLSID`
2. 一个或多个接口
3. 一个默认接口
4. 可创建性
5. 类工厂
6. 引用计数
7. 注册表注册，如果是进程外或系统级 COM

---

## 1. CLSID 和 IID

每个接口有一个 `IID`：

```text
IID_IAnimal = {XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}
```

每个 CoClass 有一个 `CLSID`：

```text
CLSID_Dog = {YYYYYYYY-YYYY-YYYY-YYYY-YYYYYYYYYYYY}
```

例如：

```vb
<Guid("A1B2C3D4-1111-2222-3333-444455556666")>
Interface IAnimal
End Interface

<Guid("B2C3D4E5-5555-6666-7777-888899990000")>
CoClass Dog
End CoClass
```

编译器需要把这些 GUID 写入类型库或注册信息。

---

# 七、实现 IUnknown

任何 COM 对象都必须实现 `IUnknown`。

核心三个函数：

```cpp
HRESULT QueryInterface(REFIID riid, void** ppvObject);
ULONG AddRef();
ULONG Release();
```

---

## 1. AddRef

增加引用计数：

```cpp
ULONG Dog_AddRef(DogObject* obj)
{
    obj->refCount++;
    return obj->refCount;
}
```

---

## 2. Release

减少引用计数，计数为 0 时销毁对象：

```cpp
ULONG Dog_Release(DogObject* obj)
{
    obj->refCount--;

    if (obj->refCount == 0)
    {
        delete obj;
        return 0;
    }

    return obj->refCount;
}
```

---

## 3. QueryInterface

根据接口 ID 返回对应接口指针：

```cpp
HRESULT Dog_QueryInterface(DogObject* obj, GUID* iid, void** ppv)
{
    if (IsEqualIID(*iid, IID_IUnknown))
    {
        *ppv = &obj->animalVtbl;
        obj->lpVtbl->AddRef(obj);
        return S_OK;
    }

    if (IsEqualIID(*iid, IID_IAnimal))
    {
        *ppv = &obj->animalVtbl;
        obj->lpVtbl->AddRef(obj);
        return S_OK;
    }

    *ppv = NULL;
    return E_NOINTERFACE;
}
```

如果有多个接口：

```cpp
if (IsEqualIID(*iid, IID_IDog))
{
    *ppv = &obj->dogVtbl;
    Dog_AddRef(obj);
    return S_OK;
}
```

---

# 八、一个完整的简单例子

假设有：

```vb
Interface IAnimal
    Sub Speak()
End Interface

Class Dog
    Implements IAnimal

    Public Sub Speak() Implements IAnimal.Speak
        Print "Woof"
    End Sub
End Class
```

编译器需要生成类似下面的底层结构。

---

## 1. 接口 ID

```cpp
const GUID IID_IAnimal =
{ 0xA1B2C3D4, 0x1111, 0x2222, { 0x33, 0x33, 0x44, 0x44, 0x55, 0x55, 0x66, 0x66 } };

const GUID CLSID_Dog =
{ 0xB2C3D4E5, 0x5555, 0x6666, { 0x77, 0x77, 0x88, 0x88, 0x99, 0x99, 0x00, 0x00 } };
```

---

## 2. 对象结构

```cpp
struct DogObject
{
    IAnimalVtbl* lpVtbl;
    long refCount;
};
```

---

## 3. 方法实现

```cpp
HRESULT Dog_Speak(DogObject* obj)
{
    printf("Woof\n");
    return S_OK;
}
```

---

## 4. 虚表

```cpp
IAnimalVtbl Dog_AnimalVtbl =
{
    Dog_QueryInterface,
    Dog_AddRef,
    Dog_Release,
    Dog_Speak
};
```

---

## 5. 创建对象

```cpp
DogObject* Dog_CreateInstance()
{
    DogObject* obj = new DogObject();
    obj->lpVtbl = &Dog_AnimalVtbl;
    obj->refCount = 1;
    return obj;
}
```

---

## 6. 类工厂

```cpp
HRESULT DogClassFactory_CreateInstance(IUnknown* outer, GUID* iid, void** ppv)
{
    if (outer != NULL)
        return CLASS_E_NOAGGREGATION;

    DogObject* obj = Dog_CreateInstance();
    return obj->lpVtbl->QueryInterface(obj, iid, ppv);
}
```

---

# 九、CoClass 的创建方式

在 COM 中通常有几种创建方式。

---

## 1. CoCreateInstance

C/C++ 客户端：

```cpp
IAnimal* pAnimal = NULL;

CoCreateInstance(
    CLSID_Dog,
    NULL,
    CLSCTX_INPROC_SERVER,
    IID_IAnimal,
    (void**)&pAnimal
);

pAnimal->lpVtbl->Speak(pAnimal);
pAnimal->lpVtbl->Release(pAnimal);
```

---

## 2. VB 风格创建

语言层可以支持：

```vb
Dim animal As IAnimal
Set animal = New Dog

animal.Speak
```

或者：

```vb
Dim animal As Object
Set animal = CreateObject("MyApp.Dog")

animal.Speak
```

如果是 COM，则 `CreateObject` 背后会：

1. 根据 `ProgID` 找 `CLSID`
2. 根据 `CLSID` 找 DLL/EXE
3. 加载组件
4. 调用类工厂
5. 创建对象
6. `QueryInterface` 返回默认接口

---

# 十、IDispatch 支持

如果你的语言类似 VB，通常需要支持自动化调用，例如：

```vb
Set obj = CreateObject("MyApp.Dog")
obj.Speak
```

脚本语言、VBA、VBScript、Excel VBA 等通常通过 `IDispatch` 调用。

所以建议你的 `CoClass` 默认支持双重接口：

```text
IUnknown
  └─ IDispatch
       └─ IAnimalDisp
```

`IDispatch` 有四个核心方法：

```cpp
GetTypeInfoCount
GetTypeInfo
GetIDsOfNames
Invoke
```

---

## 1. GetIDsOfNames

把方法名转换为 DISPID：

```cpp
if (name == "Speak")
    return DISPID_SPEAK;
```

---

## 2. Invoke

根据 DISPID 调用方法：

```cpp
switch (dispId)
{
    case DISPID_SPEAK:
        return Dog_Speak(obj);
}
```

这样脚本就可以这样调用：

```vb
obj.Speak
```

---

# 十一、类型库 Type Library

COM 组件通常需要类型库。

类型库包含：

- 接口定义
- CoClass 定义
- GUID
- 方法参数
- 返回值
- 枚举
- 结构
- 帮助字符串
- 版本信息

可以用 IDL 生成：

```idl
library MyApp
{
    importlib("stdole2.tlb");

    interface IAnimal : IDispatch
    {
        HRESULT Speak();
    };

    coclass Dog
    {
        [default] interface IAnimal;
    };
};
```

编译后生成：

```text
MyApp.tlb
```

VB6、VBA、.NET COM Interop 都可以使用这个类型库。

---

# 十二、注册表注册

如果是经典 COM，通常需要注册。

注册表位置一般是：

```text
HKEY_CLASSES_ROOT\CLSID\{CLSID}
HKEY_CLASSES_ROOT\MyApp.Dog
```

例如：

```text
HKEY_CLASSES_ROOT\MyApp.Dog
    (Default) = Dog Class
    CLSID = {B2C3D4E5-5555-6666-7777-888899990000}

HKEY_CLASSES_ROOT\CLSID\{B2C3D4E5-5555-6666-7777-888899990000}
    (Default) = Dog Class
    InprocServer32 = C:\MyApp\MyApp.dll
    TypeLib = {库GUID}
    Version = 1.0
```

组件需要导出：

```cpp
DllRegisterServer
DllUnregisterServer
DllGetClassObject
DllCanUnloadNow
```

---

# 十三、DllGetClassObject

COM 服务器 DLL 要导出这个函数：

```cpp
HRESULT DllGetClassObject(
    REFCLSID rclsid,
    REFIID riid,
    void** ppv)
{
    if (IsEqualCLSID(rclsid, CLSID_Dog))
    {
        return DogClassFactory_QueryInterface(riid, ppv);
    }

    return CLASS_E_CLASSNOTAVAILABLE;
}
```

`CoCreateInstance` 会调用它获得类工厂。

---

# 十四、如果不想做完整 COM，也可以做简化对象模型

如果你的“类似 VB 的语言”不一定需要真正的 Windows COM，可以只做简化版接口和对象系统。

例如只实现：

```text
Interface
Implements
New
引用计数
接口转换
```

不实现：

- 注册表
- GUID
- IDispatch
- 类型库
- DLL COM 服务器

这样可以更容易实现。

简化模型：

```vb
Interface IAnimal
    Sub Speak()
End Interface

Class Dog
    Implements IAnimal

    Public Sub Speak() Implements IAnimal.Speak
        Print "Dog"
    End Sub
End Class
```

运行时内部：

```text
对象头:
    class pointer
    ref count
    interface table
```

接口转换：

```vb
Dim d As Dog = New Dog()
Dim a As IAnimal = Cast(d, IAnimal)

a.Speak()
```

---

# 十五、推荐实现路线

如果你是要做一个语言或编译器，推荐分阶段。

---

## 阶段 1：只实现接口和实现

支持：

```vb
Interface IAnimal
    Sub Speak()
End Interface

Class Dog
    Implements IAnimal
End Class
```

实现内容：

- 解析 `Interface`
- 解析 `Implements`
- 检查接口方法是否全部实现
- 生成接口虚表
- 支持接口变量调用方法

此阶段不需要 COM。

---

## 阶段 2：实现引用计数和接口查询

支持：

```vb
Dim a As IAnimal
Set a = New Dog()
```

实现：

- 对象引用计数
- 接口指针
- 接口转换
- 自动释放

可以设计运行时函数：

```text
AddRef(obj)
Release(obj)
QueryInterface(obj, iid)
```

---

## 阶段 3：增加 CoClass 元数据

支持：

```vb
CoClass Dog
    Implements IAnimal
End CoClass
```

或者：

```vb
<Guid("...")>
Class Dog
End Class
```

实现：

- 每个类有 `CLSID`
- 每个接口有 `IID`
- 类实现接口列表
- 默认接口
- 创建工厂

---

## 阶段 4：实现 COM 兼容

需要支持：

```text
IUnknown
IDispatch
TypeLib
注册表
DllGetClassObject
CoCreateInstance
CreateObject
```

此阶段就可以被 VBA、VBScript、VB6、.NET 等调用。

---

# 十六、如果用 .NET 实现会简单很多

如果你的“类似 VB 的语言”最终运行在 .NET 上，可以直接利用 .NET 的 COM Interop。

例如：

```vb
Imports System.Runtime.InteropServices

<ComVisible(True)>
<Guid("A1B2C3D4-1111-2222-3333-444455556666")>
Public Interface IAnimal
    Sub Speak()
End Interface

<ComVisible(True)>
<Guid("B2C3D4E5-5555-6666-7777-888899990000")>
<ClassInterface(ClassInterfaceType.None)>
<ComDefaultInterface(GetType(IAnimal))>
Public Class Dog
    Implements IAnimal

    Public Sub Speak() Implements IAnimal.Speak
        Console.WriteLine("Woof")
    End Sub
End Class
```

然后用：

```text
regasm YourAssembly.dll /tlb:YourAssembly.tlb
```

即可生成 COM 可见类。

这种情况下：

- 接口由 .NET 转换为 COM 接口
- `CoClass` 由 .NET 类自动生成
- 引用计数由 COM Interop 处理
- `IDispatch` 可选择支持
- 类型库可以通过 `tlbexp` 或 `regasm` 生成

---

# 十七、如果是模仿 VB6 的方式

VB6 里其实没有显式写 `CoClass`。

通常：

- 接口定义在 `Class Module` 里；
- 类模块本身既是接口，又是可创建类；
- 编译后生成 COM 组件；
- 工程属性里设置组件类型：
  - ActiveX DLL
  - ActiveX EXE
  - ActiveX Control

VB6 的类模块：

```vb
' IAnimal.cls
Public Sub Speak()
End Sub
```

另一个类实现：

```vb
' Dog.cls
Implements IAnimal

Private Sub IAnimal_Speak()
    Debug.Print "Woof"
End Sub
```

创建：

```vb
Dim x As IAnimal
Set x = New Dog
```

VB6 编译器会自动处理：

- COM 类
- 默认接口
- 类型库
- GUID
- 引用计数
- IDispatch

如果你要模仿 VB6，可以这样设计：

```vb
Public Class IAnimal
    Public Sub Speak()
    End Sub
End Class
```

但更清晰的是分成：

```vb
Interface IAnimal
    Sub Speak()
End Interface

Class Dog
    Implements IAnimal
End Class
```

然后编译器自动把 `Dog` 输出为 `CoClass Dog`。

---

# 十八、一个较完整的语言设计示例

你可以这样设计语言语法：

```vb
Library MyApp
    Guid "11111111-1111-1111-1111-111111111111"
    Version 1.0
End Library

<Guid("22222222-2222-2222-2222-222222222222")>
Interface IAnimal
    Sub Speak()
    Function GetName() As String
End Interface

<Guid("33333333-3333-3333-3333-333333333333")>
ProgId("MyApp.Dog")>
CoClass Dog
    Implements IAnimal
End CoClass

Class DogImpl
    Implements IAnimal

    Public Sub Speak() Implements IAnimal.Speak
        Print "Woof"
    End Sub

    Public Function GetName() As String Implements IAnimal.GetName
        Return "Dog"
    End Function
End Class
```

或者更简单：

```vb
<Guid("33333333-3333-3333-3333-333333333333")>
ProgId("MyApp.Dog")>
Public Class Dog
    Implements IAnimal

    Public Sub Speak() Implements IAnimal.Speak
        Print "Woof"
    End Sub

    Public Function GetName() As String Implements IAnimal.GetName
        Return "Dog"
    End Function
End Class
```

编译器看到 `Class Dog` 有 `Guid` 和 `ProgId`，就把它标记为可创建 `CoClass`。

---

# 十九、编译器需要做什么

编译器大致要做这些事。

---

## 1. 语义检查

检查：

- 接口不能包含实现
- 接口成员必须可 COM 化
- 实现类必须实现所有接口成员
- 方法签名必须匹配
- 默认接口必须存在
- `CoClass` 至少实现一个接口

---

## 2. 生成接口表

例如：

```text
Class Dog
    CLSID: {3333...}
    Interfaces:
        IUnknown
        IDispatch
        IAnimal
    Default Interface:
        IAnimal
```

---

## 3. 生成 vtable

为每个接口生成虚表：

```text
Dog_IAnimal_Vtbl
    QueryInterface
    AddRef
    Release
    Speak
    GetName
```

---

## 4. 生成类工厂

每个可创建类生成：

```text
DogClassFactory
    CreateInstance
    LockServer
```

---

## 5. 生成类型库元数据

输出：

```text
.tlb 或二进制元数据
```

内容包括：

- 接口
- 方法
- 参数
- GUID
- CoClass
- ProgID
- 版本

---

## 6. 生成注册函数

如果是 COM DLL，需要生成：

```text
DllRegisterServer
DllUnregisterServer
DllGetClassObject
DllCanUnloadNow
```

---

# 二十、运行时对象内存布局示例

一个支持多个接口的对象可以这样布局：

```text
DogObject
+----------------------+
| IAnimal* vptr        |
+----------------------+
| IDog* vptr           |
+----------------------+
| refCount             |
+----------------------+
| class metadata       |
+----------------------+
| member fields        |
+----------------------+
```

或者更 COM 标准一点，每个接口指针都指向不同的 vtable：

```text
IAnimal interface pointer ----> IAnimal vtable
IDog interface pointer ----> IDog vtable
```

但这些指针最终都指向同一个对象。

---

# 二十一、接口继承

接口可以继承接口：

```vb
Interface IAnimal
    Sub Speak()
End Interface

Interface IDog
    Inherits IAnimal
    Sub Fetch()
End Interface
```

底层 IDL 类似：

```idl
interface IDog : IAnimal
{
    HRESULT Fetch();
}
```

vtable 是继承展开的：

```text
IDog vtable:
    QueryInterface
    AddRef
    Release
    Speak
    Fetch
```

---

# 二十二、事件接口

如果想支持 VB 风格事件，可以设计：

```vb
Interface IButtonEvents
    Sub Click()
End Interface

CoClass Button
    Event Source IButtonEvents
End CoClass
```

COM 中通常用连接点：

```text
IConnectionPointContainer
IConnectionPoint
```

客户端实现事件接口，然后注册到对象：

```cpp
button->Advise(eventSink, &cookie);
```

这部分比较复杂，如果只是初级实现，可以先不做事件。

---

# 二十三、属性和方法在 COM 里的表示

例如：

```vb
Interface IPerson
    Property Name As String
    Function GetAge() As Integer
End Interface
```

COM 自动化里通常变成：

```text
get_Name(BSTR* value)
put_Name(BSTR value)
GetAge(long* value)
```

所以你的编译器需要把属性转换成 `get_` 和 `put_` 方法。

例如：

```vb
Property Name As String
```

变成：

```vb
Function get_Name() As String
Sub put_Name(value As String)
```

---

# 二十四、推荐的最小可行实现

如果你只是想让语言支持接口和 `CoClass`，可以先做一个最小版本。

---

## 最小语法

```vb
Interface IAnimal
    Sub Speak()
End Interface

Class Dog
    Implements IAnimal

    Sub Speak() Implements IAnimal.Speak
        Print "Woof"
    End Sub
End Class
```

---

## 最小运行时

对象结构：

```text
Object
    classInfo
    refCount
    interfaceMap
```

接口映射：

```text
Dog interfaceMap:
    IAnimal -> Dog_IAnimal_vtable
```

---

## 最小创建语句

```vb
Dim a As IAnimal
a = New Dog
a.Speak()
```

编译为：

```text
temp = CreateInstance(CLSID_Dog)
a = QueryInterface(temp, IID_IAnimal)
Release(temp)
a.vtable.Speak(a)
```

---

# 二十五、如果目标是 VB6/ActiveX 组件

如果你的问题其实是指“在 VB6 或类 VB6 环境里增加接口和 CoClass”，那么一般不是手动实现 `CoClass`，而是：

1. 创建 ActiveX DLL 工程；
2. 添加 Class Module；
3. 设置 `Instancing` 属性；
4. 用 `Implements` 实现接口；
5. 编译成 COM DLL；
6. 注册组件。

例如：

接口类：

```vb
' IAnimal.cls
Option Explicit

Public Sub Speak()
End Sub
```

实现类：

```vb
' Dog.cls
Option Explicit

Implements IAnimal

Private Sub IAnimal_Speak()
    Debug.Print "Woof"
End Sub
```

在工程里 `Dog.cls` 的 `Instancing` 设置为：

```text
5 - MultiUse
```

这样 `Dog` 就可以被外部创建，相当于一个 `CoClass`。

使用：

```vb
Dim obj As IAnimal
Set obj = New Dog
obj.Speak
```

VB6 会自动生成类型库和 COM 类信息。

---

# 二十六、如果是 .NET/VB.NET

VB.NET 本身已经有接口和 COM 支持。

接口：

```vb
Public Interface IAnimal
    Sub Speak()
End Interface
```

实现：

```vb
Public Class Dog
    Implements IAnimal

    Public Sub Speak() Implements IAnimal.Speak
        Console.WriteLine("Woof")
    End Sub
End Class
```

如果要暴露为 COM：

```vb
Imports System.Runtime.InteropServices

<ComVisible(True)>
<Guid("11111111-1111-1111-1111-111111111111")>
Public Interface IAnimal
    Sub Speak()
End Interface

<ComVisible(True)>
<Guid("22222222-2222-2222-2222-222222222222")>
<ClassInterface(ClassInterfaceType.None)>
<ComDefaultInterface(GetType(IAnimal))>
Public Class Dog
    Implements IAnimal

    Public Sub Speak() Implements IAnimal.Speak
        Console.WriteLine("Woof")
    End Sub
End Class
```

注册：

```bat
regasm YourAssembly.dll /tlb:YourAssembly.tlb /codebase
```

或者安装到 GAC 后：

```bat
regasm YourAssembly.dll /tlb:YourAssembly.tlb
```

---

# 二十七、总结

如果你是在设计一个类似 VB 的语言，要实现接口和 `CoClass`，核心思路是：

1. 增加 `Interface` 语法，定义抽象契约；
2. 增加 `Implements` 语法，让类实现接口；
3. 在编译器中生成接口元数据和虚函数表；
4. 在运行时支持接口指针、接口查询和引用计数；
5. 如果要兼容 COM，需要实现 `IUnknown`；
6. 如果要支持脚本和自动化，需要实现 `IDispatch`；
7. 如果要让外部通过 `CoCreateInstance` 或 `CreateObject` 创建对象，需要实现 `CoClass`、类工厂、GUID、注册表和类型库；
8. 如果基于 .NET，可以直接用 `ComVisible`、`Guid`、`ClassInterface`、`regasm` 等机制，不需要手写 COM 细节。

最简实现可以先做：

```vb
Interface IAnimal
    Sub Speak()
End Interface

Class Dog
    Implements IAnimal
    Public Sub Speak() Implements IAnimal.Speak
    End Sub
End Class
```

然后逐步扩展为：

```vb
Dim animal As IAnimal
animal = New Dog
animal.Speak()
```

最后再扩展成 COM 兼容：

```vb
Set animal = CreateObject("MyApp.Dog")
animal.Speak
```

如果你的目标是真正可被 VB6/VBA/脚本调用的组件，那么最终必须走 COM 路线：`IUnknown + IDispatch + CoClass + TypeLib + 注册表`。


---

## twinBASIC 官方文档要点

来源: https://docs.twinbasic.com/Features/Language/Inheritance 与
https://docs.twinbasic.com/Features/Language/Interfaces-CoClasses

### Inheritance 页
- 三大机制: `Inherits`（类继承）、`Implements`（接口实现）、`Implements Via`（委托式实现：成员直接转交给一个持有对象，免手写转发实现）。
- Implements 支持接口链: 实现子接口时可连同父接口成员一起满足，无需重复声明；一个过程可用组合指令满足多个接口成员；扩展协议限制已移除。
- Inherits: 配合访问修饰符（Protected 仅派生域可见）；多态用 Overridable/Overrides 钩子；构造链要求显式基类限定调用（MyBase 式）。
- 语义边界: 编译器在对象分配前保证全部接口契约被满足；透明 cast 在幕后做兼容性校验。

### Interfaces-CoClasses 页
- Interface/CoClass 声明仅存在于 twin 模块（本项目需自行决定落在 .cls/.bas 的哪种宿主），须位于 Class/Module 块之前，工程级可见。
- 接口语法:

  ```vb
  [InterfaceId("{GUID}")]
  Interface Name [Extends ParentInterface]
      ' 成员仅限 Sub / Function / Property Get / Property Let / Property Set 签名
      ' 纯契约：无实现体、无可见性关键字
  End Interface
  ```

- 接口级属性: [Description]/[Hidden]/[Restricted]/[OleAutomation]/[ComImport]/[ComExtensible]；成员级: [PreserveSig]（绕过 HRESULT 包装）、[DispId]、[Description]。
- CoClass 语法:

  ```vb
  [CoClassId("{GUID}")]
  CoClass Name
      [Default] Interface ContractName
      [Default, Source] Interface EventIface
  End CoClass
  ```

  每个 CoClass 至少一个接口；[Default] 指主接口；[ComCreatable]（默认 True）决定可否 New；[CoClassCustomConstructor] 指向工厂过程（ByRef 实例入参、返回 HRESULT）。底层实现为私有 Class + Implements 映射；外部代码只经 CoClass 实例引用交互。
- 限制: 不支持定义 dispinterface；传统事件源无原生 dispinterface 支持。
