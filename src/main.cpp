#include <cstdio>
#include <cstdlib>

#include <hardware/clocks.h>
#include <hardware/pio.h>
#include <hardware/pwm.h>
#include <pico/stdlib.h>
#include <pico/multicore.h>

#include "spdif_input.hpp"
#include "i2s_output.hpp"

const uint PIN_INPUT_SPDIF = 13;

const uint PIN_OUTPUT_I2S_LRCK = 8;
const uint PIN_OUTPUT_I2S_BCK = 9;
const uint PIN_OUTPUT_I2S_DATA = 10;

static inline void spdif_to_i2s_update()
{
	auto sample = input::spdif_get_sample();
	output::i2s_send_sample(sample.left, sample.right);
}

void setup()
{
	// configure I2S output, init with 48khz but it will change
	output::i2s_init(PIN_OUTPUT_I2S_LRCK, PIN_OUTPUT_I2S_BCK, PIN_OUTPUT_I2S_DATA, 48000);
	// configure SPDIF input
	input::spdif_init(PIN_INPUT_SPDIF);
	input::spdif_set_callback_sampling_change(output::i2s_set_sample_rate);
}

void loop()
{
	spdif_to_i2s_update();
}

int main()
{
	// for best results:
	// 123Mhz for 48khz or 96khz
	// 113Mhz for 44.1khz or 88.2khz
	set_sys_clock_khz(123000, true);
	stdio_init_all();
	setup();
	while (true)
		loop();
}
