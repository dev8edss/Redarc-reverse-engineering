/*
  REDARC TVMS Rogue Emulator - Arduino IDE port

  Standalone ESP32/TWAI port of the emulated-rogue ESPHome component.
  It emulates only a TVMS Rogue CAN node. It does not use ESPHome or
  Home Assistant.

  Default hardware target:
    M5Stack Atom Lite + Atomic CAN Base / CA-IS3050G
    CAN TX GPIO22, CAN RX GPIO19, 250 kbit/s, extended IDs
*/

#include <Arduino.h>
#include <driver/twai.h>

static constexpr gpio_num_t CAN_TX_PIN = GPIO_NUM_22;
static constexpr gpio_num_t CAN_RX_PIN = GPIO_NUM_19;
static constexpr uint8_t SOURCE_ADDRESS = 0x36;   // keep different from real Rogue 0x30

static constexpr uint32_t ID_LOAD_DISCONNECT_CONFIG = 0x13F10800UL;
static constexpr uint32_t ID_CHANNEL_STATUS         = 0x1BFD0000UL;
static constexpr uint32_t ID_SENSOR_VALUES          = 0x1BFD0200UL;
static constexpr uint32_t ID_ACTIVE_CHANNELS        = 0x17FD0800UL;
static constexpr uint32_t ID_OUTPUT_CAPABILITIES    = 0x17FD0E00UL;
static constexpr uint32_t ID_OUTPUT_LEVELS          = 0x1BFD1200UL;
static constexpr uint32_t ID_OUTPUT_ACTIVITY        = 0x1BFD1400UL;
static constexpr uint32_t ID_NODE_FIRMWARE          = 0x17F40000UL;
static constexpr uint32_t ID_NODE_PRODUCT_NAME      = 0x17F40300UL;
static constexpr uint32_t ID_NODE_SERIAL_INFO       = 0x17F40400UL;
static constexpr uint32_t ID_NODE_DEVICE_ID         = 0x17F40500UL;
static constexpr uint32_t ID_DIRECT_ACK_BASE        = 0x0F040000UL;

static constexpr uint16_t SERVICE_DGN_REQUEST       = 0x0F03;
static constexpr uint16_t SERVICE_DIRECT_COMMAND    = 0x0F00;
static constexpr uint16_t SERVICE_LEGACY_DIM        = 0x0F05;

static constexpr uint8_t CHANNEL_MASTER             = 0x0B;
static constexpr uint8_t CHANNEL_OUTPUT_1           = 0x0C;
static constexpr uint8_t CHANNEL_OUTPUT_10          = 0x15;

static constexpr uint32_t IDENTITY_INTERVAL_MS      = 1000UL;
static constexpr uint32_t STATUS_INTERVAL_MS        = 1000UL;
static constexpr uint32_t HOLD_DIM_STEP_MS          = 100UL;
static constexpr uint8_t HOLD_DIM_STEP_PERCENT      = 2;

static uint8_t output_levels[11] = {0};       // outputs 1..10, percent
static bool input_states[9] = {false};        // inputs 1..8
static bool master_state = false;
static uint8_t tank1_percent = 50;
static uint8_t tank2_percent = 75;
static uint16_t input_voltage_mv = 13500;
static uint16_t input_current_ma = 2500;
static int8_t hold_dim_direction[11] = {0};
static uint32_t hold_dim_last_step_ms[11] = {0};
static uint32_t last_identity_ms = 0;
static uint32_t last_status_ms = 0;

static constexpr uint32_t SERIAL_PREFIX = 2606260001UL;
static constexpr uint16_t SERIAL_SUFFIX = 0x0013;
static constexpr uint8_t DEVICE_TYPE = 0x16;   // TVMS Rogue / DPDM
static constexpr uint8_t DEVICE_SUBTYPE = 0x00;
static const char PRODUCT_NAME[] = "TVMS Rogue";

uint32_t with_sa(uint32_t base_id) {
  return (base_id & 0x1FFFFF00UL) | SOURCE_ADDRESS;
}

uint16_t u16_le(const uint8_t *d) {
  return (uint16_t) d[0] | ((uint16_t) d[1] << 8);
}

uint8_t clamp_percent(float value) {
  if (isnan(value) || value <= 0.0f) return 0;
  if (value >= 100.0f) return 100;
  return (uint8_t) lroundf(value);
}

void send_frame(uint32_t id, const uint8_t *data, uint8_t len) {
  twai_message_t msg = {};
  msg.identifier = id & 0x1FFFFFFFUL;
  msg.extd = 1;
  msg.rtr = 0;
  msg.data_length_code = len > 8 ? 8 : len;
  for (uint8_t i = 0; i < msg.data_length_code; i++) msg.data[i] = data[i];
  esp_err_t err = twai_transmit(&msg, pdMS_TO_TICKS(10));
  if (err != ESP_OK) {
    Serial.printf("CAN TX failed id=0x%08lX err=%d\n", (unsigned long) id, (int) err);
  }
}

void send_frame8(uint32_t id,
                 uint8_t a, uint8_t b, uint8_t c, uint8_t d,
                 uint8_t e, uint8_t f, uint8_t g, uint8_t h) {
  uint8_t data[8] = {a, b, c, d, e, f, g, h};
  send_frame(id, data, 8);
}

void send_direct_ack(uint8_t requester, uint8_t command) {
  const uint32_t id = ID_DIRECT_ACK_BASE | ((uint32_t) requester << 8) | SOURCE_ADDRESS;
  send_frame8(id, 0x01, command, 0x00, 0xFF, 0x00, 0xFF, 0xFF, 0xFF);
}

void set_output_level(uint8_t output, uint8_t percent, const char *origin) {
  if (output < 1 || output > 10) return;
  if (percent > 100) percent = 100;
  output_levels[output] = percent;
  Serial.printf("%s set output %u to %u%%\n", origin, output, percent);
}

void set_master(bool on, const char *origin) {
  master_state = on;
  Serial.printf("%s set master %s\n", origin, on ? "ON" : "OFF");
  if (!on) {
    for (uint8_t output = 1; output <= 10; output++) output_levels[output] = 0;
  }
}

void send_load_disconnect_config() {
  // Captured object-derived Rogue payload: trigger Never, 10.2V/11.6V, 23/61% SOC.
  send_frame8(with_sa(ID_LOAD_DISCONNECT_CONFIG),
              0xEC, 0xD8, 0x27, 0x50, 0x2D, 0x17, 0x3D, 0x00);
}

void send_channel_status() {
  uint8_t page1[8] = {0x01};
  for (uint8_t input = 1; input <= 7; input++) page1[input] = input_states[input] ? 1 : 0;
  send_frame(with_sa(ID_CHANNEL_STATUS), page1, 8);

  send_frame8(with_sa(ID_CHANNEL_STATUS),
              0x08, input_states[8] ? 1 : 0, 0xFF, 0xFF,
              master_state ? 1 : 0,
              output_levels[1] > 0 ? 1 : 0,
              output_levels[2] > 0 ? 1 : 0,
              output_levels[3] > 0 ? 1 : 0);

  send_frame8(with_sa(ID_CHANNEL_STATUS),
              0x0F,
              output_levels[4] > 0 ? 1 : 0,
              output_levels[5] > 0 ? 1 : 0,
              output_levels[6] > 0 ? 1 : 0,
              output_levels[7] > 0 ? 1 : 0,
              output_levels[8] > 0 ? 1 : 0,
              output_levels[9] > 0 ? 1 : 0,
              output_levels[10] > 0 ? 1 : 0);

  send_frame8(with_sa(ID_CHANNEL_STATUS), 0x18, 0xF8, 0xF8, 0xF8, 0xF8, 0xF8, 0xF8, 0xF8);
  send_frame8(with_sa(ID_CHANNEL_STATUS), 0x1F, 0xF8, 0xF8, 0xF8, 0xFF, 0xFF, 0xFF, 0xFF);
}

void send_sensor_values() {
  send_frame8(with_sa(ID_SENSOR_VALUES),
              0x09, tank1_percent, tank2_percent, 0x00, 0x00, 0xFF, 0xFF, 0xFF);

  send_frame8(with_sa(ID_SENSOR_VALUES),
              0x16,
              (uint8_t) (input_voltage_mv & 0xFF),
              (uint8_t) ((input_voltage_mv >> 8) & 0xFF),
              (uint8_t) (input_current_ma & 0xFF),
              (uint8_t) ((input_current_ma >> 8) & 0xFF),
              0xFF, 0xFF, 0xFF);
}

void send_output_levels() {
  send_frame8(with_sa(ID_OUTPUT_LEVELS),
              0x0C, output_levels[1], output_levels[2], output_levels[3],
              output_levels[4], output_levels[5], output_levels[6], output_levels[7]);
  send_frame8(with_sa(ID_OUTPUT_LEVELS),
              0x13, output_levels[8], output_levels[9], output_levels[10],
              0xFF, 0xFF, 0x00, 0x00);
  send_frame8(with_sa(ID_OUTPUT_LEVELS), 0x1A, 0, 0, 0, 0, 0, 0, 0);
  send_frame8(with_sa(ID_OUTPUT_LEVELS), 0x21, 0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF);
}

void send_output_activity() {
  send_frame8(with_sa(ID_OUTPUT_ACTIVITY),
              0x0C,
              hold_dim_direction[1] ? 0x02 : 0x00,
              hold_dim_direction[2] ? 0x02 : 0x00,
              hold_dim_direction[3] ? 0x02 : 0x00,
              hold_dim_direction[4] ? 0x02 : 0x00,
              hold_dim_direction[5] ? 0x02 : 0x00,
              hold_dim_direction[6] ? 0x02 : 0x00,
              hold_dim_direction[7] ? 0x02 : 0x00);
  send_frame8(with_sa(ID_OUTPUT_ACTIVITY),
              0x13,
              hold_dim_direction[8] ? 0x02 : 0x00,
              hold_dim_direction[9] ? 0x02 : 0x00,
              hold_dim_direction[10] ? 0x02 : 0x00,
              0xFF, 0xFF, 0xFF, 0xFF);
}

void send_active_channels() {
  send_frame8(with_sa(ID_ACTIVE_CHANNELS), 0x21, 0xFF, 0xFF, 0x1E, 0xFF, 0xFF, 0xFF, 0xFF);
}

void send_output_capabilities() {
  const uint8_t caps[10] = {0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x01, 0x03, 0x03};
  for (uint8_t i = 0; i < 10; i++) {
    send_frame8(with_sa(ID_OUTPUT_CAPABILITIES), CHANNEL_OUTPUT_1 + i, caps[i], 0, 0, 0, 0, 0, 0);
  }
}

void send_all_status() {
  send_channel_status();
  send_sensor_values();
  send_output_levels();
  send_output_activity();
  send_active_channels();
  send_output_capabilities();
}

void send_node_firmware() {
  send_frame8(with_sa(ID_NODE_FIRMWARE), 0x43, 0x01, 0x01, 0x04, 0x00, 0x00, 0x00, 0x00);
  send_frame8(with_sa(ID_NODE_FIRMWARE), 0x43, 0x01, 0x00, 0x04, 0x00, 0x00, 0x01, 0x00);
}

void send_product_name() {
  const size_t len = strlen(PRODUCT_NAME);
  uint8_t seg_count = (uint8_t) (len / 7 + 1);
  for (uint8_t seg = 0; seg < seg_count; seg++) {
    uint8_t data[8] = {seg, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    for (uint8_t i = 0; i < 7; i++) {
      size_t pos = (size_t) seg * 7 + i;
      if (pos < len) data[1 + i] = (uint8_t) PRODUCT_NAME[pos];
    }
    send_frame(with_sa(ID_NODE_PRODUCT_NAME), data, 8);
  }
}

void send_serial_info() {
  send_frame8(with_sa(ID_NODE_SERIAL_INFO),
              (uint8_t) (SERIAL_PREFIX & 0xFF),
              (uint8_t) ((SERIAL_PREFIX >> 8) & 0xFF),
              (uint8_t) ((SERIAL_PREFIX >> 16) & 0xFF),
              (uint8_t) ((SERIAL_PREFIX >> 24) & 0xFF),
              (uint8_t) (SERIAL_SUFFIX & 0xFF),
              (uint8_t) ((SERIAL_SUFFIX >> 8) & 0xFF),
              0x16, 0x00);
}

void send_device_id() {
  send_frame8(with_sa(ID_NODE_DEVICE_ID), 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00);
}

void send_identity() {
  send_node_firmware();
  send_product_name();
  send_serial_info();
  send_device_id();
  send_load_disconnect_config();
}

void handle_dgn_request(uint8_t requester, uint16_t dgn) {
  Serial.printf("DGN request from 0x%02X: 0x1%04X\n", requester, dgn);
  switch (dgn) {
    case 0xF108: send_load_disconnect_config(); return;
    case 0xF400: send_node_firmware(); return;
    case 0xF403: send_product_name(); return;
    case 0xF404: send_serial_info(); return;
    case 0xF405: send_device_id(); return;
    case 0xFD00: send_channel_status(); return;
    case 0xFD02: send_sensor_values(); return;
    case 0xFD08: send_active_channels(); return;
    case 0xFD0E: send_output_capabilities(); return;
    case 0xFD12: send_output_levels(); return;
    case 0xFD14: send_output_activity(); return;
    default: Serial.printf("Unhandled DGN 0x1%04X\n", dgn); return;
  }
}

void handle_direct_command(uint8_t requester, const uint8_t *data, uint8_t len) {
  if (len < 5) return;
  const uint8_t command = data[0];

  if (command == 0xCB && data[2] == 0xFF) {
    const uint8_t channel = data[3];
    const bool state = data[4] != 0;
    if (channel == CHANNEL_MASTER) {
      set_master(state, "CAN 0xCB");
    } else if (channel >= CHANNEL_OUTPUT_1 && channel <= CHANNEL_OUTPUT_10) {
      const uint8_t output = channel - CHANNEL_MASTER;
      hold_dim_direction[output] = 0;
      uint8_t level = state ? output_levels[output] : 0;
      if (state && level == 0) level = 100;
      set_output_level(output, level, "CAN 0xCB");
    }
    send_direct_ack(requester, command);
    send_all_status();
    return;
  }

  if (command == 0x5A && data[1] == 0x01 && data[2] == 0xFF) {
    const uint8_t channel = data[3];
    if (channel >= CHANNEL_OUTPUT_1 && channel <= CHANNEL_OUTPUT_10) {
      const uint8_t output = channel - CHANNEL_MASTER;
      hold_dim_direction[output] = 0;
      set_output_level(output, data[4] > 100 ? 100 : data[4], "CAN 0x5A");
    }
    send_direct_ack(requester, command);
    send_all_status();
    return;
  }

  Serial.printf("Unsupported command 0x%02X\n", command);
  send_direct_ack(requester, command);
}

void handle_legacy_dim(const uint8_t *data, uint8_t len) {
  if (len < 3) return;
  const uint8_t channel = data[0];
  const uint8_t direction = data[2];
  if (channel < CHANNEL_OUTPUT_1 || channel > CHANNEL_OUTPUT_10) return;
  const uint8_t output = channel - CHANNEL_MASTER;

  if (direction == 0x01) hold_dim_direction[output] = -1;
  else if (direction == 0x64) hold_dim_direction[output] = 1;
  else if (direction == 0xFF) hold_dim_direction[output] = 0;

  Serial.printf("Hold dim output %u direction 0x%02X\n", output, direction);
  send_output_activity();
}

void handle_can_message(const twai_message_t &msg) {
  if (!msg.extd || msg.rtr) return;
  const uint32_t id = msg.identifier & 0x1FFFFFFFUL;
  const uint16_t service = (uint16_t) ((id >> 16) & 0xFFFFUL);
  const uint8_t destination = (uint8_t) ((id >> 8) & 0xFFUL);
  const uint8_t requester = (uint8_t) (id & 0xFFUL);
  if (destination != SOURCE_ADDRESS) return;

  if (service == SERVICE_DGN_REQUEST && msg.data_length_code >= 2) {
    handle_dgn_request(requester, u16_le(msg.data));
    return;
  }
  if (service == SERVICE_DIRECT_COMMAND) {
    handle_direct_command(requester, msg.data, msg.data_length_code);
    return;
  }
  if (service == SERVICE_LEGACY_DIM) {
    handle_legacy_dim(msg.data, msg.data_length_code);
    return;
  }
}

void receive_can() {
  twai_message_t msg = {};
  while (twai_receive(&msg, 0) == ESP_OK) handle_can_message(msg);
}

void print_status() {
  Serial.printf("SA=0x%02X master=%s tank1=%u%% tank2=%u%% Vin=%.3fV Iin=%.3fA\n",
                SOURCE_ADDRESS, master_state ? "ON" : "OFF", tank1_percent, tank2_percent,
                input_voltage_mv / 1000.0f, input_current_ma / 1000.0f);
  for (uint8_t output = 1; output <= 10; output++) Serial.printf("O%u=%u%% ", output, output_levels[output]);
  Serial.println();
}

void handle_serial_line(String line) {
  line.trim();
  line.toLowerCase();
  if (line.length() == 0) return;
  if (line == "help") {
    Serial.println("status | t1 <0-100> | t2 <0-100> | v <mV> | i <mA> | m <0|1> | o<n> <0-100> | identity | send");
    return;
  }
  if (line == "status") { print_status(); return; }
  if (line == "identity") { send_identity(); return; }
  if (line == "send") { send_all_status(); return; }
  if (line.startsWith("t1 ")) { tank1_percent = clamp_percent(line.substring(3).toFloat()); send_sensor_values(); return; }
  if (line.startsWith("t2 ")) { tank2_percent = clamp_percent(line.substring(3).toFloat()); send_sensor_values(); return; }
  if (line.startsWith("v ")) { input_voltage_mv = (uint16_t) constrain(line.substring(2).toInt(), 0, 65535); send_sensor_values(); return; }
  if (line.startsWith("i ")) { input_current_ma = (uint16_t) constrain(line.substring(2).toInt(), 0, 65535); send_sensor_values(); return; }
  if (line.startsWith("m ")) { set_master(line.substring(2).toInt() != 0, "Serial"); send_all_status(); return; }
  if (line.startsWith("o")) {
    const int space = line.indexOf(' ');
    if (space > 1) {
      const uint8_t output = (uint8_t) line.substring(1, space).toInt();
      const uint8_t percent = clamp_percent(line.substring(space + 1).toFloat());
      set_output_level(output, percent, "Serial");
      send_all_status();
      return;
    }
  }
  Serial.println("Unknown command. Type help.");
}

void poll_serial() {
  static String line;
  while (Serial.available()) {
    char ch = (char) Serial.read();
    if (ch == '\n' || ch == '\r') {
      if (line.length()) { handle_serial_line(line); line = ""; }
    } else {
      line += ch;
      if (line.length() > 80) line = "";
    }
  }
}

bool start_can() {
  twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(CAN_TX_PIN, CAN_RX_PIN, TWAI_MODE_NORMAL);
  twai_timing_config_t t_config = TWAI_TIMING_CONFIG_250KBITS();
  twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();
  g_config.rx_queue_len = 64;
  g_config.tx_queue_len = 32;
  esp_err_t err = twai_driver_install(&g_config, &t_config, &f_config);
  if (err != ESP_OK) { Serial.printf("twai_driver_install failed: %d\n", (int) err); return false; }
  err = twai_start();
  if (err != ESP_OK) { Serial.printf("twai_start failed: %d\n", (int) err); return false; }
  Serial.println("CAN/TWAI started at 250 kbit/s");
  return true;
}

void update_hold_dim() {
  const uint32_t now = millis();
  for (uint8_t output = 1; output <= 10; output++) {
    if (hold_dim_direction[output] == 0) continue;
    if (now - hold_dim_last_step_ms[output] < HOLD_DIM_STEP_MS) continue;
    hold_dim_last_step_ms[output] = now;
    int next = (int) output_levels[output] + (hold_dim_direction[output] > 0 ? HOLD_DIM_STEP_PERCENT : -HOLD_DIM_STEP_PERCENT);
    if (next <= 0) { next = 0; hold_dim_direction[output] = 0; }
    if (next >= 100) { next = 100; hold_dim_direction[output] = 0; }
    output_levels[output] = (uint8_t) next;
    send_output_levels();
    send_output_activity();
    send_channel_status();
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println("REDARC TVMS Rogue Emulator - Arduino IDE");
  Serial.printf("Source address: 0x%02X\n", SOURCE_ADDRESS);
  Serial.println("Type help for serial commands.");
  start_can();
  send_identity();
  send_all_status();
}

void loop() {
  receive_can();
  poll_serial();
  update_hold_dim();
  const uint32_t now = millis();
  if (now - last_identity_ms >= IDENTITY_INTERVAL_MS) { last_identity_ms = now; send_identity(); }
  if (now - last_status_ms >= STATUS_INTERVAL_MS) { last_status_ms = now; send_all_status(); }
}
