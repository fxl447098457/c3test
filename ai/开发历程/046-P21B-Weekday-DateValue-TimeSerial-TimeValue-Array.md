# 046-P21B-Weekday-DateValue-TimeSerial-TimeValue-Array

## 日期
2026-07-01

## 概要
P21-B批次完成：5个VB6内置函数的完整三层实现（RTL + cgen + semantic已注册）。

## 完成项

### P21-04: Array()函数
- RTL: `vb6_ArrayCreate(count)` 创建Variant SafeArray + 4个类型化setter
  - `vb6_ArraySetLong/SetDouble/SetBSTR/SetVariant` — 每个setter做VariantClear+深拷贝
- cgen: 特殊处理（类似IIf/Choose/Switch），在visit(IndexOrCallExpr&)中检测callee="Array"
  - 生成临时变量 `_arr_N = vb6_ArrayCreate(count)`
  - 根据参数类型选择Long/Double/BSTR setter
  - 类型检测：LiteralExpr(Integer→Long, Double→Double) + 表达式模式检测BSTR
  - lastExpr_指向临时变量名

### P21-05: Weekday()函数
- RTL: `vb6_Weekday(double date, int32_t firstDayOfWeek)` → int32_t
  - 1899-12-30(serial=0)是周六，用模7运算推导
  - 支持firstDayOfWeek参数(VbSunday=1默认, VbMonday=2, ...)
  - 无参数→默认=1(vbSunday)，cgen自动padding
- cgen: builtinFuncs映射 + 1参数时padding `, 1`

### P21-06: DateValue()函数
- RTL: `vb6_DateValue(BSTR dateStr)` → double (date serial)
  - 使用 `VarDateFromStr()` OLE Automation函数解析区域感知日期字符串
  - 返回整数部分（日期serial，不含时间）

### P21-07: TimeSerial()函数
- RTL: `vb6_TimeSerial(int32_t hour, int32_t minute, int32_t second)` → double
  - 溢出归一化：e.g. TimeSerial(26,30,0) = 2:30AM次日
  - 负值处理：循环加86400直到非负
  - 返回天数+时间分数

### P21-08: TimeValue()函数
- RTL: `vb6_TimeValue(BSTR timeStr)` → double (time fraction)
  - 使用 `VarDateFromStr()` 解析时间字符串
  - 仅返回小数部分（时间serial，不含日期）

## 修改文件
- `src/rtl/core/vb6rtl.h` — 新增8个函数声明（Weekday/DateValue/TimeSerial/TimeValue + ArrayCreate + 4个Setters）
- `src/rtl/core/vb6rtl.c` — 新增8个函数实现（约140行）
- `src/backend/cgen_expr.cpp` — 
  - builtinFuncs映射: weekday/datevalue/timeserial/timevalue
  - Weekday可选参数padding: 1参数时补`, 1`
  - Array()特殊处理: 约45行，在IIf/Choose/Switch区之后
- `src/semantics/semantic_analyzer.cpp` — 无修改（Array/Weekday/DateValue/TimeSerial/TimeValue已在P21-A之前注册）

## 测试
74/74 回归测试全部通过

## 备注
- VariantTimeFromStr不存在，应使用VarDateFromStr（OLE Automation API）
- Array()声明必须在vb6_SafeArray1D结构体定义之后，否则编译器报"type incomplete"
- Array()在cgen中不走builtinFuncs路径，而是像IIf/Choose/Switch一样在IndexOrCallExpr visitor中特殊处理
