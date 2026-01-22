
#ifndef __XT_AUDIO_H
#define __XT_AUDIO_H

#include "stm32f769i_eval_audio.h"

#define VOLUME_LEVEL        70

#define RECORD_MODE_WAKEWORD  1
#define RECORD_MODE_RECORDING 2

typedef struct AUDIO_DATA_STRUCT{
	uint16_t *data_ptr;
	uint32_t size;
}AUDIO_DATA;

uint8_t audio_record(uint8_t mode);
void audio_stop();
uint8_t audio_record_data_get(AUDIO_DATA *audio_data);
uint8_t audio_play(AUDIO_DATA *audio_data);

#endif
