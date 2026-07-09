# P24-Timer: Timer控件修复 — 窗口关联定时器 + WndProc分发

> 日期: 2026-07-09
> 提交: b637ad5
> 回归: 76/76 零失败

## 背景

frxParse演示中Timer1_Timer事件不触发，ImageList动画静止。VB6编译的原版exe正常切换图片，c3编译的x86版本Timer回调从未被调用。

## 根因分析

`vb6_SetTimer(interval, callback)` 内部调用 `SetTimer(NULL, id, interval, NULL)` 创建线程级定时器：
- `SetTimer(NULL, ...)` 创建的定时器理论上将 WM_TIMER 投递到线程消息队列
- **实际问题**： GetMessage() 永远收不到 WM_TIMER 消息（Windows行为，NULL hwnd定时器消息投递不可靠）
- 诊断测试确认：SetTimer返回成功(ID=100)，但消息循环中 msg.message == WM_TIMER 从未匹配

### 硬编码验证

临时修改 `SetTimer(NULL, id, interval, NULL)` → `SetTimer(hwnd, id, interval, NULL)` 后：
- WM_TIMER 正常到达 WndProc
- Timer1_Timer 事件正确触发
- ImageList1.ListImages(i).Picture 完整COM调用链均返回有效指针
- 图片动画正常切换

## 修复方案（3层）

### 第1层: RTL — vb6forms.c / vb6forms.h

1. **Timer回调表增加hwnd字段**：
   ```c
   static struct {
       int timerId;
       HWND hwnd;    // P24-Timer: 关联的窗体句柄
       vb6_TimerCallback callback;
   } g_timerTable[VB6_MAX_TIMERS];
   ```

2. **vb6_SetTimer签名变更**：
   ```c
   // Old: int vb6_SetTimer(int interval, void* callback)
   // New: int vb6_SetTimer(void* hwnd, int interval, void* callback)
   ```
   实现改用 `SetTimer((HWND)hwnd, id, interval, NULL)`

3. **vb6_KillTimer适配**：从回调表查找关联hwnd，传给KillTimer

4. **新增vb6_DispatchTimer()**：WndProc中查找timerId并调用回调
   ```c
   void vb6_DispatchTimer(int timerId) {
       for (int i = 0; i < g_timerCount; i++) {
           if (g_timerTable[i].timerId == timerId) {
               if (g_timerTable[i].callback) g_timerTable[i].callback();
               break;
           }
       }
   }
   ```

5. **移除消息循环中的WM_TIMER拦截**：
   - `vb6_MessageLoop`: 删除WM_TIMER回调分发代码
   - `vb6_DoEvents`: 同上
   - `vb6_ShowForm` 模态循环：同上
   - WM_TIMER现在由WndProc处理，不经过消息循环

### 第2层: cgen — cgen_form.cpp

1. **Timer注册位置迁移**：从 `form_create` 函数移到 WM_CREATE case 中
   - form_create 被调用时窗口尚未创建，hwnd不可用
   - WM_CREATE 时 hwnd 已存在（作为WndProc参数）

2. **传入hwnd参数**：
   ```cpp
   // Old: vb6_SetTimer(interval, (void*)timerFn)
   // New: vb6_SetTimer((void*)hwnd, interval, (void*)timerFn)
   ```

3. **WndProc生成WM_TIMER case**：
   ```cpp
   c_.emitLine("case WM_TIMER: {");
   c_.emitLine("vb6_DispatchTimer((int)wParam);");
   c_.emitLine("break;");
   c_.emitLine("}");
   ```

### 第3层: RTL库重建

- x64: `vb6forms.obj` → `vb6rtl_gui.lib`
- x86: `vb6forms.obj` → `x86/vb6rtl_gui.lib`
- `c3rtl.rc` touch → c3.exe 重建

## 验证

| 验证项 | 结果 |
|--------|------|
| c3.exe 构建 | 成功 |
| frxParse x86编译 | 成功 |
| frxParse 运行 (Timer1_Timer) | 图片正常切换 |
| 76/76 回归测试 | 全部PASS |

## 修改文件

| 文件 | 变更 |
|------|------|
| src/rtl/core/vb6forms.c | vb6_SetTimer加hwnd参数+vb6_DispatchTimer+移除3处WM_TIMER拦截 |
| src/rtl/core/vb6forms.h | vb6_SetTimer新签名+vb6_DispatchTimer声明 |
| src/backend/cgen_form.cpp | Timer注册移至WM_CREATE+传hwnd+WndProc生成WM_TIMER case |
| src/rtl/lib/vb6rtl_gui.lib | x64重建 |
| src/rtl/lib/x86/vb6rtl_gui.lib | x86重建 |
