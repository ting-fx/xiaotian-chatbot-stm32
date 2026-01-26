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

/* 录音模式的帧大小和缓冲区数量 */
#define RECORDING_FRAME_SIZE    512
#define RECORDING_FRAME_COUNT   600

/* 静音检测阈值（能量值，可根据实际情况调整） */
#define SILENCE_THRESHOLD       6000000
/* 连续静音帧数，超过此数量则判定为静音 */
#define SILENCE_FRAME_COUNT     60

struct record_info_struct{
	/* 当前已录入的样本数（单位：uint16_t） */
	uint32_t buffer_size;

	union{
		uint16_t *wakeword_frame[WAKEWORD_FRAME_COUNT];
		uint16_t *recording_frame[RECORDING_FRAME_COUNT];
	};

	uint32_t buffer_index;
	int8_t write_index;
	int8_t read_index;

	uint8_t mode;
	
	/* 静音检测相关 */
	uint32_t silence_frame_count;  /* 连续静音帧计数 */
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
	else if(record_info.mode == RECORD_MODE_RECORDING)
	{
		/* 将音频数据拷贝到录音缓冲区 */
		for(int i = 0; i < PCM_BUFFER_SIZE; i+=2)
		{
			if(record_info.buffer_index < RECORDING_FRAME_SIZE)
			{
				record_info.recording_frame[record_info.write_index][record_info.buffer_index++] = pcm_buffer[i];
			}
		}

		/* 当一帧数据填满时，通知主线程处理 */
		if(record_info.buffer_index >= RECORDING_FRAME_SIZE)
		{
			if(record_info.write_index < RECORDING_FRAME_COUNT - 1)
			{
				record_info.write_index++;
			}
			else
			{
				record_info.write_index = 0;
			}

			record_info.buffer_index = 0;
			record_info.buffer_size += RECORDING_FRAME_SIZE;

			/* 通知主线程有新的录音数据就绪 */
			tx_event_flags_set(&xt_event_group, XT_EVENT_AUDIO_VOICE_READY, TX_OR);
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
	else if(record_info.mode == RECORD_MODE_RECORDING)
	{
		/* 将音频数据拷贝到录音缓冲区 */
		for(int i = 0; i < PCM_BUFFER_SIZE; i+=2)
		{
			if(record_info.buffer_index < RECORDING_FRAME_SIZE)
			{
				record_info.recording_frame[record_info.write_index][record_info.buffer_index++] = pcm_buffer[PCM_BUFFER_SIZE + i];
			}
		}

		/* 当一帧数据填满时，通知主线程处理 */
		if(record_info.buffer_index >= RECORDING_FRAME_SIZE)
		{
			if(record_info.write_index < RECORDING_FRAME_COUNT - 1)
			{
				record_info.write_index++;
			}
			else
			{
				record_info.write_index = 0;
			}

			record_info.buffer_index = 0;
			record_info.buffer_size += RECORDING_FRAME_SIZE;

			/* 通知主线程有新的录音数据就绪 */
			tx_event_flags_set(&xt_event_group, XT_EVENT_AUDIO_VOICE_READY, TX_OR);
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
    else if(mode == RECORD_MODE_RECORDING)
    {
		/* 为录音模式分配缓冲区 */
		record_info.recording_frame[0] = record_buffer;
		record_info.recording_frame[1] = record_buffer + RECORDING_FRAME_SIZE;
		record_info.recording_frame[2] = record_buffer + RECORDING_FRAME_SIZE * 2;
		record_info.recording_frame[3] = record_buffer + RECORDING_FRAME_SIZE * 3;
		record_info.silence_frame_count = 0;
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
    else if(record_info.mode == RECORD_MODE_RECORDING)
    {
    	audio_data->data_ptr = record_info.recording_frame[record_info.read_index];
    	audio_data->size = RECORDING_FRAME_SIZE;

    	if(record_info.read_index < RECORDING_FRAME_COUNT - 1)
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

/* ========================= 静音检测接口 ========================= */

/**
 * @brief 检测音频帧是否为静音
 * @param audio_data 音频数据指针
 * @param size 音频数据大小（样本数）
 * @return 1表示静音，0表示非静音
 * 
 * 通过计算音频帧的能量来判断是否为静音
 */
uint8_t audio_is_silence(uint16_t *audio_data, uint32_t size)
{
	uint32_t energy = 0;
	uint32_t i;
	int32_t sample;

	if(!audio_data || size == 0)
	{
		return 1;  /* 空数据视为静音 */
	}

	/* 计算音频帧的能量（RMS值） */
	for(i = 0; i < size; i++)
	{
		sample = (int16_t)audio_data[i];
		energy += (uint32_t)(sample * sample);
	}

	/* 计算平均能量 */
	energy = energy / size;

	/* 如果能量低于阈值，判定为静音 */
	return (energy < SILENCE_THRESHOLD) ? 1 : 0;
}

/**
 * @brief 获取当前录音的总大小
 * @return 已录音的样本数
 */
uint32_t audio_record_get_size(void)
{
	return record_info.buffer_size;
}

/**
 * @brief 重置静音检测计数器
 */
void audio_record_reset_silence_counter(void)
{
	record_info.silence_frame_count = 0;
}

/**
 * @brief 增加静音帧计数
 */
void audio_record_increment_silence_counter(void)
{
	record_info.silence_frame_count++;
}

/**
 * @brief 检查是否达到静音停止条件
 * @return 1表示应该停止录音（连续静音帧数超过阈值），0表示继续录音
 */
uint8_t audio_record_should_stop(void)
{
	return (record_info.silence_frame_count >= SILENCE_FRAME_COUNT) ? 1 : 0;
}
