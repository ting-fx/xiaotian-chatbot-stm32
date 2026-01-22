# 添加GUIX支持

本节介绍如何在 STM32 + ThreadX 工程中集成 GUIX 图形框架，包括源码引入、工程配置以及初始化流程。

## 1. 获取 GUIX 源码

从官方仓库下载 GUIX：

```bash
https://github.com/eclipse-threadx/guix
```

## 2. 拷贝 GUIX 源码到工程

将 GUIX 相关源码拷贝到工程的 Middlewares 目录下：

![alt text](<imgs/5 添加GUIX-1-文件目录.png>)

- common/ports目录下的文件来自GUIX仓库
- drivers 目录下的文件可从 GUIX 的 examples 中获取

## 3. 添加 BSP 与 HAL 依赖

GUIX 的显示与触摸驱动依赖底层 BSP 和 HAL 文件，需要一并加入工程：

- 添加BSP文件

![alt text](<imgs/5 添加GUIX-2-bsp-hal文件目录.png>)

- 在CubeMX里启用LTDC和DMA2D外设，生成对应HAL文件。

![alt text](<imgs/5 添加GUIX-2-bsp-hal文件目录2.png>)

## 4. STM32CubeIDE 工程配置

- 在 Project Properties → C/C++ Build → Settings → Include paths 中添加相关头文件目录：

![alt text](<imgs/5 添加GUIX-3-settings.png>)

- 添加宏定义
![alt text](<imgs/5 添加GUIX-4-settings.png>)

## 5. 创建 GUIX Studio 工程

GUI 界面通过 GUIX Studio 生成。
GUIX Studio 的安装与使用说明可参考官方文档：
```bash
https://github.com/eclipse-threadx/rtos-docs/blob/main/rtos-docs/guix/guix-studio-2.md
```
创建并生成工程后，会得到：
- *_resources.c/h
- *_specifications.c/h

将生成的文件加入 STM32 工程。

![alt text](<imgs/5 添加GUIX-5-studio工程.png>)

## 6. GUIX 初始化代码

### 6.1 线程与资源定义
```c
#include "gx_api.h"
#include "gx_display_driver_stm32f779i-eval_565rgb.h"
#include "xt_gui_resources.h"
#include "xt_gui_specifications.h"

#define  CANVAS_MEMORY_SIZE      (DISPLAY_1_X_RESOLUTION * DISPLAY_1_Y_RESOLUTION)

/* GUI 线程 */
TX_THREAD             gui_thread;
UCHAR                 gui_thread_stack[4096];

/* 触摸线程 */
TX_THREAD             touch_thread;
ULONG                 touch_thread_stack[1024];

/* GUIX 画布内存，放到 SDRAM */
__attribute__((section(".SD_RAM"))) USHORT canvas_memory[CANVAS_MEMORY_SIZE];
GX_WINDOW_ROOT    *root;
```

### 6.2 GUI 线程创建


```c
void gui_setup()
{
    /* 创建Touch线程  */
    tx_thread_create(&touch_thread, "GUIX Touch Thread", touch_thread_entry, 0,
                     touch_thread_stack, sizeof(touch_thread_stack),
                     GX_SYSTEM_THREAD_PRIORITY - 1,
                     GX_SYSTEM_THREAD_PRIORITY - 1, TX_NO_TIME_SLICE, TX_AUTO_START);

    /* 创建GUI线程  */
    tx_thread_create(&gui_thread, "GUI Thread", gui_thread_entry, 0,
                     gui_thread_stack, sizeof(gui_thread_stack),
                     GX_SYSTEM_THREAD_PRIORITY,
                     GX_SYSTEM_THREAD_PRIORITY, TX_NO_TIME_SLICE, TX_AUTO_START);
}
```

说明：
- Touch 线程优先级略高于 GUI 线程，保证触摸响应及时
- GUIX 通常不使用时间片轮转（TX_NO_TIME_SLICE）

### 6.3 GUIX 初始化流程
```c
VOID gui_thread_entry(ULONG thread_input)
{
    /* 初始化 GUIX */
    gx_system_initialize();

    /* 配置display */
    gx_studio_display_configure(DISPLAY_1, stm32f779_graphics_driver_setup_565rgb,
                                LANGUAGE_ENGLISH, DISPLAY_1_THEME_1, &root);

    /* 定义画布memory */
    gx_canvas_memory_define(root->gx_window_root_canvas, (GX_COLOR *)canvas_memory, CANVAS_MEMORY_SIZE * sizeof(USHORT));

    /* 创建主窗口. */
    gx_studio_named_widget_create("main_screen", (GX_WIDGET *)root, GX_NULL);

    /* 显示root窗口.  */
    gx_widget_show(root);

    /* 运行 GUIX */
    gx_system_start();
}
```

## 7. 启用 CPU Cache

```c
  /* Enable I-Cache */
  SCB_EnableICache();

  /* Enable D-Cache */
  SCB_EnableDCache();
```

## 8. 启动 GUIX

在 App_ThreadX_Init() 中调用：
```c
gui_setup();
```



