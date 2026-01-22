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


/**************************************************************************/
/**************************************************************************/
/**                                                                       */
/** GUIX Component                                                        */
/**                                                                       */
/**   STM32F7XG Screen Driver                                             */
/**                                                                       */
/**************************************************************************/

#ifndef GX_DISPLAY_DRIVER_STM32F779I_EVAL
#define GX_DISPLAY_DRIVER_STM32F779I_EVAL

#include "gx_api.h"
#include "tx_api.h"
#include "gx_api.h"
#include "gx_system.h"
#include "gx_display.h"
#include "gx_utility.h"
#include "stm32f769i_eval_lcd.h"

#define STM32F779I_EVAL_SCREEN_WIDTH          800
#define STM32F779I_EVAL_SCREEN_HEIGHT         480

#define STM32F779I_EVAL_SCREEN_HANDLE         0x12345679

UINT stm32f779_graphics_driver_setup_565rgb(GX_DISPLAY *display);
VOID touch_thread_entry(ULONG thread_input);

#endif

