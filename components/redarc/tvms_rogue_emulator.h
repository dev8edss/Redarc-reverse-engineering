#pragma once

#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/redarc/redarc_common.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "tvms_rogue_emulator_light.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace esphome {
namespace redarc_tvms_rogue_emulator {

class TVMSRogueEmulatorComponent : public Component {
 public:
  struct VersionRecord { uint16_t product_number; uint8_t major; uint8_t minor; uint8_t record_index; };

  void setup() override;
  void dump_config() override;

  void set_source_address(uint8_t v) { source_address_ = v; }
  void set_identity_interval_ms(uint32_t v) { identity_interval_ms_ = v; }
  void set_status_interval_ms(uint32_t v) { status_interval_ms_ = v; }
  void set_random_update_interval_ms(uint32_t v) { random_update_interval_ms_ = v; }
  void set_randomize_inputs(bool v) { randomize_inputs_ = v; }
  void set_serial_prefix(uint32_t v) { serial_prefix_ = v; }
  void set_serial_suffix(uint16_t v) { serial_suffix_ = v; }
  void set_device_subtype(uint8_t v) { device_subtype_ = v; }
  void add_version_record(uint16_t p, uint8_t a, uint8_t b, uint8_t i) { version_records_.push_back({p, a, b, i}); }
  void set_manufacturing_date(uint8_t d, uint8_t m, uint16_t y) { manufacturing_day_ = d; manufacturing_month_ = m; manufacturing_year_ = y; }
  void set_product_name(const std::string &v) { product_name_ = v; }
  void set_unique_identifier(uint8_t a, uint8_t b, uint8_t c, uint8_t d, uint8_t e, uint8_t f, uint8_t g) { unique_identifier_ = {a,b,c,d,e,f,g}; }
  void set_unique_identifier_record_index(uint8_t v) { unique_identifier_record_index_ = v; }

  void set_tank1_sensor(sensor::Sensor *v) { tank1_sensor_ = v; }
  void set_tank2_sensor(sensor::Sensor *v) { tank2_sensor_ = v; }
  void set_input_voltage_sensor(sensor::Sensor *v) { input_voltage_sensor_ = v; }
  void set_input_current_sensor(sensor::Sensor *v) { input_current_sensor_ = v; }
  void set_output_status_text_sensor(text_sensor::TextSensor *v) { output_status_text_sensor_ = v; }
  void set_level_sensor(uint8_t n, sensor::Sensor *v) { if (n >= 1 && n <= 10) level_sensors_[n] = v; }
  void set_input_sensor(uint8_t n, binary_sensor::BinarySensor *v) { if (n >= 1 && n <= 8) input_sensors_[n] = v; }
  void register_light(uint8_t n, TVMSRogueEmulatorLight *v) { if (n >= 1 && n <= 10) lights_[n] = v; }

  void set_output_from_home_assistant(uint8_t output, float percent);
  void handle_can_frame(uint32_t can_id, const std::vector<uint8_t> &data);

 protected:
  void send_frame_(uint32_t can_id, const std::vector<uint8_t> &data);
  void send_identity_();
  void send_firmware_versions_();
  void send_manufacturing_date_();
  void send_product_name_();
  void send_unique_identifier_();

  bool load_configuration_object_();
  void patch_configuration_object_();
  void update_configuration_crc_();
  static int8_t base64_value_(char value);
  static void write_u32_le_(std::vector<uint8_t> &target, size_t offset, uint32_t value);

  void send_service_ack_(uint8_t requester, uint8_t opcode);
  void send_direct_ack_(uint8_t requester, uint8_t command);
  void send_empty_block_trailer_(uint8_t requester);
  void send_object_read_block_(uint8_t requester, const std::vector<uint8_t> &request);
  static uint32_t crc32c_(const uint8_t *data, size_t length);

  void randomize_sensor_values_();
  void publish_all_home_assistant_states_();
  void publish_output_status_();
  void set_output_level_(uint8_t output, uint8_t percent, const char *origin);
  void set_master_state_(bool state, const char *origin);
  void set_input_state_(uint8_t input, bool state, const char *origin);
  void send_all_status_(const char *reason);
  void send_channel_status_();
  void send_output_levels_();
  void send_sensor_values_();
  void send_output_capabilities_();
  void send_output_activity_();

  uint8_t source_address_{0x30};
  uint32_t identity_interval_ms_{1000};
  uint32_t status_interval_ms_{1000};
  uint32_t random_update_interval_ms_{5000};
  bool randomize_inputs_{true};
  uint32_t serial_prefix_{0};
  uint16_t serial_suffix_{1};
  uint8_t device_subtype_{0};

  std::vector<VersionRecord> version_records_;
  uint8_t manufacturing_day_{1};
  uint8_t manufacturing_month_{1};
  uint16_t manufacturing_year_{2026};
  std::string product_name_{"TVMS Rogue"};
  std::array<uint8_t, 7> unique_identifier_{{0,0,0,0,0,0,1}};
  uint8_t unique_identifier_record_index_{0};
  std::vector<uint8_t> configuration_object_;
  uint8_t selected_object_{0xFF};

  std::array<uint8_t, 11> output_levels_{{0}};
  std::array<bool, 9> input_states_{{false}};
  bool master_state_{false};
  uint8_t tank1_percent_{50};
  uint8_t tank2_percent_{75};
  uint16_t input_voltage_mv_{13500};
  uint16_t input_current_ma_{2500};

  std::array<TVMSRogueEmulatorLight *, 11> lights_{{nullptr}};
  std::array<sensor::Sensor *, 11> level_sensors_{{nullptr}};
  std::array<binary_sensor::BinarySensor *, 9> input_sensors_{{nullptr}};
  sensor::Sensor *tank1_sensor_{nullptr};
  sensor::Sensor *tank2_sensor_{nullptr};
  sensor::Sensor *input_voltage_sensor_{nullptr};
  sensor::Sensor *input_current_sensor_{nullptr};
  text_sensor::TextSensor *output_status_text_sensor_{nullptr};
  std::string last_output_status_;
};

class TVMSRogueActiveEmulatorComponent : public TVMSRogueEmulatorComponent {
 public:
  void setup() override;
  void dump_config() override;
  void handle_can_frame(uint32_t can_id, const std::vector<uint8_t> &data);
};

}  // namespace redarc_tvms_rogue_emulator
}  // namespace esphome
