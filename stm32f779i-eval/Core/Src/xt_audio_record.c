#include "main.h"

#define PCM_BUFFER_SIZE       1024
#define RECORD_BUFFER_SIZE    409600

#define RECORD_STATE_IDLE       0
#define RECORD_STATE_RECORDING  1
#define RECORD_STATE_COMPLETED  2

static uint16_t pcm_buffer[PCM_BUFFER_SIZE * 2]; // 双声道
static __attribute__((section(".SD_RAM"))) uint16_t record_buffer[RECORD_BUFFER_SIZE];
static uint32_t record_buffer_size = 0;
static uint8_t record_state = RECORD_STATE_IDLE;

void BSP_AUDIO_IN_TransferComplete_CallBack(void) {
	memcpy(record_buffer + record_buffer_size, pcm_buffer + PCM_BUFFER_SIZE / 2, PCM_BUFFER_SIZE * sizeof(uint16_t));

	record_buffer_size += PCM_BUFFER_SIZE / 2;

	if(record_buffer_size >= RECORD_BUFFER_SIZE)
	{
		BSP_AUDIO_IN_Stop();
		record_state = RECORD_STATE_COMPLETED;
	}
}

void BSP_AUDIO_IN_HalfTransfer_CallBack(void) {
	memcpy(record_buffer + record_buffer_size, pcm_buffer, PCM_BUFFER_SIZE * sizeof(uint16_t));

	record_buffer_size += PCM_BUFFER_SIZE / 2;

	if(record_buffer_size >= RECORD_BUFFER_SIZE)
	{
		BSP_AUDIO_IN_Stop();
		record_state = RECORD_STATE_COMPLETED;
	}
}

uint8_t audio_record()
{
uint8_t status;

	record_buffer_size = 0;

	record_state = RECORD_STATE_RECORDING;

	/* Initialize audio input and start recording into pcm_buffer. */
	status = BSP_AUDIO_IN_Init(DEFAULT_AUDIO_IN_FREQ, DEFAULT_AUDIO_IN_BIT_RESOLUTION, DEFAULT_AUDIO_IN_CHANNEL_NBR);

	if(status != AUDIO_OK)
	{
		return STATUS_AUDIO_INIT_FAILURE;
	}

	status = BSP_AUDIO_IN_Record((uint16_t *)pcm_buffer, PCM_BUFFER_SIZE * sizeof(uint16_t) * 2);

	if(status != AUDIO_OK)
	{
		return STATUS_AUDIO_RECORD_FAILURE;
	}

	return STATUS_SUCCESS;
}

uint8_t audio_record_buffer_get(AUDIO_DATA *audio_data)
{

	if(!audio_data)
	{
		return STATUS_AUDIO_NULL_PTR;
	}

	if(record_state == RECORD_STATE_COMPLETED)
	{
		audio_data->data_ptr = record_buffer;
		audio_data->size = record_buffer_size;

		return STATUS_SUCCESS;
	}

	return STATUS_FAILURE;
}
