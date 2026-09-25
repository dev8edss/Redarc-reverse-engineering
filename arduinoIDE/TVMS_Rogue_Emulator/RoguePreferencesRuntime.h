#pragma once

#include <Arduino.h>
#include <Preferences.h>

static constexpr uint8_t ROGUE_TANK_COUNT = 2;
static constexpr uint8_t ROGUE_INPUT_COUNT = 8;
static constexpr uint8_t ROGUE_OUTPUT_COUNT = 10;

enum RogueIoMode : uint8_t {
  ROGUE_IO_SIMULATED = 0,  // internal emulator-only value
  ROGUE_IO_VARIABLE  = 1,  // internal variable hook; can be driven from sketch/serial
  ROGUE_IO_PIN       = 2,  // ESP32 GPIO pin
};

struct RogueIoAssignment {
  RogueIoMode mode;
  int8_t pin;              // -1 when no GPIO is assigned
};

struct RogueSettings {
  uint8_t source_address;
  uint8_t tank1_percent;
  uint8_t tank2_percent;
  uint32_t serial_prefix;
  uint16_t serial_suffix;
  char product_name[64];

  // 1-based arrays. Index 0 is unused so REDARC channel numbering is easy.
  RogueIoAssignment tanks[ROGUE_TANK_COUNT + 1];
  RogueIoAssignment inputs[ROGUE_INPUT_COUNT + 1];
  RogueIoAssignment outputs[ROGUE_OUTPUT_COUNT + 1];
};

static inline const char *rogue_io_mode_name(RogueIoMode mode) {
  switch (mode) {
    case ROGUE_IO_SIMULATED: return "simulated";
    case ROGUE_IO_VARIABLE:  return "variable";
    case ROGUE_IO_PIN:       return "pin";
    default:                 return "unknown";
  }
}

static inline bool rogue_io_mode_from_text(String text, RogueIoMode &mode) {
  text.trim();
  text.toLowerCase();
  if (text == "sim" || text == "simulated" || text == "simulate") {
    mode = ROGUE_IO_SIMULATED;
    return true;
  }
  if (text == "var" || text == "variable") {
    mode = ROGUE_IO_VARIABLE;
    return true;
  }
  if (text == "pin" || text == "gpio" || text == "esp") {
    mode = ROGUE_IO_PIN;
    return true;
  }
  return false;
}

static inline RogueIoMode rogue_sanitize_io_mode(uint8_t raw) {
  if (raw <= (uint8_t) ROGUE_IO_PIN) return (RogueIoMode) raw;
  return ROGUE_IO_SIMULATED;
}

static inline void rogue_copy_product_name(RogueSettings &s, const char *name) {
  if (name == nullptr || name[0] == '\0') name = pref_product_name;
  strncpy(s.product_name, name, sizeof(s.product_name) - 1);
  s.product_name[sizeof(s.product_name) - 1] = '\0';
}

static inline uint8_t rogue_pref_tank_mode(uint8_t tank) {
  switch (tank) {
    case 1: return pref_tank1_mode;
    case 2: return pref_tank2_mode;
    default: return ROGUE_IO_SIMULATED;
  }
}

static inline int8_t rogue_pref_tank_pin(uint8_t tank) {
  switch (tank) {
    case 1: return pref_tank1_pin;
    case 2: return pref_tank2_pin;
    default: return -1;
  }
}

static inline uint8_t rogue_pref_input_mode(uint8_t input) {
  switch (input) {
    case 1: return pref_input_1_mode;
    case 2: return pref_input_2_mode;
    case 3: return pref_input_3_mode;
    case 4: return pref_input_4_mode;
    case 5: return pref_input_5_mode;
    case 6: return pref_input_6_mode;
    case 7: return pref_input_7_mode;
    case 8: return pref_input_8_mode;
    default: return ROGUE_IO_SIMULATED;
  }
}

static inline int8_t rogue_pref_input_pin(uint8_t input) {
  switch (input) {
    case 1: return pref_input_1_pin;
    case 2: return pref_input_2_pin;
    case 3: return pref_input_3_pin;
    case 4: return pref_input_4_pin;
    case 5: return pref_input_5_pin;
    case 6: return pref_input_6_pin;
    case 7: return pref_input_7_pin;
    case 8: return pref_input_8_pin;
    default: return -1;
  }
}

static inline uint8_t rogue_pref_output_mode(uint8_t output) {
  switch (output) {
    case 1: return pref_output_1_mode;
    case 2: return pref_output_2_mode;
    case 3: return pref_output_3_mode;
    case 4: return pref_output_4_mode;
    case 5: return pref_output_5_mode;
    case 6: return pref_output_6_mode;
    case 7: return pref_output_7_mode;
    case 8: return pref_output_8_mode;
    case 9: return pref_output_9_mode;
    case 10: return pref_output_10_mode;
    default: return ROGUE_IO_SIMULATED;
  }
}

static inline int8_t rogue_pref_output_pin(uint8_t output) {
  switch (output) {
    case 1: return pref_output_1_pin;
    case 2: return pref_output_2_pin;
    case 3: return pref_output_3_pin;
    case 4: return pref_output_4_pin;
    case 5: return pref_output_5_pin;
    case 6: return pref_output_6_pin;
    case 7: return pref_output_7_pin;
    case 8: return pref_output_8_pin;
    case 9: return pref_output_9_pin;
    case 10: return pref_output_10_pin;
    default: return -1;
  }
}

static inline void rogue_settings_defaults(RogueSettings &s) {
  s.source_address = pref_source_address;
  s.tank1_percent = pref_tank1_percent;
  s.tank2_percent = pref_tank2_percent;
  s.serial_prefix = pref_serial_prefix;
  s.serial_suffix = pref_serial_suffix;
  rogue_copy_product_name(s, pref_product_name);

  for (uint8_t i = 0; i <= ROGUE_TANK_COUNT; i++) {
    s.tanks[i].mode = ROGUE_IO_SIMULATED;
    s.tanks[i].pin = -1;
  }
  for (uint8_t i = 0; i <= ROGUE_INPUT_COUNT; i++) {
    s.inputs[i].mode = ROGUE_IO_SIMULATED;
    s.inputs[i].pin = -1;
  }
  for (uint8_t i = 0; i <= ROGUE_OUTPUT_COUNT; i++) {
    s.outputs[i].mode = ROGUE_IO_SIMULATED;
    s.outputs[i].pin = -1;
  }

  for (uint8_t tank = 1; tank <= ROGUE_TANK_COUNT; tank++) {
    s.tanks[tank].mode = rogue_sanitize_io_mode(rogue_pref_tank_mode(tank));
    s.tanks[tank].pin = rogue_pref_tank_pin(tank);
  }
  for (uint8_t input = 1; input <= ROGUE_INPUT_COUNT; input++) {
    s.inputs[input].mode = rogue_sanitize_io_mode(rogue_pref_input_mode(input));
    s.inputs[input].pin = rogue_pref_input_pin(input);
  }
  for (uint8_t output = 1; output <= ROGUE_OUTPUT_COUNT; output++) {
    s.outputs[output].mode = rogue_sanitize_io_mode(rogue_pref_output_mode(output));
    s.outputs[output].pin = rogue_pref_output_pin(output);
  }
}

static inline void rogue_settings_sanitize(RogueSettings &s) {
  if (s.source_address == 0x00 || s.source_address == 0xFF) s.source_address = 0x36;
  if (s.tank1_percent > 100) s.tank1_percent = 100;
  if (s.tank2_percent > 100) s.tank2_percent = 100;
  if (s.product_name[0] == '\0') rogue_copy_product_name(s, pref_product_name);

  for (uint8_t tank = 1; tank <= ROGUE_TANK_COUNT; tank++) {
    s.tanks[tank].mode = rogue_sanitize_io_mode((uint8_t) s.tanks[tank].mode);
    if (s.tanks[tank].mode != ROGUE_IO_PIN) s.tanks[tank].pin = -1;
  }
  for (uint8_t input = 1; input <= ROGUE_INPUT_COUNT; input++) {
    s.inputs[input].mode = rogue_sanitize_io_mode((uint8_t) s.inputs[input].mode);
    if (s.inputs[input].mode != ROGUE_IO_PIN) s.inputs[input].pin = -1;
  }
  for (uint8_t output = 1; output <= ROGUE_OUTPUT_COUNT; output++) {
    s.outputs[output].mode = rogue_sanitize_io_mode((uint8_t) s.outputs[output].mode);
    if (s.outputs[output].mode != ROGUE_IO_PIN) s.outputs[output].pin = -1;
  }
}

static inline String rogue_pref_key(const char *prefix, uint8_t n, const char *suffix) {
  char key[12];
  snprintf(key, sizeof(key), "%s%u%s", prefix, (unsigned) n, suffix);
  return String(key);
}

static inline void rogue_settings_load(Preferences &prefs, RogueSettings &s) {
  prefs.begin("rogueemu", false);
  rogue_settings_defaults(s);

  s.source_address = prefs.getUChar("sa", s.source_address);
  s.tank1_percent = prefs.getUChar("t1", s.tank1_percent);
  s.tank2_percent = prefs.getUChar("t2", s.tank2_percent);
  s.serial_prefix = prefs.getUInt("sp", s.serial_prefix);
  s.serial_suffix = prefs.getUShort("ss", s.serial_suffix);
  String saved_name = prefs.getString("name", s.product_name);
  rogue_copy_product_name(s, saved_name.c_str());

  for (uint8_t tank = 1; tank <= ROGUE_TANK_COUNT; tank++) {
    s.tanks[tank].mode = rogue_sanitize_io_mode(
        prefs.getUChar(rogue_pref_key("t", tank, "m").c_str(), (uint8_t) s.tanks[tank].mode));
    s.tanks[tank].pin =
        prefs.getChar(rogue_pref_key("t", tank, "p").c_str(), s.tanks[tank].pin);
  }
  for (uint8_t input = 1; input <= ROGUE_INPUT_COUNT; input++) {
    s.inputs[input].mode = rogue_sanitize_io_mode(
        prefs.getUChar(rogue_pref_key("i", input, "m").c_str(), (uint8_t) s.inputs[input].mode));
    s.inputs[input].pin =
        prefs.getChar(rogue_pref_key("i", input, "p").c_str(), s.inputs[input].pin);
  }
  for (uint8_t output = 1; output <= ROGUE_OUTPUT_COUNT; output++) {
    s.outputs[output].mode = rogue_sanitize_io_mode(
        prefs.getUChar(rogue_pref_key("o", output, "m").c_str(), (uint8_t) s.outputs[output].mode));
    s.outputs[output].pin =
        prefs.getChar(rogue_pref_key("o", output, "p").c_str(), s.outputs[output].pin);
  }

  rogue_settings_sanitize(s);
}

static inline void rogue_settings_save(Preferences &prefs, RogueSettings &s) {
  rogue_settings_sanitize(s);

  prefs.putUChar("sa", s.source_address);
  prefs.putUChar("t1", s.tank1_percent);
  prefs.putUChar("t2", s.tank2_percent);
  prefs.putUInt("sp", s.serial_prefix);
  prefs.putUShort("ss", s.serial_suffix);
  prefs.putString("name", s.product_name);

  for (uint8_t tank = 1; tank <= ROGUE_TANK_COUNT; tank++) {
    prefs.putUChar(rogue_pref_key("t", tank, "m").c_str(), (uint8_t) s.tanks[tank].mode);
    prefs.putChar(rogue_pref_key("t", tank, "p").c_str(), s.tanks[tank].pin);
  }
  for (uint8_t input = 1; input <= ROGUE_INPUT_COUNT; input++) {
    prefs.putUChar(rogue_pref_key("i", input, "m").c_str(), (uint8_t) s.inputs[input].mode);
    prefs.putChar(rogue_pref_key("i", input, "p").c_str(), s.inputs[input].pin);
  }
  for (uint8_t output = 1; output <= ROGUE_OUTPUT_COUNT; output++) {
    prefs.putUChar(rogue_pref_key("o", output, "m").c_str(), (uint8_t) s.outputs[output].mode);
    prefs.putChar(rogue_pref_key("o", output, "p").c_str(), s.outputs[output].pin);
  }
}

static inline void rogue_settings_reset(Preferences &prefs, RogueSettings &s) {
  prefs.clear();
  rogue_settings_defaults(s);
  rogue_settings_save(prefs, s);
}
