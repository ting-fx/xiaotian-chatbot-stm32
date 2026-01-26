#include "main.h"

/* ========================= 缓冲区与状态定义 ========================= */

/* DMA使用的音频数据缓冲区大小 */
#define PCM_BUFFER_SIZE         1024

/* 最终录音数据缓存大小（单位：uint16_t 样本数）
 * 这里约等于 400k * 2 bytes ≈ 800 KB，需要放在 SDRAM
 */
#define RECORD_BUFFER_SIZE      409600

/* DFSDM 使用的 Scratch buffer，用于内部滤波或中间运算 */
#define SCRATCH_BUFF_SIZE       512

#define WAKEWORD_FRAME_SIZE     512
#define WAKEWORD_FRAME_COUNT    2

struct record_info_struct{
	/* 当前已录入的样本数（单位：uint16_t） */
	uint32_t buffer_size;

	union{
		uint16_t *wakeword_frame[WAKEWORD_FRAME_COUNT];
	};

	uint32_t buffer_index;
	int8_t write_index;
	int8_t read_index;

	uint8_t mode;
}record_info;

/* ========================= 全局静态变量 ========================= */

/* DMA 使用的音频数据缓冲区 */
static uint16_t pcm_buffer[PCM_BUFFER_SIZE * 2];

/* 最终录音数据缓存，放在 SDRAM 中
 * 用于保存完整录音结果
 */
static __attribute__((section(".SD_RAM")))
uint16_t record_buffer[RECORD_BUFFER_SIZE];

/* DFSDM 使用的 Scratch buffer */
static int32_t Scratch[SCRATCH_BUFF_SIZE];

/* ========================= DMA 回调函数 ========================= */

/**
 * @brief DMA 半传输完成回调
 *
 * 当 DMA 将 pcm_buffer 的前一半填满时触发：
 *   pcm_buffer[0 ... PCM_BUFFER_SIZE-1]
 *
 * 在此回调中，将前半 buffer 拷贝到最终录音缓存 record_buffer
 */
void BSP_AUDIO_IN_HalfTransfer_CallBack(void)
{
	if(record_info.mode == RECORD_MODE_WAKEWORD)
	{
		for(int i = 0; i < PCM_BUFFER_SIZE; i+=2)
		{
			record_info.wakeword_frame[record_info.write_index][record_info.buffer_index++] = pcm_buffer[i];
		}

		if(record_info.buffer_index >= WAKEWORD_FRAME_SIZE)
		{
			if(record_info.write_index < WAKEWORD_FRAME_COUNT - 1)
			{
				record_info.write_index++;
			}
			else
			{
				record_info.write_index = 0;
			}

			record_info.buffer_index = 0;

			tx_event_flags_set(&xt_event_group, XT_EVENT_AUDIO_WAKEWORD_READY, 0);
		}
	}
}

/**
 * @brief DMA 全传输完成回调
 *
 * 当 DMA 将 pcm_buffer 的后一半填满时触发：
 *   pcm_buffer[PCM_BUFFER_SIZE ... 2*PCM_BUFFER_SIZE-1]
 *
 * 在此回调中，将后半 buffer 拷贝到最终录音缓存 record_buffer
 */
void BSP_AUDIO_IN_TransferComplete_CallBack(void)
{
	if(record_info.mode == RECORD_MODE_WAKEWORD)
	{
		for(int i = 0; i < PCM_BUFFER_SIZE; i+=2)
		{
			record_info.wakeword_frame[record_info.write_index][record_info.buffer_index++] = pcm_buffer[PCM_BUFFER_SIZE + i];
		}

		if(record_info.buffer_index >= WAKEWORD_FRAME_SIZE)
		{
			if(record_info.write_index < WAKEWORD_FRAME_COUNT - 1)
			{
				record_info.write_index++;
			}
			else
			{
				record_info.write_index = 0;
			}

			record_info.buffer_index = 0;

			tx_event_flags_set(&xt_event_group, XT_EVENT_AUDIO_WAKEWORD_READY, 0);
		}
	}
}

/* ========================= 录音控制接口 ========================= */

/**
 * @brief 启动录音
 *
 * 初始化音频输入并启动录音到 pcm_buffer
 */
uint8_t audio_record(uint8_t mode)
{
    uint8_t status;

    record_info.buffer_size = 0;
    record_info.mode = mode;

    if(mode == RECORD_MODE_WAKEWORD)
    {
		record_info.wakeword_frame[0] = record_buffer;
		record_info.wakeword_frame[1] = record_buffer + WAKEWORD_FRAME_SIZE;
    }

    record_info.buffer_index = 0;
    record_info.write_index = 0;
    record_info.read_index = 0;

    /* 初始化音频输入参数：采样率、位宽、通道数 */
    status = BSP_AUDIO_IN_Init(DEFAULT_AUDIO_IN_FREQ,
                               DEFAULT_AUDIO_IN_BIT_RESOLUTION,
                               DEFAULT_AUDIO_IN_CHANNEL_NBR);
    if(status != AUDIO_OK)
    {
        return STATUS_AUDIO_INIT_FAILURE;
    }

    /* 分配 DFSDM 使用的 Scratch buffer */
    status = BSP_AUDIO_IN_AllocScratch(Scratch, SCRATCH_BUFF_SIZE);
    if(status != AUDIO_OK)
    {
        return STATUS_FAILURE;
    }

    /* 启动 DMA 录音，DMA 会循环写入 pcm_buffer */
    status = BSP_AUDIO_IN_Record((uint16_t *)pcm_buffer,
                                 PCM_BUFFER_SIZE * 2);
    if(status != AUDIO_OK)
    {
        return STATUS_AUDIO_RECORD_FAILURE;
    }

    return STATUS_SUCCESS;
}

/**
 * @brief 获取录音结果
 *
 * 仅当录音完成后，才返回数据指针和大小
 */
uint8_t audio_record_data_get(AUDIO_DATA *audio_data)
{
    if(!audio_data)
    {
        return STATUS_AUDIO_NULL_PTR;
    }

    /* 只有在录音完成状态下才允许读取数据 */
    if(record_info.mode == RECORD_MODE_WAKEWORD)
    {
    	audio_data->data_ptr = record_info.wakeword_frame[record_info.read_index];
    	audio_data->size = WAKEWORD_FRAME_SIZE;

    	if(record_info.read_index < WAKEWORD_FRAME_COUNT - 1)
    	{
    		record_info.read_index ++;
    	}
    	else
    	{
    		record_info.read_index = 0;
    	}

    	return STATUS_SUCCESS;
    }

    return STATUS_FAILURE;
}

void audio_stop()
{
	BSP_AUDIO_IN_Stop();
}
