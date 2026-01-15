#include "main.h"

#define PCM_BUFFER_SIZE     1024

//static uint16_t pcm_buffer[PCM_BUFFER_SIZE * 2]; // 双声道
//static uint16_t *play_data = NULL;
//static uint32_t play_data_size = 0; // 样本数
//static uint32_t play_size = 0;
//
//// 填充 DMA buffer 的辅助函数
//// Half Transfer 回调
//void BSP_AUDIO_OUT_HalfTransfer_CallBack(void)
//{
//    if(play_size + PCM_BUFFER_SIZE/2 > play_data_size)     {
//    	BSP_AUDIO_OUT_Stop(CODEC_PDWN_SW);
//    }
//    memcpy(pcm_buffer + PCM_BUFFER_SIZE, play_data + play_size, sizeof(uint16_t) * PCM_BUFFER_SIZE);
//    play_size += PCM_BUFFER_SIZE/2;
//}
//
//// Transfer Complete 回调
//void BSP_AUDIO_OUT_TransferComplete_CallBack(void)
//{
//    if(play_size + PCM_BUFFER_SIZE/2 > play_data_size)    {
//    	BSP_AUDIO_OUT_Stop(CODEC_PDWN_SW);
//    }
//    memcpy(pcm_buffer, play_data + play_size, sizeof(uint16_t) * PCM_BUFFER_SIZE);
//    play_size += PCM_BUFFER_SIZE/2;
//}
//
//uint8_t audio_play(AUDIO_DATA *audio_data)
//{
// uint8_t status;
//
//    play_data = audio_data->data_ptr;
//    play_data_size = audio_data->size;
//    play_size = 0;
//
//    status = BSP_AUDIO_OUT_Init(OUTPUT_DEVICE_AUTO, VOLUME_LEVEL , AUDIO_FREQUENCY_16K);
//
//    if(status != AUDIO_OK)
//    {
//        return STATUS_AUDIO_INIT_FAILURE;
//    }
//
//    // 初始化前半缓冲
//    memcpy(pcm_buffer, play_data, sizeof(uint16_t) * PCM_BUFFER_SIZE);
//    play_size += PCM_BUFFER_SIZE / 2;
//
//    // 配置两个slot
//    BSP_AUDIO_OUT_SetAudioFrameSlot(CODEC_AUDIOFRAME_SLOT_02);
//
//    // 开始播放
//    status = BSP_AUDIO_OUT_Play(pcm_buffer, PCM_BUFFER_SIZE * sizeof(uint16_t) * 2);
//
//    if(status != AUDIO_OK)
//    {
//    	return STATUS_AUDIO_PLAY_FAILURE;
//    }
//
//    return STATUS_SUCCESS;
//}

static uint16_t pcm_buffer[PCM_BUFFER_SIZE * 2]; // 双声道
static int16_t *play_data = NULL;
static uint32_t play_data_size = 0; // 样本数
static uint32_t play_size = 0;

// 填充 DMA buffer 的辅助函数
static void fill_pcm_buffer(uint32_t offset, uint32_t length)
{
    for(uint32_t i=0; i<length; i++)
    {
        pcm_buffer[2*i]   = play_data[offset+i]; // 左
        pcm_buffer[2*i+1] = play_data[offset+i]; // 右
    }
}

// Half Transfer 回调
void BSP_AUDIO_OUT_HalfTransfer_CallBack(void)
{
    if(play_size + PCM_BUFFER_SIZE/2 > play_data_size)     {
    	play_size = 0;
    	//BSP_AUDIO_OUT_Stop(CODEC_PDWN_SW);
    }
    fill_pcm_buffer(play_size, PCM_BUFFER_SIZE/2);
    play_size += PCM_BUFFER_SIZE/2;
}

// Transfer Complete 回调
void BSP_AUDIO_OUT_TransferComplete_CallBack(void)
{
    if(play_size + PCM_BUFFER_SIZE/2 > play_data_size)    {
    	play_size = 0;
    	//BSP_AUDIO_OUT_Stop(CODEC_PDWN_SW);
    }
    fill_pcm_buffer(play_size, PCM_BUFFER_SIZE/2);
    play_size += PCM_BUFFER_SIZE/2;
}

uint8_t audio_play(AUDIO_DATA *data)
{
    play_data = data->data_ptr;
    play_data_size = data->size;
    play_size = 0;

    if(BSP_AUDIO_OUT_Init(OUTPUT_DEVICE_AUTO, VOLUME_LEVEL , AUDIO_FREQUENCY_16K) != AUDIO_OK)
        return;

    // 初始化前半缓冲
    fill_pcm_buffer(0, PCM_BUFFER_SIZE/2);
    play_size += PCM_BUFFER_SIZE/2;

    // 配置两个slot
    BSP_AUDIO_OUT_SetAudioFrameSlot(CODEC_AUDIOFRAME_SLOT_02);

    // 开始播放
    BSP_AUDIO_OUT_Play((uint16_t*)pcm_buffer, PCM_BUFFER_SIZE * sizeof(uint16_t) * 2);

    return 0;
}
