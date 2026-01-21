# ThreadX 线程切换机制

![alt text](<imgs/4 ThreadX线程切换-1.png>)

## PendSV

PendSV 是 Cortex-M 内核提供的一种 可挂起（Pendable）、优先级最低的异常，专门用于在合适且安全的时机执行操作系统相关的工作，最典型的用途就是 线程上下文切换（Context Switch）。

## PendSV触发时机

当ThreadX判断需要切换线程时，会通过软件方式触发PendSV，随后，在合适的时机，PendSV_Handler 被执行，完成线程上下文切换。

在实际运行过程中，PendSV 的触发场景非常多，比如：

#### 1. 线程因等待资源而阻塞（Running → Suspend）

例如，某个线程在获取信号量时：
```c
tx_semaphore_get(&sem, TX_WAIT_FOREVER);
```

如果当前信号量不可用：

- 当前线程会从 Running 状态进入 Suspended
- 调度器需要重新选择一个就绪队列中优先级最高的线程
- 触发PendSV，请求进行线程切换
- PendSV 执行后，CPU 切换到新的线程运行

#### 2. 更高优先级线程被唤醒（Ready → Running）

例如，某个线程或中断中执行：
```c
tx_semaphore_put(&sem);
```

此时如果：

- 有一个更高优先级线程正在等待该信号量
- 该线程从 Suspended 状态变为 Ready
- 且其优先级高于当前正在运行的线程

## PenSV中的上下文切换过程

PendSV 中断服务函数主要完成两件事：

- 保存当前线程的上下文
- 恢复下一个线程上下文

### 1. 保存当前线程上下文

如果当前有线程正在运行，当 PendSV 异常被响应时，在跳转到 PendSV_Handler 之前，CPU 会自动完成以下操作：

- 自动入栈：r0–r3、r12、lr、pc、xPSR
- 修改 LR 的值为一个特殊标志（例如 0xFFFFFFF9 或 0xFFFFFFFD），用于指示返回路径

进入PendSV_Handler后的处理流程：
- 将 通用寄存器 r4–r11 压入线程栈
- 如果启用了 VFP：保存 浮点寄存器 s16–s31
- 保存当前LR
- 将当前线程的 栈指针（PSP） 保存到线程控制块（TCB）中

至此，当前线程的执行现场被完整保存。

### 2. 恢复新线程上下文

接下来，PendSV 切换到即将运行的新线程：

- 从新线程的 TCB 中取出保存的 栈指针

- 依次从栈中恢复：
    - LR（EXC_RETURN）
    - 若启用 VFP，恢复 s16–s31
    - 恢复 r4–r11
- 将 PSP 设置为新线程的栈指针
- 执行：bx lr

    当中断服务函数执行 `bx lr` 时，硬件检测到这个特殊 lr 值，就会自动触发 **异常返回（Exception Return）** 流程：

    - 从栈中弹出之前保存的 PC、LR、R0–R3 等寄存器  
    - 恢复中断前的 CPU 状态  
    - 跳转回中断前的代码继续执行
