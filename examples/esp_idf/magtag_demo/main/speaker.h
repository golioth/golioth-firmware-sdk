#pragma once

#include <stdint.h>

void speaker_init(int dac_io, int enable_io);
void speaker_play_audio(const uint8_t* audio, uint32_t audio_length, uint32_t sample_rate);
