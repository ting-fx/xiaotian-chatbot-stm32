# 添加 NetXDuo

本文以 **STM32F779I-EVAL + DP83848 PHY** 为例，介绍如何在 CubeMX / CubeIDE 中添加并配置 **NetXDuo**，并完成以太网连通性验证。

## 1 CubeMX 配置

### 1.1 选择 X-CUBE-AZRTOS-F7 组件

在 **Middleware and Software Packs** 目录下找到已下载的 **X-CUBE-AZRTOS-F7**，进入组件选择页面：

![alt text](<imgs/7 添加NetXDuo-1.png>)

* 勾选 **NX Core** 和 **Ethernet Interface**
* **Ethernet PHY Interface** 选择 **LAN8742**

> 说明：
> 本文使用的开发板为 **STM32F779I-EVAL**，板载以太网 PHY 为 **DP83848**。由于 CubeMX 中没有 DP83848 的选项，这里先选择 **LAN8742**，后续再手动替换为 **DP83848 PHY 驱动**。

### 1.2 勾选组件

![alt text](<imgs/7 添加NetXDuo-2.png>)


### 1.3 启用 ETH 外设

![alt text](<imgs/7 添加NetXDuo-3-eth-config.png>)

* 启用 **ETH** 外设
* **Mode** 选择 **MII**
* 使能中断 **ETH global interrupt**

### 1.4 生成代码

完成以上配置后生成工程代码。

## 2 替换 PHY 驱动（LAN8742 → DP83848）

![ethernet hal文件](<imgs/7 添加NetXDuo-4-替换phy.png>)
![alt text](<imgs/7 添加NetXDuo-4-替换phy2.png>)
操作步骤：

* 添加 **DP83848 BSP 驱动文件**
* 将自动生成的 `lan8742` 文件夹重命名为 `dp83848`
* 修改 `nx_stm32_eth_driver.c`，将所有 `lan8742` 相关函数和结构体名称替换为 `dp83848`
* `nx_stm32_eth_config.h` 中添加 `dp83848` 头文件引用

## 3 添加 NetXDuo 初始化代码

### 3.1 参数与内存定义

```c
#define         CLIENT_ADDRESS       IP_ADDRESS(192,168,32,10)
#define         CLIENT_PACKET_SIZE   (1536)

CHAR            packet_pool_memory[(CLIENT_PACKET_SIZE + 64) * 32];
CHAR            ip_memory[2048];
CHAR            arp_cache_memory[2048];
NX_IP           client_ip;
NX_PACKET_POOL  client_pool;
```

### 3.2 NetXDuo 初始化函数

```c
UINT netxduo_setup(TX_BYTE_POOL *byte_pool)
{
    UINT status;

    /* Initialize the NetX system. */
    nx_system_initialize();

    /* Create packet pool. */
    status = nx_packet_pool_create(&client_pool,
                                   "Client Packet Pool",
                                   CLIENT_PACKET_SIZE,
                                   packet_pool_memory,
                                   sizeof(packet_pool_memory));
    if (status)
    {
        return status;
    }

    /* Create IP instance. */
    status = nx_ip_create(&client_ip,
                           "Client IP",
                           CLIENT_ADDRESS,
                           0xFFFFFF00UL,
                           &client_pool,
                           nx_stm32_eth_driver,
                           (void *)ip_memory,
                           sizeof(ip_memory),
                           1);
    if (status)
    {
        return status;
    }

    /* Enable ARP. */
    status = nx_arp_enable(&client_ip,
                            (void *)arp_cache_memory,
                            sizeof(arp_cache_memory));
    if (status)
    {
        return status;
    }

    /* Enable ICMP (ping). */
    status = nx_icmp_enable(&client_ip);
    if (status != NX_SUCCESS)
    {
        return NX_NOT_ENABLED;
    }

    return NX_SUCCESS;
}
```

## 4 在 CubeMX 初始化流程中调用

在 CubeMX 生成的 `MX_NetXDuo_Init()` 中调用 `netxduo_setup()`：

```c
UINT MX_NetXDuo_Init(VOID *memory_ptr)
{
    TX_BYTE_POOL *byte_pool = (TX_BYTE_POOL *)memory_ptr;

    netxduo_setup(byte_pool);

    return NX_SUCCESS;
}
```

## 5 STM32F779I-EVAL 跳线配置

![alt text](<imgs/7 添加NetXDuo-8-跳线配置.png>)

STM32F779I-EVAL 板上，**MCU 的 PC1 / PA2** 信号可通过跳线在不同外设之间切换。

本例中选择将 **MCU 的 PC1 / PA2** 连接到 **以太网 PHY（DP83848）**。


### 5.1 MCU 侧跳线配置

![alt text](<imgs/7 添加NetXDuo-5-JP6-Jumper.png>)
![alt text](<imgs/7 添加NetXDuo-6-JP3-Jumper.png>)

* **JP3 → 2–3**
* **JP6 → 2–3**

### 5.2 PHY 侧跳线配置

![alt text](<imgs/7 添加NetXDuo-7-JP4-JP8-Jumpers.png>)

* **JP4 → 2–3**
* **JP8 → 1–2**


### 5.3 MDC / MDIO 说明

* **MDC**：管理时钟线（Clock）
* **MDIO**：管理数据线（Data）

它们组成 **PHY 管理接口（MIIM / MDIO）**，
**仅用于读写 PHY 寄存器，不传输以太网数据**。

本配置中：

* MCU 使用硬件 ETH 外设控制 MDC / MDIO
* PHY 侧跳线与 MCU 的 PC1 / PA2 对应

## 6 测试验证

插入网线，在 PC 端对开发板 IP 地址进行 `ping` 测试：

![alt text](<imgs/7 添加NetXDuo-9-测试.png>)

若能够收到正常回复，说明：

* NetXDuo 初始化成功
* STM32 ETH MAC 工作正常
* DP83848 PHY 通信与链路建立正常

至此，**STM32F779I-EVAL + NetXDuo + DP83848** 的基础以太网环境搭建完成。
