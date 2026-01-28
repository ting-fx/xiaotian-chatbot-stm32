/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    app_netxduo.c
  * @author  MCD Application Team
  * @brief   NetXDuo applicative file
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2021 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "app_netxduo.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "nx_stm32_eth_driver.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

#define         CLIENT_ADDRESS  IP_ADDRESS(192,168,32,10)
#define         CLIENT_PACKET_SIZE  ((1536) * 1)

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */
CHAR            packet_pool_memory[(CLIENT_PACKET_SIZE+64)*32];
CHAR            ip_memory[2048];
CHAR            arp_cache_memory[2048];
NX_IP           client_ip;
NX_PACKET_POOL  client_pool;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */
UINT netxduo_setup(TX_BYTE_POOL *byte_pool);
/* USER CODE END PFP */
/**
  * @brief  Application NetXDuo Initialization.
  * @param memory_ptr: memory pointer
  * @retval int
  */
UINT MX_NetXDuo_Init(VOID *memory_ptr)
{
  UINT ret = NX_SUCCESS;
  TX_BYTE_POOL *byte_pool = (TX_BYTE_POOL*)memory_ptr;

  /* USER CODE BEGIN MX_NetXDuo_MEM_POOL */
  (void)byte_pool;
  /* USER CODE END MX_NetXDuo_MEM_POOL */

  /* USER CODE BEGIN MX_NetXDuo_Init */
  netxduo_setup(byte_pool);
  /* USER CODE END MX_NetXDuo_Init */

  return ret;
}

/* USER CODE BEGIN 1 */
UINT netxduo_setup(TX_BYTE_POOL *byte_pool)
{
UINT    status;

    /* Initialize the NetX system.  */
    nx_system_initialize();

    /* Create the Client packet pool.  */
    status =  nx_packet_pool_create(&client_pool, "HTTP Client Packet Pool", CLIENT_PACKET_SIZE,
    		                        packet_pool_memory, sizeof(packet_pool_memory));

    /* Create an IP instance.  */
    status = nx_ip_create(&client_ip, "Client IP", CLIENT_ADDRESS,
                          0xFFFFFF00UL, &client_pool, nx_stm32_eth_driver,
						  (void *)ip_memory, sizeof(ip_memory), 1);


    /* Check for IP create errors.  */
    if (status)
    {
        return status;
    }

    /* Enable ARP and supply ARP cache memory for the client IP instance.  */
    status = nx_arp_enable(&client_ip, (void *) arp_cache_memory, sizeof(arp_cache_memory));

    /* Check for ARP enable errors.  */
    if (status)
    {
        return status;
    }

    /* Enable the ICMP */
    status = nx_icmp_enable(&client_ip);

    if (status != NX_SUCCESS)
    {
      return NX_NOT_ENABLED;
    }
}
/* USER CODE END 1 */
