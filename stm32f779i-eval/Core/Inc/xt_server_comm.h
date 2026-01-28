/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    xt_server_comm.h
  * @brief   服务器通信接口头文件
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef __XT_SERVER_COMM_H
#define __XT_SERVER_COMM_H

#include "main.h"

/* 服务器通信接口函数 */
uint8_t server_send_audio_data(uint16_t *audio_data, uint32_t size);
uint8_t server_comm_init(void);
void server_comm_deinit(void);

#endif /* __XT_SERVER_COMM_H */
