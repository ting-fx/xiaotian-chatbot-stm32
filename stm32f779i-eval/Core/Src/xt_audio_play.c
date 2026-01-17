#include "main.h"

/* ========================= 播放缓冲区配置 ========================= */

/* DMA使用的音频数据缓冲区大小 */
#define PCM_BUFFER_SIZE     1024

/* DMA 使用的音频数据缓冲区 */
static uint16_t pcm_buffer[PCM_BUFFER_SIZE * 2];

/* 指向待播放音频数据的指针 */
static uint16_t *play_data = NULL;

/* 待播放音频数据的总大小（单位：uint16_t 样本数） */
static uint32_t play_data_size = 0;

/* 当前已经播放到的样本偏移量（单位：uint16_t） */
static uint32_t play_offset = 0;

/* ========================= DMA 回调函数 ========================= */

/**
 * @brief DMA 半传输完成回调
 *
 * 当播放完 pcm_buffer 的前半部分时触发，
 * 此时前半部分已经播放完成，可以安全地重新填充：
 *   pcm_buffer[0 ... PCM_BUFFER_SIZE-1]
 */
void BSP_AUDIO_OUT_HalfTransfer_CallBack(void)
{
    /* 将下一段音频数据拷贝到 pcm_buffer 前半部分 */
    memcpy(pcm_buffer,
           play_data + play_offset,
           sizeof(uint16_t) * PCM_BUFFER_SIZE);

    /* 更新播放位置 */
    play_offset += PCM_BUFFER_SIZE;

    if(play_offset > play_data_size)
    {
        BSP_AUDIO_OUT_Stop(CODEC_PDWN_SW); // 停止播放
    }
}

/**
 * @brief DMA 全传输完成回调
 *
 * 当播放完 pcm_buffer 的后半部分时触发，
 * 此时后半部分已经播放完成，可以安全地重新填充：
 *   pcm_buffer[PCM_BUFFER_SIZE ... 2*PCM_BUFFER_SIZE-1]
 */
void BSP_AUDIO_OUT_TransferComplete_CallBack(void)
{
    /* 将下一段音频数据拷贝到 pcm_buffer 后半部分 */
    memcpy(pcm_buffer + PCM_BUFFER_SIZE,
           play_data + play_offset,
           sizeof(uint16_t) * PCM_BUFFER_SIZE);

    /* 更新播放位置 */
    play_offset += PCM_BUFFER_SIZE;

    if(play_offset > play_data_size)
    {
        BSP_AUDIO_OUT_Stop(CODEC_PDWN_SW); // 停止播放
    }
}

/* ========================= 播放控制接口 ========================= */

/**
 * @brief 启动音频播放
 *
 * @param audio_data  指向待播放音频数据的结构体
 *                    - data_ptr: PCM 数据指针
 *                    - size:     样本数（uint16_t）
 */
uint8_t audio_play(AUDIO_DATA *audio_data)
{
    uint8_t status;

    /* 保存播放数据指针和长度 */
    play_data      = audio_data->data_ptr;
    play_data_size = audio_data->size;
    play_offset      = 0;

    /* 初始化音频输出设备（SAI + Codec） */
    status = BSP_AUDIO_OUT_Init(OUTPUT_DEVICE_AUTO,
                                VOLUME_LEVEL,
                                AUDIO_FREQUENCY_16K);
    if(status != AUDIO_OK)
    {
        return STATUS_AUDIO_INIT_FAILURE;
    }

    /* 初始化 DMA 双缓冲区
     * 预先填满前后两段，避免播放刚开始时出现空数据
     */
    memcpy(pcm_buffer,
           play_data,
           sizeof(uint16_t) * PCM_BUFFER_SIZE * 2);

    play_offset += PCM_BUFFER_SIZE * 2;

    /* 配置 Codec 使用的音频帧 Slot（左右声道） */
    BSP_AUDIO_OUT_SetAudioFrameSlot(CODEC_AUDIOFRAME_SLOT_02);

    /* 启动 DMA 播放
     * 长度单位为字节，因此需要乘 sizeof(uint16_t)
     */
    status = BSP_AUDIO_OUT_Play(pcm_buffer,
                                PCM_BUFFER_SIZE * sizeof(uint16_t) * 2);
    if(status != AUDIO_OK)
    {
        return STATUS_AUDIO_PLAY_FAILURE;
    }

    return STATUS_SUCCESS;
}
