# 使用 GPIO 模拟 MDIO 接口配置以太网 PHY  

本文将以 **STM32F779 + DP83848 + NetXDuo** 为例，介绍如何通过 GPIO 模拟MDIO接口，完成以太网PHY的初始化与配置。

在 STM32 以太网应用中，MCU 与 PHY 芯片之间通常通过两类接口通信：

- MDIO： 用于读写 PHY 内部寄存器  
- MII / RMII：用于收发以太网数据帧 

## 1. PHY 简介

PHY（Physical Layer）属于以太网 物理层，主要负责完成以下功能：

- 模拟信号与数字信号之间的转换
- 以太网链路的建立与维护
- 自动协商速率（10M / 100M）、双工模式（全双工 / 半双工）等

MCU 内部通常集成 MAC（媒体访问控制）层，而 PHY 则作为外部芯片，通过接口与 MAC 相连。

## 2 MDIO 接口

MDIO接口是一种双线串行总线。由时钟信号线MDC和数据信号线MDIO组成，用于MAC与PHY之间的通信，主要是用于读写PHY内部寄存器，不传输以太网数据。

MDIO接口主要在 PHY 上电或复位后的初始化阶段使用，在正常的数据通信过程中访问频率较低。

根据 IEEE 802.3 协议中将MDIO分为两种帧格式：

- Clause 22：最常用格式，适用于大多数 10M / 100M PHY
- Clause 45：Clause 22 的扩展版本，支持更多寄存器，主要用于高速以太网 PHY

本文所使用的 DP83848 采用的是 IEEE 802.3 Clause 22 协议。

### 2.1 MDIO 帧格式（Clause 22）

下图为使用 STM32 ETH 外设输出 MDC / MDIO 信号，通过逻辑分析仪捕获的通信波形：

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

## 3 MII / RMII接口：传输以太网数据

MII（Media Independent Interface） 是一种标准化的数据接口，用于连接 MAC 与 PHY，负责以太网数据的实际传输。
由于其标准化特性，同一 MAC 可以适配多种不同型号的 PHY 芯片。

RMII（Reduced MII） 是 MII 的简化版本，通过减少信号线数量来降低引脚占用，更适合引脚资源受限的 MCU 设计。

## 4 为什么使用 GPIO 模拟 MDIO 接口？
在大多数情况下，STM32 内部的 **ETH 外设** 可以直接提供硬件 MDIO 支持。然而，在某些情况下（例如硬件引脚与音频编解码器等其他外设冲突），硬件MDIO 引脚可能无法使用。此时，通过 GPIO 模拟 / MDIO 接口便成为一种可行的替代方案。


## 5 开发板跳线配置

以 STM32F779I-EVAL 开发板为例。
由于默认的 PC1/PA2 与音频编解码器冲突，这里将 PHY 的 MDC / MDIO 信号重新连接到：
- PJ13 → MDC
- PJ12 → MDIO

通过GPIO模拟MDIO接口。下面是跳线配置：
![alt text](<imgs/7 添加NetXDuo-7-JP4-JP8-Jumpers.png>)

* **JP4 → 2–3**
* **JP8 → 2–3**

## 6 代码实现

以下代码实现了基于 DP83848 的 MDIO 模拟驱动，并完成了与 Azure RTOS NetXDuo PHY 驱动接口的适配。

### 6.1 基础操作与时序
```c

#define MDC_HIGH()      HAL_GPIO_WritePin(MDC_GPIO_Port, MDC_Pin, GPIO_PIN_SET)
#define MDC_LOW()       HAL_GPIO_WritePin(MDC_GPIO_Port, MDC_Pin, GPIO_PIN_RESET)

#define MDIO_HIGH()     HAL_GPIO_WritePin(MDIO_GPIO_Port, MDIO_Pin, GPIO_PIN_SET)
#define MDIO_LOW()      HAL_GPIO_WritePin(MDIO_GPIO_Port, MDIO_Pin, GPIO_PIN_RESET)
#define MDIO_READ()     HAL_GPIO_ReadPin(MDIO_GPIO_Port, MDIO_Pin)

/* STM32F7 主频较高，增加延迟以确保频率在 1MHz - 2.5MHz 之间 */
static void mdio_delay(void)
{
    for (uint32_t i = 0; i < 100; i++) {
        __asm("NOP");
    }
}

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
```

### 6.2 读写核心逻辑

```c
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
```

### 6.3 PHY 驱动接口实现
```c
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
```
### 6.4 PHY 驱动适配 NetXDuo
```c
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
### 5.5 逻辑分析仪波形

![alt text](<imgs/8 MDIO_MDC_WAV2.png>)