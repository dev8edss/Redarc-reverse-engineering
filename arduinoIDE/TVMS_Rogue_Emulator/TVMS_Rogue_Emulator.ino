/*
  REDARC TVMS Rogue Emulator - Arduino IDE standalone port

  This sketch emulates only a TVMS Rogue / DPDM CAN node.
  It does not use ESPHome or Home Assistant.

  Target:
    ESP32 + CAN transceiver/base, e.g. M5Stack Atom Lite + Atomic CAN Base
    CAN TX GPIO22, CAN RX GPIO19, 250 kbit/s, extended IDs
*/

#include <Arduino.h>
#include <driver/twai.h>
#include <pgmspace.h>

#include "RogueObject2.h"
#include "RoguePreferences.h"

static constexpr gpio_num_t CAN_TX_PIN = GPIO_NUM_22;
static constexpr gpio_num_t CAN_RX_PIN = GPIO_NUM_19;

static constexpr uint32_t ID_LOAD_DISCONNECT_CONFIG = 0x13F10800UL;
static constexpr uint32_t ID_CHANNEL_STATUS         = 0x1BFD0000UL;
static constexpr uint32_t ID_SENSOR_VALUES          = 0x1BFD0200UL;
static constexpr uint32_t ID_CHANNEL_LABEL          = 0x17FD0400UL;
static constexpr uint32_t ID_ALARM_CONFIG           = 0x17FD0600UL;
static constexpr uint32_t ID_ALARM_STATUS           = 0x17FD0700UL;
static constexpr uint32_t ID_ACTIVE_CHANNELS        = 0x17FD0800UL;
static constexpr uint32_t ID_CHANNEL_DETAILS        = 0x17FD0A00UL;
static constexpr uint32_t ID_ANALOG_SCALING         = 0x17FD0C00UL;
static constexpr uint32_t ID_OUTPUT_CAPABILITIES    = 0x17FD0E00UL;
static constexpr uint32_t ID_DIGITAL_INPUT_CONFIG   = 0x17FD1000UL;
static constexpr uint32_t ID_OUTPUT_LEVELS          = 0x1BFD1200UL;
static constexpr uint32_t ID_OUTPUT_ACTIVITY        = 0x1BFD1400UL;
static constexpr uint32_t ID_NODE_FIRMWARE          = 0x17F40000UL;
static constexpr uint32_t ID_NODE_PRODUCT_NAME      = 0x17F40300UL;
static constexpr uint32_t ID_NODE_SERIAL_INFO       = 0x17F40400UL;
static constexpr uint32_t ID_NODE_DEVICE_ID         = 0x17F40500UL;
static constexpr uint32_t ID_DIRECT_ACK_BASE        = 0x0F040000UL;
static constexpr uint32_t ID_SERVICE_DATA_BASE      = 0x02810000UL;
static constexpr uint32_t ID_SERVICE_TRAILER_BASE   = 0x02840000UL;

static constexpr uint16_t SERVICE_DGN_REQUEST       = 0x0F03;
static constexpr uint16_t SERVICE_DIRECT_COMMAND    = 0x0F00;
static constexpr uint16_t SERVICE_LEGACY_DIM        = 0x0F05;
static constexpr uint16_t SERVICE_OBJECT_PREFIX     = 0x0E00;

static constexpr uint8_t MAIN_CONFIGURATION_OBJECT  = 0x02;
static constexpr uint8_t CHANNEL_MASTER             = 0x0B;
static constexpr uint8_t CHANNEL_OUTPUT_1           = 0x0C;
static constexpr uint8_t CHANNEL_OUTPUT_10          = 0x15;

static constexpr uint32_t IDENTITY_INTERVAL_MS      = 1000UL;
static constexpr uint32_t STATUS_INTERVAL_MS        = 1000UL;
static constexpr uint32_t IO_POLL_INTERVAL_MS       = 50UL;
static constexpr uint32_t HOLD_DIM_STEP_MS          = 100UL;
static constexpr uint8_t HOLD_DIM_STEP_PERCENT      = 2;
static constexpr uint32_t CAN_HEALTH_INTERVAL_MS    = 100UL;
static constexpr uint32_t CAN_RESTART_INTERVAL_MS   = 5000UL;
static constexpr uint32_t CAN_TX_REPORT_INTERVAL_MS = 5000UL;

Preferences prefs;
RogueSettings settings;

static bool can_installed = false;
static bool can_running = false;
static uint32_t last_can_health_ms = 0;
static uint32_t last_can_restart_ms = 0;
static uint32_t last_tx_report_ms = 0;
static uint32_t tx_fail_count = 0;
static esp_err_t last_tx_error = ESP_OK;

static uint8_t selected_object = 0xFF;
static uint8_t output_levels[ROGUE_OUTPUT_COUNT + 1] = {0};
static bool input_states[ROGUE_INPUT_COUNT + 1] = {false};
static bool input_variable_state[ROGUE_INPUT_COUNT + 1] = {false};
static uint8_t tank_variable_percent[ROGUE_TANK_COUNT + 1] = {0};
static bool master_state = false;
static uint16_t input_voltage_mv = 13500;
static uint16_t input_current_ma = 2500;
static int8_t hold_dim_direction[ROGUE_OUTPUT_COUNT + 1] = {0};
static uint32_t hold_dim_last_step_ms[ROGUE_OUTPUT_COUNT + 1] = {0};
static uint32_t last_identity_ms = 0;
static uint32_t last_status_ms = 0;
static uint32_t last_io_poll_ms = 0;

uint32_t with_sa(uint32_t base_id) {
  return (base_id & 0x1FFFFF00UL) | settings.source_address;
}

uint16_t u16_le(const uint8_t *d) {
  return (uint16_t) d[0] | ((uint16_t) d[1] << 8);
}

uint32_t u32_le(const uint8_t *d) {
  return (uint32_t) d[0] | ((uint32_t) d[1] << 8) |
         ((uint32_t) d[2] << 16) | ((uint32_t) d[3] << 24);
}

uint8_t clamp_percent(float value) {
  if (isnan(value) || value <= 0.0f) return 0;
  if (value >= 100.0f) return 100;
  return (uint8_t) lroundf(value);
}

uint32_t parse_u32(const String &text) {
  String s = text;
  s.trim();
  return strtoul(s.c_str(), nullptr, 0);
}

uint32_t crc32c_update_byte(uint32_t crc, uint8_t b) {
  crc ^= b;
  for (uint8_t bit = 0; bit < 8; bit++) {
    crc = (crc & 1U) ? ((crc >> 1) ^ 0x82F63B78UL) : (crc >> 1);
  }
  return crc;
}

uint32_t crc32c(const uint8_t *data, size_t len) {
  uint32_t crc = 0xFFFFFFFFUL;
  for (size_t i = 0; i < len; i++) crc = crc32c_update_byte(crc, data[i]);
  return ~crc;
}

uint32_t object2_stored_crc() {
  return (uint32_t) rogue_object2_byte(8) |
         ((uint32_t) rogue_object2_byte(9) << 8) |
         ((uint32_t) rogue_object2_byte(10) << 16) |
         ((uint32_t) rogue_object2_byte(11) << 24);
}

uint32_t object2_declared_length() {
  return (uint32_t) rogue_object2_byte(4) |
         ((uint32_t) rogue_object2_byte(5) << 8) |
         ((uint32_t) rogue_object2_byte(6) << 16) |
         ((uint32_t) rogue_object2_byte(7) << 24);
}

uint32_t object2_calculated_crc_zeroed() {
  uint32_t crc = 0xFFFFFFFFUL;
  for (uint32_t i = 0; i < ROGUE_OBJECT2_SIZE; i++) {
    uint8_t b = rogue_object2_byte(i);
    if (i >= 8 && i <= 11) b = 0x00;
    crc = crc32c_update_byte(crc, b);
  }
  return ~crc;
}

void object2_self_test() {
  const uint32_t declared_len = object2_declared_length();
  const uint32_t stored_crc = object2_stored_crc();
  const uint32_t calc_crc = object2_calculated_crc_zeroed();
  Serial.printf("Object2 size=%lu declared_len=%lu stored_crc=0x%08lX calc_crc=0x%08lX %s\n",
                (unsigned long) ROGUE_OBJECT2_SIZE,
                (unsigned long) declared_len,
                (unsigned long) stored_crc,
                (unsigned long) calc_crc,
                (stored_crc == calc_crc && declared_len == ROGUE_OBJECT2_SIZE) ? "OK" : "FAIL");
}

uint8_t &tank_percent_ref(uint8_t tank) {
  return tank == 1 ? settings.tank1_percent : settings.tank2_percent;
}

void save_settings() {
  rogue_settings_sanitize(settings);
  rogue_settings_save(prefs, settings);
  Serial.println("Settings saved to NVS");
}

void reset_settings_to_defaults() {
  rogue_settings_reset(prefs, settings);
  memset(input_states, 0, sizeof(input_states));
  memset(input_variable_state, 0, sizeof(input_variable_state));
  memset(output_levels, 0, sizeof(output_levels));
  memset(tank_variable_percent, 0, sizeof(tank_variable_percent));
  tank_variable_percent[1] = settings.tank1_percent;
  tank_variable_percent[2] = settings.tank2_percent;
  Serial.println("Settings restored to defaults");
}

// Returns nullptr when the GPIO can be used this way, otherwise why it cannot.
const char *gpio_pin_problem(int8_t pin, bool needs_output, bool needs_adc) {
  if (pin < 0 || !GPIO_IS_VALID_GPIO(pin)) return "is not a GPIO on this chip";
#if CONFIG_IDF_TARGET_ESP32
  if (pin >= 6 && pin <= 11) return "is reserved for the SPI flash";
#endif
  if (pin == CAN_TX_PIN || pin == CAN_RX_PIN) return "is used by CAN";
  if (needs_output && !GPIO_IS_VALID_OUTPUT_GPIO(pin)) return "is input-only";
  if (needs_adc && digitalPinToAnalogChannel(pin) < 0) return "is not an ADC pin";
  return nullptr;
}

// True when a tank/input/output other than `slot` is already bound to this GPIO.
bool gpio_pin_taken(int8_t pin, const RogueIoAssignment *slot) {
  for (uint8_t i = 1; i <= ROGUE_TANK_COUNT; i++) if (&settings.tanks[i] != slot && settings.tanks[i].mode == ROGUE_IO_PIN && settings.tanks[i].pin == pin) return true;
  for (uint8_t i = 1; i <= ROGUE_INPUT_COUNT; i++) if (&settings.inputs[i] != slot && settings.inputs[i].mode == ROGUE_IO_PIN && settings.inputs[i].pin == pin) return true;
  for (uint8_t i = 1; i <= ROGUE_OUTPUT_COUNT; i++) if (&settings.outputs[i] != slot && settings.outputs[i].mode == ROGUE_IO_PIN && settings.outputs[i].pin == pin) return true;
  return false;
}

// Checks `candidate` as the new assignment for `slot`. Returns nullptr when usable.
const char *io_assignment_problem(const RogueIoAssignment &candidate, const RogueIoAssignment *slot, bool needs_output, bool needs_adc) {
  if (candidate.mode != ROGUE_IO_PIN) return nullptr;
  const char *problem = gpio_pin_problem(candidate.pin, needs_output, needs_adc);
  if (problem == nullptr && gpio_pin_taken(candidate.pin, slot)) problem = "is already assigned to another channel";
  return problem;
}

// Returns a GPIO to high-impedance input when a channel stops using it, so an
// output that was driven HIGH does not stay on after being reassigned.
void release_io_pin(const RogueIoAssignment &old, bool was_output) {
  if (old.mode != ROGUE_IO_PIN || old.pin < 0) return;
  if (was_output) digitalWrite((uint8_t) old.pin, LOW);
  pinMode((uint8_t) old.pin, INPUT);
}

void disable_unusable_assignment(RogueIoAssignment &a, const char *kind, uint8_t n, bool needs_output, bool needs_adc) {
  const char *problem = io_assignment_problem(a, &a, needs_output, needs_adc);
  if (problem == nullptr) return;
  Serial.printf("%s %u: GPIO%d %s, channel disabled\n", kind, (unsigned) n, a.pin, problem);
  a.mode = ROGUE_IO_DISABLED;
  a.pin = -1;
}

void validate_io_assignments() {
  for (uint8_t i = 1; i <= ROGUE_TANK_COUNT; i++) disable_unusable_assignment(settings.tanks[i], "Tank", i, false, true);
  for (uint8_t i = 1; i <= ROGUE_INPUT_COUNT; i++) disable_unusable_assignment(settings.inputs[i], "Input", i, false, false);
  for (uint8_t i = 1; i <= ROGUE_OUTPUT_COUNT; i++) disable_unusable_assignment(settings.outputs[i], "Output", i, true, false);
}

uint8_t analog_raw_to_percent(int raw) {
  if (raw <= 0) return 0;
  if (raw >= 4095) return 100;
  return (uint8_t) ((raw * 100L + 2047L) / 4095L);
}

void apply_output_assignment(uint8_t output) {
  if (output < 1 || output > ROGUE_OUTPUT_COUNT) return;
  const RogueIoAssignment &a = settings.outputs[output];
  if (a.mode == ROGUE_IO_PIN) {
    digitalWrite((uint8_t) a.pin, output_levels[output] > 0 ? HIGH : LOW);
  }
}

void configure_io_pins() {
  for (uint8_t tank = 1; tank <= ROGUE_TANK_COUNT; tank++) {
    const RogueIoAssignment &a = settings.tanks[tank];
    if (a.mode == ROGUE_IO_PIN) {
      pinMode((uint8_t) a.pin, INPUT);
      tank_percent_ref(tank) = analog_raw_to_percent(analogRead((uint8_t) a.pin));
    } else if (a.mode == ROGUE_IO_DISABLED) {
      tank_percent_ref(tank) = 0;
    }
  }
  for (uint8_t input = 1; input <= ROGUE_INPUT_COUNT; input++) {
    const RogueIoAssignment &a = settings.inputs[input];
    if (a.mode == ROGUE_IO_PIN) {
      pinMode((uint8_t) a.pin, INPUT);
      input_states[input] = digitalRead((uint8_t) a.pin) == HIGH;
    } else if (a.mode == ROGUE_IO_DISABLED) {
      input_states[input] = false;
    }
  }
  for (uint8_t output = 1; output <= ROGUE_OUTPUT_COUNT; output++) {
    const RogueIoAssignment &a = settings.outputs[output];
    if (a.mode == ROGUE_IO_DISABLED) {
      output_levels[output] = 0;
      hold_dim_direction[output] = 0;
    } else if (a.mode == ROGUE_IO_PIN) {
      pinMode((uint8_t) a.pin, OUTPUT);
      apply_output_assignment(output);
    }
  }
}

bool poll_tank_assignments() {
  bool changed = false;
  for (uint8_t tank = 1; tank <= ROGUE_TANK_COUNT; tank++) {
    uint8_t next = tank_percent_ref(tank);
    const RogueIoAssignment &a = settings.tanks[tank];
    if (a.mode == ROGUE_IO_PIN) {
      next = analog_raw_to_percent(analogRead((uint8_t) a.pin));
    } else if (a.mode == ROGUE_IO_VARIABLE) {
      next = tank_variable_percent[tank];
    } else if (a.mode == ROGUE_IO_DISABLED) {
      next = 0;
    }
    uint8_t &target = tank_percent_ref(tank);
    if (target != next) {
      target = next;
      changed = true;
      Serial.printf("Tank %u changed to %u%% from %s\n", (unsigned) tank, (unsigned) next, rogue_io_describe(a).c_str());
    }
  }
  return changed;
}

bool poll_input_assignments() {
  bool changed = false;
  for (uint8_t input = 1; input <= ROGUE_INPUT_COUNT; input++) {
    bool next = input_states[input];
    const RogueIoAssignment &a = settings.inputs[input];
    if (a.mode == ROGUE_IO_PIN) {
      next = digitalRead((uint8_t) a.pin) == HIGH;
    } else if (a.mode == ROGUE_IO_VARIABLE) {
      next = input_variable_state[input];
    } else if (a.mode == ROGUE_IO_DISABLED) {
      next = false;
    }
    if (input_states[input] != next) {
      input_states[input] = next;
      changed = true;
      Serial.printf("Input %u changed to %s from %s\n", (unsigned) input, next ? "ON" : "OFF", rogue_io_describe(a).c_str());
    }
  }
  return changed;
}

void send_frame(uint32_t id, const uint8_t *data, uint8_t len) {
  if (!can_running) return;
  twai_message_t msg = {};
  msg.identifier = id & 0x1FFFFFFFUL;
  msg.extd = 1;
  msg.rtr = 0;
  msg.data_length_code = len > 8 ? 8 : len;
  for (uint8_t i = 0; i < msg.data_length_code; i++) msg.data[i] = data[i];
  // Never block: if the TX queue is full (e.g. no other node is ACKing), drop the frame.
  // Failures are counted and reported periodically by maintain_can().
  esp_err_t err = twai_transmit(&msg, 0);
  if (err != ESP_OK) {
    tx_fail_count++;
    last_tx_error = err;
  }
}

void send_frame8(uint32_t id, uint8_t a, uint8_t b, uint8_t c, uint8_t d, uint8_t e, uint8_t f, uint8_t g, uint8_t h) {
  uint8_t data[8] = {a, b, c, d, e, f, g, h};
  send_frame(id, data, 8);
}

void send_direct_ack(uint8_t requester, uint8_t command) {
  const uint32_t id = ID_DIRECT_ACK_BASE | ((uint32_t) requester << 8) | settings.source_address;
  send_frame8(id, 0x01, command, 0x00, 0xFF, 0x00, 0xFF, 0xFF, 0xFF);
}

void set_output_level(uint8_t output, uint8_t percent, const char *origin) {
  if (output < 1 || output > ROGUE_OUTPUT_COUNT) return;
  if (percent > 100) percent = 100;
  if (settings.outputs[output].mode == ROGUE_IO_DISABLED && percent > 0) {
    Serial.printf("%s: output %u is disabled, ignored\n", origin, output);
    return;
  }
  output_levels[output] = percent;
  apply_output_assignment(output);
  Serial.printf("%s set output %u to %u%% (%s)\n", origin, output, percent, rogue_io_describe(settings.outputs[output]).c_str());
}

void set_tank_percent(uint8_t tank, uint8_t percent, const char *origin, bool persist) {
  if (tank < 1 || tank > ROGUE_TANK_COUNT) return;
  if (settings.tanks[tank].mode == ROGUE_IO_DISABLED) {
    Serial.printf("%s: tank %u is disabled, ignored\n", origin, (unsigned) tank);
    return;
  }
  if (percent > 100) percent = 100;
  tank_percent_ref(tank) = percent;
  tank_variable_percent[tank] = percent;
  if (persist) save_settings();
  Serial.printf("%s set tank %u to %u%% (%s)\n", origin, (unsigned) tank, (unsigned) percent, rogue_io_describe(settings.tanks[tank]).c_str());
}

bool set_input_variable(uint8_t input, bool on, const char *origin) {
  if (input < 1 || input > ROGUE_INPUT_COUNT) return false;
  if (settings.inputs[input].mode == ROGUE_IO_DISABLED) {
    Serial.printf("%s: input %u is disabled, ignored\n", origin, (unsigned) input);
    return false;
  }
  input_variable_state[input] = on;
  if (settings.inputs[input].mode != ROGUE_IO_PIN) input_states[input] = on;
  Serial.printf("%s set input %u %s (%s)\n", origin, (unsigned) input, on ? "ON" : "OFF", rogue_io_describe(settings.inputs[input]).c_str());
  return true;
}

void set_master(bool on, const char *origin) {
  master_state = on;
  Serial.printf("%s set master %s\n", origin, on ? "ON" : "OFF");
  if (!on) {
    for (uint8_t output = 1; output <= ROGUE_OUTPUT_COUNT; output++) set_output_level(output, 0, "Master OFF");
  }
}

void send_load_disconnect_config() { send_frame8(with_sa(ID_LOAD_DISCONNECT_CONFIG), 0xEC, 0xD8, 0x27, 0x50, 0x2D, 0x17, 0x3D, 0x00); }

void send_channel_status() {
  uint8_t page1[8] = {0x01};
  for (uint8_t input = 1; input <= 7; input++) page1[input] = input_states[input] ? 1 : 0;
  send_frame(with_sa(ID_CHANNEL_STATUS), page1, 8);
  send_frame8(with_sa(ID_CHANNEL_STATUS), 0x08, input_states[8] ? 1 : 0, 0xFF, 0xFF, master_state ? 1 : 0, output_levels[1] > 0 ? 1 : 0, output_levels[2] > 0 ? 1 : 0, output_levels[3] > 0 ? 1 : 0);
  send_frame8(with_sa(ID_CHANNEL_STATUS), 0x0F, output_levels[4] > 0 ? 1 : 0, output_levels[5] > 0 ? 1 : 0, output_levels[6] > 0 ? 1 : 0, output_levels[7] > 0 ? 1 : 0, output_levels[8] > 0 ? 1 : 0, output_levels[9] > 0 ? 1 : 0, output_levels[10] > 0 ? 1 : 0);
  send_frame8(with_sa(ID_CHANNEL_STATUS), 0x18, 0xF8, 0xF8, 0xF8, 0xF8, 0xF8, 0xF8, 0xF8);
  send_frame8(with_sa(ID_CHANNEL_STATUS), 0x1F, 0xF8, 0xF8, 0xF8, 0xFF, 0xFF, 0xFF, 0xFF);
}

void send_sensor_values() {
  send_frame8(with_sa(ID_SENSOR_VALUES), 0x09, settings.tank1_percent, settings.tank2_percent, 0x00, 0x00, 0xFF, 0xFF, 0xFF);
  send_frame8(with_sa(ID_SENSOR_VALUES), 0x16, (uint8_t) (input_voltage_mv & 0xFF), (uint8_t) ((input_voltage_mv >> 8) & 0xFF), (uint8_t) (input_current_ma & 0xFF), (uint8_t) ((input_current_ma >> 8) & 0xFF), 0xFF, 0xFF, 0xFF);
}

void send_output_levels() {
  send_frame8(with_sa(ID_OUTPUT_LEVELS), 0x0C, output_levels[1], output_levels[2], output_levels[3], output_levels[4], output_levels[5], output_levels[6], output_levels[7]);
  send_frame8(with_sa(ID_OUTPUT_LEVELS), 0x13, output_levels[8], output_levels[9], output_levels[10], 0xFF, 0xFF, 0x00, 0x00);
  send_frame8(with_sa(ID_OUTPUT_LEVELS), 0x1A, 0, 0, 0, 0, 0, 0, 0);
  send_frame8(with_sa(ID_OUTPUT_LEVELS), 0x21, 0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF);
}

void send_output_activity() {
  send_frame8(with_sa(ID_OUTPUT_ACTIVITY), 0x0C, hold_dim_direction[1] ? 0x02 : 0x00, hold_dim_direction[2] ? 0x02 : 0x00, hold_dim_direction[3] ? 0x02 : 0x00, hold_dim_direction[4] ? 0x02 : 0x00, hold_dim_direction[5] ? 0x02 : 0x00, hold_dim_direction[6] ? 0x02 : 0x00, hold_dim_direction[7] ? 0x02 : 0x00);
  send_frame8(with_sa(ID_OUTPUT_ACTIVITY), 0x13, hold_dim_direction[8] ? 0x02 : 0x00, hold_dim_direction[9] ? 0x02 : 0x00, hold_dim_direction[10] ? 0x02 : 0x00, 0xFF, 0xFF, 0xFF, 0xFF);
}

void send_active_channels() { send_frame8(with_sa(ID_ACTIVE_CHANNELS), 0x21, 0xFF, 0xFF, 0x1E, 0xFF, 0xFF, 0xFF, 0xFF); }
uint8_t output_capability(uint8_t output) { static const uint8_t caps[10] = {0x83,0x83,0x83,0x83,0x83,0x83,0x83,0x01,0x03,0x03}; return output >= 1 && output <= 10 ? caps[output - 1] : 0; }
void send_output_capabilities() { for (uint8_t i = 1; i <= 10; i++) send_frame8(with_sa(ID_OUTPUT_CAPABILITIES), CHANNEL_OUTPUT_1 + i - 1, output_capability(i), 0, 0, 0, 0, 0, 0); }

void send_label_chunk(uint8_t channel, uint8_t segment, const char *label) {
  uint8_t data[8] = {channel, segment, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  const size_t len = strlen(label);
  for (uint8_t i = 0; i < 6; i++) { size_t pos = (size_t) segment * 6U + i; if (pos < len) data[2 + i] = (uint8_t) label[pos]; }
  send_frame(with_sa(ID_CHANNEL_LABEL), data, 8);
}
void send_channel_label(uint8_t channel, const char *label) { const size_t len = strlen(label); const uint8_t segments = (uint8_t) (len / 6U + 1U); for (uint8_t seg = 0; seg < segments; seg++) send_label_chunk(channel, seg, label); }
void send_channel_labels() {
  static const char *const labels[34] = {"", "Left", "Strip", "Dome", "Digital Input 4", "Digital Input 5", "Digital Input 6", "Digital Input 7", "Digital Input 8", "Rear", "Front", "Master", "Left", "Right", "Rear", "Kitchen", "Handle", "LED", "Dome Light", "Lights", "Amber lights", "Amber Kitchen", "Rogue Input Voltage", "Input Current", "Remote Input  1", "Remote Input  2", "Remote Input  3", "Remote Input  4", "Remote Input  5", "Remote Input  6", "Remote Input  7", "Remote Input  8", "Remote Input  9", "Remote Input  10"};
  for (uint8_t ch = 1; ch <= 33; ch++) send_channel_label(ch, labels[ch]);
}

struct ChannelDetail { uint8_t channel; uint8_t category; uint16_t subtype; uint16_t icon; uint8_t enabled; uint8_t present; };
void send_channel_details() {
  const ChannelDetail details[] = {{1,0x00,0,0x003B,1,1},{2,0x00,0,0,1,1},{3,0x00,0,0,1,1},{4,0x00,0,0,1,1},{5,0x00,0,0,1,1},{6,0x00,0,0,1,1},{7,0x00,0,0,1,1},{8,0x00,0,0,1,1},{9,0x0C,0,0x8052,1,1},{10,0x0C,0,0x8146,1,1},{11,0x08,0,0,1,1},{12,0x0A,1,0x0001,1,1},{13,0x0A,1,0x1252,1,1},{14,0x0A,1,0x1000,0,1},{15,0x0A,1,0,1,1},{16,0x0A,1,0,1,1},{17,0x0A,1,0,1,1},{18,0x0A,1,0,1,1},{19,0x0A,0,0,1,1},{20,0x0A,0,0,1,1},{21,0x0A,0,0,1,1},{22,0x02,0,0,1,1},{23,0x02,0,0,1,1},{24,0x0B,0,0,0,1},{25,0x0B,0,0,0,1},{26,0x0B,0,0,0,1},{27,0x0B,0,0,0,1},{28,0x0B,0,0,0,1},{29,0x0B,0,0,0,1},{30,0x0B,0,0,0,1},{31,0x0B,0,0,0,1},{32,0x0B,0,0,0,1},{33,0x0B,0,0,0,1}};
  for (size_t i = 0; i < sizeof(details) / sizeof(details[0]); i++) { const ChannelDetail &d = details[i]; send_frame8(with_sa(ID_CHANNEL_DETAILS), d.channel, d.category, (uint8_t)(d.subtype & 0xFF), (uint8_t)(d.subtype >> 8), (uint8_t)(d.icon & 0xFF), (uint8_t)(d.icon >> 8), d.enabled, d.present); }
}

void send_alarm_config() { send_frame8(with_sa(ID_ALARM_CONFIG),0x09,0,0,0,0,0,0xFF,0xFF); send_frame8(with_sa(ID_ALARM_CONFIG),0x0A,0,0,0,0,0,0xFF,0xFF); send_frame8(with_sa(ID_ALARM_CONFIG),0x16,0,0,0,0,0,0xFF,0xFF); send_frame8(with_sa(ID_ALARM_CONFIG),0x17,0,0x1E,0,0x28,0,0xFF,0xFF); }
void send_alarm_status() { send_frame8(with_sa(ID_ALARM_STATUS),0x09,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF); send_frame8(with_sa(ID_ALARM_STATUS),0x16,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF); }
void send_analog_scaling() { send_frame8(with_sa(ID_ANALOG_SCALING),0x09,0x64,0,0,0,0,0x64,0); send_frame8(with_sa(ID_ANALOG_SCALING),0x0A,0x64,0,0,0,0,0x64,0); send_frame8(with_sa(ID_ANALOG_SCALING),0x16,0x61,0,0,0,0,0x60,0xEA); send_frame8(with_sa(ID_ANALOG_SCALING),0x17,0x61,0,0,0,0,0x60,0xEA); }
void send_digital_input_config() { for (uint8_t ch = 1; ch <= 8; ch++) send_frame8(with_sa(ID_DIGITAL_INPUT_CONFIG), ch, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF); }
void send_all_status() { send_channel_status(); send_sensor_values(); send_output_levels(); send_output_activity(); send_active_channels(); send_output_capabilities(); }

void send_node_firmware() { send_frame8(with_sa(ID_NODE_FIRMWARE),0x43,0x01,0x01,0x04,0,0,0,0); send_frame8(with_sa(ID_NODE_FIRMWARE),0x43,0x01,0,0x04,0,0,0x01,0); }
void send_product_name() { const size_t len = strlen(settings.product_name); uint8_t seg_count = (uint8_t)(len / 7 + 1); for (uint8_t seg = 0; seg < seg_count; seg++) { uint8_t data[8] = {seg,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF}; for (uint8_t i = 0; i < 7; i++) { size_t pos = (size_t) seg * 7 + i; if (pos < len) data[1 + i] = (uint8_t) settings.product_name[pos]; } send_frame(with_sa(ID_NODE_PRODUCT_NAME), data, 8); } }
void send_serial_info() { send_frame8(with_sa(ID_NODE_SERIAL_INFO), (uint8_t)(settings.serial_prefix & 0xFF), (uint8_t)((settings.serial_prefix >> 8) & 0xFF), (uint8_t)((settings.serial_prefix >> 16) & 0xFF), (uint8_t)((settings.serial_prefix >> 24) & 0xFF), (uint8_t)(settings.serial_suffix & 0xFF), (uint8_t)((settings.serial_suffix >> 8) & 0xFF), 0x16, 0); }
void send_device_id() { send_frame8(with_sa(ID_NODE_DEVICE_ID), 0,0,0,0,0, settings.source_address, 0x01, 0); }
void send_identity() { send_node_firmware(); send_product_name(); send_serial_info(); send_device_id(); send_load_disconnect_config(); }

void handle_object_select(const uint8_t *data, uint8_t len) { if (len < 1) return; selected_object = data[0]; Serial.printf("Selected REDARC object %u\n", selected_object); }
void handle_object_read(uint8_t requester, const uint8_t *data, uint8_t len) {
  if (len < 8) return;
  const uint32_t offset = u32_le(data); const uint32_t requested_length = u32_le(data + 4);
  if (requested_length > 8192UL) { Serial.printf("Refusing oversized object read length %lu\n", (unsigned long) requested_length); return; }
  uint8_t *block = (uint8_t *) malloc(requested_length == 0 ? 1 : requested_length); if (block == nullptr) { Serial.println("Object read malloc failed"); return; }
  memset(block, 0xFF, requested_length);
  if (selected_object == MAIN_CONFIGURATION_OBJECT && offset < ROGUE_OBJECT2_SIZE) { const uint32_t available = ROGUE_OBJECT2_SIZE - offset; const uint32_t copy_len = requested_length < available ? requested_length : available; for (uint32_t i = 0; i < copy_len; i++) block[i] = rogue_object2_byte(offset + i); }
  const uint32_t data_id = ID_SERVICE_DATA_BASE | ((uint32_t) requester << 8) | settings.source_address;
  for (uint32_t pos = 0; pos < requested_length; pos += 8) { uint8_t frame[8] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF}; const uint8_t chunk = (uint8_t)((requested_length - pos) < 8UL ? (requested_length - pos) : 8UL); memcpy(frame, block + pos, chunk); send_frame(data_id, frame, 8); }
  const uint32_t block_crc = crc32c(block, requested_length); const uint32_t trailer_id = ID_SERVICE_TRAILER_BASE | ((uint32_t) requester << 8) | settings.source_address;
  send_frame8(trailer_id, (uint8_t)(requested_length & 0xFF), (uint8_t)((requested_length >> 8) & 0xFF), (uint8_t)((requested_length >> 16) & 0xFF), (uint8_t)((requested_length >> 24) & 0xFF), (uint8_t)(block_crc & 0xFF), (uint8_t)((block_crc >> 8) & 0xFF), (uint8_t)((block_crc >> 16) & 0xFF), (uint8_t)((block_crc >> 24) & 0xFF));
  Serial.printf("Object %u read offset=%lu length=%lu crc=0x%08lX\n", selected_object, (unsigned long) offset, (unsigned long) requested_length, (unsigned long) block_crc); free(block);
}

void handle_dgn_request(uint8_t requester, uint16_t dgn) {
  Serial.printf("DGN request from 0x%02X: 0x1%04X\n", requester, dgn);
  switch (dgn) { case 0xF108: send_load_disconnect_config(); return; case 0xF400: send_node_firmware(); return; case 0xF403: send_product_name(); return; case 0xF404: send_serial_info(); return; case 0xF405: send_device_id(); return; case 0xFD00: send_channel_status(); return; case 0xFD02: send_sensor_values(); return; case 0xFD04: send_channel_labels(); return; case 0xFD06: send_alarm_config(); return; case 0xFD07: send_alarm_status(); return; case 0xFD08: send_active_channels(); return; case 0xFD0A: send_channel_details(); return; case 0xFD0C: send_analog_scaling(); return; case 0xFD0E: send_output_capabilities(); return; case 0xFD10: send_digital_input_config(); return; case 0xFD12: send_output_levels(); return; case 0xFD14: send_output_activity(); return; default: Serial.printf("Unhandled DGN 0x1%04X\n", dgn); return; }
}

void handle_direct_command(uint8_t requester, const uint8_t *data, uint8_t len) {
  if (len < 5) return; const uint8_t command = data[0];
  if (command == 0xCB && data[2] == 0xFF) { const uint8_t channel = data[3]; const bool state = data[4] != 0; if (channel == CHANNEL_MASTER) set_master(state, "CAN 0xCB"); else if (channel >= CHANNEL_OUTPUT_1 && channel <= CHANNEL_OUTPUT_10) { const uint8_t output = channel - CHANNEL_MASTER; hold_dim_direction[output] = 0; uint8_t level = state ? output_levels[output] : 0; if (state && level == 0) level = 100; set_output_level(output, level, "CAN 0xCB"); } send_direct_ack(requester, command); send_all_status(); return; }
  if (command == 0x5A && data[1] == 0x01 && data[2] == 0xFF) { const uint8_t channel = data[3]; if (channel >= CHANNEL_OUTPUT_1 && channel <= CHANNEL_OUTPUT_10) { const uint8_t output = channel - CHANNEL_MASTER; hold_dim_direction[output] = 0; set_output_level(output, data[4] > 100 ? 100 : data[4], "CAN 0x5A"); } send_direct_ack(requester, command); send_all_status(); return; }
  Serial.printf("Unsupported command 0x%02X\n", command); send_direct_ack(requester, command);
}

void handle_legacy_dim(const uint8_t *data, uint8_t len) { if (len < 3) return; const uint8_t channel = data[0]; const uint8_t direction = data[2]; if (channel < CHANNEL_OUTPUT_1 || channel > CHANNEL_OUTPUT_10) return; const uint8_t output = channel - CHANNEL_MASTER; if (settings.outputs[output].mode == ROGUE_IO_DISABLED) return; if (direction == 0x01) hold_dim_direction[output] = -1; else if (direction == 0x64) hold_dim_direction[output] = 1; else if (direction == 0xFF) hold_dim_direction[output] = 0; Serial.printf("Hold dim output %u direction 0x%02X\n", output, direction); send_output_activity(); }

void handle_can_message(const twai_message_t &msg) {
  if (!msg.extd || msg.rtr) return; const uint32_t id = msg.identifier & 0x1FFFFFFFUL; const uint16_t service = (uint16_t)((id >> 16) & 0xFFFFUL); const uint8_t destination = (uint8_t)((id >> 8) & 0xFFUL); const uint8_t requester = (uint8_t)(id & 0xFFUL); if (destination != settings.source_address) return;
  if ((service & 0xFF00U) == SERVICE_OBJECT_PREFIX) { const uint8_t object_service = (uint8_t)(service & 0x00FFU); if (object_service == 0x85) { handle_object_select(msg.data, msg.data_length_code); return; } if (object_service == 0x86) { handle_object_read(requester, msg.data, msg.data_length_code); return; } }
  if (service == SERVICE_DGN_REQUEST && msg.data_length_code >= 2) { handle_dgn_request(requester, u16_le(msg.data)); return; }
  if (service == SERVICE_DIRECT_COMMAND) { handle_direct_command(requester, msg.data, msg.data_length_code); return; }
  if (service == SERVICE_LEGACY_DIM) { handle_legacy_dim(msg.data, msg.data_length_code); return; }
}

void receive_can() { if (!can_installed) return; twai_message_t msg = {}; while (twai_receive(&msg, 0) == ESP_OK) handle_can_message(msg); }

void print_io_assignments() {
  Serial.println("Tank assignments:");
  for (uint8_t i = 1; i <= ROGUE_TANK_COUNT; i++) Serial.printf("  tank %u: %s value=%u%%\n", (unsigned) i, rogue_io_describe(settings.tanks[i]).c_str(), tank_percent_ref(i));
  Serial.println("Input assignments:");
  for (uint8_t i = 1; i <= ROGUE_INPUT_COUNT; i++) Serial.printf("  input %u: %s state=%s\n", (unsigned) i, rogue_io_describe(settings.inputs[i]).c_str(), input_states[i] ? "ON" : "OFF");
  Serial.println("Output assignments:");
  for (uint8_t i = 1; i <= ROGUE_OUTPUT_COUNT; i++) Serial.printf("  output %u: %s level=%u%%\n", (unsigned) i, rogue_io_describe(settings.outputs[i]).c_str(), output_levels[i]);
}

void print_status() {
  Serial.printf("SA=0x%02X object=%u object2=%lu bytes master=%s tank1=%u%% tank2=%u%% Vin=%.3fV Iin=%.3fA CAN=%s\n", settings.source_address, selected_object, (unsigned long) ROGUE_OBJECT2_SIZE, master_state ? "ON" : "OFF", settings.tank1_percent, settings.tank2_percent, input_voltage_mv / 1000.0f, input_current_ma / 1000.0f, can_running ? "running" : "down");
  Serial.printf("Serial=%lu-%04u name=\"%s\"\n", (unsigned long) settings.serial_prefix, (unsigned) settings.serial_suffix, settings.product_name);
  for (uint8_t output = 1; output <= ROGUE_OUTPUT_COUNT; output++) Serial.printf("O%u=%u%% ", output, output_levels[output]);
  Serial.println(); print_io_assignments();
}

void print_help() {
  Serial.println("status | io | t1 <0-100> | t2 <0-100> | v <mV> | i <mA> | m <0|1> | o<n> <0-100> | in<n> <0|1>");
  Serial.println("tank <n> <a> | input <n> <a> | output <n> <a>   where <a> = GPIO<n> | simulate | disabled | <variable-name>");
  Serial.println("set <variable-name> <value> | sa <0x01-0xFE> | serial <prefix> <suffix> | name <text>");
  Serial.println("save | defaults | crc | identity | send");
}

// Parses "<word> <n> <GPIO<n>|simulate|disabled|variable-name>".
bool parse_assignment_common(String raw, uint8_t max_channel, uint8_t &channel, RogueIoAssignment &assignment) {
  raw.trim(); int first = raw.indexOf(' '); if (first <= 0) return false; String rest = raw.substring(first + 1); rest.trim(); int second = rest.indexOf(' '); if (second <= 0) return false;
  channel = (uint8_t) rest.substring(0, second).toInt(); if (channel < 1 || channel > max_channel) return false;
  return rogue_io_parse(rest.substring(second + 1), assignment);
}

bool assign_io(RogueIoAssignment &slot, const RogueIoAssignment &next, const char *kind, uint8_t n, bool is_output, bool needs_adc) {
  const char *problem = io_assignment_problem(next, &slot, is_output, needs_adc);
  if (problem != nullptr) { Serial.printf("%s %u not changed: GPIO%d %s\n", kind, (unsigned) n, next.pin, problem); return false; }
  if (!(slot.mode == ROGUE_IO_PIN && next.mode == ROGUE_IO_PIN && slot.pin == next.pin)) release_io_pin(slot, is_output);
  slot = next;
  configure_io_pins(); save_settings(); Serial.printf("%s %u assigned to %s\n", kind, (unsigned) n, rogue_io_describe(slot).c_str()); return true;
}

bool parse_tank_assignment(String raw) {
  uint8_t tank = 0; RogueIoAssignment next;
  if (!parse_assignment_common(raw, ROGUE_TANK_COUNT, tank, next)) return false;
  if (next.mode == ROGUE_IO_VARIABLE) tank_variable_percent[tank] = tank_percent_ref(tank);
  if (assign_io(settings.tanks[tank], next, "Tank", tank, false, true)) send_sensor_values(); return true;
}

bool parse_input_assignment(String raw) {
  uint8_t input = 0; RogueIoAssignment next;
  if (!parse_assignment_common(raw, ROGUE_INPUT_COUNT, input, next)) return false;
  if (next.mode == ROGUE_IO_VARIABLE) input_variable_state[input] = input_states[input];
  if (assign_io(settings.inputs[input], next, "Input", input, false, false)) send_channel_status(); return true;
}

bool parse_output_assignment(String raw) {
  uint8_t output = 0; RogueIoAssignment next;
  if (!parse_assignment_common(raw, ROGUE_OUTPUT_COUNT, output, next)) return false;
  if (assign_io(settings.outputs[output], next, "Output", output, true, false)) { send_output_levels(); send_channel_status(); } return true;
}

// Sets every tank/input/output assigned to the named variable (case-insensitive).
// Tanks and outputs take 0-100, inputs take 0/1.
void set_named_variable(const String &name, const String &value_text) {
  uint8_t matches = 0;
  for (uint8_t i = 1; i <= ROGUE_TANK_COUNT; i++) if (settings.tanks[i].mode == ROGUE_IO_VARIABLE && name.equalsIgnoreCase(settings.tanks[i].variable)) { set_tank_percent(i, clamp_percent(value_text.toFloat()), "Variable", false); matches++; }
  for (uint8_t i = 1; i <= ROGUE_INPUT_COUNT; i++) if (settings.inputs[i].mode == ROGUE_IO_VARIABLE && name.equalsIgnoreCase(settings.inputs[i].variable)) { set_input_variable(i, value_text.toInt() != 0, "Variable"); matches++; }
  for (uint8_t i = 1; i <= ROGUE_OUTPUT_COUNT; i++) if (settings.outputs[i].mode == ROGUE_IO_VARIABLE && name.equalsIgnoreCase(settings.outputs[i].variable)) { set_output_level(i, clamp_percent(value_text.toFloat()), "Variable"); matches++; }
  if (matches == 0) { Serial.printf("No tank/input/output is assigned to variable \"%s\"\n", name.c_str()); return; }
  send_all_status();
}

void release_all_io_pins() {
  for (uint8_t i = 1; i <= ROGUE_TANK_COUNT; i++) release_io_pin(settings.tanks[i], false);
  for (uint8_t i = 1; i <= ROGUE_INPUT_COUNT; i++) release_io_pin(settings.inputs[i], false);
  for (uint8_t i = 1; i <= ROGUE_OUTPUT_COUNT; i++) release_io_pin(settings.outputs[i], true);
}

void handle_serial_line(String raw) {
  raw.trim(); if (raw.length() == 0) return; String lower = raw; lower.toLowerCase();
  if (lower == "help") { print_help(); return; } if (lower == "status") { print_status(); return; } if (lower == "io") { print_io_assignments(); return; } if (lower == "identity") { send_identity(); return; } if (lower == "send") { send_all_status(); return; } if (lower == "save") { save_settings(); return; } if (lower == "defaults") { release_all_io_pins(); reset_settings_to_defaults(); validate_io_assignments(); configure_io_pins(); send_identity(); send_all_status(); return; } if (lower == "crc") { object2_self_test(); return; }
  if (lower.startsWith("tank ")) { if (!parse_tank_assignment(raw)) Serial.println("Usage: tank <1-2> <GPIO<n>|simulate|disabled|variable-name>"); return; }
  if (lower.startsWith("input ")) { if (!parse_input_assignment(raw)) Serial.println("Usage: input <1-8> <GPIO<n>|simulate|disabled|variable-name>"); return; }
  if (lower.startsWith("output ")) { if (!parse_output_assignment(raw)) Serial.println("Usage: output <1-10> <GPIO<n>|simulate|disabled|variable-name>"); return; }
  if (lower.startsWith("set ")) { String rest = raw.substring(4); rest.trim(); int space = rest.indexOf(' '); if (space <= 0) { Serial.println("Usage: set <variable-name> <value>"); return; } set_named_variable(rest.substring(0, space), rest.substring(space + 1)); return; }
  if (lower.startsWith("t1 ")) { set_tank_percent(1, clamp_percent(raw.substring(3).toFloat()), "Serial", true); send_sensor_values(); return; }
  if (lower.startsWith("t2 ")) { set_tank_percent(2, clamp_percent(raw.substring(3).toFloat()), "Serial", true); send_sensor_values(); return; }
  if (lower.startsWith("v ")) { input_voltage_mv = (uint16_t) constrain(raw.substring(2).toInt(), 0, 65535); send_sensor_values(); return; }
  if (lower.startsWith("i ")) { input_current_ma = (uint16_t) constrain(raw.substring(2).toInt(), 0, 65535); send_sensor_values(); return; }
  if (lower.startsWith("m ")) { set_master(raw.substring(2).toInt() != 0, "Serial"); send_all_status(); return; }
  if (lower.startsWith("in")) { const int space = raw.indexOf(' '); if (space > 2) { const uint8_t input = (uint8_t) raw.substring(2, space).toInt(); if (input < 1 || input > ROGUE_INPUT_COUNT) { Serial.println("Input number must be 1..8."); return; } if (set_input_variable(input, raw.substring(space + 1).toInt() != 0, "Serial")) send_channel_status(); return; } }
  if (lower.startsWith("sa ")) { uint32_t value = parse_u32(raw.substring(3)); if (value == 0 || value > 0xFE) { Serial.println("Invalid source address. Use 0x01..0xFE."); return; } settings.source_address = (uint8_t) value; save_settings(); send_identity(); send_all_status(); return; }
  if (lower.startsWith("serial ")) { String rest = raw.substring(7); rest.trim(); int space = rest.indexOf(' '); if (space <= 0) { Serial.println("Usage: serial <prefix> <suffix>"); return; } settings.serial_prefix = parse_u32(rest.substring(0, space)); settings.serial_suffix = (uint16_t) parse_u32(rest.substring(space + 1)); save_settings(); send_serial_info(); return; }
  if (lower.startsWith("name ")) { String new_name = raw.substring(5); new_name.trim(); if (new_name.length() == 0) { Serial.println("Name cannot be empty."); return; } rogue_copy_product_name(settings, new_name.c_str()); save_settings(); send_product_name(); return; }
  if (lower.startsWith("o")) { const int space = raw.indexOf(' '); if (space > 1) { const uint8_t output = (uint8_t) raw.substring(1, space).toInt(); const uint8_t percent = clamp_percent(raw.substring(space + 1).toFloat()); set_output_level(output, percent, "Serial"); send_all_status(); return; } }
  Serial.println("Unknown command. Type help.");
}

void poll_serial() { static String line; while (Serial.available()) { char ch = (char) Serial.read(); if (ch == '\n' || ch == '\r') { if (line.length()) { handle_serial_line(line); line = ""; } } else { line += ch; if (line.length() > 160) line = ""; } } }

// Installs the TWAI driver (once) and starts it. Safe to call again after a failure
// or after bus-off recovery has returned the controller to the stopped state.
bool start_can() {
  last_can_restart_ms = millis();
  if (!can_installed) {
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(CAN_TX_PIN, CAN_RX_PIN, TWAI_MODE_NORMAL); twai_timing_config_t t_config = TWAI_TIMING_CONFIG_250KBITS(); twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL(); g_config.rx_queue_len = 64; g_config.tx_queue_len = 32;
    esp_err_t err = twai_driver_install(&g_config, &t_config, &f_config); if (err != ESP_OK) { Serial.printf("twai_driver_install failed: %s, retrying in %lus\n", esp_err_to_name(err), (unsigned long) (CAN_RESTART_INTERVAL_MS / 1000UL)); return false; }
    can_installed = true;
  }
  esp_err_t err = twai_start(); if (err != ESP_OK) { Serial.printf("twai_start failed: %s, retrying in %lus\n", esp_err_to_name(err), (unsigned long) (CAN_RESTART_INTERVAL_MS / 1000UL)); return false; }
  can_running = true; Serial.println("CAN/TWAI started at 250 kbit/s"); return true;
}

// Reports dropped TX frames, recovers from bus-off, and retries a failed CAN start.
void maintain_can() {
  const uint32_t now = millis();
  if (tx_fail_count > 0 && now - last_tx_report_ms >= CAN_TX_REPORT_INTERVAL_MS) { Serial.printf("CAN TX: %lu frames dropped (last error %s); is another node on the bus to ACK?\n", (unsigned long) tx_fail_count, esp_err_to_name(last_tx_error)); tx_fail_count = 0; last_tx_report_ms = now; }
  if (!can_installed) { if (now - last_can_restart_ms >= CAN_RESTART_INTERVAL_MS) start_can(); return; }
  if (now - last_can_health_ms < CAN_HEALTH_INTERVAL_MS) return; last_can_health_ms = now;
  twai_status_info_t status; if (twai_get_status_info(&status) != ESP_OK) return;
  switch (status.state) {
    case TWAI_STATE_BUS_OFF: if (can_running) Serial.println("CAN bus-off, starting recovery"); can_running = false; twai_initiate_recovery(); break;
    case TWAI_STATE_RECOVERING: can_running = false; break;
    case TWAI_STATE_STOPPED: can_running = false; if (now - last_can_restart_ms >= CAN_RESTART_INTERVAL_MS) start_can(); break;
    default: break;
  }
}

void update_hold_dim() { const uint32_t now = millis(); for (uint8_t output = 1; output <= ROGUE_OUTPUT_COUNT; output++) { if (hold_dim_direction[output] == 0) continue; if (now - hold_dim_last_step_ms[output] < HOLD_DIM_STEP_MS) continue; hold_dim_last_step_ms[output] = now; int next = (int) output_levels[output] + (hold_dim_direction[output] > 0 ? HOLD_DIM_STEP_PERCENT : -HOLD_DIM_STEP_PERCENT); if (next <= 0) { next = 0; hold_dim_direction[output] = 0; } if (next >= 100) { next = 100; hold_dim_direction[output] = 0; } set_output_level(output, (uint8_t) next, "Hold dim"); send_output_levels(); send_output_activity(); send_channel_status(); } }

void poll_io_assignments() {
  const uint32_t now = millis(); if (now - last_io_poll_ms < IO_POLL_INTERVAL_MS) return; last_io_poll_ms = now;
  if (poll_tank_assignments()) send_sensor_values();
  if (poll_input_assignments()) send_channel_status();
}

void setup() {
  Serial.begin(115200); delay(500); Serial.println(); Serial.println("REDARC TVMS Rogue Emulator - Arduino IDE standalone"); rogue_settings_load(prefs, settings); validate_io_assignments(); tank_variable_percent[1] = settings.tank1_percent; tank_variable_percent[2] = settings.tank2_percent; configure_io_pins(); Serial.printf("Source address: 0x%02X\n", settings.source_address); Serial.printf("Serial: %lu-%04u  Product: %s\n", (unsigned long) settings.serial_prefix, (unsigned) settings.serial_suffix, settings.product_name); Serial.println("Type help for serial commands."); object2_self_test(); start_can(); send_identity(); send_all_status();
}

void loop() {
  maintain_can(); receive_can(); poll_serial(); poll_io_assignments(); update_hold_dim(); const uint32_t now = millis(); if (now - last_identity_ms >= IDENTITY_INTERVAL_MS) { last_identity_ms = now; send_identity(); } if (now - last_status_ms >= STATUS_INTERVAL_MS) { last_status_ms = now; send_all_status(); }
}
