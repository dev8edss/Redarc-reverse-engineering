/*
  REDARC Prime / TVMS1280 Emulator - Arduino IDE standalone port

  This sketch emulates a TVMS1280 / Prime CAN node.
  It does not use ESPHome or Home Assistant.

  Object 2 is captured from readwrite changed 6.csv:
    source address 0x24, declared length 2628 bytes, CRC-32C 0x3815A305.

  Default source address is 0x25 so it will not collide with a real Prime at 0x24.
  To replace the real Prime, disconnect the real 0x24 module and run:
    sa 0x24
*/

#include <Arduino.h>
#include <Preferences.h>
#include <driver/twai.h>
#include <pgmspace.h>

#include "PrimeObject2.h"

static constexpr gpio_num_t CAN_TX_PIN = GPIO_NUM_22;
static constexpr gpio_num_t CAN_RX_PIN = GPIO_NUM_19;

static constexpr uint8_t DEFAULT_SOURCE_ADDRESS = 0x25;  // real Prime/1280 usually uses 0x24
static constexpr uint32_t DEFAULT_SERIAL_PREFIX = 2509151234UL;
static constexpr uint16_t DEFAULT_SERIAL_SUFFIX = 0x0019;
static constexpr char DEFAULT_PRODUCT_NAME[] = "TVMS 1280 Prime";

static constexpr uint8_t DEVICE_TYPE_TVMS1280 = 0x0E;
static constexpr uint8_t DEVICE_SUBTYPE_TVMS1280 = 0x00;
static constexpr uint8_t MAIN_CONFIGURATION_OBJECT = 0x02;

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

static uint8_t source_address = DEFAULT_SOURCE_ADDRESS;
static uint32_t serial_prefix = DEFAULT_SERIAL_PREFIX;
static uint16_t serial_suffix = DEFAULT_SERIAL_SUFFIX;
static char product_name[64] = DEFAULT_PRODUCT_NAME;
static bool can_installed = false;
static bool can_running = false;
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
static uint8_t output_level[12] = {0};
static bool master_state = true;
static uint32_t last_identity_ms = 0;
static uint32_t last_status_ms = 0;

uint32_t with_sa(uint32_t base_id) { return (base_id & 0x1FFFFF00UL) | source_address; }
uint16_t u16_le(const uint8_t *d) { return (uint16_t)d[0] | ((uint16_t)d[1] << 8); }
uint32_t u32_le(const uint8_t *d) { return (uint32_t)d[0] | ((uint32_t)d[1] << 8) | ((uint32_t)d[2] << 16) | ((uint32_t)d[3] << 24); }
uint32_t parse_u32(const String &text) { String s = text; s.trim(); return strtoul(s.c_str(), nullptr, 0); }

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

void load_settings() {
  prefs.begin("primeemu", false);
  source_address = prefs.getUChar("sa", DEFAULT_SOURCE_ADDRESS);
  serial_prefix = prefs.getUInt("sp", DEFAULT_SERIAL_PREFIX);
  serial_suffix = prefs.getUShort("ss", DEFAULT_SERIAL_SUFFIX);
  String saved_name = prefs.getString("name", DEFAULT_PRODUCT_NAME);
  saved_name.toCharArray(product_name, sizeof(product_name));
  if (source_address == 0x00 || source_address == 0xFF) source_address = DEFAULT_SOURCE_ADDRESS;
  if (product_name[0] == '\0') strncpy(product_name, DEFAULT_PRODUCT_NAME, sizeof(product_name) - 1);
}
void save_settings() { prefs.putUChar("sa", source_address); prefs.putUInt("sp", serial_prefix); prefs.putUShort("ss", serial_suffix); prefs.putString("name", product_name); Serial.println("Settings saved to NVS"); }

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
void send_direct_ack(uint8_t requester, uint8_t command) { send_frame8(ID_DIRECT_ACK_BASE | ((uint32_t)requester << 8) | source_address, 0x01, command, 0x00, 0xFF, 0x00, 0xFF, 0xFF, 0xFF); }
void send_programming_status(uint8_t requester, uint8_t status, uint8_t opcode) { send_frame8(ID_SERVICE_ACK_BASE | ((uint32_t)requester << 8) | source_address, status, 0x00, opcode, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF); }

void send_load_disconnect_config() { send_frame8(with_sa(ID_LOAD_DISCONNECT_CONFIG), 0xEC, 0xD8, 0x27, 0x24, 0x2C, 0x28, 0x3C, 0x00); }
void send_channel_status() { send_frame8(with_sa(ID_CHANNEL_STATUS),0x01,0x00,0x00,0x00,0x24,0x24,0x24,0x24); send_frame8(with_sa(ID_CHANNEL_STATUS),0x08,0x24,0x24,0x24,0x24,0x24,0x24,0x24); send_frame8(with_sa(ID_CHANNEL_STATUS),0x0F,master_state?0x01:0x00,0x0C,0xFF,0xFF,0xFF,0xFF,0xFF); }
void send_sensor_values() { send_frame8(with_sa(ID_SENSOR_VALUES),0x11,0x00,0x00,0x48,0x36,0x75,0x00,0xFF); send_frame8(with_sa(ID_SENSOR_VALUES),0x14,0x78,0x00,0x00,0x00,0x00,0x00,0xFF); send_frame8(with_sa(ID_SENSOR_VALUES),0x17,0x00,0x00,0x00,0x00,0x00,0x00,0xFF); send_frame8(with_sa(ID_SENSOR_VALUES),0x1A,0x00,0x00,0xFF,0xFF,0xFF,0xFF,0xFF); }
void send_node_firmware() { send_frame8(with_sa(ID_NODE_FIRMWARE),0xBF,0x00,0x03,0x04,0,0,0,0); send_frame8(with_sa(ID_NODE_FIRMWARE),0xBF,0x00,0,0x04,0,0,0x01,0); }
void send_product_name() { const size_t len = strlen(product_name); const uint8_t seg_count = (uint8_t)(len / 7 + 1); for (uint8_t seg = 0; seg < seg_count; seg++) { uint8_t data[8] = {seg,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF}; for (uint8_t i = 0; i < 7; i++) { size_t pos = (size_t)seg * 7 + i; if (pos < len) data[1 + i] = (uint8_t)product_name[pos]; } send_frame(with_sa(ID_NODE_PRODUCT_NAME), data, 8); } }
void send_serial_info() { send_frame8(with_sa(ID_NODE_SERIAL_INFO),(uint8_t)(serial_prefix&0xFF),(uint8_t)((serial_prefix>>8)&0xFF),(uint8_t)((serial_prefix>>16)&0xFF),(uint8_t)((serial_prefix>>24)&0xFF),(uint8_t)(serial_suffix&0xFF),(uint8_t)((serial_suffix>>8)&0xFF),DEVICE_TYPE_TVMS1280,DEVICE_SUBTYPE_TVMS1280); }
void send_device_id() { send_frame8(with_sa(ID_NODE_DEVICE_ID),0,0,0,0,0,source_address,0x01,0); }
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
  const uint32_t data_id = ID_SERVICE_DATA_BASE | ((uint32_t)requester << 8) | source_address;
  for (uint32_t pos = 0; pos < requested_length; pos += 8) { uint8_t frame[8] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF}; const uint8_t chunk = (uint8_t)((requested_length - pos) < 8UL ? (requested_length - pos) : 8UL); memcpy(frame, block + pos, chunk); send_frame(data_id, frame, 8); }
  const uint32_t block_crc = crc32c(block, requested_length); const uint32_t trailer_id = ID_SERVICE_TRAILER_BASE | ((uint32_t)requester << 8) | source_address;
  send_frame8(trailer_id,(uint8_t)(requested_length&0xFF),(uint8_t)((requested_length>>8)&0xFF),(uint8_t)((requested_length>>16)&0xFF),(uint8_t)((requested_length>>24)&0xFF),(uint8_t)(block_crc&0xFF),(uint8_t)((block_crc>>8)&0xFF),(uint8_t)((block_crc>>16)&0xFF),(uint8_t)((block_crc>>24)&0xFF));
  Serial.printf("Object %u read offset=%lu length=%lu crc=0x%08lX\n", selected_object, (unsigned long)offset, (unsigned long)requested_length, (unsigned long)block_crc); free(block);
}
void end_config_write() { write_active = false; write_closed = false; write_overflow = false; write_block_len = 0; free(write_staging); write_staging = nullptr; }
void handle_write_capability(uint8_t requester) { send_frame8(ID_SERVICE_DATA_BASE | ((uint32_t)requester << 8) | source_address,0x00,0x04,0x00,0x00,0xFF,0xFF,0xFF,0xFF); send_frame8(ID_SERVICE_TRAILER_BASE | ((uint32_t)requester << 8) | source_address,0x04,0x00,0x00,0x00,0xDD,0xEF,0xB9,0xD6); }
void handle_write_start(uint8_t requester) { end_config_write(); if (selected_object == MAIN_CONFIGURATION_OBJECT) write_staging = (uint8_t *)malloc(MAX_CONFIG_OBJECT_SIZE); if (write_staging == nullptr) { send_programming_status(requester, PROGRAMMING_STATUS_ERROR, 0x87); return; } memset(write_staging,0xFF,MAX_CONFIG_OBJECT_SIZE); memset(write_received,0,sizeof(write_received)); write_active = true; send_programming_status(requester,PROGRAMMING_STATUS_OK,0x87); Serial.printf("Started Prime Object 2 write from 0x%02X\n", requester); }
void handle_write_data(const uint8_t *data, uint8_t len) { if (!write_active) return; if (write_block_len + len > WRITE_WINDOW_SIZE) { write_overflow = true; return; } memcpy(write_block + write_block_len, data, len); write_block_len += len; }
void handle_write_block(uint8_t requester, const uint8_t *data, uint8_t len) { send_programming_status(requester,PROGRAMMING_STATUS_BUSY,0x88); const uint32_t offset = len >= 4 ? u32_le(data) : 0; const uint32_t expected_crc = len >= 8 ? u32_le(data + 4) : 0; const uint32_t actual_crc = crc32c(write_block, write_block_len); const bool valid = write_active && len >= 8 && !write_overflow && write_block_len > 0 && offset <= MAX_CONFIG_OBJECT_SIZE && write_block_len <= MAX_CONFIG_OBJECT_SIZE - offset && actual_crc == expected_crc; if (valid) { memcpy(write_staging + offset, write_block, write_block_len); for (uint32_t i = offset; i < offset + write_block_len; i++) write_received[i >> 3] |= (uint8_t)(1U << (i & 7)); send_programming_status(requester, PROGRAMMING_STATUS_OK, 0x88); } else send_programming_status(requester, PROGRAMMING_STATUS_ERROR, 0x88); write_block_len = 0; write_overflow = false; }
void handle_object_close(uint8_t requester) { selected_object = 0xFF; if (!write_active) { send_programming_status(requester, PROGRAMMING_STATUS_OK, 0x89); return; } const bool valid = write_block_len == 0 && !write_overflow; write_active = false; write_closed = valid; send_programming_status(requester, valid ? PROGRAMMING_STATUS_OK : PROGRAMMING_STATUS_ERROR, 0x89); if (!valid) end_config_write(); }
bool write_range_received(uint32_t length) { for (uint32_t i = 0; i < length; i++) if ((write_received[i >> 3] & (uint8_t)(1U << (i & 7))) == 0) return false; return true; }
void handle_write_commit(uint8_t requester) { uint32_t declared_len=0, stored_crc=0, calc_crc=0; bool complete=false, saved=false; bool valid = write_closed && write_staging != nullptr && validate_config_object(write_staging, MAX_CONFIG_OBJECT_SIZE, declared_len, stored_crc, calc_crc); if (valid) valid = complete = write_range_received(declared_len); if (valid) valid = saved = object_prefs.putBytes("obj2", write_staging, declared_len) == declared_len; if (valid) { memcpy(config_object, write_staging, declared_len); config_object_len = declared_len; config_from_flash = true; send_programming_status(requester, PROGRAMMING_STATUS_OK, 0x8A); Serial.printf("Committed and saved Prime Object 2: %lu bytes CRC-32C=0x%08lX\n", (unsigned long)declared_len, (unsigned long)stored_crc); } else { send_programming_status(requester, PROGRAMMING_STATUS_ERROR, 0x8A); Serial.printf("Rejected Prime commit: length=%lu complete=%s saved=%s\n", (unsigned long)declared_len, complete ? "yes" : "no", saved ? "yes" : "no"); } end_config_write(); if (valid) { send_identity(); send_all_status(); } }
void handle_object_service(uint8_t requester, uint8_t opcode, const uint8_t *data, uint8_t len) { switch (opcode) { case 0x81: handle_write_data(data,len); return; case 0x83: handle_write_capability(requester); return; case 0x85: handle_object_select(requester,data,len); return; case 0x86: handle_object_read(requester,data,len); return; case 0x87: handle_write_start(requester); return; case 0x88: handle_write_block(requester,data,len); return; case 0x89: handle_object_close(requester); return; case 0x8A: handle_write_commit(requester); return; default: Serial.printf("Unhandled object service 0x0E%02X from 0x%02X\n", opcode, requester); return; } }
void handle_dgn_request(uint8_t requester, uint16_t dgn) { Serial.printf("DGN request from 0x%02X: 0x1%04X\n", requester, dgn); switch (dgn) { case 0xF108: send_load_disconnect_config(); return; case 0xF400: send_node_firmware(); return; case 0xF403: send_product_name(); return; case 0xF404: send_serial_info(); return; case 0xF405: send_device_id(); return; case 0xFD00: send_channel_status(); return; case 0xFD02: send_sensor_values(); return; case 0xFD04: send_captured_frames(ID_CHANNEL_LABEL, reply_channel_labels); return; case 0xFD06: send_captured_frames(ID_ALARM_CONFIG, reply_alarm_config); return; case 0xFD07: send_captured_frames(ID_ALARM_STATUS, reply_alarm_status); return; case 0xFD08: send_captured_frames(ID_ACTIVE_CHANNELS, reply_active_channels); return; case 0xFD0A: send_captured_frames(ID_CHANNEL_DETAILS, reply_channel_details); return; case 0xFD0C: send_captured_frames(ID_ANALOG_SCALING, reply_analog_scaling); return; case 0xFD0E: send_captured_frames(ID_OUTPUT_CAPABILITIES, reply_output_capabilities); return; case 0xFD10: send_captured_frames(ID_INPUT_CONFIG, reply_input_config); return; default: Serial.printf("Unhandled DGN 0x1%04X\n", dgn); return; } }
void set_output_from_channel(uint8_t channel, uint8_t percent, const char *origin) { if (channel < 0x04 || channel > 0x0E) return; const uint8_t output = channel - 0x03; if (percent > 100) percent = 100; output_level[output] = percent; Serial.printf("%s set Prime output channel 0x%02X to %u%%\n", origin, channel, percent); }
void handle_direct_command(uint8_t requester, const uint8_t *data, uint8_t len) { if (len < 5) return; const uint8_t command = data[0]; if (command == 0xCB && data[2] == 0xFF) { const uint8_t channel = data[3]; const bool on = data[4] != 0; if (channel == 0x0F) master_state = on; else set_output_from_channel(channel, on ? 100 : 0, "CAN 0xCB"); send_direct_ack(requester, command); send_channel_status(); return; } if (command == 0x5A && data[1] == 0x01 && data[2] == 0xFF) { set_output_from_channel(data[3], data[4] > 100 ? 100 : data[4], "CAN 0x5A"); send_direct_ack(requester, command); send_channel_status(); return; } Serial.printf("Unsupported direct command 0x%02X\n", command); send_direct_ack(requester, command); }
void handle_can_message(const twai_message_t &msg) { if (!msg.extd || msg.rtr) return; const uint32_t id = msg.identifier & 0x1FFFFFFFUL; const uint16_t service = (uint16_t)((id >> 16) & 0xFFFFUL); const uint8_t destination = (uint8_t)((id >> 8) & 0xFFUL); const uint8_t requester = (uint8_t)(id & 0xFFUL); if (destination != source_address) return; if ((service & 0xFF00U) == SERVICE_OBJECT_PREFIX) { handle_object_service(requester, (uint8_t)(service & 0x00FFU), msg.data, msg.data_length_code); return; } if (service == SERVICE_DGN_REQUEST && msg.data_length_code >= 2) { handle_dgn_request(requester, u16_le(msg.data)); return; } if (service == SERVICE_DIRECT_COMMAND) { handle_direct_command(requester, msg.data, msg.data_length_code); return; } }
void receive_can() { if (!can_installed) return; twai_message_t msg = {}; while (twai_receive(&msg, 0) == ESP_OK) handle_can_message(msg); }
bool start_can() { last_can_restart_ms = millis(); if (!can_installed) { twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(CAN_TX_PIN, CAN_RX_PIN, TWAI_MODE_NORMAL); twai_timing_config_t t_config = TWAI_TIMING_CONFIG_250KBITS(); twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL(); g_config.rx_queue_len = 256; g_config.tx_queue_len = 32; const esp_err_t err = twai_driver_install(&g_config, &t_config, &f_config); if (err != ESP_OK) { Serial.printf("twai_driver_install failed: %s\n", esp_err_to_name(err)); return false; } can_installed = true; } const esp_err_t err = twai_start(); if (err != ESP_OK) { Serial.printf("twai_start failed: %s\n", esp_err_to_name(err)); return false; } can_running = true; Serial.printf("CAN/TWAI started at 250 kbit/s on TX GPIO%d RX GPIO%d\n", (int)CAN_TX_PIN, (int)CAN_RX_PIN); return true; }
void maintain_can() { const uint32_t now = millis(); if (tx_fail_count > 0 && now - last_tx_report_ms >= CAN_TX_REPORT_INTERVAL_MS) { Serial.printf("CAN TX: %lu frames dropped (last error %s); is another node on the bus to ACK?\n", (unsigned long)tx_fail_count, esp_err_to_name(last_tx_error)); tx_fail_count = 0; last_tx_report_ms = now; } if (!can_installed) { if (now - last_can_restart_ms >= CAN_RESTART_INTERVAL_MS) start_can(); return; } if (now - last_can_health_ms < CAN_HEALTH_INTERVAL_MS) return; last_can_health_ms = now; twai_status_info_t status; if (twai_get_status_info(&status) != ESP_OK) return; if (status.state == TWAI_STATE_BUS_OFF) { can_running = false; twai_initiate_recovery(); } else if (status.state == TWAI_STATE_RECOVERING) { can_running = false; } else if (status.state == TWAI_STATE_STOPPED) { can_running = false; if (now - last_can_restart_ms >= CAN_RESTART_INTERVAL_MS) start_can(); } }
void print_status() { Serial.printf("Prime emulator SA=0x%02X object=%u object2=%lu bytes (%s) name=\"%s\" serial=%lu-%04u CAN=%s\n", source_address, selected_object, (unsigned long)config_object_len, config_from_flash ? "saved" : "factory", product_name, (unsigned long)serial_prefix, (unsigned)serial_suffix, can_running ? "running" : "down"); }
void print_help() { Serial.println("status | crc | identity | send | factory"); Serial.println("sa <0x01-0xFE> | serial <prefix> [suffix] | name <text> | save"); Serial.println("o<n> <0-100> where n=1..11 maps to Prime channels 0x04..0x0E"); }
void handle_serial_line(String raw) { raw.trim(); if (raw.length() == 0) return; String lower = raw; lower.toLowerCase(); if (lower == "help") { print_help(); return; } if (lower == "status") { print_status(); return; } if (lower == "crc") { object2_self_test(); return; } if (lower == "identity") { send_identity(); return; } if (lower == "send") { send_all_status(); return; } if (lower == "save") { save_settings(); return; } if (lower == "factory") { if (object_prefs.isKey("obj2")) object_prefs.remove("obj2"); load_factory_config_object(); config_from_flash = false; Serial.println("Saved Prime configuration erased, factory Object 2 restored"); object2_self_test(); send_identity(); send_all_status(); return; } if (lower.startsWith("sa ")) { const uint32_t value = parse_u32(raw.substring(3)); if (value == 0 || value > 0xFE) { Serial.println("Invalid source address. Use 0x01..0xFE."); return; } source_address = (uint8_t)value; save_settings(); send_identity(); send_all_status(); return; } if (lower.startsWith("serial ")) { String rest = raw.substring(7); rest.trim(); const int space = rest.indexOf(' '); const uint32_t prefix = parse_u32(space < 0 ? rest : rest.substring(0, space)); const uint32_t suffix = space < 0 ? serial_suffix : parse_u32(rest.substring(space + 1)); if (prefix == 0 || suffix > 0xFFFF) { Serial.println("Usage: serial <prefix> [suffix]"); return; } serial_prefix = prefix; serial_suffix = (uint16_t)suffix; save_settings(); send_serial_info(); return; } if (lower.startsWith("name ")) { String name = raw.substring(5); name.trim(); if (name.length() == 0) { Serial.println("Name cannot be empty."); return; } name.toCharArray(product_name, sizeof(product_name)); save_settings(); send_product_name(); return; } if (lower.startsWith("o")) { const int space = raw.indexOf(' '); if (space > 1) { const uint8_t output = (uint8_t)raw.substring(1, space).toInt(); const int pct = constrain(raw.substring(space + 1).toInt(), 0, 100); if (output < 1 || output > 11) { Serial.println("Output number must be 1..11."); return; } output_level[output] = (uint8_t)pct; send_channel_status(); return; } } Serial.println("Unknown command. Type help."); }
void poll_serial() { static String line; while (Serial.available()) { const char ch = (char)Serial.read(); if (ch == '\n' || ch == '\r') { if (line.length()) { handle_serial_line(line); line = ""; } } else { line += ch; if (line.length() > 160) line = ""; } } }

void setup() { Serial.begin(115200); delay(500); Serial.println(); Serial.println("REDARC Prime / TVMS1280 Emulator - Arduino IDE standalone"); load_settings(); load_config_object(); Serial.printf("Source address: 0x%02X  Product: %s\n", source_address, product_name); Serial.println("Type help for serial commands."); object2_self_test(); start_can(); send_identity(); send_all_status(); }
void loop() { maintain_can(); receive_can(); poll_serial(); const uint32_t now = millis(); if (now - last_identity_ms >= IDENTITY_INTERVAL_MS) { last_identity_ms = now; send_identity(); } if (now - last_status_ms >= STATUS_INTERVAL_MS) { last_status_ms = now; send_all_status(); } }
