// PrimePreferences.h - built-in default settings. Edit the values only.
//
// Each line is `name = value;`. The types live in struct PrimeDefaults in
// Prime_1280_Emulator.ino, which includes this file inside PrimeDefaults::load().
// Values saved in NVS from Serial Monitor override these; run `defaults` to reload
// what is written here.
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

// Real Prime / TVMS1280 is normally 0x24. Default to 0x25 so it can be tested beside it.
source_address = 0x25;

serial_prefix = 2509151234UL;
serial_suffix = 0x0019;
product_name  = "TVMS 1280 Prime";

// Prime / TVMS1280 tanks 1..6. These map to the captured Prime analogue/tank channels
// around 0x15..0x1A.
tank1_percent = 0;
tank1         = "simulate";
tank2_percent = 0;
tank2         = "simulate";
tank3_percent = 0;
tank3         = "simulate";
tank4_percent = 0;
tank4         = "simulate";
tank5_percent = 0;
tank5         = "simulate";
tank6_percent = 0;
tank6         = "simulate";

// Prime / TVMS1280 digital inputs 1..3, captured as channels 0x01..0x03.
input_1 = "simulate";
input_2 = "simulate";
input_3 = "simulate";

// GPIO outputs are PWM-capable by default so 0..100% levels can be represented.
output_pwm_frequency_hz    = 5000;
output_pwm_resolution_bits = 10;

// Prime / TVMS1280 outputs 1..11, captured as channels 0x04..0x0E.
// Output 11 is the inverter output in the captured labels.
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
output_11 = "simulate";
