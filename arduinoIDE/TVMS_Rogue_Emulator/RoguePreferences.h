#pragma once

#include <Arduino.h>
#include <Preferences.h>

static constexpr uint8_t ROGUE_INPUT_COUNT = 8;
static constexpr uint8_t ROGUE_OUTPUT_COUNT = 10;

static constexpr uint8_t ROGUE_DEFAULT_SOURCE_ADDRESS = 0x36;   // keep different from real Rogue 0x30
static constexpr uint32_t ROGUE_DEFAULT_SERIAL_PREFIX = 2606260001UL;
static constexpr uint16_t ROGUE_DEFAULT_SERIAL_SUFFIX = 0x0013;
static constexpr char ROGUE_DEFAULT_PRODUCT_NAME[] = "TVMS Rogue";

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

static inline void rogue_copy_product_name(RogueSettings &s, const char *name) {
  if (name == nullptr || name[0] == '\0') name = ROGUE_DEFAULT_PRODUCT_NAME;
  strncpy(s.product_name, name, sizeof(s.product_name) - 1);
  s.product_name[sizeof(s.product_name) - 1] = '\0';
}

static inline void rogue_settings_defaults(RogueSettings &s) {
  s.source_address = ROGUE_DEFAULT_SOURCE_ADDRESS;
  s.tank1_percent = 50;
  s.tank2_percent = 75;
  s.serial_prefix = ROGUE_DEFAULT_SERIAL_PREFIX;
  s.serial_suffix = ROGUE_DEFAULT_SERIAL_SUFFIX;
  rogue_copy_product_name(s, ROGUE_DEFAULT_PRODUCT_NAME);

  for (uint8_t i = 0; i <= ROGUE_INPUT_COUNT; i++) {
    s.inputs[i].mode = ROGUE_IO_SIMULATED;
    s.inputs[i].pin = -1;
  }
  for (uint8_t i = 0; i <= ROGUE_OUTPUT_COUNT; i++) {
    s.outputs[i].mode = ROGUE_IO_SIMULATED;
    s.outputs[i].pin = -1;
  }
}

static inline RogueIoMode rogue_clamp_mode(uint8_t raw) {
  if (raw > (uint8_t) ROGUE_IO_PIN) return ROGUE_IO_SIMULATED;
  return (RogueIoMode) raw;
}

static inline int8_t rogue_clamp_pin(int32_t pin) {
  if (pin < -1) return -1;
  if (pin > 127) return -1;
  return (int8_t) pin;
}

static inline void rogue_settings_sanitize(RogueSettings &s) {
  if (s.source_address == 0x00 || s.source_address == 0xFF) {
    s.source_address = ROGUE_DEFAULT_SOURCE_ADDRESS;
  }
  if (s.tank1_percent > 100) s.tank1_percent = 100;
  if (s.tank2_percent > 100) s.tank2_percent = 100;
  if (s.product_name[0] == '\0') rogue_copy_product_name(s, ROGUE_DEFAULT_PRODUCT_NAME);

  for (uint8_t i = 1; i <= ROGUE_INPUT_COUNT; i++) {
    s.inputs[i].mode = rogue_clamp_mode((uint8_t) s.inputs[i].mode);
    s.inputs[i].pin = rogue_clamp_pin(s.inputs[i].pin);
    if (s.inputs[i].mode != ROGUE_IO_PIN) s.inputs[i].pin = -1;
  }
  for (uint8_t i = 1; i <= ROGUE_OUTPUT_COUNT; i++) {
    s.outputs[i].mode = rogue_clamp_mode((uint8_t) s.outputs[i].mode);
    s.outputs[i].pin = rogue_clamp_pin(s.outputs[i].pin);
    if (s.outputs[i].mode != ROGUE_IO_PIN) s.outputs[i].pin = -1;
  }
}

static inline void rogue_settings_load(Preferences &prefs, RogueSettings &s) {
  rogue_settings_defaults(s);
  prefs.begin("rogueemu", false);

  s.source_address = prefs.getUChar("sa", ROGUE_DEFAULT_SOURCE_ADDRESS);
  s.tank1_percent = prefs.getUChar("t1", 50);
  s.tank2_percent = prefs.getUChar("t2", 75);
  s.serial_prefix = prefs.getUInt("sp", ROGUE_DEFAULT_SERIAL_PREFIX);
  s.serial_suffix = prefs.getUShort("ss", ROGUE_DEFAULT_SERIAL_SUFFIX);
  String saved_name = prefs.getString("name", ROGUE_DEFAULT_PRODUCT_NAME);
  rogue_copy_product_name(s, saved_name.c_str());

  char key[12];
  for (uint8_t i = 1; i <= ROGUE_INPUT_COUNT; i++) {
    snprintf(key, sizeof(key), "im%u", i);
    s.inputs[i].mode = rogue_clamp_mode(prefs.getUChar(key, (uint8_t) ROGUE_IO_SIMULATED));
    snprintf(key, sizeof(key), "ip%u", i);
    s.inputs[i].pin = rogue_clamp_pin(prefs.getChar(key, -1));
  }
  for (uint8_t i = 1; i <= ROGUE_OUTPUT_COUNT; i++) {
    snprintf(key, sizeof(key), "om%u", i);
    s.outputs[i].mode = rogue_clamp_mode(prefs.getUChar(key, (uint8_t) ROGUE_IO_SIMULATED));
    snprintf(key, sizeof(key), "op%u", i);
    s.outputs[i].pin = rogue_clamp_pin(prefs.getChar(key, -1));
  }

  rogue_settings_sanitize(s);
}

static inline void rogue_settings_save(Preferences &prefs, const RogueSettings &s) {
  prefs.putUChar("sa", s.source_address);
  prefs.putUChar("t1", s.tank1_percent);
  prefs.putUChar("t2", s.tank2_percent);
  prefs.putUInt("sp", s.serial_prefix);
  prefs.putUShort("ss", s.serial_suffix);
  prefs.putString("name", s.product_name);

  char key[12];
  for (uint8_t i = 1; i <= ROGUE_INPUT_COUNT; i++) {
    snprintf(key, sizeof(key), "im%u", i);
    prefs.putUChar(key, (uint8_t) s.inputs[i].mode);
    snprintf(key, sizeof(key), "ip%u", i);
    prefs.putChar(key, s.inputs[i].pin);
  }
  for (uint8_t i = 1; i <= ROGUE_OUTPUT_COUNT; i++) {
    snprintf(key, sizeof(key), "om%u", i);
    prefs.putUChar(key, (uint8_t) s.outputs[i].mode);
    snprintf(key, sizeof(key), "op%u", i);
    prefs.putChar(key, s.outputs[i].pin);
  }
}

static inline void rogue_settings_reset(Preferences &prefs, RogueSettings &s) {
  prefs.clear();
  rogue_settings_defaults(s);
  rogue_settings_save(prefs, s);
}
