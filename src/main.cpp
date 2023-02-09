#include <cstdio>
#include <cstdlib>

#include <hardware/clocks.h>
#include <hardware/pio.h>
#include <hardware/pwm.h>
#include <pico/stdlib.h>
#include <pico/multicore.h>

#include "i2s_output.pio.h"
#include "spdif_decode.pio.h"

const uint PIN_DEBUG_LED = 12;
const uint PIN_SPDIF = 13;

const uint PIN_I2S_BCK = 8;
const uint PIN_I2S_LRCK = PIN_I2S_BCK + 1; // LRCK pin must be BCK + 1
const uint PIN_I2S_DATA = 10;

static pio_hw_t *spdif_pio = pio0;
static uint spdif_sm = 0;
static pio_hw_t *i2s_pio = pio1;
static uint i2s_sm = 1;

typedef uint32_t spdif_subframe;

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

static bool spdif_check_parity(spdif_subframe subframe)
{
	return !!(subframe & 0x80000000) == __builtin_parity(subframe & 0x7FFFFFF0);
}

void i2s_init(uint bck_pin, uint lrck_pin, uint data_out_pin)
{
	gpio_init(bck_pin);
	gpio_init(lrck_pin);
	gpio_init(data_out_pin);
	gpio_set_function(bck_pin, GPIO_FUNC_PIO1);
	gpio_set_function(lrck_pin, GPIO_FUNC_PIO1);
	gpio_set_function(data_out_pin, GPIO_FUNC_PIO1);

	auto offset = pio_add_program(i2s_pio, &audio_i2s_program);
	audio_i2s_program_init(i2s_pio, i2s_sm, offset, PIN_I2S_DATA, PIN_I2S_BCK);
	pio_sm_set_clkdiv(i2s_pio, i2s_sm, clock_get_hz(clk_sys) / (2 * 2 * 16 * 48000.0));
	pio_sm_set_enabled(i2s_pio, i2s_sm, true);
}
void spdif_init(uint spdif_pin)
{
	// configure I2S output
	i2s_init(PIN_I2S_BCK, PIN_I2S_LRCK, PIN_I2S_DATA);

	// configure SPDIF decoder
	gpio_init(spdif_pin);
	gpio_set_dir(spdif_pin, GPIO_IN);

	printf("\n\nSTART clock=%ld\n", clock_get_hz(clk_sys));
	uint offset = 0;
	pio_add_program_at_offset(spdif_pio, &spdif_subframe_decode_program, offset);
	printf("spdif_subframe_decode loaded at %x\n", offset);
	spdif_subframe_decode_program_init(spdif_pio, spdif_sm, offset, spdif_pin, 2);
}
static void spdif_to_i2s_update()
{
	int32_t sample = 0;

	uint32_t sample1 = pio_sm_get_blocking(pio0, 0);
	if (spdif_get_preamble(sample1) == 'W')
		sample |= ((sample1 >> 12) & 0xFFFF) << 16;
	else
		sample |= (sample1 >> 12) & 0xFFFF;

	uint32_t sample2 = pio_sm_get_blocking(pio0, 0);
	if (spdif_get_preamble(sample2) == 'W')
		sample |= ((sample2 >> 12) & 0xFFFF) << 16;
	else
		sample |= (sample2 >> 12) & 0xFFFF;

	pio_sm_put(i2s_pio, i2s_sm, sample);
}

void setup()
{
	gpio_init(PIN_DEBUG_LED);
	gpio_set_dir(PIN_DEBUG_LED, GPIO_OUT);

	gpio_put(PIN_DEBUG_LED, 1);
	sleep_ms(500);
	gpio_put(PIN_DEBUG_LED, 0);
	sleep_ms(500);
	gpio_put(PIN_DEBUG_LED, 1);
	sleep_ms(500);
	gpio_put(PIN_DEBUG_LED, 0);

	spdif_init(PIN_SPDIF);
}

static auto cc = 0;
void loop()
{
	spdif_to_i2s_update();
}

int main()
{
	set_sys_clock_khz(123000, true);
	stdio_init_all();
	setup();
	while (true)
		loop();
}
