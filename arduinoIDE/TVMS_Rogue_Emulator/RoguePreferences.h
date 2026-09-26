#pragma once

#include <Arduino.h>

// RoguePreferences.h is intentionally only editable variables and values.
// The sketch/runtime owns all structs, NVS load/save code, validation, and logic.
//
// Tank / input / output assignment text:
//   "GPIO34"      = bound to ESP32 GPIO 34
//   "simulate"    = internal emulator-only value
//   "disabled"    = not used; value held at 0/off and commands ignored
//   anything else = a named variable, e.g. "fresh_water" (max 15 chars, no spaces)
//
// Tank GPIO mode uses analogRead() and maps raw ADC 0..4095 to 0..100%.

static uint8_t pref_source_address = 0x36;        // keep different from a real Rogue at 0x30

static uint32_t pref_serial_prefix = 2606260001UL;
static uint16_t pref_serial_suffix = 0x0013;
static char pref_product_name[] = "TVMS Rogue";

// Tank levels 1..2
static uint8_t pref_tank1_percent = 50;
static const char pref_tank1[] = "simulate";

static uint8_t pref_tank2_percent = 75;
static const char pref_tank2[] = "simulate";

// Digital inputs 1..8
static const char pref_input_1[] = "simulate";
static const char pref_input_2[] = "simulate";
static const char pref_input_3[] = "simulate";
static const char pref_input_4[] = "simulate";
static const char pref_input_5[] = "simulate";
static const char pref_input_6[] = "simulate";
static const char pref_input_7[] = "simulate";
static const char pref_input_8[] = "simulate";

// Whether an output is dimmable comes from the Rogue configuration (Object 2), not from here.
// Dimmable GPIO outputs are driven with LEDC PWM so the output level (0-100%) sets the duty
// cycle: 0% = LOW, 100% = HIGH, anything between dims the load. Non-dimmable GPIO outputs
// are plain on/off.
// frequency * 2^resolution must not exceed 80 MHz (e.g. 5000 Hz at 10 bits is fine).
static uint32_t pref_output_pwm_frequency_hz    = 5000;
static uint8_t  pref_output_pwm_resolution_bits = 10;

// Outputs 1..10
static const char pref_output_1[]  = "simulate";
static const char pref_output_2[]  = "simulate";
static const char pref_output_3[]  = "simulate";
static const char pref_output_4[]  = "simulate";
static const char pref_output_5[]  = "simulate";
static const char pref_output_6[]  = "simulate";
static const char pref_output_7[]  = "simulate";
static const char pref_output_8[]  = "simulate";
static const char pref_output_9[]  = "simulate";
static const char pref_output_10[] = "simulate";

#include "RoguePreferencesRuntime.h"
