# 使用 GPIO 模拟 MDC / MDIO 接口  

在 STM32 以太网应用中，MCU 与 PHY 芯片之间通常通过两类接口通信：

- **数据接口**：MII / RMII，用于收发以太网数据帧  
- **管理接口**：MDC / MDIO，用于配置和管理 PHY 寄存器  

在大多数情况下，STM32 内部的 **ETH 外设** 可以直接提供硬件 MDC / MDIO 支持。但是由于引脚和audio codec冲突，这边通过 GPIO 模拟 MDC / MDIO 接口成为了一种可行的替代方案。

本文将以 **STM32F779 + DP83848 + NetXDuo** 为例，介绍如何通过 GPIO 模拟 MDC / MDIO，并成功驱动以太网 PHY 工作。

## 1 什么是 MDC 和 MDIO？

MDC / MDIO 是 IEEE 802.3 定义的 **PHY 管理接口（MIIM）**。

| 信号 | 方向 | 说明 |
|----|----|----|
| MDC | MCU → PHY | 管理接口时钟 |
| MDIO | 双向 | 管理接口数据 |

需要注意的是：

- MDC / MDIO **不传输以太网数据**
- 仅用于 **读写 PHY 内部寄存器**

## 2 MDIO 通信协议（Clause 22）

DP83848 使用的是 **IEEE 802.3 Clause 22** 标准协议。

### 2.1 MDIO 帧格式


下图是使用eth外设输出MDC/MDIO信号逻辑分析仪捕获的 MDC / MDIO 通信波形：

![alt text](<imgs/8 MDIO_MDC_WAV.png>)
字段说明：

- **Preamble**：32 个连续的 `1`
- **START**：起始字段，固定为 `01`
- **OPCODE**：操作码  
  - 写：`01`  
  - 读：`10`
- **PHY Address**：PHY 地址（5 位）
- **Register Address**：寄存器地址（5 位）
- **Turnaround**：方向切换
- **DATA**：16 位寄存器数据

### 2.2 写寄存器流程

- 发送 Preamble
- 发送 Start + Opcode
- 发送 PHY 地址
- 发送寄存器地址
- 发送 Turnaround
- 发送 16 位数据

### 2.3 读寄存器流程

- 发送 Preamble
- 发送 Start + Opcode
- 发送 PHY 地址
- 发送寄存器地址
- 切换 MDIO 为输入
- 读取 16 位数据

整个过程完全符合 IEEE 802.3 Clause 22 规范。

## 3 开发板跳线配置

因为 STM32F779I-EVAL 板上 **PC1 / PA2** 引脚和audio codec冲突，我们需要使用audio codec播放声音，所以这边在phy侧将MDC和MDIO连接到PJ13和PJ12引脚。通过GPIO模拟MDC/MDIO接口。下面是跳线配置：
![alt text](<imgs/7 添加NetXDuo-7-JP4-JP8-Jumpers.png>)

* **JP4 → 2–3**
* **JP8 → 2–3**

## 4 代码实现

### 代码实现
```c
/**************************************************************************/
/*                                                                        */
/*  GPIO Bit-Bang MDC / MDIO driver for DP83848 (Clause 22)               */
/*  Target: STM32F7 Series                                                */
/*                                                                        */
/**************************************************************************/

#include "stm32f7xx_hal.h"
#include "nx_stm32_phy_driver.h"
#include "nx_stm32_eth_config.h"

/* ===================================================================== */
/* ======================= GPIO CONFIGURATION ========================== */
/* ===================================================================== */

/* 请根据实际硬件电路修改引脚定义 */
#define MDC_GPIO_Port   GPIOJ
#define MDC_Pin         GPIO_PIN_13

#define MDIO_GPIO_Port  GPIOJ
#define MDIO_Pin        GPIO_PIN_12

/* ===================================================================== */
/* ======================= GPIO OPERATIONS ============================== */
/* ===================================================================== */

#define MDC_HIGH()      HAL_GPIO_WritePin(MDC_GPIO_Port, MDC_Pin, GPIO_PIN_SET)
#define MDC_LOW()       HAL_GPIO_WritePin(MDC_GPIO_Port, MDC_Pin, GPIO_PIN_RESET)

#define MDIO_HIGH()     HAL_GPIO_WritePin(MDIO_GPIO_Port, MDIO_Pin, GPIO_PIN_SET)
#define MDIO_LOW()      HAL_GPIO_WritePin(MDIO_GPIO_Port, MDIO_Pin, GPIO_PIN_RESET)
#define MDIO_READ()     HAL_GPIO_ReadPin(MDIO_GPIO_Port, MDIO_Pin)

/* ===================================================================== */
/* ======================= DELAY (MDC CLOCK) ============================ */
/* ===================================================================== */

/* STM32F7 主频较高，增加延迟以确保频率在 1MHz - 2.5MHz 之间 */
static void mdio_delay(void)
{
    for (uint32_t i = 0; i < 100; i++) {
        __asm("NOP");
    }
}

/* ===================================================================== */
/* ======================= MDIO DIRECTION =============================== */
/* ===================================================================== */

static void mdio_set_output(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = MDIO_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP; /* 使用推挽输出提高信号质量 */
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(MDIO_GPIO_Port, &GPIO_InitStruct);
}

static void mdio_set_input(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = MDIO_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP; /* 使用内部上拉增强空闲状态稳定性 */
    HAL_GPIO_Init(MDIO_GPIO_Port, &GPIO_InitStruct);
}

/* ===================================================================== */
/* ======================= BIT OPERATIONS =============================== */
/* ===================================================================== */

/* 写一位数据：在 MDC 上升沿前准备数据，PHY 在上升沿采样 */
static void mdio_write_bit(uint8_t bit)
{
    if (bit) MDIO_HIGH(); else MDIO_LOW();
    mdio_delay();
    MDC_HIGH();
    mdio_delay();
    MDC_LOW();
    mdio_delay();
}

/* 读一位数据：在 MDC 上升沿后数据有效，在高电平期间采样 */
static uint8_t mdio_read_bit(void)
{
    uint8_t bit;
    MDC_LOW();
    mdio_delay();
    MDC_HIGH();
    mdio_delay(); /* 等待 PHY 输出数据稳定 */
    bit = (uint8_t)MDIO_READ();
    MDC_LOW();
    mdio_delay();
    return bit;
}

/* 发送 32 位全 1 前导码 */
static void mdio_send_preamble(void)
{
    mdio_set_output();
    for (int i = 0; i < 32; i++)
    {
        mdio_write_bit(1);
    }
}

/* ===================================================================== */
/* ======================= PHY IO FUNCTIONS ============================= */
/* ===================================================================== */

static int32_t dp83848_io_init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* 1. 开启 GPIO 时钟 (关键修正) */
    if (MDC_GPIO_Port == GPIOB || MDIO_GPIO_Port == GPIOB) __HAL_RCC_GPIOB_CLK_ENABLE();
    // 如使用其他 Port 请在此补充使能

    /* 2. 初始化 MDC 引脚 (关键修正) */
    GPIO_InitStruct.Pin = MDC_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(MDC_GPIO_Port, &GPIO_InitStruct);

    /* 3. 初始化 MDIO 引脚 */
    mdio_set_output();

    MDC_LOW();
    return ETH_PHY_STATUS_OK;
}

static int32_t dp83848_io_deinit(void)
{
    return ETH_PHY_STATUS_OK;
}

/* 写入 PHY 寄存器 */
static int32_t dp83848_io_write_reg(uint32_t DevAddr, uint32_t RegAddr, uint32_t RegVal)
{
    mdio_send_preamble();

    /* Start(01) + OpCode(Write=01) */
    mdio_write_bit(0); mdio_write_bit(1);
    mdio_write_bit(0); mdio_write_bit(1);

    /* PHY Address (5 bits) */
    for (int i = 4; i >= 0; i--) mdio_write_bit((DevAddr >> i) & 1);

    /* Register Address (5 bits) */
    for (int i = 4; i >= 0; i--) mdio_write_bit((RegAddr >> i) & 1);

    /* Turnaround (10) */
    mdio_write_bit(1); mdio_write_bit(0);

    /* Data (16 bits) */
    for (int i = 15; i >= 0; i--) mdio_write_bit((RegVal >> i) & 1);

    mdio_set_input(); /* 释放总线 */
    return ETH_PHY_STATUS_OK;
}

/* 读取 PHY 寄存器 */
static int32_t dp83848_io_read_reg(uint32_t DevAddr, uint32_t RegAddr, uint32_t *pRegVal)
{
    uint32_t val = 0;

    mdio_send_preamble();

    /* Start(01) + OpCode(Read=10) */
    mdio_write_bit(0); mdio_write_bit(1);
    mdio_write_bit(1); mdio_write_bit(0);

    /* PHY Address (5 bits) */
    for (int i = 4; i >= 0; i--) mdio_write_bit((DevAddr >> i) & 1);

    /* Register Address (5 bits) */
    for (int i = 4; i >= 0; i--) mdio_write_bit((RegAddr >> i) & 1);

    /* Turnaround (切换为输入并跳过第一个 Z 位) */
    mdio_set_input();
    mdio_delay();

    MDC_LOW(); mdio_delay(); MDC_HIGH(); mdio_delay(); // TA 第一位 (Z)
    MDC_LOW(); mdio_delay(); MDC_HIGH(); mdio_delay(); // TA 第二位 (0)

    /* Data (16 bits) */
    for (int i = 15; i >= 0; i--)
    {
        if (mdio_read_bit()) val |= (1 << i);
    }

    *pRegVal = val;
    return ETH_PHY_STATUS_OK;
}

static int32_t dp83848_io_get_tick(void)
{
    return (int32_t)HAL_GetTick();
}

/* ===================================================================== */
/* ======================= Azure RTOS Interface ======================== */
/* ===================================================================== */

static dp83848_Object_t DP83848;

static dp83848_IOCtx_t DP83848_IOCtx =
{
    dp83848_io_init,
    dp83848_io_deinit,
    dp83848_io_write_reg,
    dp83848_io_read_reg,
    dp83848_io_get_tick
};

int32_t nx_eth_phy_init(void)
{
    /* 将手动实现的 IO 绑定到 DP83848 驱动 */
    DP83848_RegisterBusIO(&DP83848, &DP83848_IOCtx);

    if (DP83848_Init(&DP83848) == DP83848_STATUS_OK)
        return ETH_PHY_STATUS_OK;

    return ETH_PHY_STATUS_ERROR;
}

int32_t nx_eth_phy_get_link_state(void)
{
    return DP83848_GetLinkState(&DP83848);
}

nx_eth_phy_handle_t nx_eth_phy_get_handle(void)
{
    return (nx_eth_phy_handle_t)&DP83848;
}
```
### 逻辑分析仪波形
![alt text](<imgs/8 MDIO_MDC_WAV2.png>)