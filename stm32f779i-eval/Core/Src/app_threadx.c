/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    app_threadx.c
  * @author  MCD Application Team
  * @brief   ThreadX applicative file
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
#include "app_threadx.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "main.h"
#include "xt_server_comm.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */
TX_EVENT_FLAGS_GROUP xt_event_group;


TX_THREAD             chat_thread;
UCHAR                 chat_thread_stack[4096];

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */
void chat_thread_setup();
VOID chat_thread_entry(ULONG thread_input);
void voice_recording_after_wakeword(void);
/* USER CODE END PFP */

/**
  * @brief  Application ThreadX Initialization.
  * @param memory_ptr: memory pointer
  * @retval int
  */
UINT App_ThreadX_Init(VOID *memory_ptr)
{
  UINT ret = TX_SUCCESS;
  TX_BYTE_POOL *byte_pool = (TX_BYTE_POOL*)memory_ptr;

  /* USER CODE BEGIN App_ThreadX_MEM_POOL */
  (void)byte_pool;
  /* USER CODE END App_ThreadX_MEM_POOL */

  /* USER CODE BEGIN App_ThreadX_Init */

  tx_event_flags_create(&xt_event_group, "xt_event_group");

  gui_setup();
  chat_thread_setup();
  /* USER CODE END App_ThreadX_Init */

  return ret;
}

/**
  * @brief  MX_ThreadX_Init
  * @param  None
  * @retval None
  */
void MX_ThreadX_Init(void)
{
  /* USER CODE BEGIN  Before_Kernel_Start */

  /* USER CODE END  Before_Kernel_Start */

  tx_kernel_enter();

  /* USER CODE BEGIN  Kernel_Start_Error */

  /* USER CODE END  Kernel_Start_Error */
}

/* USER CODE BEGIN 1 */
void chat_thread_setup()
{
    /* 创建Touch线程  */
    tx_thread_create(&chat_thread, "Chat Thread", chat_thread_entry, 0,
                     chat_thread_stack, sizeof(chat_thread_stack),
					 XT_CHAT_THREAD_PRIORITY,
					 XT_CHAT_THREAD_PRIORITY, TX_NO_TIME_SLICE, TX_AUTO_START);
}

VOID chat_thread_entry(ULONG thread_input)
{
	tx_thread_sleep(100);

	/* 初始化服务器通信 */
	server_comm_init();

	while(1)
	{
		/* 检测唤醒词 */
		if(wakeword_detection() == STATUS_SUCCESS)
		{
			/* 检测到唤醒词后，开始录音并检测静音 */
			voice_recording_after_wakeword();
		}
		
		tx_thread_sleep(100);
	}
}

/**
 * @brief 唤醒词检测后的录音处理
 * 
 * 检测到唤醒词后：
 * 1. 切换到录音模式开始录音
 * 2. 每录制一段检测是否为静音
 * 3. 如果不是静音，调用接口函数发送到服务器
 * 4. 如果是静音（连续静音帧超过阈值），停止录音
 */
void voice_recording_after_wakeword(void)
{
	uint8_t status;
	ULONG actual_events;
	AUDIO_DATA audio_data;

	/* 停止唤醒词检测模式的录音 */
	audio_stop();
	tx_thread_sleep(50);

	/* 切换到录音模式 */
	status = audio_record(RECORD_MODE_RECORDING);
	if(status != STATUS_SUCCESS)
	{
		return;
	}

	/* 重置静音检测计数器 */
	audio_record_reset_silence_counter();

	/* 通知GUI线程开始录音 */
	tx_event_flags_set(&xt_event_group, XT_EVENT_GUI_RECORDING, TX_OR);

	/* 开始录音循环 */
	while(1)
	{
		/* 等待录音数据就绪 */
		tx_event_flags_get(&xt_event_group, XT_EVENT_AUDIO_VOICE_READY, TX_OR_CLEAR, 
		                   &actual_events, TX_WAIT_FOREVER);

		if(actual_events & XT_EVENT_AUDIO_VOICE_READY)
		{
			/* 获取录音数据 */
			if(audio_record_data_get(&audio_data) == STATUS_SUCCESS)
			{
				/* 检测是否为静音 */
				if(audio_is_silence(audio_data.data_ptr, audio_data.size))
				{
					/* 是静音，增加静音帧计数 */
					audio_record_increment_silence_counter();

					/* 检查是否应该停止录音（连续静音帧超过阈值） */
					if(audio_record_should_stop())
					{
						/* 停止录音 */
						break;
					}
				}
				else
				{
					/* 不是静音，重置静音计数器 */
					audio_record_reset_silence_counter();

					/* 调用接口函数发送音频数据到服务器 */
					server_send_audio_data(audio_data.data_ptr, audio_data.size);
				}
			}
		}
	}

	/* 停止录音 */
	audio_stop();

	/* 通知GUI线程录音结束 */
	tx_event_flags_set(&xt_event_group, XT_EVENT_GUI_DETECTED, TX_OR);
}
/* USER CODE END 1 */
