# RP2040 simple audio input/output
In the current state, it's just a SPDIF input to I²S output.

The input and output are synchronized on the same sampling rate (output in 16bits, input in 24 or 16bits).

## Goal
Use the RP2040 as an audio adapter: can take a format and transform into another (SPDIF => I²S, I²S => SPDIF, etc).

### Personal goal
For my usage, it will be coupled with an ESP32 to create a small "pre-amplifier" with multiple inputs: SPDIF (TV, Console, etc), Bluetooth (any phone), Airplay, maybe an ADC for a jack or RCA. \
It will be remotely controlled with a simple IR remote.

## TODO
### Required
- [x] decode 48khz SPDIF
- [x] decode 44.1khz SPDIF
- [ ] Decode SPDIF user data
- [x] Decode more than 16bits if possible
- [x] Detect sampling rate
- [x] Detect signal lost
- [x] I²S output
- [ ] SPI output & control

### Nice to have
- [ ] 32 bits I²S output
- [ ] DMA (is it possible?)
- [ ] Resampling
- [ ] PWM DAC output
- [ ] SPI input
- [ ] SPDIF output
- [ ] I²S input
