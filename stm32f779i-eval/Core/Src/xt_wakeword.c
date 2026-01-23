/* This is a small demo of the high-performance GUIX graphics framework. */

#include "main.h"
#include "picovoice.h"
#include "pv_porcupine_mcu.h"
#include "pv_porcupine_params.h"

#define MEMORY_BUFFER_SIZE (50 * 1024)

static const char* ACCESS_KEY = "faD51PcWzhgb64ZjYg6bWvlUGxWk3xVshkFliz+TZNnnThNKOh2L+g==";
uint8_t memory_buffer[MEMORY_BUFFER_SIZE] __attribute__((aligned(16)));
const int32_t keyword_model_sizes = sizeof(HELLO_COMPUTER_KEYWORD_ARRAY);
const void *keyword_models = HELLO_COMPUTER_KEYWORD_ARRAY;
const float sensitivity = 0.5f;

/* Define prototypes.   */
uint8_t wakeword_detection(){
    uint8_t status = audio_record(RECORD_MODE_WAKEWORD);

    if(status != STATUS_SUCCESS)
    {
    	return status;
    }

    pv_porcupine_t *handle = NULL;

 	status = pv_porcupine_init(
	        ACCESS_KEY,
	        MEMORY_BUFFER_SIZE,
	        memory_buffer,
	        1,
	        &keyword_model_sizes,
	        &keyword_models,
	        &sensitivity,
	        &handle);

    if (status != PV_STATUS_SUCCESS)
    {
    	return STATUS_FAILURE;
    }

    ULONG actual_events;

    /* 通知GUI线程录音开始，更新界面显示 */
    tx_event_flags_set(&xt_event_group, XT_EVENT_GUI_RECORDING, TX_OR);

    while (1)
    {
    	/* 获取录音准备就绪标志 */
    	tx_event_flags_get(&xt_event_group, XT_EVENT_AUDIO_MASK, TX_OR_CLEAR , &actual_events, TX_WAIT_FOREVER);

    	if(actual_events & XT_EVENT_AUDIO_WAKEWORD_READY)
    	{
    		AUDIO_DATA audio_data;

    		/* 获取录音数据 */
			if (audio_record_data_get(&audio_data) == STATUS_SUCCESS) {
				int32_t keyword_index;

				/* 开始检测唤醒词 */
				const pv_status_t is_wakeword = pv_porcupine_process(handle, (int16_t *)audio_data.data_ptr, &keyword_index);
				if((is_wakeword == PV_STATUS_SUCCESS) && (keyword_index != -1))
				{
					/* 检测到唤醒词 */
					break;
				}
			}
    	}
    }

    /* 通知GUI线程检测到唤醒词，更新界面显示 */
    tx_event_flags_set(&xt_event_group, XT_EVENT_GUI_DETECTED, TX_OR);

    pv_porcupine_delete(handle);
    audio_stop();

    return STATUS_SUCCESS;
}
