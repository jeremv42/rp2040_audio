#pragma once

#include <cstdint>
#include <cstdio>
#include <pico/stdlib.h>

// ATTENTION: for better performance, the SPDIF input will change the system clock to match a good one with the sampling rate
// 113Mhz for 44100 (and multiples), 123Mhz for 48000 (and multiples)
// overclock needed for >100khz (226Mhz or 246Mhz)
namespace input
{
    union AudioSample
    {
        uint64_t both;
        struct
        {
            int32_t left;
            int32_t right;
        };
    };

    typedef uint32_t spdif_subframe;

    void spdif_init(uint spdif_pin);
    uint32_t spdif_get_sampling_rate();
    void spdif_set_callback_sampling_change(void (*callback)(uint32_t sampling_rate_hz));

    extern AudioSample (*spdif_get_sample)();
}
