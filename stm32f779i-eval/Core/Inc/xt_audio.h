
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

/* 静音检测相关函数 */
uint8_t audio_is_silence(uint16_t *audio_data, uint32_t size);
uint32_t audio_record_get_size(void);
void audio_record_reset_silence_counter(void);
void audio_record_increment_silence_counter(void);
uint8_t audio_record_should_stop(void);

#endif
