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

/* 录音状态机定义 */
#define RECORD_STATE_IDLE       0   // 空闲
#define RECORD_STATE_RECORDING  1   // 正在录音
#define RECORD_STATE_COMPLETED  2   // 录音完成

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

/* 当前已录入的样本数（单位：uint16_t） */
static uint32_t record_buffer_size = 0;

/* 当前录音状态 */
static uint8_t record_state = RECORD_STATE_IDLE;

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
    /* 将 DMA buffer 前半部分拷贝到录音 buffer */
    memcpy(record_buffer + record_buffer_size,
           pcm_buffer,
           PCM_BUFFER_SIZE * sizeof(uint16_t));

    /* 更新已录入样本数 */
    record_buffer_size += PCM_BUFFER_SIZE;

    /* 如果录音 buffer 已满，停止录音 */
    if(record_buffer_size >= RECORD_BUFFER_SIZE)
    {
        BSP_AUDIO_IN_Stop();
        record_state = RECORD_STATE_COMPLETED;
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
    /* 将 DMA buffer 后半部分拷贝到录音 buffer */
    memcpy(record_buffer + record_buffer_size,
           pcm_buffer + PCM_BUFFER_SIZE,
           PCM_BUFFER_SIZE * sizeof(uint16_t));

    /* 更新已录入样本数 */
    record_buffer_size += PCM_BUFFER_SIZE;

    /* 如果录音 buffer 已满，停止录音 */
    if(record_buffer_size >= RECORD_BUFFER_SIZE)
    {
        BSP_AUDIO_IN_Stop();
        record_state = RECORD_STATE_COMPLETED;
    }
}

/* ========================= 录音控制接口 ========================= */

/**
 * @brief 启动录音
 *
 * 初始化音频输入并启动录音到 pcm_buffer
 */
uint8_t audio_record(void)
{
    uint8_t status;

    /* 清空录音计数 */
    record_buffer_size = 0;

    /* 设置状态为录音中 */
    record_state = RECORD_STATE_RECORDING;

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
uint8_t audio_record_buffer_get(AUDIO_DATA *audio_data)
{
    if(!audio_data)
    {
        return STATUS_AUDIO_NULL_PTR;
    }

    /* 只有在录音完成状态下才允许读取数据 */
    if(record_state == RECORD_STATE_COMPLETED)
    {
        audio_data->data_ptr = record_buffer;
        audio_data->size     = record_buffer_size;
        return STATUS_SUCCESS;
    }

    return STATUS_FAILURE;
}
