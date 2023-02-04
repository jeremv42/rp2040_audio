#include <stdio.h>
#include <stdlib.h>

#include <hardware/clocks.h>
#include <hardware/pio.h>
#include <hardware/pwm.h>
#include <pico/stdlib.h>
#include <pico/multicore.h>

#include "spdif_decode.pio.h"
#include "test.pio.h"

const uint PIN_DEBUG_LED = PICO_DEFAULT_LED_PIN;
const uint PIN_SPDIF = 13;
const uint PIN_DAC = 14;

auto pio = pio0;
uint sm_rx = 0;

static char get_preamble(uint32_t subframe) {
	switch (subframe & 0xF)
	{
		case 0x8:
		case 0x7:
			return 'B';
		case 0x2:
		case 0xD:
			return 'M';
		case 0x4:
		case 0xB:
			return 'W';
	}

	return '?';
}

static bool check_parity(uint32_t subframe)
{
	return !!(subframe & 0x80000000) == __builtin_parity(subframe & 0x7FFFFFF0);
}

void core1_main()
{
	gpio_init(PIN_SPDIF);
	gpio_set_dir(PIN_SPDIF, GPIO_IN);

	gpio_set_function(PIN_DAC, GPIO_FUNC_PWM);
	uint slice_num = pwm_gpio_to_slice_num(PIN_DAC);
	pwm_set_clkdiv(slice_num, 1);
	pwm_set_wrap(slice_num, 255); // Set period of 1024 cycles (0 to 1023 inclusive)
	pwm_set_enabled(slice_num, true);

	printf("\n\nSTART clock=%ld\n", clock_get_hz(clk_sys));

	uint offset = 0;
	pio_add_program_at_offset(pio, &spdif_subframe_decode_program, offset);
	printf("spdif_subframe_decode loaded at %x\n", offset);
	spdif_subframe_decode_program_init(pio, sm_rx, offset, PIN_SPDIF, 1);

	while (true)
	{
		if (pio_sm_get_rx_fifo_level(pio, sm_rx) < 1)
			continue;

		auto value = pio_sm_get_blocking(pio, sm_rx);
		//pio->ctrl |= 1u << (PIO_CTRL_SM_RESTART_LSB + sm_rx); // restart
		multicore_fifo_push_timeout_us(value, 1);
	};
}

#define ARRAY_LENGTH(arr) (sizeof(arr)/sizeof(*arr))

void setup()
{
	gpio_init(PIN_DEBUG_LED);
	gpio_set_dir(PIN_DEBUG_LED, GPIO_OUT);

	gpio_init(8);
	gpio_set_dir(8, GPIO_OUT);
	auto pio = pio1;
	auto offset = pio_add_program(pio, &test_program);
	test_program_init(pio, 1, offset, 8, 10);

	multicore_launch_core1(core1_main);
}

static auto cc = 0;
void loop()
{
	static uint32_t values[16];

	values[0] = multicore_fifo_pop_blocking();
	if (get_preamble(values[0]) != 'B')
		return;
	for (auto cc = 1; cc < ARRAY_LENGTH(values); ++cc)
		values[cc] = multicore_fifo_pop_blocking();

	printf("[%06ld] ---- START ---- \n", time_us_32());
	for (auto value: values)
	{
		auto sample = (value >> 8) & 0xFFFF;
		{
			printf("%c %02lx ", get_preamble(value), value & 0xFF);
			printf("%04lx (%06d) ", sample, (int16_t)sample);
			printf("%01lx", (value >> 28) & 0xF);
		}
		if (!check_parity(value))
			printf(" - invalid parity\n");
		else
		{
			printf("\n");
		}
	}
	printf("[%06ld] ----  END  ---- \n", time_us_32());
}

int main()
{
	stdio_init_all();
	setup();
	while (true)
		loop();
}
