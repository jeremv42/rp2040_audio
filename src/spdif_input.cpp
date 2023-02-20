#include "spdif_input.hpp"
#include "spdif_decode.pio.h"
#include <cstdio>
#include <cstdlib>
#include <hardware/clocks.h>
#include <hardware/pio.h>

namespace input
{

    static pio_hw_t *spdif_pio = pio0;
    static uint spdif_sm = 0;

    static AudioSample spdif_get_sample_init();
    static uint32_t spdif_sampling_rate_hz = 0;

    AudioSample (*spdif_get_sample)() = spdif_get_sample_init;
    static void (*spdif_callback_sampling_change)(uint32_t sampling_rate_hz) = nullptr;

    static char spdif_get_preamble(spdif_subframe subframe)
    {
        switch (subframe & 0xF)
        {
        case 0x1:
        case 0xE:
            return 'B';
        case 0x4:
        case 0xB:
            return 'M';
        case 0x2:
        case 0xD:
            return 'W';
        }

        return '?';
    }

    static inline bool spdif_check_parity(spdif_subframe subframe)
    {
        return !!(subframe & 0x80000000) == __builtin_parity(subframe & 0x7FFFFFF0);
    }

    static inline bool spdif_check_subframe(spdif_subframe subframe)
    {
        auto preamble = spdif_get_preamble(subframe);
        auto parity = spdif_check_parity(subframe);
        return preamble != '?' && parity;
    }

    void spdif_init(uint spdif_pin)
    {
        // configure SPDIF decoder
        gpio_init(spdif_pin);
        gpio_set_dir(spdif_pin, GPIO_IN);

        printf("\n\nSTART clock=%ld\n", clock_get_hz(clk_sys));
        uint offset = 0;
        pio_add_program_at_offset(spdif_pio, &spdif_subframe_decode_program, offset);
        printf("spdif_subframe_decode loaded at %x\n", offset);
        spdif_subframe_decode_program_init(spdif_pio, spdif_sm, offset, spdif_pin, 2); // divide by 1 for 96khz or 88.2khz
    }

    uint32_t spdif_get_sampling_rate()
    {
        return spdif_sampling_rate_hz;
    }

    void spdif_set_callback_sampling_change(void (*callback)(uint32_t sampling_rate_hz))
    {
        spdif_callback_sampling_change = callback;
    }

    static int spdif_error_counter = 0;
    static spdif_subframe spdif_get_subframe()
    {
        auto timeout = 200;
        while (timeout-- && pio_sm_is_rx_fifo_empty(spdif_pio, spdif_sm))
            tight_loop_contents();

        spdif_subframe subframe = pio_sm_get(spdif_pio, spdif_sm);
        if (timeout && input::spdif_check_subframe(subframe))
        {
            if (spdif_error_counter && input::spdif_get_preamble(subframe) == 'B')
                spdif_error_counter--;
            return subframe;
        }

        if (spdif_error_counter++ > 10)
        {
            spdif_get_sample = spdif_get_sample_init;
            spdif_error_counter = 0;
        }
        return 0;
    }

    static uint32_t spdif_last_subframe_time = 0;
    static AudioSample spdif_get_sample_16bits()
    {
        AudioSample sample = { .both = 0 };

        auto frame1 = spdif_get_subframe();
        if (input::spdif_get_preamble(frame1) == 'W')
            sample.right = ((frame1 >> 12) & 0xFFFF) << 16;
        else
            sample.left = ((frame1 >> 12) & 0xFFFF) << 16;

        auto frame2 = spdif_get_subframe();
        if (input::spdif_get_preamble(frame2) == 'W')
            sample.right = ((frame2 >> 12) & 0xFFFF) << 16;
        else
            sample.left = ((frame2 >> 12) & 0xFFFF) << 16;

        auto time = time_us_32() - spdif_last_subframe_time;
        auto target = 1000000 / spdif_sampling_rate_hz;
        if (((time - 1) > target || (time + 1) < target) && spdif_error_counter++ > 10)
        {
            spdif_get_sample = spdif_get_sample_init;
            spdif_error_counter = 0;
        }
        spdif_last_subframe_time = time_us_32();
        return sample;
    }

    static AudioSample spdif_get_sample_24bits()
    {
        AudioSample sample = { .both = 0 };

        auto frame1 = spdif_get_subframe();
        if (input::spdif_get_preamble(frame1) == 'W')
            sample.right = ((frame1 >> 4) & 0xFFFFFF) << 8;
        else
            sample.left = ((frame1 >> 4) & 0xFFFFFF) << 8;

        auto frame2 = spdif_get_subframe();
        if (input::spdif_get_preamble(frame2) == 'W')
            sample.right = ((frame2 >> 4) & 0xFFFFFF) << 8;
        else
            sample.left = ((frame2 >> 4) & 0xFFFFFF) << 8;

        auto time = time_us_32() - spdif_last_subframe_time;
        auto target = 1000000 / spdif_sampling_rate_hz;
        if (((time - 1) > target || (time + 1) < target) && spdif_error_counter++ > 10)
        {
            spdif_get_sample = spdif_get_sample_init;
            spdif_error_counter = 0;
        }
        spdif_last_subframe_time = time_us_32();
        return sample;
    }

    static bool spdif_check_signal_sampling(uint32_t sampling_rate_hz)
    {
        auto last_frame = time_us_32();
        auto error = 0;
        for (auto count = 4000; count; --count)
        {
            auto subframe = spdif_get_subframe();
            if (!subframe || !spdif_check_subframe(subframe))
                error += 1;
            if ((time_us_32() - last_frame - 1) > (1000000 / sampling_rate_hz))
                error += 1;
            last_frame = time_us_32();
        }
        return error < 2;
    }

    static AudioSample spdif_get_sample_init()
    {
        static uint32_t samplings[] = {
            // 226000000, // overclock
            // 176400,
            113000000,
            88100, // not tested
            44100,

            // 246000000, // overclock
            // 192000,
            123000000,
            96000, // didn't have good result, staying in 44.1khz or 48khz is better
            48000,
        };

        printf("[SPDIF] search signal...\n");
        while (true)
        {
            for (auto sampling : samplings)
            {
                if (sampling > 1000000) // change sysclock
                {
                    set_sys_clock_khz(sampling / 1000, true);
                    continue;
                }
                pio_sm_clear_fifos(spdif_pio, spdif_sm);
                pio_sm_clkdiv_restart(spdif_pio, spdif_sm);
                pio_sm_set_clkdiv(spdif_pio, spdif_sm, clock_get_hz(clk_sys) / (2 * 32 * sampling * 20.0f)); // 2 words per sample * 20 cycles per bit (see PIO decoder)

                // check if we capture something...
                if (spdif_check_signal_sampling(sampling))
                {
                    spdif_sampling_rate_hz = sampling;
                    printf("[SPDIF] signal detected: %ldhz, sysclock=%ldkhz\n", spdif_sampling_rate_hz, clock_get_hz(clk_sys));
                    // TODO get bit depth
                    spdif_get_sample = spdif_get_sample_24bits;
                    if (spdif_callback_sampling_change)
                        spdif_callback_sampling_change(sampling);
                    return spdif_get_sample();
                }
            }
        }

        return { .both = 0 };
    }
}
