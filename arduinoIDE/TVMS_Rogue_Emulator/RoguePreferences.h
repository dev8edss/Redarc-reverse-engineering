#pragma once

#include <Arduino.h>

// RoguePreferences.h is intentionally only editable variables and values.
// The sketch/runtime owns all structs, NVS load/save code, validation, and logic.
//
// I/O mode values:
//   0 = simulated  (internal emulator value only)
//   1 = variable   (internal variable hook; can be driven from Serial/sketch code)
//   2 = pin        (ESP32 GPIO pin)
//
// Pin value:
//   -1 = no pin assigned
//
// Tank pin mode uses analogRead() and maps raw ADC 0..4095 to 0..100%.

static uint8_t pref_source_address = 0x36;        // keep different from a real Rogue at 0x30

static uint32_t pref_serial_prefix = 2606260001UL;
static uint16_t pref_serial_suffix = 0x0013;
static char pref_product_name[] = "TVMS Rogue";

// Tank levels 1..2
static uint8_t pref_tank1_percent = 50;
static uint8_t pref_tank1_mode    = 0;
static int8_t  pref_tank1_pin     = -1;

static uint8_t pref_tank2_percent = 75;
static uint8_t pref_tank2_mode    = 0;
static int8_t  pref_tank2_pin     = -1;

// Digital inputs 1..8
static uint8_t pref_input_1_mode = 0;
static int8_t  pref_input_1_pin  = -1;
static uint8_t pref_input_2_mode = 0;
static int8_t  pref_input_2_pin  = -1;
static uint8_t pref_input_3_mode = 0;
static int8_t  pref_input_3_pin  = -1;
static uint8_t pref_input_4_mode = 0;
static int8_t  pref_input_4_pin  = -1;
static uint8_t pref_input_5_mode = 0;
static int8_t  pref_input_5_pin  = -1;
static uint8_t pref_input_6_mode = 0;
static int8_t  pref_input_6_pin  = -1;
static uint8_t pref_input_7_mode = 0;
static int8_t  pref_input_7_pin  = -1;
static uint8_t pref_input_8_mode = 0;
static int8_t  pref_input_8_pin  = -1;

// Outputs 1..10
static uint8_t pref_output_1_mode = 0;
static int8_t  pref_output_1_pin  = -1;
static uint8_t pref_output_2_mode = 0;
static int8_t  pref_output_2_pin  = -1;
static uint8_t pref_output_3_mode = 0;
static int8_t  pref_output_3_pin  = -1;
static uint8_t pref_output_4_mode = 0;
static int8_t  pref_output_4_pin  = -1;
static uint8_t pref_output_5_mode = 0;
static int8_t  pref_output_5_pin  = -1;
static uint8_t pref_output_6_mode = 0;
static int8_t  pref_output_6_pin  = -1;
static uint8_t pref_output_7_mode = 0;
static int8_t  pref_output_7_pin  = -1;
static uint8_t pref_output_8_mode = 0;
static int8_t  pref_output_8_pin  = -1;
static uint8_t pref_output_9_mode = 0;
static int8_t  pref_output_9_pin  = -1;
static uint8_t pref_output_10_mode = 0;
static int8_t  pref_output_10_pin  = -1;

#include "RoguePreferencesRuntime.h"
