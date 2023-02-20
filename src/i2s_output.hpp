#pragma once

#include <cstdint>
#include <pico/stdlib.h>
#include <hardware/pio.h>

namespace output
{
    extern pio_hw_t *_i2s_pio;
    extern uint _i2s_sm;

    void i2s_init(uint bck_pin, uint lrck_pin, uint data_out_pin, uint32_t sample_rate_hz);
    void i2s_set_sample_rate(uint32_t sample_rate_hz);

    inline void i2s_send_sample(uint32_t left, uint32_t right)
    {
        // pio_sm_put(_i2s_pio, _i2s_sm, (left & 0xFFFF0000) | (left & 0x10000 ? 0xFFFF : 0x0000));
        // pio_sm_put(_i2s_pio, _i2s_sm, (right & 0xFFFF0000) | (right & 0x10000 ? 0xFFFF : 0x0000));
        pio_sm_put(_i2s_pio, _i2s_sm, ((left >> 16) & 0xFFFF) | (right & 0xFFFF0000));
    }

}
