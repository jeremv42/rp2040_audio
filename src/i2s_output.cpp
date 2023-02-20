#include "i2s_output.hpp"
#include "i2s_output.pio.h"
#include <hardware/clocks.h>
#include <hardware/pio.h>
#include <cstdio>

namespace output
{
    pio_hw_t *_i2s_pio = pio1;
    uint _i2s_sm = 1;

    static uint _i2s_program_offset;
    void i2s_init(uint lrck_pin, uint bck_pin, uint data_out_pin, uint32_t sample_rate_hz)
    {
        //assert(bck_pin - lrck_pin == 1);

        gpio_init(lrck_pin);
        gpio_init(bck_pin);
        gpio_init(data_out_pin);
        gpio_set_function(lrck_pin, GPIO_FUNC_PIO1);
        gpio_set_function(bck_pin, GPIO_FUNC_PIO1);
        gpio_set_function(data_out_pin, GPIO_FUNC_PIO1);

        _i2s_program_offset = pio_add_program(_i2s_pio, &audio_i2s_program);
        audio_i2s_program_init(_i2s_pio, _i2s_sm, _i2s_program_offset, data_out_pin, lrck_pin);
        pio_sm_set_clkdiv(_i2s_pio, _i2s_sm, clock_get_hz(clk_sys) / (2 * 2 * 16.0 * sample_rate_hz));
        pio_sm_set_enabled(_i2s_pio, _i2s_sm, true);
    }

    void i2s_set_sample_rate(uint32_t sample_rate_hz)
    {
        printf("i2s output: change sampling rate: %ldhz\n", sample_rate_hz);
        pio_sm_set_enabled(_i2s_pio, _i2s_sm, false);

        pio_sm_set_clkdiv(_i2s_pio, _i2s_sm, clock_get_hz(clk_sys) / (2 * 2 * 16.0 * sample_rate_hz));

        pio_sm_clear_fifos(_i2s_pio, _i2s_sm);
        pio_sm_restart(_i2s_pio, _i2s_sm);
        pio_sm_clkdiv_restart(_i2s_pio, _i2s_sm);
        pio_sm_exec(_i2s_pio, _i2s_sm, pio_encode_jmp(_i2s_program_offset));
        pio_sm_set_enabled(_i2s_pio, _i2s_sm, true);
    }

}
