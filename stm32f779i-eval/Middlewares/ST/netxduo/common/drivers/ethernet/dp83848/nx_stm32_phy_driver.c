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
#define MDC_GPIO_Port   GPIOB
#define MDC_Pin         GPIO_PIN_1

#define MDIO_GPIO_Port  GPIOB
#define MDIO_Pin        GPIO_PIN_2

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
