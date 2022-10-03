#include "speaker.h"
#include "esp_timer.h"
#include <driver/dac.h>

#define DAC_CHANNEL DAC_CHANNEL_1

static int _dac_io;
static int _enable_io;

static uint64_t micros(void) {
    int64_t time_us = esp_timer_get_time();
    if (time_us < 0) {
        time_us = 0;
    }
    return time_us;
}

void speaker_init(int dac_io, int enable_io) {
    _dac_io = dac_io;
    _enable_io = enable_io;

    gpio_config_t enable_config = {
            .pin_bit_mask = (1ULL << _enable_io),
            .mode = GPIO_MODE_OUTPUT,
    };
    ESP_ERROR_CHECK(gpio_config(&enable_config));

    gpio_config_t dac_config = {
            .pin_bit_mask = (1ULL << _dac_io),
            .mode = GPIO_MODE_OUTPUT,
    };
    ESP_ERROR_CHECK(gpio_config(&dac_config));

    gpio_set_level(_dac_io, 0);
}

void speaker_play_audio(const uint8_t* audio, uint32_t audio_length, uint32_t sample_rate) {
    uint32_t t = 0;
    uint64_t usec = 1000000L / sample_rate;

    gpio_set_level(_enable_io, 1);
    dac_output_enable(DAC_CHANNEL);

    uint64_t prior = micros();
    for (uint32_t i = 0; i < audio_length; i++) {
        while ((t = micros()) - prior < usec)
            ;
        dac_output_voltage(DAC_CHANNEL, audio[i]);
        prior = t;
    }

    gpio_set_level(_enable_io, 0);
}
