#include "main.h"

#define PCM_BUFFER_SIZE     1024

static uint16_t pcm_buffer[PCM_BUFFER_SIZE * 2]; // 双声道
static uint16_t *play_data = NULL;
static uint32_t play_data_size = 0; // 样本数
static uint32_t play_size = 0;

// 填充 DMA buffer 的辅助函数
// Half Transfer 回调
void BSP_AUDIO_OUT_HalfTransfer_CallBack(void)
{
    memcpy(pcm_buffer, play_data + play_size, sizeof(uint16_t) * PCM_BUFFER_SIZE);

    play_size += PCM_BUFFER_SIZE;

    if(play_size + PCM_BUFFER_SIZE > play_data_size)     {
    	play_size = 0;
    	//BSP_AUDIO_OUT_Stop(CODEC_PDWN_SW);
    }
}

// Transfer Complete 回调
void BSP_AUDIO_OUT_TransferComplete_CallBack(void)
{
    memcpy(pcm_buffer + PCM_BUFFER_SIZE, play_data + play_size, sizeof(uint16_t) * PCM_BUFFER_SIZE);

    play_size += PCM_BUFFER_SIZE;

    if(play_size + PCM_BUFFER_SIZE > play_data_size)    {
    	//BSP_AUDIO_OUT_Stop(CODEC_PDWN_SW);
    	play_size = 0;
    }
}

uint8_t audio_play(AUDIO_DATA *audio_data)
{
 uint8_t status;

    play_data = audio_data->data_ptr;
    play_data_size = audio_data->size;
    play_size = 0;

    status = BSP_AUDIO_OUT_Init(OUTPUT_DEVICE_AUTO, VOLUME_LEVEL , AUDIO_FREQUENCY_16K);

    if(status != AUDIO_OK)
    {
        return STATUS_AUDIO_INIT_FAILURE;
    }

    // 初始化前半缓冲
    memcpy(pcm_buffer, play_data, sizeof(uint16_t) * PCM_BUFFER_SIZE);
    play_size += PCM_BUFFER_SIZE;

    // 配置两个slot
    BSP_AUDIO_OUT_SetAudioFrameSlot(CODEC_AUDIOFRAME_SLOT_02);

    // 开始播放
    status = BSP_AUDIO_OUT_Play(pcm_buffer, PCM_BUFFER_SIZE * sizeof(uint16_t) * 2);

    if(status != AUDIO_OK)
    {
    	return STATUS_AUDIO_PLAY_FAILURE;
    }

    return STATUS_SUCCESS;
}
