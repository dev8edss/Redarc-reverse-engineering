/*
  REDARC Prime / TVMS1280 Emulator - Arduino IDE standalone port

  Based on the Rogue Arduino emulator structure, but with Prime / TVMS1280 channel counts:
    - 6 tank/analogue level channels
    - 3 digital inputs
    - 11 outputs, captured as channels 0x04..0x0E

  Object 2 is captured from readwrite changed 6.csv:
    source address 0x24, declared length 2628 bytes, CRC-32C 0x3815A305.

  Default source address is 0x25 so it will not collide with a real Prime at 0x24.
  To replace the real Prime, disconnect the real 0x24 module and run:
    sa 0x24
*/

#include <Arduino.h>
#include <Preferences.h>
#include <driver/gpio.h>
#include <driver/twai.h>
#include <pgmspace.h>

#if !defined(ESP_ARDUINO_VERSION_MAJOR) || ESP_ARDUINO_VERSION_MAJOR < 3
#error "This sketch needs the esp32 Arduino core 3.x (uses the ledcAttach/ledcWrite PWM API)."
#endif

struct PrimeDefaults {
  int8_t      can_tx_pin;
  int8_t      can_rx_pin;
  uint8_t     source_address;
  uint32_t    serial_prefix;
  uint16_t    serial_suffix;
  const char *product_name;
  uint8_t     tank1_percent; const char *tank1;
  uint8_t     tank2_percent; const char *tank2;
  uint8_t     tank3_percent; const char *tank3;
  uint8_t     tank4_percent; const char *tank4;
  uint8_t     tank5_percent; const char *tank5;
  uint8_t     tank6_percent; const char *tank6;
  const char *input_1; const char *input_2; const char *input_3;
  uint32_t    output_pwm_frequency_hz;
  uint8_t     output_pwm_resolution_bits;
  const char *output_1; const char *output_2; const char *output_3; const char *output_4;
  const char *output_5; const char *output_6; const char *output_7; const char *output_8;
  const char *output_9; const char *output_10; const char *output_11;

  void load() {
#include "PrimePreferences.h"
  }
};
static PrimeDefaults defaults;

#include "PrimeObject2.h"
#include "PrimePreferencesRuntime.h"

static constexpr uint8_t DEVICE_TYPE_TVMS1280       = 0x0E;
static constexpr uint8_t DEVICE_SUBTYPE_TVMS1280    = 0x00;
static constexpr uint8_t MAIN_CONFIGURATION_OBJECT  = 0x02;

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
static constexpr uint32_t ID_INPUT_CONFIG           = 0x17FD1000UL;
static constexpr uint32_t ID_NODE_FIRMWARE          = 0x17F40000UL;
static constexpr uint32_t ID_NODE_PRODUCT_NAME      = 0x17F40300UL;
static constexpr uint32_t ID_NODE_SERIAL_INFO       = 0x17F40400UL;
static constexpr uint32_t ID_NODE_DEVICE_ID         = 0x17F40500UL;
static constexpr uint32_t ID_DIRECT_ACK_BASE        = 0x0F040000UL;
static constexpr uint32_t ID_SERVICE_ACK_BASE       = 0x02800000UL;
static constexpr uint32_t ID_SERVICE_DATA_BASE      = 0x02810000UL;
static constexpr uint32_t ID_SERVICE_TRAILER_BASE   = 0x02840000UL;

static constexpr uint16_t SERVICE_DGN_REQUEST       = 0x0F03;
static constexpr uint16_t SERVICE_DIRECT_COMMAND    = 0x0F00;
static constexpr uint16_t SERVICE_OBJECT_PREFIX     = 0x0E00;

static constexpr uint32_t IDENTITY_INTERVAL_MS      = 1000UL;
static constexpr uint32_t STATUS_INTERVAL_MS        = 1000UL;
static constexpr uint32_t IO_POLL_INTERVAL_MS       = 50UL;
static constexpr uint32_t CAN_HEALTH_INTERVAL_MS    = 100UL;
static constexpr uint32_t CAN_RESTART_INTERVAL_MS   = 5000UL;
static constexpr uint32_t CAN_TX_REPORT_INTERVAL_MS = 5000UL;

static constexpr uint32_t MAX_CONFIG_OBJECT_SIZE    = 8192UL;
static constexpr uint32_t WRITE_WINDOW_SIZE         = 1024UL;
static constexpr uint8_t PROGRAMMING_STATUS_OK      = 0x00;
static constexpr uint8_t PROGRAMMING_STATUS_BUSY    = 0x01;
static constexpr uint8_t PROGRAMMING_STATUS_ERROR   = 0x03;

// Captured Prime/1280 config DGN replies from readwrite changed 6.csv.
static const uint8_t reply_active_channels[][8] PROGMEM = {
  {0x1A, 0x32, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
};
static const uint8_t reply_output_capabilities[][8] PROGMEM = {
  {0x04, 0x0B, 0x00, 0x01, 0xFF, 0xFF, 0xFF, 0xFF},
  {0x05, 0x03, 0x00, 0x02, 0xFF, 0xFF, 0xFF, 0xFF},
  {0x06, 0x0B, 0x00, 0x03, 0xFF, 0xFF, 0xFF, 0xFF},
  {0x07, 0x03, 0x00, 0x01, 0xFF, 0xFF, 0xFF, 0xFF},
  {0x08, 0x0B, 0x00, 0x01, 0xFF, 0xFF, 0xFF, 0xFF},
  {0x09, 0x03, 0x00, 0x02, 0xFF, 0xFF, 0xFF, 0xFF},
  {0x0A, 0x3B, 0x00, 0x01, 0xFF, 0xFF, 0xFF, 0xFF},
  {0x0B, 0x23, 0x00, 0x01, 0xFF, 0xFF, 0xFF, 0xFF},
  {0x0C, 0x13, 0x00, 0x01, 0xFF, 0xFF, 0xFF, 0xFF},
  {0x0D, 0x03, 0x00, 0x01, 0xFF, 0xFF, 0xFF, 0xFF},
  {0x0E, 0x03, 0x00, 0x02, 0xFF, 0xFF, 0xFF, 0xFF},
};
static const uint8_t reply_channel_details[][8] PROGMEM = {
  {0x01,0x00,0x00,0x00,0x00,0x00,0x01,0x00},{0x02,0x00,0x00,0x00,0x00,0x00,0x01,0x00},{0x03,0x00,0x00,0x00,0x00,0x00,0x01,0x00},
  {0x04,0x01,0x00,0x00,0x00,0x00,0x01,0x00},{0x05,0x01,0x00,0x00,0x00,0x00,0x01,0x00},{0x06,0x01,0x00,0x00,0x00,0x00,0x01,0x00},
  {0x07,0x01,0x00,0x00,0x00,0x00,0x01,0x00},{0x08,0x01,0x00,0x00,0x00,0x00,0x01,0x00},{0x09,0x01,0x00,0x00,0x11,0x00,0x01,0x00},
  {0x0A,0x01,0x00,0x00,0x03,0x00,0x01,0x00},{0x0B,0x01,0x00,0x00,0x00,0x00,0x01,0x00},{0x0C,0x01,0x00,0x00,0x00,0x00,0x01,0x00},
  {0x0D,0x01,0x00,0x00,0x00,0x00,0x01,0x00},{0x0E,0x04,0xFF,0xFF,0x08,0x00,0x01,0x00},{0x0F,0x08,0xFF,0xFF,0x00,0x00,0x01,0x00},
  {0x10,0x07,0xFF,0xFF,0x00,0x00,0x01,0x00},{0x11,0x02,0x64,0x00,0x00,0x00,0x01,0x00},{0x12,0x02,0x64,0x00,0x00,0x00,0x01,0x00},
  {0x13,0x02,0x66,0x00,0x64,0x00,0x01,0x00},{0x14,0x02,0x66,0x00,0x1B,0x00,0x01,0x00},{0x15,0x02,0x66,0x00,0x00,0x00,0x01,0x00},
  {0x16,0x02,0x66,0x00,0x00,0x00,0x01,0x00},{0x17,0x02,0x66,0x00,0x00,0x00,0x01,0x00},{0x18,0x02,0x66,0x00,0xD7,0x00,0x01,0x00},
  {0x19,0x02,0x66,0x00,0x7B,0x01,0x01,0x00},{0x1A,0x02,0x65,0x00,0x00,0x00,0x01,0x00},
};
static const uint8_t reply_alarm_config[][8] PROGMEM = {
  {0x11,0x00,0x00,0x00,0x00,0x00,0xFF,0xFF},{0x12,0x00,0x00,0x00,0x00,0x00,0xFF,0xFF},{0x13,0x00,0x64,0x00,0x64,0x00,0xFF,0xFF},
  {0x14,0x00,0x64,0x00,0x64,0x00,0xFF,0xFF},{0x15,0x00,0x00,0x00,0x00,0x00,0xFF,0xFF},{0x16,0x01,0x11,0x00,0x53,0x00,0xFF,0xFF},
  {0x17,0x02,0x0D,0x00,0x47,0x00,0xFF,0xFF},{0x18,0x00,0x00,0x00,0x00,0x00,0xFF,0xFF},{0x19,0x00,0x00,0x00,0x00,0x00,0xFF,0xFF},
  {0x1A,0x00,0x00,0x00,0x00,0x00,0xFF,0xFF},
};
static const uint8_t reply_alarm_status[][8] PROGMEM = {
  {0x11,0xFC,0xFC,0xFC,0xFC,0xFC,0xFC,0xFD},{0x18,0xFC,0xFC,0xFC,0xFF,0xFF,0xFF,0xFF},
};
static const uint8_t reply_analog_scaling[][8] PROGMEM = {
  {0x11,0x61,0x00,0x00,0x00,0x00,0x30,0x75},{0x12,0x61,0x00,0x00,0x00,0x00,0x20,0x4E},{0x13,0x64,0x64,0x00,0x3C,0x00,0xFA,0x00},
  {0x14,0x64,0x64,0x00,0x3C,0x00,0xFA,0x00},{0x15,0x64,0x00,0x00,0x00,0x00,0x64,0x00},{0x16,0x64,0x00,0x00,0x00,0x00,0x64,0x00},
  {0x17,0x64,0x00,0x00,0x00,0x00,0x64,0x00},{0x18,0x64,0x00,0x00,0x00,0x00,0x64,0x00},{0x19,0x64,0x00,0x00,0x00,0x00,0x64,0x00},
  {0x1A,0x64,0x00,0x00,0x00,0x00,0x64,0x00},
};
static const uint8_t reply_input_config[][8] PROGMEM = {
  {0x01,0x01,0x00,0xFF,0xFF,0xFF,0xFF,0xFF},{0x02,0x01,0x00,0xFF,0xFF,0xFF,0xFF,0xFF},{0x03,0x01,0x00,0xFF,0xFF,0xFF,0xFF,0xFF},
};
static const uint8_t reply_channel_labels[][8] PROGMEM = {
  {0x01,0x00,0x49,0x6E,0x70,0x75,0x74,0x20},{0x01,0x01,0x31,0xFF,0xFF,0xFF,0xFF,0xFF},{0x02,0x00,0x49,0x6E,0x70,0x75,0x74,0x20},{0x02,0x01,0x32,0xFF,0xFF,0xFF,0xFF,0xFF},{0x03,0x00,0x49,0x6E,0x70,0x75,0x74,0x20},{0x03,0x01,0x33,0xFF,0xFF,0xFF,0xFF,0xFF},
  {0x04,0x00,0x4F,0x75,0x74,0x70,0x75,0x74},{0x04,0x01,0x20,0x31,0xFF,0xFF,0xFF,0xFF},{0x05,0x00,0x4F,0x75,0x74,0x70,0x75,0x74},{0x05,0x01,0x20,0x32,0xFF,0xFF,0xFF,0xFF},{0x06,0x00,0x4F,0x75,0x74,0x70,0x75,0x74},{0x06,0x01,0x20,0x33,0xFF,0xFF,0xFF,0xFF},{0x07,0x00,0x4F,0x75,0x74,0x70,0x75,0x74},{0x07,0x01,0x20,0x34,0xFF,0xFF,0xFF,0xFF},{0x08,0x00,0x4F,0x75,0x74,0x70,0x75,0x74},{0x08,0x01,0x20,0x35,0xFF,0xFF,0xFF,0xFF},
  {0x09,0x00,0x53,0x74,0x61,0x72,0x6C,0x69},{0x09,0x01,0x6E,0x6B,0xFF,0xFF,0xFF,0xFF},{0x0A,0x00,0x50,0x75,0x6D,0x70,0xFF,0xFF},{0x0B,0x00,0x4F,0x75,0x74,0x70,0x75,0x74},{0x0B,0x01,0x20,0x38,0xFF,0xFF,0xFF,0xFF},{0x0C,0x00,0x4F,0x75,0x74,0x70,0x75,0x74},{0x0C,0x01,0x20,0x39,0xFF,0xFF,0xFF,0xFF},{0x0D,0x00,0x4F,0x75,0x74,0x70,0x75,0x74},{0x0D,0x01,0x20,0x31,0x30,0xFF,0xFF,0xFF},{0x0E,0x00,0x49,0x6E,0x76,0x65,0x72,0x74},{0x0E,0x01,0x65,0x72,0xFF,0xFF,0xFF,0xFF},{0x0F,0x00,0x4D,0x61,0x73,0x74,0x65,0x72},{0x0F,0x01,0x20,0x73,0x77,0x69,0x74,0x63},{0x0F,0x02,0x68,0xFF,0xFF,0xFF,0xFF,0xFF},
  {0x10,0x00,0x53,0x42,0x49,0xFF,0xFF,0xFF},{0x11,0x00,0x31,0x32,0x38,0x30,0x20,0x56},{0x11,0x01,0x6F,0x6C,0x74,0x61,0x67,0x65},{0x12,0x00,0x31,0x32,0x38,0x30,0x20,0x56},{0x12,0x01,0x6F,0x6C,0x74,0x61,0x67,0x65},{0x13,0x00,0x49,0x6E,0x73,0x69,0x64,0x65},{0x14,0x00,0x4F,0x75,0x74,0x73,0x69,0x64},{0x14,0x01,0x65,0xFF,0xFF,0xFF,0xFF,0xFF},{0x15,0x00,0x47,0x72,0x65,0x79,0xFF,0xFF},{0x16,0x00,0x57,0x61,0x74,0x65,0x72,0x20},{0x16,0x01,0x54,0x61,0x6E,0x6B,0x20,0x32},{0x17,0x00,0x57,0x61,0x74,0x65,0x72,0x20},{0x17,0x01,0x54,0x61,0x6E,0x6B,0x20,0x33},{0x18,0x00,0x57,0x61,0x74,0x65,0x72,0x20},{0x18,0x01,0x54,0x61,0x6E,0x6B,0x20,0x34},{0x19,0x00,0x57,0x61,0x74,0x65,0x72,0x20},{0x19,0x01,0x54,0x61,0x6E,0x6B,0x20,0x35},{0x1A,0x00,0x57,0x61,0x74,0x65,0x72,0x20},{0x1A,0x01,0x54,0x61,0x6E,0x6B,0x20,0x36},
};

Preferences prefs;
Preferences object_prefs;
PrimeSettings settings;

static bool can_installed = false;
static bool can_running = false;
static bool can_pins_invalid = false;
static uint32_t last_can_health_ms = 0;
static uint32_t last_can_restart_ms = 0;
static uint32_t last_tx_report_ms = 0;
static uint32_t tx_fail_count = 0;
static esp_err_t last_tx_error = ESP_OK;
static uint8_t selected_object = 0xFF;
static uint8_t *config_object = nullptr;
static uint32_t config_object_len = 0;
static bool config_from_flash = false;
static bool write_active = false;
static bool write_closed = false;
static bool write_overflow = false;
static uint8_t *write_staging = nullptr;
static uint8_t write_received[MAX_CONFIG_OBJECT_SIZE / 8];
static uint8_t write_block[WRITE_WINDOW_SIZE];
static uint32_t write_block_len = 0;
static uint8_t output_levels[PRIME_OUTPUT_COUNT + 1] = {0};
static uint8_t output_variable_percent[PRIME_OUTPUT_COUNT + 1] = {0};
static int8_t output_pwm_pin[PRIME_OUTPUT_COUNT + 1] = {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};
static bool input_states[PRIME_INPUT_COUNT + 1] = {false};
static bool input_variable_state[PRIME_INPUT_COUNT + 1] = {false};
static uint8_t tank_variable_percent[PRIME_TANK_COUNT + 1] = {0};
static bool master_state = true;
static uint32_t last_identity_ms = 0;
static uint32_t last_status_ms = 0;
static uint32_t last_io_poll_ms = 0;

uint32_t with_sa(uint32_t base_id) { return (base_id & 0x1FFFFF00UL) | settings.source_address; }
uint16_t u16_le(const uint8_t *d) { return (uint16_t)d[0] | ((uint16_t)d[1] << 8); }
uint32_t u32_le(const uint8_t *d) { return (uint32_t)d[0] | ((uint32_t)d[1] << 8) | ((uint32_t)d[2] << 16) | ((uint32_t)d[3] << 24); }
uint32_t parse_u32(const String &text) { String s = text; s.trim(); return strtoul(s.c_str(), nullptr, 0); }
uint8_t clamp_percent(float value) { if (isnan(value) || value <= 0.0f) return 0; if (value >= 100.0f) return 100; return (uint8_t) lroundf(value); }

uint32_t crc32c_update_byte(uint32_t crc, uint8_t b) { crc ^= b; for (uint8_t bit = 0; bit < 8; bit++) crc = (crc & 1U) ? ((crc >> 1) ^ 0x82F63B78UL) : (crc >> 1); return crc; }
uint32_t crc32c(const uint8_t *data, size_t len) { uint32_t crc = 0xFFFFFFFFUL; for (size_t i = 0; i < len; i++) crc = crc32c_update_byte(crc, data[i]); return ~crc; }

bool validate_config_object(const uint8_t *data, uint32_t available, uint32_t &declared_len, uint32_t &stored_crc, uint32_t &calc_crc) {
  declared_len = stored_crc = calc_crc = 0;
  if (data == nullptr || available < 12UL) return false;
  declared_len = u32_le(data + 4);
  if (declared_len < 12UL || declared_len > available || declared_len > MAX_CONFIG_OBJECT_SIZE) return false;
  stored_crc = u32_le(data + 8);
  uint32_t crc = 0xFFFFFFFFUL;
  for (uint32_t i = 0; i < declared_len; i++) crc = crc32c_update_byte(crc, (i >= 8 && i <= 11) ? 0x00 : data[i]);
  calc_crc = ~crc;
  return stored_crc == calc_crc;
}

void load_factory_config_object() { for (uint32_t i = 0; i < PRIME_OBJECT2_SIZE; i++) config_object[i] = prime_object2_byte(i); config_object_len = PRIME_OBJECT2_SIZE; config_from_flash = false; }
void load_config_object() {
  config_object = (uint8_t *)malloc(MAX_CONFIG_OBJECT_SIZE);
  if (config_object == nullptr) { Serial.println("Object2: out of memory"); while (true) delay(1000); }
  object_prefs.begin("primeobj", false);
  const size_t saved_len = object_prefs.isKey("obj2") ? object_prefs.getBytesLength("obj2") : 0;
  if (saved_len >= 12 && saved_len <= MAX_CONFIG_OBJECT_SIZE && object_prefs.getBytes("obj2", config_object, saved_len) == saved_len) {
    uint32_t declared_len = 0, stored_crc = 0, calc_crc = 0;
    if (validate_config_object(config_object, saved_len, declared_len, stored_crc, calc_crc) && declared_len == saved_len) { config_object_len = saved_len; config_from_flash = true; Serial.printf("Object2: loaded saved Prime config, %lu bytes CRC-32C=0x%08lX\n", (unsigned long)saved_len, (unsigned long)stored_crc); return; }
    Serial.println("Object2: saved configuration invalid, using factory Prime object");
  }
  load_factory_config_object();
}

void object2_self_test() {
  uint32_t declared_len = 0, stored_crc = 0, calc_crc = 0;
  const bool ok = validate_config_object(config_object, config_object_len, declared_len, stored_crc, calc_crc) && declared_len == config_object_len;
  Serial.printf("Prime Object2 (%s) size=%lu declared_len=%lu stored_crc=0x%08lX calc_crc=0x%08lX %s\n", config_from_flash ? "saved" : "factory", (unsigned long)config_object_len, (unsigned long)declared_len, (unsigned long)stored_crc, (unsigned long)calc_crc, ok ? "OK" : "FAIL");
}

void send_frame(uint32_t id, const uint8_t *data, uint8_t len) {
  if (!can_running) return;
  twai_message_t msg = {};
  msg.identifier = id & 0x1FFFFFFFUL;
  msg.extd = 1;
  msg.rtr = 0;
  msg.data_length_code = len > 8 ? 8 : len;
  for (uint8_t i = 0; i < msg.data_length_code; i++) msg.data[i] = data[i];
  const esp_err_t err = twai_transmit(&msg, 0);
  if (err != ESP_OK) { tx_fail_count++; last_tx_error = err; }
}
void send_frame8(uint32_t id, uint8_t a,uint8_t b,uint8_t c,uint8_t d,uint8_t e,uint8_t f,uint8_t g,uint8_t h) { const uint8_t data[8] = {a,b,c,d,e,f,g,h}; send_frame(id, data, 8); }

template <size_t N> void send_captured_frames(uint32_t base_id, const uint8_t (&frames)[N][8]) {
  for (size_t i = 0; i < N; i++) { uint8_t data[8]; for (uint8_t j = 0; j < 8; j++) data[j] = pgm_read_byte(&frames[i][j]); send_frame(with_sa(base_id), data, 8); }
}

const char *gpio_pin_problem(int8_t pin, bool needs_output, bool needs_adc) {
  if (pin < 0 || !GPIO_IS_VALID_GPIO(pin)) return "is not a GPIO on this chip";
#if CONFIG_IDF_TARGET_ESP32
  if (pin >= 6 && pin <= 11) return "is reserved for the SPI flash";
#endif
  if (pin == defaults.can_tx_pin || pin == defaults.can_rx_pin) return "is used by CAN";
  if (needs_output && !GPIO_IS_VALID_OUTPUT_GPIO(pin)) return "is input-only";
  if (needs_adc && digitalPinToAnalogChannel(pin) < 0) return "is not an ADC pin";
  return nullptr;
}

bool gpio_pin_taken(int8_t pin, const PrimeIoAssignment *slot) {
  for (uint8_t i = 1; i <= PRIME_TANK_COUNT; i++) if (&settings.tanks[i] != slot && settings.tanks[i].mode == PRIME_IO_PIN && settings.tanks[i].pin == pin) return true;
  for (uint8_t i = 1; i <= PRIME_INPUT_COUNT; i++) if (&settings.inputs[i] != slot && settings.inputs[i].mode == PRIME_IO_PIN && settings.inputs[i].pin == pin) return true;
  for (uint8_t i = 1; i <= PRIME_OUTPUT_COUNT; i++) if (&settings.outputs[i] != slot && settings.outputs[i].mode == PRIME_IO_PIN && settings.outputs[i].pin == pin) return true;
  return false;
}

const char *io_assignment_problem(const PrimeIoAssignment &candidate, const PrimeIoAssignment *slot, bool needs_output, bool needs_adc) {
  if (candidate.mode != PRIME_IO_PIN) return nullptr;
  const char *problem = gpio_pin_problem(candidate.pin, needs_output, needs_adc);
  if (problem == nullptr && gpio_pin_taken(candidate.pin, slot)) problem = "is already assigned to another channel";
  return problem;
}

void release_io_pin(const PrimeIoAssignment &old, bool was_output) {
  if (old.mode != PRIME_IO_PIN || old.pin < 0) return;
  if (was_output) {
    for (uint8_t i = 1; i <= PRIME_OUTPUT_COUNT; i++) {
      if (output_pwm_pin[i] != old.pin) continue;
      ledcWrite((uint8_t) old.pin, 0);
      ledcDetach((uint8_t) old.pin);
      output_pwm_pin[i] = -1;
    }
    digitalWrite((uint8_t) old.pin, LOW);
  }
  pinMode((uint8_t) old.pin, INPUT);
}

void disable_unusable_assignment(PrimeIoAssignment &a, const char *kind, uint8_t n, bool needs_output, bool needs_adc) {
  const char *problem = io_assignment_problem(a, &a, needs_output, needs_adc);
  if (problem == nullptr) return;
  Serial.printf("%s %u: GPIO%d %s, channel disabled\n", kind, (unsigned) n, a.pin, problem);
  a.mode = PRIME_IO_DISABLED;
  a.pin = -1;
  a.variable[0] = '\0';
}

void validate_io_assignments() {
  for (uint8_t i = 1; i <= PRIME_TANK_COUNT; i++) disable_unusable_assignment(settings.tanks[i], "Tank", i, false, true);
  for (uint8_t i = 1; i <= PRIME_INPUT_COUNT; i++) disable_unusable_assignment(settings.inputs[i], "Input", i, false, false);
  for (uint8_t i = 1; i <= PRIME_OUTPUT_COUNT; i++) disable_unusable_assignment(settings.outputs[i], "Output", i, true, false);
}

uint8_t analog_raw_to_percent(int raw) { if (raw <= 0) return 0; if (raw >= 4095) return 100; return (uint8_t)((raw * 100L + 2047L) / 4095L); }

void apply_output_assignment(uint8_t output) {
  if (output < 1 || output > PRIME_OUTPUT_COUNT) return;
  const PrimeIoAssignment &a = settings.outputs[output];
  if (a.mode == PRIME_IO_VARIABLE) { output_variable_percent[output] = output_levels[output]; return; }
  if (a.mode != PRIME_IO_PIN) return;
  if (output_pwm_pin[output] == a.pin) {
    const uint32_t max_duty = (1UL << defaults.output_pwm_resolution_bits) - 1UL;
    ledcWrite((uint8_t) a.pin, (output_levels[output] * max_duty + 50UL) / 100UL);
  } else {
    digitalWrite((uint8_t) a.pin, output_levels[output] > 0 ? HIGH : LOW);
  }
}

void attach_output_pwm(uint8_t output) {
  const PrimeIoAssignment &a = settings.outputs[output];
  if (a.mode != PRIME_IO_PIN) return;
  if (output_pwm_pin[output] == a.pin) return;
  if (ledcAttach((uint8_t) a.pin, defaults.output_pwm_frequency_hz, defaults.output_pwm_resolution_bits)) { output_pwm_pin[output] = a.pin; return; }
  output_pwm_pin[output] = -1;
  pinMode((uint8_t) a.pin, OUTPUT);
  Serial.printf("Output %u: PWM unavailable on GPIO%d, using on/off only\n", (unsigned) output, a.pin);
}

void configure_io_pins() {
  for (uint8_t tank = 1; tank <= PRIME_TANK_COUNT; tank++) {
    const PrimeIoAssignment &a = settings.tanks[tank];
    if (a.mode == PRIME_IO_PIN) { pinMode((uint8_t) a.pin, INPUT); settings.tank_percent[tank] = analog_raw_to_percent(analogRead((uint8_t) a.pin)); }
    else if (a.mode == PRIME_IO_DISABLED) settings.tank_percent[tank] = 0;
  }
  for (uint8_t input = 1; input <= PRIME_INPUT_COUNT; input++) {
    const PrimeIoAssignment &a = settings.inputs[input];
    if (a.mode == PRIME_IO_PIN) { pinMode((uint8_t) a.pin, INPUT); input_states[input] = digitalRead((uint8_t) a.pin) == HIGH; }
    else if (a.mode == PRIME_IO_DISABLED) input_states[input] = false;
  }
  for (uint8_t output = 1; output <= PRIME_OUTPUT_COUNT; output++) {
    const PrimeIoAssignment &a = settings.outputs[output];
    if (a.mode == PRIME_IO_DISABLED) { output_levels[output] = 0; continue; }
    if (a.mode == PRIME_IO_PIN) { attach_output_pwm(output); apply_output_assignment(output); }
  }
}

bool poll_tank_assignments() {
  bool changed = false;
  for (uint8_t tank = 1; tank <= PRIME_TANK_COUNT; tank++) {
    uint8_t next = settings.tank_percent[tank];
    const PrimeIoAssignment &a = settings.tanks[tank];
    if (a.mode == PRIME_IO_PIN) next = analog_raw_to_percent(analogRead((uint8_t) a.pin));
    else if (a.mode == PRIME_IO_VARIABLE) next = tank_variable_percent[tank];
    else if (a.mode == PRIME_IO_DISABLED) next = 0;
    if (settings.tank_percent[tank] != next) { settings.tank_percent[tank] = next; changed = true; Serial.printf("Tank %u changed to %u%% from %s\n", (unsigned)tank, (unsigned)next, prime_io_describe(a).c_str()); }
  }
  return changed;
}

bool poll_input_assignments() {
  bool changed = false;
  for (uint8_t input = 1; input <= PRIME_INPUT_COUNT; input++) {
    bool next = input_states[input];
    const PrimeIoAssignment &a = settings.inputs[input];
    if (a.mode == PRIME_IO_PIN) next = digitalRead((uint8_t) a.pin) == HIGH;
    else if (a.mode == PRIME_IO_VARIABLE) next = input_variable_state[input];
    else if (a.mode == PRIME_IO_DISABLED) next = false;
    if (input_states[input] != next) { input_states[input] = next; changed = true; Serial.printf("Input %u changed to %s from %s\n", (unsigned)input, next ? "ON" : "OFF", prime_io_describe(a).c_str()); }
  }
  return changed;
}

void save_settings() { prime_settings_sanitize(settings); prime_settings_save(prefs, settings); Serial.println("Settings saved to NVS"); }
void reset_settings_to_defaults() {
  prime_settings_reset(prefs, settings);
  memset(input_states, 0, sizeof(input_states));
  memset(input_variable_state, 0, sizeof(input_variable_state));
  memset(output_levels, 0, sizeof(output_levels));
  memset(output_variable_percent, 0, sizeof(output_variable_percent));
  memset(tank_variable_percent, 0, sizeof(tank_variable_percent));
  for (uint8_t tank = 1; tank <= PRIME_TANK_COUNT; tank++) tank_variable_percent[tank] = settings.tank_percent[tank];
  Serial.println("Settings restored to defaults");
}

uint8_t output_status_byte(uint8_t output) { return output >= 1 && output <= PRIME_OUTPUT_COUNT && output_levels[output] > 0 ? 0x24 : 0x00; }

void send_direct_ack(uint8_t requester, uint8_t command) { send_frame8(ID_DIRECT_ACK_BASE | ((uint32_t)requester << 8) | settings.source_address, 0x01, command, 0x00, 0xFF, 0x00, 0xFF, 0xFF, 0xFF); }
void send_programming_status(uint8_t requester, uint8_t status, uint8_t opcode) { send_frame8(ID_SERVICE_ACK_BASE | ((uint32_t)requester << 8) | settings.source_address, status, 0x00, opcode, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF); }

void send_load_disconnect_config() { send_frame8(with_sa(ID_LOAD_DISCONNECT_CONFIG), 0xEC, 0xD8, 0x27, 0x24, 0x2C, 0x28, 0x3C, 0x00); }
void send_channel_status() {
  send_frame8(with_sa(ID_CHANNEL_STATUS),0x01,input_states[1]?1:0,input_states[2]?1:0,input_states[3]?1:0,output_status_byte(1),output_status_byte(2),output_status_byte(3),output_status_byte(4));
  send_frame8(with_sa(ID_CHANNEL_STATUS),0x08,output_status_byte(5),output_status_byte(6),output_status_byte(7),output_status_byte(8),output_status_byte(9),output_status_byte(10),output_status_byte(11));
  send_frame8(with_sa(ID_CHANNEL_STATUS),0x0F,master_state?0x01:0x00,0x0C,0xFF,0xFF,0xFF,0xFF,0xFF);
}
void send_sensor_values() {
  send_frame8(with_sa(ID_SENSOR_VALUES),0x11,0x00,0x00,0x48,0x36,0x75,0x00,0xFF);
  send_frame8(with_sa(ID_SENSOR_VALUES),0x15,settings.tank_percent[1],settings.tank_percent[2],settings.tank_percent[3],settings.tank_percent[4],settings.tank_percent[5],settings.tank_percent[6],0xFF);
}
void send_node_firmware() { send_frame8(with_sa(ID_NODE_FIRMWARE),0xBF,0x00,0x03,0x04,0,0,0,0); send_frame8(with_sa(ID_NODE_FIRMWARE),0xBF,0x00,0,0x04,0,0,0x01,0); }
void send_product_name() { const size_t len = strlen(settings.product_name); const uint8_t seg_count = (uint8_t)(len / 7 + 1); for (uint8_t seg = 0; seg < seg_count; seg++) { uint8_t data[8] = {seg,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF}; for (uint8_t i = 0; i < 7; i++) { size_t pos = (size_t)seg * 7 + i; if (pos < len) data[1 + i] = (uint8_t)settings.product_name[pos]; } send_frame(with_sa(ID_NODE_PRODUCT_NAME), data, 8); } }
void send_serial_info() { send_frame8(with_sa(ID_NODE_SERIAL_INFO),(uint8_t)(settings.serial_prefix&0xFF),(uint8_t)((settings.serial_prefix>>8)&0xFF),(uint8_t)((settings.serial_prefix>>16)&0xFF),(uint8_t)((settings.serial_prefix>>24)&0xFF),(uint8_t)(settings.serial_suffix&0xFF),(uint8_t)((settings.serial_suffix>>8)&0xFF),DEVICE_TYPE_TVMS1280,DEVICE_SUBTYPE_TVMS1280); }
void send_device_id() { send_frame8(with_sa(ID_NODE_DEVICE_ID),0,0,0,0,0,settings.source_address,0x01,0); }
void send_identity() { send_node_firmware(); send_product_name(); send_serial_info(); send_device_id(); send_load_disconnect_config(); }
void send_all_status() { send_channel_status(); send_sensor_values(); send_captured_frames(ID_ACTIVE_CHANNELS, reply_active_channels); send_captured_frames(ID_OUTPUT_CAPABILITIES, reply_output_capabilities); }

void handle_object_select(uint8_t requester, const uint8_t *data, uint8_t len) { selected_object = len >= 1 ? data[0] : 0xFF; send_programming_status(requester, PROGRAMMING_STATUS_OK, 0x85); Serial.printf("Selected REDARC object %u\n", selected_object); }
void handle_object_read(uint8_t requester, const uint8_t *data, uint8_t len) {
  if (len < 8) return;
  const uint32_t offset = u32_le(data); const uint32_t requested_length = u32_le(data + 4);
  if (requested_length > MAX_CONFIG_OBJECT_SIZE) { Serial.printf("Refusing oversized object read length %lu\n", (unsigned long)requested_length); return; }
  uint8_t *block = (uint8_t *)malloc(requested_length == 0 ? 1 : requested_length); if (block == nullptr) { Serial.println("Object read malloc failed"); return; }
  memset(block, 0xFF, requested_length);
  if (selected_object == MAIN_CONFIGURATION_OBJECT && offset < config_object_len) { const uint32_t available = config_object_len - offset; memcpy(block, config_object + offset, requested_length < available ? requested_length : available); }
  const uint32_t data_id = ID_SERVICE_DATA_BASE | ((uint32_t)requester << 8) | settings.source_address;
  for (uint32_t pos = 0; pos < requested_length; pos += 8) { uint8_t frame[8] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF}; const uint8_t chunk = (uint8_t)((requested_length - pos) < 8UL ? (requested_length - pos) : 8UL); memcpy(frame, block + pos, chunk); send_frame(data_id, frame, 8); }
  const uint32_t block_crc = crc32c(block, requested_length); const uint32_t trailer_id = ID_SERVICE_TRAILER_BASE | ((uint32_t)requester << 8) | settings.source_address;
  send_frame8(trailer_id,(uint8_t)(requested_length&0xFF),(uint8_t)((requested_length>>8)&0xFF),(uint8_t)((requested_length>>16)&0xFF),(uint8_t)((requested_length>>24)&0xFF),(uint8_t)(block_crc&0xFF),(uint8_t)((block_crc>>8)&0xFF),(uint8_t)((block_crc>>16)&0xFF),(uint8_t)((block_crc>>24)&0xFF));
  Serial.printf("Object %u read offset=%lu length=%lu crc=0x%08lX\n", selected_object, (unsigned long)offset, (unsigned long)requested_length, (unsigned long)block_crc); free(block);
}
void end_config_write() { write_active = false; write_closed = false; write_overflow = false; write_block_len = 0; free(write_staging); write_staging = nullptr; }
void handle_write_capability(uint8_t requester) { send_frame8(ID_SERVICE_DATA_BASE | ((uint32_t)requester << 8) | settings.source_address,0x00,0x04,0x00,0x00,0xFF,0xFF,0xFF,0xFF); send_frame8(ID_SERVICE_TRAILER_BASE | ((uint32_t)requester << 8) | settings.source_address,0x04,0x00,0x00,0x00,0xDD,0xEF,0xB9,0xD6); }
void handle_write_start(uint8_t requester) { end_config_write(); if (selected_object == MAIN_CONFIGURATION_OBJECT) write_staging = (uint8_t *)malloc(MAX_CONFIG_OBJECT_SIZE); if (write_staging == nullptr) { send_programming_status(requester, PROGRAMMING_STATUS_ERROR, 0x87); return; } memset(write_staging,0xFF,MAX_CONFIG_OBJECT_SIZE); memset(write_received,0,sizeof(write_received)); write_active = true; send_programming_status(requester,PROGRAMMING_STATUS_OK,0x87); Serial.printf("Started Prime Object 2 write from 0x%02X\n", requester); }
void handle_write_data(const uint8_t *data, uint8_t len) { if (!write_active) return; if (write_block_len + len > WRITE_WINDOW_SIZE) { write_overflow = true; return; } memcpy(write_block + write_block_len, data, len); write_block_len += len; }
void handle_write_block(uint8_t requester, const uint8_t *data, uint8_t len) { send_programming_status(requester,PROGRAMMING_STATUS_BUSY,0x88); const uint32_t offset = len >= 4 ? u32_le(data) : 0; const uint32_t expected_crc = len >= 8 ? u32_le(data + 4) : 0; const uint32_t actual_crc = crc32c(write_block, write_block_len); const bool valid = write_active && len >= 8 && !write_overflow && write_block_len > 0 && offset <= MAX_CONFIG_OBJECT_SIZE && write_block_len <= MAX_CONFIG_OBJECT_SIZE - offset && actual_crc == expected_crc; if (valid) { memcpy(write_staging + offset, write_block, write_block_len); for (uint32_t i = offset; i < offset + write_block_len; i++) write_received[i >> 3] |= (uint8_t)(1U << (i & 7)); send_programming_status(requester, PROGRAMMING_STATUS_OK, 0x88); } else send_programming_status(requester, PROGRAMMING_STATUS_ERROR, 0x88); write_block_len = 0; write_overflow = false; }
void handle_object_close(uint8_t requester) { selected_object = 0xFF; if (!write_active) { send_programming_status(requester, PROGRAMMING_STATUS_OK, 0x89); return; } const bool valid = write_block_len == 0 && !write_overflow; write_active = false; write_closed = valid; send_programming_status(requester, valid ? PROGRAMMING_STATUS_OK : PROGRAMMING_STATUS_ERROR, 0x89); if (!valid) end_config_write(); }
bool write_range_received(uint32_t length) { for (uint32_t i = 0; i < length; i++) if ((write_received[i >> 3] & (uint8_t)(1U << (i & 7))) == 0) return false; return true; }
void handle_write_commit(uint8_t requester) { uint32_t declared_len=0, stored_crc=0, calc_crc=0; bool complete=false, saved=false; bool valid = write_closed && write_staging != nullptr && validate_config_object(write_staging, MAX_CONFIG_OBJECT_SIZE, declared_len, stored_crc, calc_crc); if (valid) valid = complete = write_range_received(declared_len); if (valid) valid = saved = object_prefs.putBytes("obj2", write_staging, declared_len) == declared_len; if (valid) { memcpy(config_object, write_staging, declared_len); config_object_len = declared_len; config_from_flash = true; send_programming_status(requester, PROGRAMMING_STATUS_OK, 0x8A); Serial.printf("Committed and saved Prime Object 2: %lu bytes CRC-32C=0x%08lX\n", (unsigned long)declared_len, (unsigned long)stored_crc); } else { send_programming_status(requester, PROGRAMMING_STATUS_ERROR, 0x8A); Serial.printf("Rejected Prime commit: length=%lu complete=%s saved=%s\n", (unsigned long)declared_len, complete ? "yes" : "no", saved ? "yes" : "no"); } end_config_write(); if (valid) { send_identity(); send_all_status(); } }
void handle_object_service(uint8_t requester, uint8_t opcode, const uint8_t *data, uint8_t len) { switch (opcode) { case 0x81: handle_write_data(data,len); return; case 0x83: handle_write_capability(requester); return; case 0x85: handle_object_select(requester,data,len); return; case 0x86: handle_object_read(requester,data,len); return; case 0x87: handle_write_start(requester); return; case 0x88: handle_write_block(requester,data,len); return; case 0x89: handle_object_close(requester); return; case 0x8A: handle_write_commit(requester); return; default: Serial.printf("Unhandled object service 0x0E%02X from 0x%02X\n", opcode, requester); return; } }
void handle_dgn_request(uint8_t requester, uint16_t dgn) { Serial.printf("DGN request from 0x%02X: 0x1%04X\n", requester, dgn); switch (dgn) { case 0xF108: send_load_disconnect_config(); return; case 0xF400: send_node_firmware(); return; case 0xF403: send_product_name(); return; case 0xF404: send_serial_info(); return; case 0xF405: send_device_id(); return; case 0xFD00: send_channel_status(); return; case 0xFD02: send_sensor_values(); return; case 0xFD04: send_captured_frames(ID_CHANNEL_LABEL, reply_channel_labels); return; case 0xFD06: send_captured_frames(ID_ALARM_CONFIG, reply_alarm_config); return; case 0xFD07: send_captured_frames(ID_ALARM_STATUS, reply_alarm_status); return; case 0xFD08: send_captured_frames(ID_ACTIVE_CHANNELS, reply_active_channels); return; case 0xFD0A: send_captured_frames(ID_CHANNEL_DETAILS, reply_channel_details); return; case 0xFD0C: send_captured_frames(ID_ANALOG_SCALING, reply_analog_scaling); return; case 0xFD0E: send_captured_frames(ID_OUTPUT_CAPABILITIES, reply_output_capabilities); return; case 0xFD10: send_captured_frames(ID_INPUT_CONFIG, reply_input_config); return; default: Serial.printf("Unhandled DGN 0x1%04X\n", dgn); return; } }

void set_output_level(uint8_t output, uint8_t percent, const char *origin) {
  if (output < 1 || output > PRIME_OUTPUT_COUNT) return;
  if (percent > 100) percent = 100;
  if (settings.outputs[output].mode == PRIME_IO_DISABLED && percent > 0) { Serial.printf("%s: output %u is disabled, ignored\n", origin, output); return; }
  output_levels[output] = percent;
  apply_output_assignment(output);
  Serial.printf("%s set Prime output %u channel 0x%02X to %u%% (%s)\n", origin, output, output + 0x03, percent, prime_io_describe(settings.outputs[output]).c_str());
}
void set_output_from_channel(uint8_t channel, uint8_t percent, const char *origin) { if (channel < 0x04 || channel > 0x0E) return; set_output_level(channel - 0x03, percent, origin); }
void handle_direct_command(uint8_t requester, const uint8_t *data, uint8_t len) { if (len < 5) return; const uint8_t command = data[0]; if (command == 0xCB && data[2] == 0xFF) { const uint8_t channel = data[3]; const bool on = data[4] != 0; if (channel == 0x0F) master_state = on; else set_output_from_channel(channel, on ? 100 : 0, "CAN 0xCB"); send_direct_ack(requester, command); send_channel_status(); return; } if (command == 0x5A && data[1] == 0x01 && data[2] == 0xFF) { set_output_from_channel(data[3], data[4] > 100 ? 100 : data[4], "CAN 0x5A"); send_direct_ack(requester, command); send_channel_status(); return; } Serial.printf("Unsupported direct command 0x%02X\n", command); send_direct_ack(requester, command); }
void handle_can_message(const twai_message_t &msg) { if (!msg.extd || msg.rtr) return; const uint32_t id = msg.identifier & 0x1FFFFFFFUL; const uint16_t service = (uint16_t)((id >> 16) & 0xFFFFUL); const uint8_t destination = (uint8_t)((id >> 8) & 0xFFUL); const uint8_t requester = (uint8_t)(id & 0xFFUL); if (destination != settings.source_address) return; if ((service & 0xFF00U) == SERVICE_OBJECT_PREFIX) { handle_object_service(requester, (uint8_t)(service & 0x00FFU), msg.data, msg.data_length_code); return; } if (service == SERVICE_DGN_REQUEST && msg.data_length_code >= 2) { handle_dgn_request(requester, u16_le(msg.data)); return; } if (service == SERVICE_DIRECT_COMMAND) { handle_direct_command(requester, msg.data, msg.data_length_code); return; } }
void receive_can() { if (!can_installed) return; twai_message_t msg = {}; while (twai_receive(&msg, 0) == ESP_OK) handle_can_message(msg); }

const char *can_pins_problem() {
  if (defaults.can_tx_pin == defaults.can_rx_pin) return "TX and RX are the same pin";
  if (defaults.can_tx_pin < 0 || !GPIO_IS_VALID_OUTPUT_GPIO(defaults.can_tx_pin)) return "TX is not an output-capable GPIO";
  if (defaults.can_rx_pin < 0 || !GPIO_IS_VALID_GPIO(defaults.can_rx_pin)) return "RX is not a GPIO on this chip";
#if CONFIG_IDF_TARGET_ESP32
  if ((defaults.can_tx_pin >= 6 && defaults.can_tx_pin <= 11) || (defaults.can_rx_pin >= 6 && defaults.can_rx_pin <= 11)) return "GPIO6-11 are reserved for the SPI flash";
#endif
  return nullptr;
}

bool start_can() { last_can_restart_ms = millis(); if (!can_installed) { const char *problem = can_pins_problem(); if (problem != nullptr) { can_pins_invalid = true; Serial.printf("CAN disabled: can_tx_pin=%d can_rx_pin=%d, %s. Fix PrimePreferences.h.\n", defaults.can_tx_pin, defaults.can_rx_pin, problem); return false; } twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT((gpio_num_t)defaults.can_tx_pin, (gpio_num_t)defaults.can_rx_pin, TWAI_MODE_NORMAL); twai_timing_config_t t_config = TWAI_TIMING_CONFIG_250KBITS(); twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL(); g_config.rx_queue_len = 256; g_config.tx_queue_len = 32; const esp_err_t err = twai_driver_install(&g_config, &t_config, &f_config); if (err != ESP_OK) { Serial.printf("twai_driver_install failed: %s\n", esp_err_to_name(err)); return false; } can_installed = true; } const esp_err_t err = twai_start(); if (err != ESP_OK) { Serial.printf("twai_start failed: %s\n", esp_err_to_name(err)); return false; } can_running = true; Serial.printf("CAN/TWAI started at 250 kbit/s on TX GPIO%d RX GPIO%d\n", defaults.can_tx_pin, defaults.can_rx_pin); return true; }
void maintain_can() { const uint32_t now = millis(); if (tx_fail_count > 0 && now - last_tx_report_ms >= CAN_TX_REPORT_INTERVAL_MS) { Serial.printf("CAN TX: %lu frames dropped (last error %s); is another node on the bus to ACK?\n", (unsigned long)tx_fail_count, esp_err_to_name(last_tx_error)); tx_fail_count = 0; last_tx_report_ms = now; } if (!can_installed) { if (!can_pins_invalid && now - last_can_restart_ms >= CAN_RESTART_INTERVAL_MS) start_can(); return; } if (now - last_can_health_ms < CAN_HEALTH_INTERVAL_MS) return; last_can_health_ms = now; twai_status_info_t status; if (twai_get_status_info(&status) != ESP_OK) return; if (status.state == TWAI_STATE_BUS_OFF) { can_running = false; twai_initiate_recovery(); } else if (status.state == TWAI_STATE_RECOVERING) { can_running = false; } else if (status.state == TWAI_STATE_STOPPED) { can_running = false; if (now - last_can_restart_ms >= CAN_RESTART_INTERVAL_MS) start_can(); } }

void print_io_assignments() { Serial.println("Tank assignments:"); for (uint8_t i = 1; i <= PRIME_TANK_COUNT; i++) Serial.printf("  tank %u: %s value=%u%%\n", (unsigned)i, prime_io_describe(settings.tanks[i]).c_str(), settings.tank_percent[i]); Serial.println("Input assignments:"); for (uint8_t i = 1; i <= PRIME_INPUT_COUNT; i++) Serial.printf("  input %u: %s state=%s\n", (unsigned)i, prime_io_describe(settings.inputs[i]).c_str(), input_states[i] ? "ON" : "OFF"); Serial.println("Output assignments:"); for (uint8_t i = 1; i <= PRIME_OUTPUT_COUNT; i++) Serial.printf("  output %u ch0x%02X: %s level=%u%%\n", (unsigned)i, i + 0x03, prime_io_describe(settings.outputs[i]).c_str(), output_levels[i]); }
void print_status() { Serial.printf("Prime emulator SA=0x%02X object=%u object2=%lu bytes (%s) name=\"%s\" serial=%lu-%04u CAN=%s\n", settings.source_address, selected_object, (unsigned long)config_object_len, config_from_flash ? "saved" : "factory", settings.product_name, (unsigned long)settings.serial_prefix, (unsigned)settings.serial_suffix, can_running ? "running" : "down"); print_io_assignments(); }
void print_help() { Serial.println("status | io | crc | identity | send | factory | defaults | save"); Serial.println("sa <0x01-0xFE> | serial <prefix> [suffix] | name <text>"); Serial.println("tank <1-6> <GPIO<n>|simulate|disabled|variable-name> | t<n> <0-100>"); Serial.println("input <1-3> <GPIO<n>|simulate|disabled|variable-name> | in<n> <0|1>"); Serial.println("output <1-11> <GPIO<n>|simulate|disabled|variable-name> | o<n> <0-100>"); Serial.println("set <variable-name> <value>"); }

bool parse_assignment_common(String raw, uint8_t max_channel, uint8_t &channel, PrimeIoAssignment &assignment) { raw.trim(); int first = raw.indexOf(' '); if (first <= 0) return false; String rest = raw.substring(first + 1); rest.trim(); int second = rest.indexOf(' '); if (second <= 0) return false; channel = (uint8_t)rest.substring(0, second).toInt(); if (channel < 1 || channel > max_channel) return false; return prime_io_parse(rest.substring(second + 1), assignment); }
bool assign_io(PrimeIoAssignment &slot, const PrimeIoAssignment &next, const char *kind, uint8_t n, bool is_output, bool needs_adc) { const char *problem = io_assignment_problem(next, &slot, is_output, needs_adc); if (problem != nullptr) { Serial.printf("%s %u not changed: GPIO%d %s\n", kind, (unsigned)n, next.pin, problem); return false; } if (!(slot.mode == PRIME_IO_PIN && next.mode == PRIME_IO_PIN && slot.pin == next.pin)) release_io_pin(slot, is_output); slot = next; configure_io_pins(); save_settings(); Serial.printf("%s %u assigned to %s\n", kind, (unsigned)n, prime_io_describe(slot).c_str()); return true; }
bool parse_tank_assignment(String raw) { uint8_t tank = 0; PrimeIoAssignment next; if (!parse_assignment_common(raw, PRIME_TANK_COUNT, tank, next)) return false; if (next.mode == PRIME_IO_VARIABLE) tank_variable_percent[tank] = settings.tank_percent[tank]; if (assign_io(settings.tanks[tank], next, "Tank", tank, false, true)) send_sensor_values(); return true; }
bool parse_input_assignment(String raw) { uint8_t input = 0; PrimeIoAssignment next; if (!parse_assignment_common(raw, PRIME_INPUT_COUNT, input, next)) return false; if (next.mode == PRIME_IO_VARIABLE) input_variable_state[input] = input_states[input]; if (assign_io(settings.inputs[input], next, "Input", input, false, false)) send_channel_status(); return true; }
bool parse_output_assignment(String raw) { uint8_t output = 0; PrimeIoAssignment next; if (!parse_assignment_common(raw, PRIME_OUTPUT_COUNT, output, next)) return false; if (assign_io(settings.outputs[output], next, "Output", output, true, false)) { send_channel_status(); } return true; }

void set_tank_percent(uint8_t tank, uint8_t percent, const char *origin, bool persist) { if (tank < 1 || tank > PRIME_TANK_COUNT) return; if (settings.tanks[tank].mode == PRIME_IO_DISABLED) { Serial.printf("%s: tank %u is disabled, ignored\n", origin, (unsigned)tank); return; } if (percent > 100) percent = 100; settings.tank_percent[tank] = percent; tank_variable_percent[tank] = percent; if (persist) save_settings(); Serial.printf("%s set tank %u to %u%% (%s)\n", origin, (unsigned)tank, (unsigned)percent, prime_io_describe(settings.tanks[tank]).c_str()); }
bool set_input_variable(uint8_t input, bool on, const char *origin) { if (input < 1 || input > PRIME_INPUT_COUNT) return false; if (settings.inputs[input].mode == PRIME_IO_DISABLED) { Serial.printf("%s: input %u is disabled, ignored\n", origin, (unsigned)input); return false; } input_variable_state[input] = on; if (settings.inputs[input].mode != PRIME_IO_PIN) input_states[input] = on; Serial.printf("%s set input %u %s (%s)\n", origin, (unsigned)input, on ? "ON" : "OFF", prime_io_describe(settings.inputs[input]).c_str()); return true; }
void set_named_variable(const String &name, const String &value_text) { uint8_t matches = 0; for (uint8_t i = 1; i <= PRIME_TANK_COUNT; i++) if (settings.tanks[i].mode == PRIME_IO_VARIABLE && name.equalsIgnoreCase(settings.tanks[i].variable)) { set_tank_percent(i, clamp_percent(value_text.toFloat()), "Variable", false); matches++; } for (uint8_t i = 1; i <= PRIME_INPUT_COUNT; i++) if (settings.inputs[i].mode == PRIME_IO_VARIABLE && name.equalsIgnoreCase(settings.inputs[i].variable)) { set_input_variable(i, value_text.toInt() != 0, "Variable"); matches++; } for (uint8_t i = 1; i <= PRIME_OUTPUT_COUNT; i++) if (settings.outputs[i].mode == PRIME_IO_VARIABLE && name.equalsIgnoreCase(settings.outputs[i].variable)) { set_output_level(i, clamp_percent(value_text.toFloat()), "Variable"); matches++; } if (matches == 0) { Serial.printf("No tank/input/output is assigned to variable \"%s\"\n", name.c_str()); return; } send_all_status(); }
void release_all_io_pins() { for (uint8_t i = 1; i <= PRIME_TANK_COUNT; i++) release_io_pin(settings.tanks[i], false); for (uint8_t i = 1; i <= PRIME_INPUT_COUNT; i++) release_io_pin(settings.inputs[i], false); for (uint8_t i = 1; i <= PRIME_OUTPUT_COUNT; i++) release_io_pin(settings.outputs[i], true); }

void handle_serial_line(String raw) { raw.trim(); if (raw.length() == 0) return; String lower = raw; lower.toLowerCase(); if (lower == "help") { print_help(); return; } if (lower == "status") { print_status(); return; } if (lower == "io") { print_io_assignments(); return; } if (lower == "crc") { object2_self_test(); return; } if (lower == "identity") { send_identity(); return; } if (lower == "send") { send_all_status(); return; } if (lower == "save") { save_settings(); return; } if (lower == "defaults") { release_all_io_pins(); reset_settings_to_defaults(); validate_io_assignments(); configure_io_pins(); send_identity(); send_all_status(); return; } if (lower == "factory") { if (object_prefs.isKey("obj2")) object_prefs.remove("obj2"); load_factory_config_object(); config_from_flash = false; Serial.println("Saved Prime configuration erased, factory Object 2 restored"); object2_self_test(); send_identity(); send_all_status(); return; } if (lower.startsWith("tank ")) { if (!parse_tank_assignment(raw)) Serial.println("Usage: tank <1-6> <GPIO<n>|simulate|disabled|variable-name>"); return; } if (lower.startsWith("input ")) { if (!parse_input_assignment(raw)) Serial.println("Usage: input <1-3> <GPIO<n>|simulate|disabled|variable-name>"); return; } if (lower.startsWith("output ")) { if (!parse_output_assignment(raw)) Serial.println("Usage: output <1-11> <GPIO<n>|simulate|disabled|variable-name>"); return; } if (lower.startsWith("set ")) { String rest = raw.substring(4); rest.trim(); int space = rest.indexOf(' '); if (space <= 0) { Serial.println("Usage: set <variable-name> <value>"); return; } set_named_variable(rest.substring(0, space), rest.substring(space + 1)); return; } if (lower.startsWith("sa ")) { const uint32_t value = parse_u32(raw.substring(3)); if (value == 0 || value > 0xFE) { Serial.println("Invalid source address. Use 0x01..0xFE."); return; } settings.source_address = (uint8_t)value; save_settings(); send_identity(); send_all_status(); return; } if (lower.startsWith("serial ")) { String rest = raw.substring(7); rest.trim(); const int space = rest.indexOf(' '); const uint32_t prefix = parse_u32(space < 0 ? rest : rest.substring(0, space)); const uint32_t suffix = space < 0 ? settings.serial_suffix : parse_u32(rest.substring(space + 1)); if (prefix == 0 || suffix > 0xFFFF) { Serial.println("Usage: serial <prefix> [suffix]"); return; } settings.serial_prefix = prefix; settings.serial_suffix = (uint16_t)suffix; save_settings(); send_serial_info(); return; } if (lower.startsWith("name ")) { String name = raw.substring(5); name.trim(); if (name.length() == 0) { Serial.println("Name cannot be empty."); return; } name.toCharArray(settings.product_name, sizeof(settings.product_name)); save_settings(); send_product_name(); return; } if (lower.startsWith("t")) { const int space = raw.indexOf(' '); if (space > 1) { const uint8_t tank = (uint8_t)raw.substring(1, space).toInt(); set_tank_percent(tank, clamp_percent(raw.substring(space + 1).toFloat()), "Serial", true); send_sensor_values(); return; } } if (lower.startsWith("in")) { const int space = raw.indexOf(' '); if (space > 2) { const uint8_t input = (uint8_t)raw.substring(2, space).toInt(); if (set_input_variable(input, raw.substring(space + 1).toInt() != 0, "Serial")) send_channel_status(); return; } } if (lower.startsWith("o")) { const int space = raw.indexOf(' '); if (space > 1) { const uint8_t output = (uint8_t)raw.substring(1, space).toInt(); set_output_level(output, clamp_percent(raw.substring(space + 1).toFloat()), "Serial"); send_channel_status(); return; } } Serial.println("Unknown command. Type help."); }
void poll_serial() { static String line; while (Serial.available()) { const char ch = (char)Serial.read(); if (ch == '\n' || ch == '\r') { if (line.length()) { handle_serial_line(line); line = ""; } } else { line += ch; if (line.length() > 180) line = ""; } } }
void poll_io_assignments() { const uint32_t now = millis(); if (now - last_io_poll_ms < IO_POLL_INTERVAL_MS) return; last_io_poll_ms = now; if (poll_tank_assignments()) send_sensor_values(); if (poll_input_assignments()) send_channel_status(); }

void setup() { defaults.load(); Serial.begin(115200); delay(500); Serial.println(); Serial.println("REDARC Prime / TVMS1280 Emulator - Arduino IDE standalone"); prime_settings_load(prefs, settings); load_config_object(); validate_io_assignments(); for (uint8_t tank = 1; tank <= PRIME_TANK_COUNT; tank++) tank_variable_percent[tank] = settings.tank_percent[tank]; configure_io_pins(); Serial.printf("Source address: 0x%02X  Product: %s\n", settings.source_address, settings.product_name); Serial.println("Type help for serial commands."); object2_self_test(); start_can(); send_identity(); send_all_status(); }
void loop() { maintain_can(); receive_can(); poll_serial(); poll_io_assignments(); const uint32_t now = millis(); if (now - last_identity_ms >= IDENTITY_INTERVAL_MS) { last_identity_ms = now; send_identity(); } if (now - last_status_ms >= STATUS_INTERVAL_MS) { last_status_ms = now; send_all_status(); } }
