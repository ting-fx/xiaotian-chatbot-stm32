
#ifndef __AUDIO_H

#define VOLUME_LEVEL        70

typedef struct AUDIO_DATA_STRUCT{
	uint16_t *data_ptr;
	uint16_t *size;
}AUDIO_DATA;

void audio_record();
uint8_t audio_record_buffer_get(AUDIO_DATA *audio_data);
uint8_t audio_play(AUDIO_DATA *audio_data);

#endif
