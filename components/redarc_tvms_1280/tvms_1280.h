#pragma once
#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/redarc_common/redarc_common.h"
#include <array>
#include <vector>

namespace esphome {
namespace redarc_tvms_1280 {

class TVMS1280Component;

class TVMS1280Switch : public switch_::Switch {
 public:
  void set_parent(TVMS1280Component *parent) { this->parent_ = parent; }
  void set_output_number(uint8_t n) { this->output_number_ = n; }
  void set_channel(uint8_t ch) { this->channel_ = ch; }
  void set_is_inverter(bool v) { this->is_inverter_ = v; }
  void write_state(bool state) override;
  uint8_t output_number() const { return this->output_number_; }
  uint8_t channel() const { return this->channel_; }
  bool is_inverter() const { return this->is_inverter_; }
 protected:
  TVMS1280Component *parent_{nullptr};
  uint8_t output_number_{0};
  uint8_t channel_{0x04};
  bool is_inverter_{false};
};

class TVMS1280Component : public Component {
 public:
  void set_source_address(uint8_t sa) { this->source_address_ = sa; }
  void set_host_address(uint8_t ha) { this->host_address_ = ha; }
  void set_filter_interval_ms(uint32_t ms) { this->filter_interval_ms_ = ms; }
  void set_temp1_sensor(sensor::Sensor *s) { this->temp1_sensor_ = s; }
  void set_temp2_sensor(sensor::Sensor *s) { this->temp2_sensor_ = s; }
  void set_supply_voltage_sensor(sensor::Sensor *s) { this->supply_voltage_sensor_ = s; }
  void set_voltage_input1_sensor(sensor::Sensor *s) { this->voltage_input1_sensor_ = s; }
  void set_voltage_input2_sensor(sensor::Sensor *s) { this->voltage_input2_sensor_ = s; }
  void set_tank_sensor(uint8_t tank, sensor::Sensor *s) { if (tank >= 1 && tank <= 6) this->tank_sensors_[tank] = s; }
  void set_last_command_channel_sensor(sensor::Sensor *s) { this->last_command_channel_sensor_ = s; }
  void set_last_command_state_sensor(sensor::Sensor *s) { this->last_command_state_sensor_ = s; }

  void setup() override;
  void register_output_switch(TVMS1280Switch *sw);
  void register_inverter_switch(TVMS1280Switch *sw) { this->inverter_switch_ = sw; }
  void register_master_switch(TVMS1280Switch *sw) { this->master_switch_ = sw; }
  void send_channel(uint8_t channel, bool state);
  void handle_can_frame(uint32_t can_id, const std::vector<uint8_t> &data);
  void dump_config() override;

 protected:
  void publish_output_(uint8_t output_number, bool state);
  void publish_channel_(uint8_t channel, bool state);

  uint8_t source_address_{0x24};
  uint8_t host_address_{0x20};
  uint32_t filter_interval_ms_{5000};
  uint32_t last_input_status_ms_{0};
  uint32_t last_mux_0x14_ms_{0};
  uint32_t last_mux_0x11_ms_{0};
  uint32_t last_mux_0x17_ms_{0};
  uint32_t last_mux_0x1A_ms_{0};
  uint32_t command_id_{0};
  std::array<TVMS1280Switch *, 10> output_switches_{};
  TVMS1280Switch *inverter_switch_{nullptr};
  TVMS1280Switch *master_switch_{nullptr};
  sensor::Sensor *temp1_sensor_{nullptr};
  sensor::Sensor *temp2_sensor_{nullptr};
  sensor::Sensor *supply_voltage_sensor_{nullptr};
  sensor::Sensor *voltage_input1_sensor_{nullptr};
  sensor::Sensor *voltage_input2_sensor_{nullptr};
  std::array<sensor::Sensor *, 7> tank_sensors_{};
  sensor::Sensor *last_command_channel_sensor_{nullptr};
  sensor::Sensor *last_command_state_sensor_{nullptr};
};

}  // namespace redarc_tvms_1280
}  // namespace esphome
