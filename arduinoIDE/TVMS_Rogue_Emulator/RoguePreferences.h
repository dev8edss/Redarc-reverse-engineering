// RoguePreferences.h - built-in default settings. Edit the values only.
//
// Each line is `name = value;`. The types live in struct RogueDefaults in
// TVMS_Rogue_Emulator.ino, which includes this file inside RogueDefaults::load().
// Values saved in NVS from the Serial Monitor override these; run `defaults` to go
// back to what is written here.
//
// Tank / input / output assignment text:
//   "GPIO34"      = bound to ESP32 GPIO 34
//   "simulate"    = internal emulator-only value
//   "disabled"    = not used; value held at 0/off and commands ignored
//   anything else = a named variable, e.g. "fresh_water" (max 15 chars, no spaces)
//
// Tank GPIO mode uses analogRead() and maps raw ADC 0..4095 to 0..100%.

// CAN bus (250 kbit/s, extended IDs). M5Stack Atomic CAN Base: TX 22, RX 19.
can_tx_pin = 22;
can_rx_pin = 19;

source_address = 0x36;        // keep different from a real Rogue at 0x30

// This device's serial number (fixed on a real Rogue). Its name comes from the TVMS Rogue
// record with this serial in the configuration object (Object 2), which RedVision writes.
serial_prefix = 2606260001UL;
serial_suffix = 0x0013;

// Tank levels 1..2
tank1_percent = 50;
tank1         = "simulate";

tank2_percent = 75;
tank2         = "simulate";

// Digital inputs 1..8
input_1 = "simulate";
input_2 = "simulate";
input_3 = "simulate";
input_4 = "simulate";
input_5 = "simulate";
input_6 = "simulate";
input_7 = "simulate";
input_8 = "simulate";

// Whether an output is dimmable comes from the Rogue configuration (Object 2), not from here.
// Dimmable GPIO outputs are driven with LEDC PWM so the output level (0-100%) sets the duty
// cycle: 0% = LOW, 100% = HIGH, anything between dims the load. Non-dimmable GPIO outputs
// are plain on/off.
// frequency * 2^resolution must not exceed 80 MHz (e.g. 5000 Hz at 10 bits is fine).
output_pwm_frequency_hz    = 5000;
output_pwm_resolution_bits = 10;

// Outputs 1..10
output_1  = "simulate";
output_2  = "simulate";
output_3  = "simulate";
output_4  = "simulate";
output_5  = "simulate";
output_6  = "simulate";
output_7  = "simulate";
output_8  = "simulate";
output_9  = "simulate";
output_10 = "simulate";

// DGN replies that are not decoded from Object 2 yet. Defaults are the captured real-Rogue
// replies. Each is a list of 8-byte CAN frames: hex bytes separated by spaces, frames
// separated by commas (up to 16 frames). "" sends no reply. Known layout:
//   0x1FD07 D1 base item, D2-D8 status per item (FF = OK/unknown; FC/FD seen on other devices)
//   0x1FD08 D1 highest channel (0x21), D2-D8 not decoded
//   0x1FD0C D1 channel, D2 format (64 tank %, 61 analogue), D7-D8 range max LE (100 / 60000)
//   0x1FD10 D1 input 1-8, D2-D3 not decoded (a real Rogue sends 00 00)
reply_1fd07_sensor_status   = "09 FF FF FF FF FF FF FF, 16 FF FF FF FF FF FF FF";
reply_1fd08_active_channels = "21 FF FF 1E FF FF FF FF";
reply_1fd0c_sensor_format   = "09 64 00 00 00 00 64 00, 0A 64 00 00 00 00 64 00, "
                              "16 61 00 00 00 00 60 EA, 17 61 00 00 00 00 60 EA";
reply_1fd10_input_config    = "01 00 00 FF FF FF FF FF, 02 00 00 FF FF FF FF FF, "
                              "03 00 00 FF FF FF FF FF, 04 00 00 FF FF FF FF FF, "
                              "05 00 00 FF FF FF FF FF, 06 00 00 FF FF FF FF FF, "
                              "07 00 00 FF FF FF FF FF, 08 00 00 FF FF FF FF FF";
