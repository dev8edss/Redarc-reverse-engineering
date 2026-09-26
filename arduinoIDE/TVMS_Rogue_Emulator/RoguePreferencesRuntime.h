#pragma once

#include <Arduino.h>
#include <Preferences.h>

static constexpr uint8_t ROGUE_TANK_COUNT = 2;
static constexpr uint8_t ROGUE_INPUT_COUNT = 8;
static constexpr uint8_t ROGUE_OUTPUT_COUNT = 10;
static constexpr size_t ROGUE_VARIABLE_NAME_MAX = 15;

// The mode is never configured directly; it is derived from the assignment text.
// See RoguePreferences.h for the text format.
enum RogueIoMode : uint8_t {
  ROGUE_IO_SIMULATED = 0,  // "simulate": internal emulator-only value
  ROGUE_IO_VARIABLE  = 1,  // any other text: named variable, driven from sketch/serial
  ROGUE_IO_PIN       = 2,  // "GPIO<n>": ESP32 GPIO pin
  ROGUE_IO_DISABLED  = 3,  // "disabled": held at 0/off, commands ignored
};

struct RogueIoAssignment {
  RogueIoMode mode;
  int8_t pin;                                   // -1 unless mode is ROGUE_IO_PIN
  char variable[ROGUE_VARIABLE_NAME_MAX + 1];   // empty unless mode is ROGUE_IO_VARIABLE
};

struct RogueSettings {
  uint8_t source_address;
  uint8_t tank1_percent;
  uint8_t tank2_percent;
  uint32_t serial_prefix;  // this device's serial (fixed on a real Rogue); also selects
  uint16_t serial_suffix;  // its name record in Object 2

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
    case ROGUE_IO_DISABLED:  return "disabled";
    default:                 return "unknown";
  }
}

static inline void rogue_io_set_simulated(RogueIoAssignment &a) {
  a.mode = ROGUE_IO_SIMULATED;
  a.pin = -1;
  a.variable[0] = '\0';
}

// Parses assignment text into mode/pin/variable. Returns false for malformed text,
// leaving the assignment untouched. GPIO numbers are only range-checked here; the
// sketch checks whether a pin is usable for a given tank/input/output.
static inline bool rogue_io_parse(String text, RogueIoAssignment &a) {
  text.trim();
  String lower = text;
  lower.toLowerCase();
  RogueIoAssignment out;
  rogue_io_set_simulated(out);

  if (lower.startsWith("gpio")) {
    const String num = text.substring(4);
    if (num.length() == 0 || num.length() > 2) return false;
    for (unsigned i = 0; i < num.length(); i++) {
      if (!isDigit(num[i])) return false;
    }
    out.mode = ROGUE_IO_PIN;
    out.pin = (int8_t) num.toInt();
  } else if (lower == "simulate" || lower == "simulated") {
    // already simulated
  } else if (lower == "disabled" || lower == "disable") {
    out.mode = ROGUE_IO_DISABLED;
  } else {
    if (text.length() == 0 || text.length() > ROGUE_VARIABLE_NAME_MAX || text.indexOf(' ') >= 0) return false;
    out.mode = ROGUE_IO_VARIABLE;
    strncpy(out.variable, text.c_str(), sizeof(out.variable) - 1);
    out.variable[sizeof(out.variable) - 1] = '\0';
  }
  a = out;
  return true;
}

// Canonical assignment text, as stored in NVS: "GPIO34", "simulate", "disabled" or the variable name.
static inline String rogue_io_text(const RogueIoAssignment &a) {
  switch (a.mode) {
    case ROGUE_IO_PIN:      return String("GPIO") + String((int) a.pin);
    case ROGUE_IO_VARIABLE: return String(a.variable);
    case ROGUE_IO_DISABLED: return String("disabled");
    default:                return String("simulate");
  }
}

// Human-readable assignment for Serial Monitor output.
static inline String rogue_io_describe(const RogueIoAssignment &a) {
  if (a.mode == ROGUE_IO_VARIABLE) return String("variable ") + a.variable;
  if (a.mode == ROGUE_IO_PIN) return rogue_io_text(a);
  return String(rogue_io_mode_name(a.mode));
}

static inline const char *rogue_default_tank(uint8_t tank) {
  switch (tank) {
    case 1: return defaults.tank1;
    case 2: return defaults.tank2;
    default: return "simulate";
  }
}

static inline const char *rogue_default_input(uint8_t input) {
  switch (input) {
    case 1: return defaults.input_1;
    case 2: return defaults.input_2;
    case 3: return defaults.input_3;
    case 4: return defaults.input_4;
    case 5: return defaults.input_5;
    case 6: return defaults.input_6;
    case 7: return defaults.input_7;
    case 8: return defaults.input_8;
    default: return "simulate";
  }
}

static inline const char *rogue_default_output(uint8_t output) {
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
    default: return "simulate";
  }
}

static inline void rogue_io_from_pref(RogueIoAssignment &a, const char *kind, uint8_t n, const char *text) {
  rogue_io_set_simulated(a);
  if (!rogue_io_parse(text, a)) {
    Serial.printf("RoguePreferences.h: invalid %s %u assignment \"%s\", using simulate\n", kind, (unsigned) n, text);
  }
}

static inline void rogue_settings_defaults(RogueSettings &s) {
  s.source_address = defaults.source_address;
  s.tank1_percent = defaults.tank1_percent;
  s.tank2_percent = defaults.tank2_percent;
  s.serial_prefix = defaults.serial_prefix;
  s.serial_suffix = defaults.serial_suffix;

  rogue_io_set_simulated(s.tanks[0]);
  rogue_io_set_simulated(s.inputs[0]);
  rogue_io_set_simulated(s.outputs[0]);
  for (uint8_t tank = 1; tank <= ROGUE_TANK_COUNT; tank++) rogue_io_from_pref(s.tanks[tank], "tank", tank, rogue_default_tank(tank));
  for (uint8_t input = 1; input <= ROGUE_INPUT_COUNT; input++) rogue_io_from_pref(s.inputs[input], "input", input, rogue_default_input(input));
  for (uint8_t output = 1; output <= ROGUE_OUTPUT_COUNT; output++) rogue_io_from_pref(s.outputs[output], "output", output, rogue_default_output(output));
}

static inline void rogue_io_sanitize(RogueIoAssignment &a) {
  if ((uint8_t) a.mode > (uint8_t) ROGUE_IO_DISABLED) a.mode = ROGUE_IO_SIMULATED;
  a.variable[sizeof(a.variable) - 1] = '\0';
  if (a.mode == ROGUE_IO_VARIABLE && a.variable[0] == '\0') a.mode = ROGUE_IO_SIMULATED;
  if (a.mode != ROGUE_IO_PIN) a.pin = -1;
  if (a.mode != ROGUE_IO_VARIABLE) a.variable[0] = '\0';
}

static inline void rogue_settings_sanitize(RogueSettings &s) {
  if (s.source_address == 0x00 || s.source_address == 0xFF) s.source_address = 0x36;
  if (s.tank1_percent > 100) s.tank1_percent = 100;
  if (s.tank2_percent > 100) s.tank2_percent = 100;

  for (uint8_t tank = 1; tank <= ROGUE_TANK_COUNT; tank++) rogue_io_sanitize(s.tanks[tank]);
  for (uint8_t input = 1; input <= ROGUE_INPUT_COUNT; input++) rogue_io_sanitize(s.inputs[input]);
  for (uint8_t output = 1; output <= ROGUE_OUTPUT_COUNT; output++) rogue_io_sanitize(s.outputs[output]);
}

static inline String rogue_pref_key(const char *prefix, uint8_t n, const char *suffix) {
  char key[12];
  snprintf(key, sizeof(key), "%s%u%s", prefix, (unsigned) n, suffix);
  return String(key);
}

// Older builds stored separate mode ("t1m": 0 simulated, 1 variable, 2 pin) and pin ("t1p")
// keys. They are converted to assignment text once, then removed from NVS.
static inline void rogue_io_migrate_legacy(Preferences &prefs, RogueIoAssignment &a, const String &key,
                                           const char *legacy_prefix, const char *kind, uint8_t n) {
  const String mode_key = rogue_pref_key(legacy_prefix, n, "m");
  const String pin_key = rogue_pref_key(legacy_prefix, n, "p");
  if (!prefs.isKey(mode_key.c_str())) return;
  const uint8_t legacy_mode = prefs.getUChar(mode_key.c_str(), 0);
  const int8_t legacy_pin = prefs.getChar(pin_key.c_str(), -1);
  if (!prefs.isKey(key.c_str())) {
    RogueIoAssignment migrated;
    rogue_io_set_simulated(migrated);
    if (legacy_mode == 2 && legacy_pin >= 0) {
      migrated.mode = ROGUE_IO_PIN;
      migrated.pin = legacy_pin;
    } else if (legacy_mode == 1) {
      migrated.mode = ROGUE_IO_VARIABLE;
      snprintf(migrated.variable, sizeof(migrated.variable), "%s%u", kind, (unsigned) n);
    }
    prefs.putString(key.c_str(), rogue_io_text(migrated));
    Serial.printf("NVS: migrated %s %u to \"%s\"\n", kind, (unsigned) n, rogue_io_text(migrated).c_str());
  }
  prefs.remove(mode_key.c_str());
  if (prefs.isKey(pin_key.c_str())) prefs.remove(pin_key.c_str());
}

static inline void rogue_io_load(Preferences &prefs, RogueIoAssignment &a, const char *key_prefix,
                                 const char *legacy_prefix, const char *kind, uint8_t n) {
  const String key = rogue_pref_key(key_prefix, n, "");
  rogue_io_migrate_legacy(prefs, a, key, legacy_prefix, kind, n);
  if (!prefs.isKey(key.c_str())) return;
  const String text = prefs.getString(key.c_str(), rogue_io_text(a));
  if (!rogue_io_parse(text, a)) {
    Serial.printf("NVS: ignoring invalid assignment %s=\"%s\"\n", key.c_str(), text.c_str());
  }
}

static inline void rogue_settings_load(Preferences &prefs, RogueSettings &s) {
  prefs.begin("rogueemu", false);
  rogue_settings_defaults(s);

  s.source_address = prefs.getUChar("sa", s.source_address);
  s.tank1_percent = prefs.getUChar("t1", s.tank1_percent);
  s.tank2_percent = prefs.getUChar("t2", s.tank2_percent);
  s.serial_prefix = prefs.getUInt("sp", s.serial_prefix);
  s.serial_suffix = prefs.getUShort("ss", s.serial_suffix);
  // The product name now comes from Object 2; drop a name saved by older builds.
  if (prefs.isKey("name")) prefs.remove("name");

  for (uint8_t tank = 1; tank <= ROGUE_TANK_COUNT; tank++) rogue_io_load(prefs, s.tanks[tank], "ta", "t", "tank", tank);
  for (uint8_t input = 1; input <= ROGUE_INPUT_COUNT; input++) rogue_io_load(prefs, s.inputs[input], "ia", "i", "input", input);
  for (uint8_t output = 1; output <= ROGUE_OUTPUT_COUNT; output++) rogue_io_load(prefs, s.outputs[output], "oa", "o", "output", output);

  rogue_settings_sanitize(s);
}

static inline void rogue_settings_save(Preferences &prefs, RogueSettings &s) {
  rogue_settings_sanitize(s);

  prefs.putUChar("sa", s.source_address);
  prefs.putUChar("t1", s.tank1_percent);
  prefs.putUChar("t2", s.tank2_percent);
  prefs.putUInt("sp", s.serial_prefix);
  prefs.putUShort("ss", s.serial_suffix);

  for (uint8_t tank = 1; tank <= ROGUE_TANK_COUNT; tank++) prefs.putString(rogue_pref_key("ta", tank, "").c_str(), rogue_io_text(s.tanks[tank]));
  for (uint8_t input = 1; input <= ROGUE_INPUT_COUNT; input++) prefs.putString(rogue_pref_key("ia", input, "").c_str(), rogue_io_text(s.inputs[input]));
  for (uint8_t output = 1; output <= ROGUE_OUTPUT_COUNT; output++) prefs.putString(rogue_pref_key("oa", output, "").c_str(), rogue_io_text(s.outputs[output]));
}

static inline void rogue_settings_reset(Preferences &prefs, RogueSettings &s) {
  prefs.clear();
  rogue_settings_defaults(s);
  rogue_settings_save(prefs, s);
}
