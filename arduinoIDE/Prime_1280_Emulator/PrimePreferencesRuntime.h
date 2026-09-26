#pragma once

#include <Arduino.h>
#include <Preferences.h>

static constexpr uint8_t PRIME_TANK_COUNT = 6;
static constexpr uint8_t PRIME_INPUT_COUNT = 3;
static constexpr uint8_t PRIME_OUTPUT_COUNT = 11;
static constexpr size_t PRIME_VARIABLE_NAME_MAX = 15;

enum PrimeIoMode : uint8_t {
  PRIME_IO_SIMULATED = 0,
  PRIME_IO_VARIABLE  = 1,
  PRIME_IO_PIN       = 2,
  PRIME_IO_DISABLED  = 3,
};

struct PrimeIoAssignment {
  PrimeIoMode mode;
  int8_t pin;
  char variable[PRIME_VARIABLE_NAME_MAX + 1];
};

struct PrimeSettings {
  uint8_t source_address;
  uint32_t serial_prefix;
  uint16_t serial_suffix;
  char product_name[64];
  uint8_t tank_percent[PRIME_TANK_COUNT + 1];

  // 1-based arrays. Index 0 is unused.
  PrimeIoAssignment tanks[PRIME_TANK_COUNT + 1];
  PrimeIoAssignment inputs[PRIME_INPUT_COUNT + 1];
  PrimeIoAssignment outputs[PRIME_OUTPUT_COUNT + 1];
};

static inline const char *prime_io_mode_name(PrimeIoMode mode) {
  switch (mode) {
    case PRIME_IO_SIMULATED: return "simulated";
    case PRIME_IO_VARIABLE:  return "variable";
    case PRIME_IO_PIN:       return "pin";
    case PRIME_IO_DISABLED:  return "disabled";
    default:                 return "unknown";
  }
}

static inline void prime_io_set_simulated(PrimeIoAssignment &a) {
  a.mode = PRIME_IO_SIMULATED;
  a.pin = -1;
  a.variable[0] = '\0';
}

static inline bool prime_io_parse(String text, PrimeIoAssignment &a) {
  text.trim();
  String lower = text;
  lower.toLowerCase();
  PrimeIoAssignment out;
  prime_io_set_simulated(out);

  if (lower.startsWith("gpio")) {
    const String num = text.substring(4);
    if (num.length() == 0 || num.length() > 2) return false;
    for (unsigned i = 0; i < num.length(); i++) if (!isDigit(num[i])) return false;
    out.mode = PRIME_IO_PIN;
    out.pin = (int8_t) num.toInt();
  } else if (lower == "simulate" || lower == "simulated") {
    // already simulated
  } else if (lower == "disabled" || lower == "disable") {
    out.mode = PRIME_IO_DISABLED;
  } else {
    if (text.length() == 0 || text.length() > PRIME_VARIABLE_NAME_MAX || text.indexOf(' ') >= 0) return false;
    out.mode = PRIME_IO_VARIABLE;
    strncpy(out.variable, text.c_str(), sizeof(out.variable) - 1);
    out.variable[sizeof(out.variable) - 1] = '\0';
  }
  a = out;
  return true;
}

static inline String prime_io_text(const PrimeIoAssignment &a) {
  switch (a.mode) {
    case PRIME_IO_PIN:      return String("GPIO") + String((int) a.pin);
    case PRIME_IO_VARIABLE: return String(a.variable);
    case PRIME_IO_DISABLED: return String("disabled");
    default:                return String("simulate");
  }
}

static inline String prime_io_describe(const PrimeIoAssignment &a) {
  if (a.mode == PRIME_IO_VARIABLE) return String("variable ") + a.variable;
  if (a.mode == PRIME_IO_PIN) return prime_io_text(a);
  return String(prime_io_mode_name(a.mode));
}

static inline void prime_copy_product_name(PrimeSettings &s, const char *name) {
  if (name == nullptr || name[0] == '\0') name = defaults.product_name;
  strncpy(s.product_name, name, sizeof(s.product_name) - 1);
  s.product_name[sizeof(s.product_name) - 1] = '\0';
}

static inline const char *prime_default_tank(uint8_t tank) {
  switch (tank) {
    case 1: return defaults.tank1;
    case 2: return defaults.tank2;
    case 3: return defaults.tank3;
    case 4: return defaults.tank4;
    case 5: return defaults.tank5;
    case 6: return defaults.tank6;
    default: return "simulate";
  }
}

static inline uint8_t prime_default_tank_percent(uint8_t tank) {
  switch (tank) {
    case 1: return defaults.tank1_percent;
    case 2: return defaults.tank2_percent;
    case 3: return defaults.tank3_percent;
    case 4: return defaults.tank4_percent;
    case 5: return defaults.tank5_percent;
    case 6: return defaults.tank6_percent;
    default: return 0;
  }
}

static inline const char *prime_default_input(uint8_t input) {
  switch (input) {
    case 1: return defaults.input_1;
    case 2: return defaults.input_2;
    case 3: return defaults.input_3;
    default: return "simulate";
  }
}

static inline const char *prime_default_output(uint8_t output) {
  switch (output) {
    case 1: return defaults.output_1;
    case 2: return defaults.output_2;
    case 3: return defaults.output_3;
    case 4: return defaults.output_4;
    case 5: return defaults.output_5;
    case 6: return defaults.output_6;
    case 7: return defaults.output_7;
    case 8: return defaults.output_8;
    case 9: return defaults.output_9;
    case 10: return defaults.output_10;
    case 11: return defaults.output_11;
    default: return "simulate";
  }
}

static inline void prime_io_from_pref(PrimeIoAssignment &a, const char *kind, uint8_t n, const char *text) {
  prime_io_set_simulated(a);
  if (!prime_io_parse(text, a)) {
    Serial.printf("PrimePreferences.h: invalid %s %u assignment \"%s\", using simulate\n", kind, (unsigned) n, text);
  }
}

static inline void prime_settings_defaults(PrimeSettings &s) {
  s.source_address = defaults.source_address;
  s.serial_prefix = defaults.serial_prefix;
  s.serial_suffix = defaults.serial_suffix;
  prime_copy_product_name(s, defaults.product_name);

  prime_io_set_simulated(s.tanks[0]);
  prime_io_set_simulated(s.inputs[0]);
  prime_io_set_simulated(s.outputs[0]);
  s.tank_percent[0] = 0;

  for (uint8_t tank = 1; tank <= PRIME_TANK_COUNT; tank++) {
    s.tank_percent[tank] = prime_default_tank_percent(tank);
    prime_io_from_pref(s.tanks[tank], "tank", tank, prime_default_tank(tank));
  }
  for (uint8_t input = 1; input <= PRIME_INPUT_COUNT; input++) prime_io_from_pref(s.inputs[input], "input", input, prime_default_input(input));
  for (uint8_t output = 1; output <= PRIME_OUTPUT_COUNT; output++) prime_io_from_pref(s.outputs[output], "output", output, prime_default_output(output));
}

static inline void prime_io_sanitize(PrimeIoAssignment &a) {
  if ((uint8_t) a.mode > (uint8_t) PRIME_IO_DISABLED) a.mode = PRIME_IO_SIMULATED;
  a.variable[sizeof(a.variable) - 1] = '\0';
  if (a.mode == PRIME_IO_VARIABLE && a.variable[0] == '\0') a.mode = PRIME_IO_SIMULATED;
  if (a.mode != PRIME_IO_PIN) a.pin = -1;
  if (a.mode != PRIME_IO_VARIABLE) a.variable[0] = '\0';
}

static inline void prime_settings_sanitize(PrimeSettings &s) {
  if (s.source_address == 0x00 || s.source_address == 0xFF) s.source_address = defaults.source_address;
  if (s.product_name[0] == '\0') prime_copy_product_name(s, defaults.product_name);
  for (uint8_t tank = 1; tank <= PRIME_TANK_COUNT; tank++) {
    if (s.tank_percent[tank] > 100) s.tank_percent[tank] = 100;
    prime_io_sanitize(s.tanks[tank]);
  }
  for (uint8_t input = 1; input <= PRIME_INPUT_COUNT; input++) prime_io_sanitize(s.inputs[input]);
  for (uint8_t output = 1; output <= PRIME_OUTPUT_COUNT; output++) prime_io_sanitize(s.outputs[output]);
}

static inline String prime_pref_key(const char *prefix, uint8_t n, const char *suffix) {
  char key[12];
  snprintf(key, sizeof(key), "%s%u%s", prefix, (unsigned) n, suffix);
  return String(key);
}

static inline void prime_io_load(Preferences &prefs, PrimeIoAssignment &a, const char *key_prefix, const char *kind, uint8_t n) {
  const String key = prime_pref_key(key_prefix, n, "");
  if (!prefs.isKey(key.c_str())) return;
  const String text = prefs.getString(key.c_str(), prime_io_text(a));
  if (!prime_io_parse(text, a)) Serial.printf("NVS: ignoring invalid assignment %s=\"%s\"\n", key.c_str(), text.c_str());
}

static inline void prime_settings_load(Preferences &prefs, PrimeSettings &s) {
  prefs.begin("primeemu", false);
  prime_settings_defaults(s);

  s.source_address = prefs.getUChar("sa", s.source_address);
  s.serial_prefix = prefs.getUInt("sp", s.serial_prefix);
  s.serial_suffix = prefs.getUShort("ss", s.serial_suffix);
  String saved_name = prefs.getString("name", s.product_name);
  prime_copy_product_name(s, saved_name.c_str());

  for (uint8_t tank = 1; tank <= PRIME_TANK_COUNT; tank++) {
    s.tank_percent[tank] = prefs.getUChar(prime_pref_key("t", tank, "").c_str(), s.tank_percent[tank]);
    prime_io_load(prefs, s.tanks[tank], "ta", "tank", tank);
  }
  for (uint8_t input = 1; input <= PRIME_INPUT_COUNT; input++) prime_io_load(prefs, s.inputs[input], "ia", "input", input);
  for (uint8_t output = 1; output <= PRIME_OUTPUT_COUNT; output++) prime_io_load(prefs, s.outputs[output], "oa", "output", output);

  prime_settings_sanitize(s);
}

static inline void prime_settings_save(Preferences &prefs, PrimeSettings &s) {
  prime_settings_sanitize(s);

  prefs.putUChar("sa", s.source_address);
  prefs.putUInt("sp", s.serial_prefix);
  prefs.putUShort("ss", s.serial_suffix);
  prefs.putString("name", s.product_name);

  for (uint8_t tank = 1; tank <= PRIME_TANK_COUNT; tank++) {
    prefs.putUChar(prime_pref_key("t", tank, "").c_str(), s.tank_percent[tank]);
    prefs.putString(prime_pref_key("ta", tank, "").c_str(), prime_io_text(s.tanks[tank]));
  }
  for (uint8_t input = 1; input <= PRIME_INPUT_COUNT; input++) prefs.putString(prime_pref_key("ia", input, "").c_str(), prime_io_text(s.inputs[input]));
  for (uint8_t output = 1; output <= PRIME_OUTPUT_COUNT; output++) prefs.putString(prime_pref_key("oa", output, "").c_str(), prime_io_text(s.outputs[output]));
}

static inline void prime_settings_reset(Preferences &prefs, PrimeSettings &s) {
  prefs.clear();
  prime_settings_defaults(s);
  prime_settings_save(prefs, s);
}
