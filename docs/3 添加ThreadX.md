
## 添加ThreadX（X-CUBE-AZRTOS-F7)

### 1. 下载 Azure RTOS 软件包
- 打开 CubeMX，依次选择：Help → Embedded Software Packages Manager

![alt text](<imgs/3 添加ThreadX-1-下载软件包.png>)

在弹出的窗口中：
- 选择 STMicroelectronics 选项卡
- 找到 X-CUBE-AZRTOS-F7
- 下载软件包

### 2. 启用ThreadX组件

- 在Middleware and Software Packs目录下找到刚刚下载的 X-CUBE-AZRTOS-F7，打开组件选择页面

![alt text](<imgs/3 添加ThreadX-2-add-componets.png>)

- 勾选ThreadX -> Core，保存配置

![alt text](<imgs/3 添加ThreadX-3-add-threadx-component.png>)


- 勾选RTOS ThreadX

![alt text](<imgs/3 添加ThreadX-4-勾选rtos.png>)

### 3. 修改STM32 HAL时基源

由于 ThreadX 使用 SysTick 作为内核时基（RTOS Tick），需要避免与 HAL 冲突，因此必须修改 STM32 的 HAL 时间基准。

在 CubeMX 中：SYS → Timebase Source

- 将SysTick修改为其他硬件定时器，例如：TIM6

![alt text](<imgs/3 添加ThreadX-5-SYS时基修改.png>)

说明：
- SysTick → ThreadX 内核调度
- TIMx → HAL_Delay / HAL 超时机制

## 生成代码分析

完成代码生成后，在 app_threadx.c 中可以看到 CubeMX 自动生成的 ThreadX 启动框架。

### 1. ThreadX启动入口函数
```c
void MX_ThreadX_Init(void)
{
  /* USER CODE BEGIN  Before_Kernel_Start */

  /* USER CODE END  Before_Kernel_Start */

  tx_kernel_enter();

  /* USER CODE BEGIN  Kernel_Start_Error */

  /* USER CODE END  Kernel_Start_Error */
}
```

MX_ThreadX_Init() 会在 main.c 中被调用，其核心作用是调用：
```c
tx_kernel_enter();
```
该函数用于完成 ThreadX 内核层的初始化，并启动 RTOS。

### 2. 用户初始化函数App_ThreadX_Init

```c
UINT App_ThreadX_Init(VOID *memory_ptr)
{
  UINT ret = TX_SUCCESS;
  TX_BYTE_POOL *byte_pool = (TX_BYTE_POOL*)memory_ptr;

  /* USER CODE BEGIN App_ThreadX_MEM_POOL */
  (void)byte_pool;
  /* USER CODE END App_ThreadX_MEM_POOL */

  /* USER CODE BEGIN App_ThreadX_Init */

  /* USER CODE END App_ThreadX_Init */

  return ret;
}
```

用户线程在 `App_ThreadX_Init()` 里面创建。`App_ThreadX_Init()` 并不是由用户直接调用的，而是在 ThreadX 启动流程中，由 `tx_application_define()` 间接调用。因此，用户只需在 App_ThreadX_Init() 中完成初始化即可。

CubeMX 已经为我们生成了 tx_application_define() 的实现：

```c
VOID tx_application_define(VOID *first_unused_memory)
{
  /* USER CODE BEGIN  tx_application_define */

  /* USER CODE END  tx_application_define */

  VOID *memory_ptr;

  if (tx_byte_pool_create(&tx_app_byte_pool, "Tx App memory pool", tx_byte_pool_buffer, TX_APP_MEM_POOL_SIZE) != TX_SUCCESS)
  {
    /* USER CODE BEGIN TX_Byte_Pool_Error */

    /* USER CODE END TX_Byte_Pool_Error */
  }
  else
  {
    /* USER CODE BEGIN TX_Byte_Pool_Success */

    /* USER CODE END TX_Byte_Pool_Success */

    memory_ptr = (VOID *)&tx_app_byte_pool;

    if (App_ThreadX_Init(memory_ptr) != TX_SUCCESS)
    {
      /* USER CODE BEGIN  App_ThreadX_Init_Error */

      /* USER CODE END  App_ThreadX_Init_Error */
    }

    /* USER CODE BEGIN  App_ThreadX_Init_Success */

    /* USER CODE END  App_ThreadX_Init_Success */

  }

}
```

## ThreadX启动流程说明

在 `main.c` 中，程序会调用 `MX_ThreadX_Init()`，其内部调用 `tx_kernel_enter()`。

ThreadX 内核的启动流程（简化后）如下：

```c
main()
  ↓
MX_ThreadX_Init()
  ↓
tx_kernel_enter()
  ↓
tx_application_define()
  ↓
_tx_thread_schedule()
```

- tx_application_define():
    - 用于用户层初始化
    - CubeMX 已自动生成该函数
    - 并在内部调用 App_ThreadX_Init()

- _tx_thread_schedule():
    - 系统交由RTOS调度
    - 该函数不会返回
