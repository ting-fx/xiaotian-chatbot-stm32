
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


#include "nx_stm32_phy_driver.h"
#include "nx_stm32_eth_config.h"


/* DP83848 IO functions */
static int32_t DP83848_io_init(void);
static int32_t DP83848_io_deinit (void);

static int32_t DP83848_io_write_reg(uint32_t DevAddr, uint32_t RegAddr, uint32_t RegVal);
static int32_t DP83848_io_read_reg(uint32_t DevAddr, uint32_t RegAddr, uint32_t *pRegVal);

static int32_t DP83848_io_get_tick(void);

/* DP83848 IO context object */
static DP83848_IOCtx_t  DP83848_IOCtx = { DP83848_io_init,
                                          DP83848_io_deinit,
                                          DP83848_io_write_reg,
                                          DP83848_io_read_reg,
                                          DP83848_io_get_tick
                                        };
/* DP83848 main object */
static DP83848_Object_t DP83848;

/**
  * @brief  Initialize the PHY interface
  * @param  none
  * @retval ETH_PHY_STATUS_OK on success, ETH_PHY_STATUS_ERROR otherwise
  */

int32_t nx_eth_phy_init(void)
{
    int32_t ret = ETH_PHY_STATUS_ERROR;
    /* Set PHY IO functions */

    DP83848_RegisterBusIO(&DP83848, &DP83848_IOCtx);
    /* Initialize the DP83848 ETH PHY */

    if (DP83848_Init(&DP83848) == DP83848_STATUS_OK)
    {
        ret = ETH_PHY_STATUS_OK;
    }

    return ret;
}

/**
  * @brief  get the Phy link status.
  * @param  none
  * @retval the link status.
  */

int32_t nx_eth_phy_get_link_state(void)
{
    int32_t  linkstate = DP83848_GetLinkState(&DP83848);

    return linkstate;
}

/**
  * @brief  get the driver object handle
  * @param  none
  * @retval pointer to the DP83848 main object
  */

nx_eth_phy_handle_t nx_eth_phy_get_handle(void)
{
    return (nx_eth_phy_handle_t)&DP83848;
}

/**
  * @brief  Initialize the PHY MDIO interface
  * @param  None
  * @retval 0 if OK, -1 if ERROR
  */

int32_t DP83848_io_init(void)
{
  /* We assume that MDIO GPIO configuration is already done
     in the ETH_MspInit() else it should be done here
  */

  /* Configure the MDIO Clock */
  HAL_ETH_SetMDIOClockRange(&eth_handle);

  return ETH_PHY_STATUS_OK;
}

/**
  * @brief  De-Initialize the MDIO interface
  * @param  None
  * @retval 0 if OK, -1 if ERROR
  */
int32_t DP83848_io_deinit (void)
{
    return ETH_PHY_STATUS_OK;
}

/**
  * @brief  Read a PHY register through the MDIO interface.
  * @param  DevAddr: PHY port address
  * @param  RegAddr: PHY register address
  * @param  pRegVal: pointer to hold the register value
  * @retval 0 if OK -1 if Error
  */
int32_t DP83848_io_read_reg(uint32_t DevAddr, uint32_t RegAddr, uint32_t *pRegVal)
{
  if(HAL_ETH_ReadPHYRegister(&eth_handle, DevAddr, RegAddr, pRegVal) != HAL_OK)
  {
    return ETH_PHY_STATUS_ERROR;
  }

  return ETH_PHY_STATUS_OK;
}

int32_t DP83848_io_write_reg(uint32_t DevAddr, uint32_t RegAddr, uint32_t RegVal)
{
  if(HAL_ETH_WritePHYRegister(&eth_handle, DevAddr, RegAddr, RegVal) != HAL_OK)
  {
    return ETH_PHY_STATUS_ERROR;
  }

  return ETH_PHY_STATUS_OK;
}

/**
  * @brief  Get the time in millisecons used for internal PHY driver process.
  * @retval Time value
  */
int32_t DP83848_io_get_tick(void)
{
  return HAL_GetTick();
}
