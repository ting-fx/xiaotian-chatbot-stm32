/**************************************************************************/
/*                                                                        */
/*       Copyright (c) Microsoft Corporation. All rights reserved.        */
/*                                                                        */
/*       This software is licensed under the Microsoft Software License   */
/*       Terms for Microsoft Azure RTOS. Full text of the license can be  */
/*       found in the LICENSE file at https://aka.ms/AzureRTOS_EULA       */
/*       and in the root directory of this software.                      */
/*                                                                        */
/**************************************************************************/

/* Private includes ----------------------------------------------------------*/
#include "nx_stm32_phy_driver.h"
#include "nx_stm32_eth_config.h"

/* Simple DP83848 PHY driver implementation for NetXDuo target
   - Uses HAL MDIO read/write APIs
   - Performs a PHY reset and starts autonegotiation on init
   - Reports link up/down and tries to infer speed/duplex from PHY special registers
*/

/* If project doesn't provide a PHY address, use 0x01 (adjust if needed) */
#ifndef DP83848_PHY_ADDR
#define DP83848_PHY_ADDR    0x01U
#endif

/* Use common PHY register defines from stm32f7xx_hal_conf.h when available */
#ifndef PHY_BCR
#define PHY_BCR                         ((uint16_t)0x0000U)
#endif
#ifndef PHY_BSR
#define PHY_BSR                         ((uint16_t)0x0001U)
#endif

int32_t nx_eth_phy_init(void)
{
    int32_t ret = ETH_PHY_STATUS_ERROR;

    /* Initialize MDIO clock range (GPIO/config assumed done in ETH MSP) */
    HAL_ETH_SetMDIOClockRange(&eth_handle);

    /* Reset PHY */
    if (HAL_ETH_WritePHYRegister(&eth_handle, DP83848_PHY_ADDR, PHY_BCR, PHY_RESET) != HAL_OK)
    {
        return ETH_PHY_STATUS_ERROR;
    }

    HAL_Delay(PHY_RESET_DELAY);

    /* Start auto-negotiation */
    if (HAL_ETH_WritePHYRegister(&eth_handle, DP83848_PHY_ADDR, PHY_BCR,
                                 PHY_AUTONEGOTIATION | PHY_RESTART_AUTONEGOTIATION) != HAL_OK)
    {
        return ETH_PHY_STATUS_ERROR;
    }

    ret = ETH_PHY_STATUS_OK;
    return ret;
}

int32_t nx_eth_phy_get_link_state(void)
{
  uint32_t regval = 0U;

  /* Read Basic Status Register */
  if (HAL_ETH_ReadPHYRegister(&eth_handle, DP83848_PHY_ADDR, PHY_BSR, &regval) != HAL_OK)
  {
    return ETH_PHY_STATUS_LINK_ERROR;
  }

  /* Check link status */
  if ((regval & PHY_LINKED_STATUS) == 0U)
  {
    return ETH_PHY_STATUS_LINK_DOWN;
  }

  /* Link is up — attempt to read PHY special/status register (0x11 common for many PHYs)
     to infer speed/duplex. If read fails, assume 100M full-duplex. */
  uint32_t sr = 0U;
  if (HAL_ETH_ReadPHYRegister(&eth_handle, DP83848_PHY_ADDR, 0x11U, &sr) == HAL_OK)
  {
    /* Common DP83848: bit 2 -> speed (1 = 100Mb), bit 3 -> duplex (1 = full) */
    uint32_t speed100 = (sr & (1U << 2)) ? 1U : 0U;
    uint32_t fullDup  = (sr & (1U << 3)) ? 1U : 0U;

    if (speed100)
    {
      if (fullDup) return ETH_PHY_STATUS_100MBITS_FULLDUPLEX;
      else         return ETH_PHY_STATUS_100MBITS_HALFDUPLEX;
    }
    else
    {
      if (fullDup) return ETH_PHY_STATUS_10MBITS_FULLDUPLEX;
      else         return ETH_PHY_STATUS_10MBITS_HALFDUPLEX;
    }
  }

  /* Fallback: report 100M full duplex when link is up but status register unavailable */
  return ETH_PHY_STATUS_100MBITS_FULLDUPLEX;
}

nx_eth_phy_handle_t nx_eth_phy_get_handle(void)
{
 nx_eth_phy_handle_t handle = NULL;
 /* No external PHY object in this simple driver; return NULL handle */
 return handle;
}

/* Optional: allow upper layers to request a specific link state (stub) */
int32_t nx_eth_phy_set_link_state(int32_t linkstate)
{
  /* Not implemented: this driver relies on PHY autonegotiation.
     Return OK to avoid breaking callers; adapt if explicit force required. */
  (void)linkstate;
  return ETH_PHY_STATUS_OK;
}
